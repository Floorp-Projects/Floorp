/* $Id: saving.cpp,v 1.1 1998/09/25 18:01:41 ramiro%netscape.com Exp $
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

#include "QtBrowserContext.h"
#include "DialogPool.h"
#include "SaveAsDialog.h"
#include "streaming.h"

#include <fe_proto.h>
#include <xlate.h>
#include <xpgetstr.h>
#include <xp_str.h>

#include <qdialog.h>
#include <qmainwindow.h>

extern void fe_RaiseSynchronousURLDialog (MWContext *context, QWidget* parent,
					  const char *title);
extern int fe_AwaitSynchronousURL( MWContext* context );
void fe_LowerSynchronousURLDialog (MWContext *context);
bool QTFE_StatReadWrite( const char *file_name, bool isFileP, bool existsP );
void fe_save_as_complete( PrintSetup* ps );

extern int QTFE_ERROR_READ_ONLY;
extern int QTFE_ERROR_OPENING_FILE;

#if defined(_CC_MSVC_)
#define stat _stat
#endif


static char* get_URLBasename(const char* url);


void QtBrowserContext::cmdSaveAs()
{
   int   res;
   char* basename = 0;
   URL_Struct* url = SHIST_CreateWysiwygURLStruct(mwContext(),
                                                  SHIST_GetCurrent (&mwContext()->hist));
   if( !dialogpool->saveasdialog )
      dialogpool->saveasdialog = new SaveAsDialog( this, browserWidget );
   if (url)
      basename = get_URLBasename(url->address);
#ifdef DEBUG
   fprintf( stderr, "Setting as suggested name: %s\n", basename );
#endif
   dialogpool->saveasdialog->setSelection(basename); 
   res = dialogpool->saveasdialog->exec();
   free(basename);
   if(res == QDialog::Accepted) 
   {
      save_as_data* sad = make_save_as_data(mwContext(), false, 
                                            dialogpool->saveasdialog->type(), 
                                            url, 
                                            dialogpool->saveasdialog->selectedFile() );
      if (sad)
         fe_save_as_nastiness (this, url, sad, TRUE);
      else
         goto error_exit;
   }
   else 
       goto error_exit;
   return;
  error_exit:
   NET_FreeURLStruct (url);
}


void fe_save_as_complete( PrintSetup* ps )
{
    MWContext* context = (MWContext*) ps->carg;

    fclose( ps->out );

    fe_LowerSynchronousURLDialog( context );
    
    free( ps->filename );
    NET_FreeURLStruct( ps->url );
}


// Note:  isFileP - if true and the file_name is a directory,
//                  we return False...
//
//        existsP - if true, the file_name must exist or we return False,
//                  otherwise we just make sure we can read/write into the 
//                  directory specified in file_name...
//
bool
QTFE_StatReadWrite( const char *file_name, bool isFileP, bool existsP ) 
{
    // NOTE:  let's get the directory that this file resides in...
    //
    XP_StatStruct  dir_info;
    XP_StatStruct  file_info;
    bool        isDir = false;
    char*          p;
    char*          base_name = (char *) XP_ALLOC(XP_STRLEN(file_name) + 1);
    
    p = XP_STRRCHR(file_name, '/');
    
    if (p) {
	if (p == file_name) {
	    XP_STRCPY(base_name, "/");
	}
	else {
	    uint32 len = p - file_name;
	    
	    if (len == XP_STRLEN(file_name)) {
		isDir = true;
		XP_STRCPY(base_name, file_name);
	    }
	    else {
		XP_MEMCPY(base_name, file_name, len);
		base_name[len] = '\0';
	    }
	}
    }
    else {
	// WARNING... [ we may get into some trouble with this one ]
	//
	XP_STRCPY(base_name, ".");
    }
    
    // NOTE:  we need to verify that we can read/write the directory
    //        before we check the file...
    //
    //        - need to check all three mode groups... [ owner, group, other ]
    //
    if ( stat(base_name, &dir_info) >= 0 ) {
	bool gotcha = false;
	
	if ( isFileP && 
	     isDir
	     ) {
	    // NOTE:  the specifed file_name is a directory... [ fail ]
	    //
	    XP_FREE(base_name);
	    
	    return false;
	}
	
#if defined(XP_UNIX)
	if (dir_info.st_uid == getuid()) {
	    if ((dir_info.st_mode & S_IRUSR) &&
		(dir_info.st_mode & S_IWUSR)) {
		gotcha = true;
	    }
	}
	
	if (!gotcha) {
	    if (dir_info.st_gid == getgid()) {
		if ((dir_info.st_mode & S_IRGRP) &&
		    (dir_info.st_mode & S_IWGRP)) {
		    gotcha = true;
		}
	    }
	    
	    if (!gotcha) {
		if ((dir_info.st_mode & S_IROTH) &&
		    (dir_info.st_mode & S_IWOTH)) {
		    gotcha = true;
		}
	    }
	}
#elif defined(XP_WIN)
	gotcha = TRUE;
#endif
	if (gotcha) {
	    if (isDir) {
				// NOTE: we're done... [ we can read/write this directory ]
				//
		XP_FREE(base_name);
		
		return true;
	    }
	    else {
				// NOTE:  let's do the same thing for the file...
				//
		if ( stat(file_name, &file_info) >= 0 ) {
		    gotcha = false;
		    
		    if (isFileP &&
			(file_info.st_mode & S_IFDIR)) {
			// NOTE:  the specifed file_name is a directory...
			//
			XP_FREE(base_name);
			
			return false;
		    }
		    
#if defined(XP_UNIX)
		    if (file_info.st_uid == getuid()) {
			if ((file_info.st_mode & S_IRUSR) &&
			    (file_info.st_mode & S_IWUSR)) {
			    gotcha = true;
			}
		    }
		    
		    if (!gotcha) {
			if (file_info.st_gid == getgid()) {
			    if ((file_info.st_mode & S_IRGRP) &&
				(file_info.st_mode & S_IWGRP)) {
				gotcha = true;
			    }
			}
			
			if (!gotcha) {
			    if ((file_info.st_mode & S_IROTH) &&
				(file_info.st_mode & S_IWOTH)) {
				gotcha = true;
			    }
			}
		    }
#elif defined(XP_WIN)
		    gotcha = TRUE;
#endif
		    if (gotcha) {
			// NOTE:  we can read/write this directory/file...
			//
			XP_FREE(base_name);
			
			return true;
		    }
		    else {
			// NOTE:  we failed the file check...
			//
			XP_FREE(base_name);
			
			return false;
		    }
		}
		else {
		    if (existsP) {
			// NOTE:  failed to stat the file...
			//
			XP_FREE(base_name);
			
			return false;
		    }
		    else {
			// NOTE:  we can read/write this directory...
			//
			XP_FREE(base_name);
			
			return true;
		    }
		}
	    }
	}
	else {
	    // NOTE:  we failed the directory check...
	    //
	    XP_FREE(base_name);
	    
	    return false;
	}
    }
    else {
	// NOTE:  failed to stat the directory...
	//
	XP_FREE(base_name);
	
	return false;
    }
}

// Strips URL address from path and returns the basename
// Warning, allocates string that must be deallocated.
static char* get_URLBasename(const char* url)
{
   const char* def;
   char*       basename;
   char*       tail;

   if (url==0)
      return 0; // No address given, return immediately. 

   if ((def = strrchr (url, '/')))
      def++;
   else
      def = url;
   basename = strdup(def);
   
   //   Grab the file name part and map out the characters which ought not go into file names.
   //   Also, terminate the file name at ? or #, since those aren't
   //   really a part of the URL's file, but are modifiers to it.
   for (tail = basename; *tail; tail++)
   {
      if (*tail == '?' || *tail == '#')
      {
         *tail = 0;
         break;
      }
      else if (*tail < '+' || *tail > 'z' ||
               *tail == ':' || *tail == ';' ||
               *tail == '<' || *tail == '>' ||
               *tail == '\\' || *tail == '^' ||
               *tail == '\'' || *tail == '`' ||
               *tail == ',')
         *tail = '_';
   }
   return basename;
}
