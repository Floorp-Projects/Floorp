/* $Id: misc.cpp,v 1.1 1998/09/25 18:01:36 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include <fe_proto.h>
#include <xp_file.h>
#include <xlate.h>
#include "qtfe.h"

#if 0
#include <bkmks.h>
#endif
#include <xp_file.h>
#include <libi18n.h>
#include <fe_rgn.h>
#include "csid.h"

#define Boolean XP_Bool

#if defined(XP_WIN)
#define _RESDEF_H_
#define RES_OFFSET 7000
#undef  RES_START
#define RES_START
#undef  BEGIN_STR
#define BEGIN_STR(arg) static char *(arg)(int16 i) { switch (i) {
#undef  ResDef
#define ResDef(name,id,msg)     case (id)+RES_OFFSET: return (msg);
#undef  END_STR
#define END_STR(arg) } return NULL; }
#else
#define RESOURCE_STR
#endif
#include "qtfe_err.h"

static void useArgs( const char *fn, ... )
{
    if (0&&fn)
	printf( "%s\n", fn );
}


/* From ./xfe.c: */
extern "C" 
const char *
FE_UsersMailAddress(void)
{
    useArgs( "FE_UsersMailAddress \n" );
    return "unknown@unknown";
}

/* From ./xfe.c: */
extern "C" 
const char *
FE_UsersFullName(void)
{
    useArgs( "FE_UsersFullName \n" );
    return "Unknown";
}

/* From ./xfe.c: */
extern "C" 
const char *
FE_UsersSignature(void)
{
    useArgs( "FE_UsersSignature \n" );
    return "This is not a signature";
}

/* From ./editor.c: */
extern "C" 
Boolean FE_EditorPrefConvertFileCaseOnWrite(void)
{
    useArgs( "Boolean FE_EditorPrefConvertFileCaseOnWrite");
    return 0;
}

/* From ./src/MozillaApp.cpp: */

/*
 * Returns a string representing the default installed location of
 * the NetHelp directory as a file:// URL.  The caller is expected to
 * free the returned string.  Used by mkhelp.c to retrieve NetHelp content.
 */
extern "C" 
char * FE_GetNetHelpDir()
{
    useArgs( "FE_GetNetHelpDir" );
    return "file://~hanord/images/scientific";
}

/* From ./src/MozillaApp.cpp: */

/*
 * Returns the "default" MWContext to use when bringing up a nethelp
 * topic.  This should try to return the context associated with the topmost
 * visible window.
 */
extern "C" 
MWContext *FE_GetNetHelpContext()
{
    useArgs( "FE_GetNetHelpContext");
    return 0;
}

/* Header file comment: */
/*
 * If the Netcaster component is not currently running, starts it and opens
 * the Netcaster drawer.  If the component is running, but it does not have
 * focus (it's not on top), or the drawer is closed, brings the window to top
 * and opens the drawer.  If the component is running, has focus, and is open,
 * does nothing.
 */
/* From ./src/context_funcs.cpp: */
extern "C" 
void FE_RunNetcaster(MWContext * )
{}

extern "C" void fe_sgiStart()
{
}

/* From ./icons.c: */
extern "C" 
void
FE_GetPSIconDimensions(int icon_number, int *width, int *height)
{
    useArgs( "FE_GetPSIconDimensions", icon_number, width, height);
}

extern "C"
XP_Bool
FE_GetPSIconData(int icon_number, IL_Pixmap *image, IL_Pixmap *mask)
{
    useArgs( "FE_GetPSIconData", icon_number, image, mask);
    return 0;
}

/*
 * Returns true if Netcaster is installed, false otherwise.
 */
extern "C"
XP_Bool FE_IsNetcasterInstalled(void)
{
    useArgs( "XP_Bool FE_IsNetcasterInstalled");
    return 0;
}

#ifdef XP_UNIX
extern "C"
void FE_ShowMinibuffer(MWContext *)
{
}
#endif                  

#ifdef XP_UNIX
//extern "C"
char **fe_encoding_extensions=0; // This is global - used in lib/libnet/mkcache.c
#endif

/* asks the user if they want to stream data.
 * -1 cancel    
 * 0  No, don't stream data, play from the file
 * 1  Yes, stream the data from the network
 */
extern "C"
int FE_AskStreamQuestion(MWContext * window_id)
{
    useArgs( "FE_AskStreamQuestion", window_id);
    return 0;
}

// Called from modules/rdf/src/glue.c
extern "C"
MWContext *FE_GetRDFContext(void)
{
    useArgs( "MWContext *FE_GetRDFContext");
    return 0;
}

