/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/**********************************************************************
 e_kit.c
 By Daniel Malmer

 Implementation for client customization.
**********************************************************************/


#include "mozilla.h"
#include "xfe.h"
#include "xpgetstr.h"
#include "e_kit.h"
#include "e_kit_patch.h"
#include "prefapi.h"
#include "AnimationType.h"

/* Messages declared in xfe_err.h */
extern int XFE_EKIT_LOCK_FILE_NOT_FOUND;
extern int XFE_EKIT_MODIFIED;
extern int XFE_EKIT_CANT_OPEN;
extern int XFE_EKIT_FILESIZE;
extern int XFE_EKIT_CANT_READ;
extern int XFE_ANIM_CANT_OPEN;
extern int XFE_ANIM_READING_SIZES;
extern int XFE_ANIM_READING_NUM_COLORS;
extern int XFE_ANIM_READING_COLORS;
extern int XFE_ANIM_READING_FRAMES;
extern int XFE_ANIM_NOT_AT_EOF;
extern int XFE_EKIT_ABOUT_MESSAGE;

static char* get_database_filename(Widget top_level);
static XP_Bool read_lock_file(Widget top_level);

static int ekit_loaded = 0;

struct fe_icon_data* anim_custom_large = NULL;
struct fe_icon_data* anim_custom_small = NULL;

static int ekit_errno = 0;

/*
 * ekit_error
 * Pops up an error dialog if an ekit error has occured.
 * Error strings are found in 'xfe_err.h'.
 */
static void
ekit_error(void)
{
  if ( ekit_errno ) {
    FE_Alert(NULL, XP_GetString(ekit_errno));
  }
}


/*
 * ekit_Initialize
 */
void
ekit_Initialize(Widget toplevel)
{
    /* If this is an e-kit version, modify resource database */
    if ( (ekit_loaded = read_lock_file(toplevel)) == False ) {
        if ( ekit_required() ) {
            ekit_error();
            exit(0);
        }
    }

    ekit_LoadCustomAnimation();
 
    if ( ekit_AutoconfigUrl() ) {
      NET_SetNoProxyFailover();
    }
}




#if 0
/*
 * ekit_LockPrefs
 * First, set resources back to their default values.
 * If this is an Enterprise Kit binary, try to read in
 * the lock file.  If it doesn't exist, or has been changed,
 * complain and die.
 */
void
ekit_LockPrefs(void)
{
    if ( ekit_Enabled() == FALSE || ekit_loaded == FALSE ) return;
 
    update_preferences();
 
    if ( ekit_AnimationFile() ) {
      if ( load_animation(ekit_AnimationFile()) == False ) {
        ekit_error();
      }
    }
 
    if ( ekit_AutoconfigUrl() ) {
      NET_SetNoProxyFailover();
    }
}
#endif


/*
 * ekit_UserAgent
 */
char*
ekit_UserAgent(void)
{
    char* user_agent = NULL;

    PREF_CopyCharPref("user_agent", &user_agent);
    
    return user_agent;
}


/*
 * ekit_LogoUrl
 */
char*
ekit_LogoUrl(void)
{
    char* logo_url = NULL;

    PREF_CopyCharPref("toolbar.logo.url", &logo_url);
    
    return logo_url;
}


/*
 * ekit_AnimationFile
 */
char*
ekit_AnimationFile(void)
{
    char* animation_file = NULL;

    PREF_CopyCharPref("x_animation_file", &animation_file);
    
    return animation_file;
}


/*
 * ekit_Enabled
 */
XP_Bool
ekit_Enabled(void)
{
    return TRUE;
}


/*
 * ekit_AutoconfigUrl
 */
char*
ekit_AutoconfigUrl(void)
{
    char* autoconfig_url = NULL;

    if ( fe_IsPolarisInstalled() ) {
        PREF_CopyCharPref("autoadmin.global_config_url", &autoconfig_url);
    }
    
    return autoconfig_url;
}


/*
 * get_database_filename
 * Looks for the Netscape.cfg file.
 * "path" represents the search path for the config file.
 * %E is replaced with the root directory that is specified
 * at configure time by the e-kit locker, which is a binary
 * patch program.
 */
static char*
get_database_filename(Widget top_level)
{
    String filename;
    SubstitutionRec sub = {'E', NULL};
    Cardinal num_substitutions = 1;
    String path = "%E/%L/%T/%N%C%S:"
                  "%E/%l/%T/%N%C%S:"
                  "%E/%T/%N%C%S:"
                  "%E/%L/%T/%N%S:"
                  "%E/%l/%T/%N%S:"
                  "%E/%T/%N%S";
 
    sub.substitution = ekit_root();
 
    filename = XtResolvePathname(XtDisplay(top_level),
                                 "app-defaults",
                                 "netscape",
                                 ".cfg",
                                 path,
                                 &sub,
                                 num_substitutions,
                                 NULL);
 
    return filename;
}


/*
 * read_lock_file
 * If there is a locked resource file 'netscape.cfg', read it in.
 */
static XP_Bool
read_lock_file(Widget top_level)
{
    String filename;
 
    if ( (filename = get_database_filename(top_level)) == NULL ) {
        return False;
    }
 
    return ( PREF_ReadLockFile(filename) == PREF_NOERROR );
}
 

/*
 * ekit_AboutData
 */
char*
ekit_AboutData(void)
{
    return NULL;
}


/*
 * ekit_LoadCustomUrl
 */