// Called from lib/layout/edtsave.cpp
extern "C"
ED_SaveOption FE_SaveFileExistsDialog( MWContext *pContext, char* pFilename )
{
    useArgs( "FE_SaveFileExistsDialog",  pContext, pFilename );
    return (ED_SaveOption) 0;
}



/*
 * return a valid local base file name for the given URL.  If there is no
 *  base name, return 0.
*/
extern "C"
PUBLIC char*  FE_URLToLocalName ( char* )
{
    printf( "FE_URLToLocalName \n" );
    return "/home/agulbra/tmp/sex";
}

/* Similar to above, but dialog to ask user if they want to save changes
 *    should have "AutoSave" caption and extra line to
 *    tell them "Cancel" will turn off Autosave until they
 *    save the file later.
*/
extern "C"
Bool  FE_CheckAndAutoSaveDocument (MWContext *pMWContext)
{
    printf( "FE_CheckAndAutoSaveDocument %p \n", pMWContext );
    return TRUE;
}


/* Given a URL, this might return a better suggested name to save it as.

When you have a URL, you can sometimes get a suggested name from
   URL_s->content_name, but if you're saving a URL to disk before the
   URL_Struct has been filled in by netlib, you don't have that yet.

So if you're about to prompt for a file name *before* you call FE_GetURL
   with a format_out of FO_SAVE_AS, call this function first to see if it
   can offer you advice about what the suggested name for that URL should be.

(This works by looking in a cache of recently-displayed MIME objects, and
   seeing if this URL matches.  If it does, the remembered content-name will
   be used.)
 */
#if 0
Temporary suspended until I know why this function is duplicated
extern "C"
char * MimeGuessURLContentName (MWContext *context, const char *url)
{
    printf( "MimeGuessURLContentName %p, %s\n", context, url );
    return 0;
}
#endif

/* If pszLocalName is not NULL, we return the full pathname
 *   in local platform syntax. Caller must free this string.
 * Returns TRUE if file already exists
 * Windows version implemented in winfe\fegui.cpp
 */
extern "C"
Bool  XP_ConvertUrlToLocalFile (const char * szURL, char **pszLocalName)
{
    printf( "XP_ConvertUrlToLocalFile %s, %p\n", szURL, pszLocalName );
    return 0;
}

/* Construct a temporary filename in same directory as supplied "file:///" type URL
 * Used as write destination for saving edited document
 * User must free string.
 * Windows version implemented in winfe\fegui.cpp
 */
extern "C"
char *  XP_BackupFileName ( const char * szURL )
{
    printf( "XP_BackupFileName %s \n", szURL );
    return 0;
}

extern "C"
XP_Bool FE_FindCommand(MWContext *context, char *szName,
		       XP_Bool bCaseSensitve,
		       XP_Bool bBackwards, XP_Bool bWrap)
{
    printf( "FE_FindCommand %p, %s, %d, %d, %d  \n",
	    context, szName, (int)bCaseSensitve,
	    (int)bBackwards, (int)bWrap );
    return 0;
}

// Called from modules/libpref/src/unix/unixpref.c, where the
// alternative is direct Motif calls.
//
extern "C"
XP_Bool
FE_GetLabelAndMnemonic(char* name, char** str, void* v_xm_str, void* v_mnemonic)
{
    printf( "FE_GetLabelAndMnemonic %s, %p, %p, %p\n",
	    name, str, v_xm_str, v_mnemonic);
    return 0;
}


/**
 * Get a cross platform string resource by ID.
 *
 * This function makes localization easier for cross platform strings.
 * The cross platfrom string resources are defined in allxpstr.h.
 * You should use XP_GetString when:
 * <OL>
 * <LI>Any human readable string that not in front-end
 * <LI>With the exception of HTML strings (use XP_GetStringForHTML)
 * <LI>The translator/localizer will then translate the string defined
 * in allxpstr.h and show the translated version to user.
 * </OL>
 * The caller should make a copy of the returned string if it needs to use
 * it for a while. The same memory buffer will be used to store
 * another string the next time this function is called. The caller
 * does not need to free the memory of the returned string.
 * @param id	Specifies the string resource ID
 * @return		Localized (translated) string
 * @see XP_GetStringForHTML
 * @see INTL_ResourceCharSet
 */

extern "C" char *XP_GetBuiltinString(int16 i);

extern "C"
PUBLIC char * XP_GetString (int id)
{
#if defined(XP_WIN)
    static char buf[256];
#endif
    char * ret = mcom_cmd_qtfe_qtfe_err_h_strings (id + RES_OFFSET);
    if ( !ret ) {
#if 0
	sprintf( buf, "XP_GetString: %d", id );
	ret = buf;
#else
	ret = XP_GetBuiltinString(id);
#endif
    }
    return ret;
}