void
ekit_LoadCustomUrl(char* prefix, MWContext* context)
{
    char* url;

    if ( PREF_GetUrl(prefix, &url) ) {
        FE_GetURL(context, NET_CreateURLStruct(url, FALSE));
    }
}


/*
 * read_frames
 * Reads animation frames from the animation input file.
 */
static int
read_frames(FILE* file, int num_frames, int width, int height,
                        struct fe_icon_data* anim)
{
    int i;
    int j;
    int n = width*height;
    int bytes_per_line = (width+7)/8;
    int total_bytes = bytes_per_line*height;
 
    for ( i = 0; i < num_frames; i++ ) {
        anim[i].width = width;
        anim[i].height = height;
 
        anim[i].mono_bits = (uint8*) malloc(total_bytes);
        anim[i].mask_bits = (uint8*) malloc(total_bytes);
        anim[i].color_bits = (uint8*) malloc(n);
 
        if ( anim[i].mono_bits == NULL ||
             anim[i].mask_bits == NULL ||
             anim[i].color_bits == NULL ) return -1;

        if ( fread(anim[i].mono_bits, sizeof(uint8), total_bytes, file) !=
             total_bytes ) {
            return -1;
        }
 
        if ( fread(anim[i].mask_bits, sizeof(uint8), total_bytes, file) !=
             total_bytes ) {
            return -1;
        }
 
        if ( fread(anim[i].color_bits, sizeof(uint8), n, file) != n ) {
            return -1;
        }
 
        for ( j = 0; j < width*height; j++ ) {
            anim[i].color_bits[j]+= fe_n_icon_colors;
        }
    }
 
    return 0;
}


/*
 * read_colors
 * Reads color data from the animation data file.
 */
static int
read_colors(FILE* file, int num_colors)
{
    int i;
    uint16 r;
    uint16 g;
    uint16 b;
 
    for ( i = 0; i < num_colors; i++ ) {
        if ( fscanf(file, " %hx %hx %hx", &r, &g, &b) != 3 ) {
            return -1;
        }
        fe_icon_colors[fe_n_icon_colors+i][0] = r;
        fe_icon_colors[fe_n_icon_colors+i][1] = g;
        fe_icon_colors[fe_n_icon_colors+i][2] = b;
    }
 
    if ( getc(file) != '\n' ) {
      return -1;
    }
 
    return 0;
}


/*
 * load_animation
 * Given the name of an animation file, reads in data from
 * that file.
 */
static Boolean
load_animation(char* filename)
{
    int num_frames_small;
    int width_small;
    int height_small;
    int num_frames_large;
    int width_large;
    int height_large;
    int num_colors;
    FILE* file;
 
    if ( filename == NULL ) {
    return False;
    }
 
    if ( (file = fopen(filename, "r")) == NULL ) {
        ekit_errno = XFE_ANIM_CANT_OPEN;
        return False;
    }
 
    if ( fscanf(file, " %d %d %d ",
                      &num_frames_large,
                      &width_large,
                      &height_large) != 3 ||
        fscanf(file, " %d %d %d ",
                      &num_frames_small,
                      &width_small,
                      &height_small) != 3 || 
        num_frames_large != num_frames_small || num_frames_large == 0 ) {
        ekit_errno = XFE_ANIM_READING_SIZES;
        return False;
    }
 
    fe_anim_frames[XFE_ANIMATION_CUSTOM*2] = num_frames_large;
    fe_anim_frames[XFE_ANIMATION_CUSTOM*2+1] = num_frames_small;
 
    if ( fscanf(file, " %d ", &num_colors) != 1 ) {
        ekit_errno = XFE_ANIM_READING_NUM_COLORS;
        return False;
    }
 
    if ( read_colors(file, num_colors) == -1 ) {
        ekit_errno = XFE_ANIM_READING_COLORS;
        return False;
    }
 
    if ( num_frames_large ) {
      anim_custom_large = (struct fe_icon_data*)
                          malloc(sizeof(struct fe_icon_data) * num_frames_large);
      if ( anim_custom_large == NULL ) return False;
    }
 
    if ( num_frames_small ) {
      anim_custom_small = (struct fe_icon_data*)
                          malloc(sizeof(struct fe_icon_data) * num_frames_small);
      if ( anim_custom_small == NULL ) return False;
    }
 
    if ( read_frames(file, num_frames_large, width_large, height_large,
                     anim_custom_large) == -1 ||
         read_frames(file, num_frames_small, width_small, height_small,
                     anim_custom_small) == -1 ) {
        ekit_errno = XFE_ANIM_READING_FRAMES;
        return False;
    }
 
    fe_n_icon_colors+= num_colors;
 
    fscanf(file, " ");
 
    if ( !feof(file) ) {
      ekit_errno = XFE_ANIM_NOT_AT_EOF;
      return False;
    }
 
    return True;
}


static int custom_anim = 0;


/*
 * ekit_LoadCustomAnimation
 */
void
ekit_LoadCustomAnimation(void)
{
    char* anim_file = NULL;

    PREF_CopyConfigString("x_animation_file", &anim_file);

    if ( anim_file == NULL || *anim_file == '\0' ) return;

    if ( load_animation(anim_file) == 0 ) {
        ekit_error();
    } else {
        custom_anim = 1;
    }
}


/*
 * ekit_CustomAnimation
 */
Boolean
ekit_CustomAnimation(void)
{
    return custom_anim;
}


