/* $Id: qtfe_err.h,v 1.1 1998/09/25 18:01:38 ramiro%netscape.com Exp $
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

#ifndef __QTFE_QTFE_ERR_H_
#define __QTFE_QTFE_ERR_H_


#include <resdef.h>


#define QTFE_ERR_OFFSET 1000

RES_START
BEGIN_STR(mcom_cmd_qtfe_qtfe_err_h_strings)
ResDef(DEFAULT_ADD_BOOKMARK_ROOT, QTFE_ERR_OFFSET + 0, "End of List")
ResDef(DEFAULT_MENU_BOOKMARK_ROOT, QTFE_ERR_OFFSET + 1, "Entire List")
ResDef(QTFE_SAVE_AS_TYPE_ENCODING, QTFE_ERR_OFFSET + 3, "Save As... (type %.90s, encoding %.90s)")
ResDef(QTFE_SAVE_AS_TYPE, QTFE_ERR_OFFSET + 4, "Save As... (type %.90s)")
ResDef(QTFE_SAVE_AS_ENCODING, QTFE_ERR_OFFSET + 5, "Save As... (encoding %.90s)")
ResDef(QTFE_SAVE_AS, QTFE_ERR_OFFSET + 6, "Save As...")
ResDef(QTFE_ERROR_OPENING_FILE, QTFE_ERR_OFFSET + 7, "Error opening %.900s:")
ResDef(QTFE_ERROR_DELETING_FILE, QTFE_ERR_OFFSET + 8, "Error deleting %.900s:")
ResDef(QTFE_LOG_IN_AS, QTFE_ERR_OFFSET + 9, "When connected, log in as user `%.900s'")
ResDef(QTFE_OUT_OF_MEMORY_URL, QTFE_ERR_OFFSET + 10, "Out Of Memory -- Can't Open URL")
ResDef(QTFE_COULD_NOT_LOAD_FONT, QTFE_ERR_OFFSET + 11, "couldn't load:\n%s")
ResDef(QTFE_USING_FALLBACK_FONT, QTFE_ERR_OFFSET + 12, "%s\nNo other resources were reasonable!\nUsing fallback font \"%s\" instead.")
ResDef(QTFE_NO_FALLBACK_FONT, QTFE_ERR_OFFSET + 13, "%s\nNo other resources were reasonable!\nThe fallback font \"%s\" could not be loaded!\nGiving up.")
ResDef(QTFE_DISCARD_BOOKMARKS, QTFE_ERR_OFFSET + 14, "Bookmarks file has changed on disk: discard your changes?")
ResDef(QTFE_RELOAD_BOOKMARKS, QTFE_ERR_OFFSET + 15, "Bookmarks file has changed on disk: reload it?")
ResDef(QTFE_NEW_ITEM, QTFE_ERR_OFFSET + 16, "New Item")
ResDef(QTFE_NEW_HEADER, QTFE_ERR_OFFSET + 17, "New Header")
ResDef(QTFE_REMOVE_SINGLE_ENTRY, QTFE_ERR_OFFSET + 18, "Remove category \"%.900s\" and its %d entry?")
ResDef(QTFE_REMOVE_MULTIPLE_ENTRIES, QTFE_ERR_OFFSET + 19, "Remove category \"%.900s\" and its %d entries?")
ResDef(QTFE_EXPORT_BOOKMARK, QTFE_ERR_OFFSET + 20, "Export Bookmark")
ResDef(QTFE_IMPORT_BOOKMARK, QTFE_ERR_OFFSET + 21, "Import Bookmark")
ResDef(QTFE_SECURITY_WITH, QTFE_ERR_OFFSET + 22, "This version supports %s security with %s.")
ResDef(QTFE_SECURITY_DISABLED, QTFE_ERR_OFFSET + 23, "Security disabled")
ResDef(QTFE_WELCOME_HTML, QTFE_ERR_OFFSET + 24, "file:/usr/local/lib/netscape/docs/Welcome.html")
ResDef(QTFE_DOCUMENT_DONE, QTFE_ERR_OFFSET + 25, "Document: Done.")
ResDef(QTFE_OPEN_FILE, QTFE_ERR_OFFSET + 26, "Open File")
ResDef(QTFE_ERROR_OPENING_PIPE, QTFE_ERR_OFFSET + 27, "Error opening pipe to %.900s")
ResDef(QTFE_WARNING, QTFE_ERR_OFFSET + 28, "Warning:\n\n")
ResDef(QTFE_FILE_DOES_NOT_EXIST, QTFE_ERR_OFFSET + 29, "%s \"%.255s\" does not exist.\n")
ResDef(QTFE_HOST_IS_UNKNOWN, QTFE_ERR_OFFSET + 30, "%s \"%.255s\" is unknown.\n")
ResDef(QTFE_NO_PORT_NUMBER, QTFE_ERR_OFFSET + 31, "No port number specified for %s.\n")
ResDef(QTFE_MAIL_HOST, QTFE_ERR_OFFSET + 32, "Mail host")
ResDef(QTFE_NEWS_HOST, QTFE_ERR_OFFSET + 33, "News host")
ResDef(QTFE_NEWS_RC_DIRECTORY, QTFE_ERR_OFFSET + 34, "News RC directory")
ResDef(QTFE_TEMP_DIRECTORY, QTFE_ERR_OFFSET + 35, "Temporary directory")
ResDef(QTFE_FTP_PROXY_HOST, QTFE_ERR_OFFSET + 36, "FTP proxy host")
ResDef(QTFE_GOPHER_PROXY_HOST, QTFE_ERR_OFFSET + 37, "Gopher proxy host")
ResDef(QTFE_HTTP_PROXY_HOST, QTFE_ERR_OFFSET + 38, "HTTP proxy host")
ResDef(QTFE_HTTPS_PROXY_HOST, QTFE_ERR_OFFSET + 39, "HTTPS proxy host")
ResDef(QTFE_WAIS_PROXY_HOST, QTFE_ERR_OFFSET + 40, "WAIS proxy host")
ResDef(QTFE_SOCKS_HOST, QTFE_ERR_OFFSET + 41, "SOCKS host")
ResDef(QTFE_GLOBAL_MIME_FILE, QTFE_ERR_OFFSET + 42, "Global MIME types file")
ResDef(QTFE_PRIVATE_MIME_FILE, QTFE_ERR_OFFSET + 43, "Private MIME types file")
ResDef(QTFE_GLOBAL_MAILCAP_FILE, QTFE_ERR_OFFSET + 44, "Global mailcap file")
ResDef(QTFE_PRIVATE_MAILCAP_FILE, QTFE_ERR_OFFSET + 45, "Private mailcap file")
ResDef(QTFE_BM_CANT_DEL_ROOT, QTFE_ERR_OFFSET + 46, "Cannot delete toplevel bookmark")
ResDef(QTFE_BM_CANT_CUT_ROOT, QTFE_ERR_OFFSET + 47, "Cannot cut toplevel bookmark")
ResDef(QTFE_BM_ALIAS, QTFE_ERR_OFFSET + 48, "This is an alias to the following Bookmark:")
/* NPS i18n stuff */
ResDef(QTFE_FILE_OPEN, QTFE_ERR_OFFSET + 49, "File Open...")
ResDef(QTFE_PRINTING_OF_FRAMES_IS_CURRENTLY_NOT_SUPPORTED, QTFE_ERR_OFFSET + 50, "Printing of frames is currently not supported.")
ResDef(QTFE_ERROR_SAVING_OPTIONS, QTFE_ERR_OFFSET + 51, "error saving options")
ResDef(QTFE_UNKNOWN_ESCAPE_CODE, QTFE_ERR_OFFSET + 52,
"unknown %s escape code %%%c:\n%%h = host, %%p = port, %%u = user")
ResDef(QTFE_COULDNT_FORK, QTFE_ERR_OFFSET + 53, "couldn't fork():")
ResDef(QTFE_EXECVP_FAILED, QTFE_ERR_OFFSET + 54, "%s: execvp(%s) failed")
ResDef(QTFE_SAVE_FRAME_AS, QTFE_ERR_OFFSET + 55, "Save Frame as...")
ResDef(QTFE_PRINT_FRAME, QTFE_ERR_OFFSET + 57, "Print Frame...")
ResDef(QTFE_PRINT, QTFE_ERR_OFFSET + 58 , "Print...")
ResDef(QTFE_DOWNLOAD_FILE, QTFE_ERR_OFFSET + 59, "Download File: %s")
ResDef(QTFE_COMPOSE_NO_SUBJECT, QTFE_ERR_OFFSET + 60, "Compose: (No Subject)")
ResDef(QTFE_COMPOSE, QTFE_ERR_OFFSET + 61, "Compose: %s")
ResDef(QTFE_NETSCAPE_UNTITLED, QTFE_ERR_OFFSET + 62, "Mozilla: <untitled>")
ResDef(QTFE_NETSCAPE, QTFE_ERR_OFFSET + 63, "Mozilla: %s")
ResDef(QTFE_NO_SUBJECT, QTFE_ERR_OFFSET + 64, "(no subject)")
ResDef(QTFE_UNKNOWN_ERROR_CODE, QTFE_ERR_OFFSET + 65, "unknown error code %d")
ResDef(QTFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST, QTFE_ERR_OFFSET + 66,
"Invalid File attachment.\n%s: doesn't exist.\n")
ResDef(QTFE_INVALID_FILE_ATTACHMENT_NOT_READABLE, QTFE_ERR_OFFSET + 67,
"Invalid File attachment.\n%s: not readable.\n")
ResDef(QTFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY, QTFE_ERR_OFFSET + 68,
"Invalid File attachment.\n%s: is a directory.\n")
ResDef(QTFE_COULDNT_FORK_FOR_MOVEMAIL, QTFE_ERR_OFFSET + 69,
"couldn't fork() for movemail")
ResDef(QTFE_PROBLEMS_EXECUTING, QTFE_ERR_OFFSET + 70, "problems executing %s:")
ResDef(QTFE_TERMINATED_ABNORMALLY, QTFE_ERR_OFFSET + 71, "%s terminated abnormally:")
ResDef(QTFE_UNABLE_TO_OPEN, QTFE_ERR_OFFSET + 72, "Unable to open %.900s")
ResDef(QTFE_PLEASE_ENTER_NEWS_HOST, QTFE_ERR_OFFSET + 73,
"Please enter your news host in one\n\
of the following formats:\n\n\
    news://HOST,\n\
    news://HOST:PORT,\n\
    snews://HOST, or\n\
    snews://HOST:PORT\n\n" )

ResDef(QTFE_MOVEMAIL_FAILURE_SUFFIX, QTFE_ERR_OFFSET + 74,
       "For the internal movemail method to work, we must be able to create\n\
lock files in the mail spool directory.  On many systems, this is best\n\
accomplished by making that directory be mode 01777.  If that is not\n\
possible, then an external setgid/setuid movemail program must be used.\n\
Please see the Release Notes for more information.")
ResDef(QTFE_CANT_MOVE_MAIL, QTFE_ERR_OFFSET + 75, "Can't move mail from %.200s")
ResDef(QTFE_CANT_GET_NEW_MAIL_LOCK_FILE_EXISTS, QTFE_ERR_OFFSET + 76, "Can't get new mail; a lock file %.200s exists.")
ResDef(QTFE_CANT_GET_NEW_MAIL_UNABLE_TO_CREATE_LOCK_FILE, QTFE_ERR_OFFSET + 77, "Can't get new mail; unable to create lock file %.200s")
ResDef(QTFE_CANT_GET_NEW_MAIL_SYSTEM_ERROR, QTFE_ERR_OFFSET + 78, "Can't get new mail; a system error occurred.")
ResDef(QTFE_CANT_MOVE_MAIL_UNABLE_TO_OPEN, QTFE_ERR_OFFSET + 79, "Can't move mail; unable to open %.200s")
ResDef(QTFE_CANT_MOVE_MAIL_UNABLE_TO_READ, QTFE_ERR_OFFSET + 80, "Can't move mail; unable to read %.200s")
ResDef(QTFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE, QTFE_ERR_OFFSET + 81, "Can't move mail; unable to write to %.200s")
ResDef(QTFE_THERE_WERE_PROBLEMS_MOVING_MAIL, QTFE_ERR_OFFSET + 82, "There were problems moving mail")
ResDef(QTFE_THERE_WERE_PROBLEMS_MOVING_MAIL_EXIT_STATUS, QTFE_ERR_OFFSET + 83, "There were problems moving mail: exit status %d" )
ResDef(QTFE_THERE_WERE_PROBLEMS_CLEANING_UP, QTFE_ERR_OFFSET + 134, "There were problems cleaning up %s")
ResDef(QTFE_USAGE_MSG1, QTFE_ERR_OFFSET + 85, "%s\n\
usage: %s [ options ... ]\n\
       where options include:\n\
\n\
       -help                     to show this message.\n\
       -version                  to show the version number and build date.\n\
       -display <dpy>            to specify the X server to use.\n\
       -geometry =WxH+X+Y        to position and size the window.\n\
       -visual <id-or-number>    to use a specific server visual.\n\
       -install                  to install a private colormap.\n\
       -no-install               to use the default colormap.\n" )
ResDef(QTFE_USAGE_MSG2, QTFE_ERR_OFFSET + 154, "\
       -share                    when -install, cause each window to use the\n\
                                 same colormap instead of each window using\n\
                                 a new one.\n\
       -no-share                 cause each window to use the same colormap.\n")
#if defined(XP_WIN)
ResDef(QTFE_USAGE_MSG3, QTFE_ERR_OFFSET + 86, "        No use at all")
#else
ResDef(QTFE_USAGE_MSG3, QTFE_ERR_OFFSET + 86, "\
       -ncols <N>                when not using -install, set the maximum\n\
                                 number of colors to allocate for images.\n\
       -mono                     to force 1-bit-deep image display.\n\
       -iconic                   to start up iconified.\n\
       -xrm <resource-spec>      to set a specific X resource.\n\
\n\
       -remote <remote-command>  to execute a command in an already-running\n\
                                 Mozilla process.  For more info, see\n\
			  http://home.netscape.com/newsref/std/x-remote.html\n\
       -id <window-id>           the id of an X window to which the -remote\n\
                                 commands should be sent; if unspecified,\n\
                                 the first window found will be used.\n\
       -raise                    whether following -remote commands should\n\
                                 cause the window to raise itself to the top\n\
                                 (this is the default.)\n\
       -noraise                  the opposite of -raise: following -remote\n\
                                 commands will not auto-raise the window.\n\
\n\
       -nethelp                  Show nethelp.  Requires nethelp: URL.\n\
\n\
       -dont-force-window-stacking  Ignore the alwaysraised, alwayslowered \n\
                                    and z-lock JavaScript window.open() \n\
                                    attributes.\n\
\n\
       -no-about-splash          Bypass the startup license page.\n\
\n\
       -no-session-management\n\
       -session-management       Mozilla supports session management\n\
                                 by default.  Use these flags to force\n\
                                 it on/off.\n\
\n\
       -no-irix-session-management\n\
       -irix-session-management  Different platforms deal with session\n\
                                 management in fundamentally different\n\
                                 ways.  Use these flags if you experience\n\
                                 session management problems.\n\
\n\
                                 IRIX session management is on by default\n\
                                 only on SGI systems.  It is also available\n\
                                 on other platforms and might work with\n\
                                 session managers other than the IRIX\n\
                                 desktop.\n\
\n\
       -dont-save-geometry-prefs Don't save window geometry for session.\n\
\n\
       -ignore-geometry-prefs    Ignore saved window geometry for session.\n\n" )
#endif
ResDef(QTFE_VERSION_COMPLAINT_FORMAT, QTFE_ERR_OFFSET + 87, "%s: program is version %s, but resources are version %s.\n\n\
\tThis means that there is an inappropriate `%s' file in\n\
\the system-wide app-defaults directory, or possibly in your\n\
\thome directory.  Check these environment variables and the\n\
\tdirectories to which they point:\n\n\
  $XAPPLRESDIR\n\
  $XFILESEARCHPATH\n\
  $XUSERFILESEARCHPATH\n\n\
\tAlso check for this file in your home directory, or in the\n\
\tdirectory called `app-defaults' somewhere under /usr/lib/." )
ResDef(QTFE_INAPPROPRIATE_APPDEFAULT_FILE, QTFE_ERR_OFFSET + 88, "%s: couldn't find our resources?\n\n\
\tThis could mean that there is an inappropriate `%s' file\n\
\tinstalled in the app-defaults directory.  Check these environment\n\
\tvariables and the directories to which they point:\n\n\
  $XAPPLRESDIR\n\
  $XFILESEARCHPATH\n\
  $XUSERFILESEARCHPATH\n\n\
\tAlso check for this file in your home directory, or in the\n\
\tdirectory called `app-defaults' somewhere under /usr/lib/." )
ResDef(QTFE_INVALID_GEOMETRY_SPEC, QTFE_ERR_OFFSET + 89, "%s: invalid geometry specification.\n\n\
 Apparently \"%s*geometry: %s\" or \"%s*geometry: %s\"\n\
 was specified in the resource database.  Specifying \"*geometry\"\n\
 will make %s (and most other X programs) malfunction in obscure\n\
 ways.  You should always use \".geometry\" instead.\n" )
ResDef(QTFE_UNRECOGNISED_OPTION, QTFE_ERR_OFFSET + 90, "%s: unrecognized option \"%s\"\n")
ResDef(QTFE_APP_HAS_DETECTED_LOCK, QTFE_ERR_OFFSET + 91, "%s has detected a %s\nfile.\n")
ResDef(QTFE_ANOTHER_USER_IS_RUNNING_APP, QTFE_ERR_OFFSET + 92, "\nThis may indicate that another user is running\n%s using your %s files.\n" )
ResDef(QTFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID, QTFE_ERR_OFFSET + 93,"It appears to be running on host %s\nunder process-ID %u.\n" )
ResDef(QTFE_YOU_MAY_CONTINUE_TO_USE, QTFE_ERR_OFFSET + 94, "\nYou may continue to use %s, but you will\n\
be unable to use the disk cache, global history,\n\
or your personal certificates.\n" )
ResDef(QTFE_OTHERWISE_CHOOSE_CANCEL, QTFE_ERR_OFFSET + 95, "\nOtherwise, you may choose Abort, make sure that\n\
you are not running another %s Navigator,\n\
delete the %s file, and\n\
restart %s." )
ResDef(QTFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY, QTFE_ERR_OFFSET + 96, "%s: %s existed, but was not a directory.\n\
The old file has been renamed to %s\n\
and a directory has been created in its place.\n\n" )
ResDef(QTFE_EXISTS_BUT_UNABLE_TO_RENAME, QTFE_ERR_OFFSET + 97, "%s: %s exists but is not a directory,\n\
and we were unable to rename it!\n\
Please remove this file: it is in the way.\n\n" )
ResDef(QTFE_UNABLE_TO_CREATE_DIRECTORY, QTFE_ERR_OFFSET + 98,
"%s: unable to create the directory `%s'.\n%s\n\
Please create this directory.\n\n" )
ResDef(QTFE_UNKNOWN_ERROR, QTFE_ERR_OFFSET + 99, "unknown error")
ResDef(QTFE_ERROR_CREATING, QTFE_ERR_OFFSET + 100, "error creating %s")
ResDef(QTFE_ERROR_WRITING, QTFE_ERR_OFFSET + 101, "error writing %s")
ResDef(QTFE_CREATE_CONFIG_FILES, QTFE_ERR_OFFSET + 102,
"This version of %s uses different names for its config files\n\
than previous versions did.  Those config files which still use\n\
the same file formats have been copied to their new names, and\n\
those which don't will be recreated as necessary.\n%s\n\n\
Would you like us to delete the old files now?" )
ResDef(QTFE_OLD_FILES_AND_CACHE, QTFE_ERR_OFFSET + 103,
"\nThe old files still exist, including a disk cache directory\n\
(which might be large.)" )
ResDef(QTFE_OLD_FILES, QTFE_ERR_OFFSET + 104, "The old files still exist.")
ResDef(QTFE_GENERAL, QTFE_ERR_OFFSET + 105, "General")
ResDef(QTFE_PASSWORDS, QTFE_ERR_OFFSET + 106, "Passwords")
ResDef(QTFE_PERSONAL_CERTIFICATES, QTFE_ERR_OFFSET + 107, "Personal Certificates")
ResDef(QTFE_SITE_CERTIFICATES, QTFE_ERR_OFFSET + 108, "Site Certificates")
ResDef(QTFE_ESTIMATED_TIME_REMAINING_CHECKED, QTFE_ERR_OFFSET + 109,
"Checked %s (%d left)\n%d%% completed)\n\n\
Estimated time remaining: %s\n\
(Remaining time depends on the sites selected and\n\
the network traffic.)" )
ResDef(QTFE_ESTIMATED_TIME_REMAINING_CHECKING, QTFE_ERR_OFFSET + 110,
"Checking ... (%d left)\n%d%% completed)\n\n\
Estimated time remaining: %s\n\
(Remaining time depends on the sites selected and\n\
the network traffic.)" )
ResDef(QTFE_RE, QTFE_ERR_OFFSET + 111, "Re: ")
ResDef(QTFE_DONE_CHECKING_ETC, QTFE_ERR_OFFSET + 112,
"Done checking %d bookmarks.\n\
%d documents were reached.\n\
%d documents have changed and are marked as new." )
ResDef(QTFE_APP_EXITED_WITH_STATUS, QTFE_ERR_OFFSET + 115,"\"%s\" exited with status %d" )
ResDef(QTFE_THE_MOTIF_KEYSYMS_NOT_DEFINED, QTFE_ERR_OFFSET + 116,
"%s: The Motif keysyms seem not to be defined.\n\n\
 This is usually because the proper XKeysymDB file was not found.\n\
 You can set the $XKEYSYMDB environment variable to the location\n\
 of a file which contains the right keysyms.\n\n\
 Without the right XKeysymDB, many warnings will be generated,\n\
 and most keyboard accelerators will not work.\n\n\
 (An appropriate XKeysymDB file was included with the %s\n\
 distribution.)\n\n" )
ResDef(QTFE_SOME_MOTIF_KEYSYMS_NOT_DEFINED, QTFE_ERR_OFFSET + 117,
 "%s: Some of the Motif keysyms seem not to be defined.\n\n\
 This is usually because the proper XKeysymDB file was not found.\n\
 You can set the $XKEYSYMDB environment variable to the location\n\
 of a file which contains the right keysyms.\n\n\
 Without the right XKeysymDB, many warnings will be generated,\n\
 and some keyboard accelerators will not work.\n\n\
 (An appropriate XKeysymDB file was included with the %s\n\
 distribution.)\n\n" )
ResDef(QTFE_VISUAL_GRAY_DIRECTCOLOR, QTFE_ERR_OFFSET + 118,
"Visual 0x%02x is a%s %d bit %s visual.\n\
This is not a supported visual; images %s.\n\n\
Currently supported visuals are:\n\n\
       StaticGray, all depths\n\
        GrayScale, all depths\n\
        TrueColor, depth 8 or greater\n\
        DirectColor, depth 8 or greater\n\
        StaticColor, depth 8 or greater\n\
        PseudoColor, depth 8 only\n\n\
If you have any of the above visuals (see `xdpyinfo'),\n\
it is recommended that you start %s with\n\
the `-visual' command-line option to specify one.\n\n\
More visuals may be directly supported in the future;\n\
your feedback is welcome." )
ResDef(QTFE_VISUAL_GRAY, QTFE_ERR_OFFSET + 119,
"Visual 0x%02x is a%s %d bit %s visual.\n\
This is not a supported visual; images %s.\n\n\
Currently supported visuals are:\n\n\
       StaticGray, all depths\n\
       GrayScale, all depths\n\
       TrueColor, depth 8 or greater\n\
        StaticColor, depth 8 or greater\n\
        PseudoColor, depth 8 only\n\n\
If you have any of the above visuals (see `xdpyinfo'),\n\
it is recommended that you start %s with\n\
the `-visual' command-line option to specify one.\n\n\
More visuals may be directly supported in the future;\n\
your feedback is welcome." )
ResDef(QTFE_VISUAL_DIRECTCOLOR, QTFE_ERR_OFFSET + 120,
"Visual 0x%02x is a%s %d bit %s visual.\n\
This is not a supported visual; images %s.\n\n\
Currently supported visuals are:\n\n\
       StaticGray, all depths\n\
        TrueColor, depth 8 or greater\n\
        DirectColor, depth 8 or greater\n\
        StaticColor, depth 8 or greater\n\
        PseudoColor, depth 8 only\n\n\
If you have any of the above visuals (see `xdpyinfo'),\n\
it is recommended that you start %s with\n\
the `-visual' command-line option to specify one.\n\n\
More visuals may be directly supported in the future;\n\
your feedback is welcome." )
ResDef(QTFE_VISUAL_NORMAL, QTFE_ERR_OFFSET + 121,
"Visual 0x%02x is a%s %d bit %s visual.\n\
This is not a supported visual; images %s.\n\n\
Currently supported visuals are:\n\n\
       StaticGray, all depths\n\
        TrueColor, depth 8 or greater\n\
        StaticColor, depth 8 or greater\n\
        PseudoColor, depth 8 only\n\n\
If you have any of the above visuals (see `xdpyinfo'),\n\
it is recommended that you start %s with\n\
the `-visual' command-line option to specify one.\n\n\
More visuals may be directly supported in the future;\n\
your feedback is welcome." )
ResDef(QTFE_WILL_BE_DISPLAYED_IN_MONOCHROME, QTFE_ERR_OFFSET + 122,
"will be\ndisplayed in monochrome" )
ResDef(QTFE_WILL_LOOK_BAD, QTFE_ERR_OFFSET + 123, "will look bad")
ResDef(QTFE_APPEARANCE, QTFE_ERR_OFFSET + 124, "Appearance")
ResDef(QTFE_BOOKMARKS, QTFE_ERR_OFFSET + 125, "Bookmarks")
ResDef(QTFE_COLORS, QTFE_ERR_OFFSET + 126, "Colors")
ResDef(QTFE_FONTS, QTFE_ERR_OFFSET + 127, "Fonts" )
ResDef(QTFE_APPLICATIONS, QTFE_ERR_OFFSET + 128, "Applications")
ResDef(QTFE_HELPERS, QTFE_ERR_OFFSET + 155, "Helpers")
ResDef(QTFE_IMAGES, QTFE_ERR_OFFSET + 129, "Images")
ResDef(QTFE_LANGUAGES, QTFE_ERR_OFFSET + 130, "Languages")
ResDef(QTFE_CACHE, QTFE_ERR_OFFSET + 131, "Cache")
ResDef(QTFE_CONNECTIONS, QTFE_ERR_OFFSET + 132, "Connections")
ResDef(QTFE_PROXIES, QTFE_ERR_OFFSET + 133, "Proxies")
ResDef(QTFE_COMPOSE_DLG, QTFE_ERR_OFFSET + 135, "Compose")
ResDef(QTFE_SERVERS, QTFE_ERR_OFFSET + 136, "Servers")
ResDef(QTFE_IDENTITY, QTFE_ERR_OFFSET + 137, "Identity")
ResDef(QTFE_ORGANIZATION, QTFE_ERR_OFFSET + 138, "Organization")
ResDef(QTFE_MAIL_FRAME, QTFE_ERR_OFFSET + 139, "Mail Frame" )
ResDef(QTFE_MAIL_DOCUMENT, QTFE_ERR_OFFSET + 140, "Mail Document" )
ResDef(QTFE_NETSCAPE_MAIL, QTFE_ERR_OFFSET + 141, "Mozilla Mail & Discussions" )
ResDef(QTFE_NETSCAPE_NEWS, QTFE_ERR_OFFSET + 142, "Mozilla Discussions" )
ResDef(QTFE_ADDRESS_BOOK, QTFE_ERR_OFFSET + 143, "Address Book" )
ResDef(QTFE_X_RESOURCES_NOT_INSTALLED_CORRECTLY, QTFE_ERR_OFFSET + 144,
"X resources not installed correctly!" )
ResDef(QTFE_GG_EMPTY_LL, QTFE_ERR_OFFSET + 145, "<< Empty >>")
ResDef(QTFE_ERROR_SAVING_PASSWORD, QTFE_ERR_OFFSET + 146, "error saving password")
ResDef(QTFE_UNIMPLEMENTED, QTFE_ERR_OFFSET + 147, "Unimplemented.")
ResDef(QTFE_TILDE_USER_SYNTAX_DISALLOWED, QTFE_ERR_OFFSET + 148,
"%s: ~user/ syntax is not allowed in preferences file, only ~/\n" )
ResDef(QTFE_UNRECOGNISED_VISUAL, QTFE_ERR_OFFSET + 149,
"%s: unrecognized visual \"%s\".\n" )
ResDef(QTFE_NO_VISUAL_WITH_ID, QTFE_ERR_OFFSET + 150,
"%s: no visual with id 0x%x.\n" )
ResDef(QTFE_NO_VISUAL_OF_CLASS, QTFE_ERR_OFFSET + 151,
"%s: no visual of class %s.\n" )
ResDef(QTFE_STDERR_DIAGNOSTICS_HAVE_BEEN_TRUNCATED, QTFE_ERR_OFFSET + 152,
"\n\n<< stderr diagnostics have been truncated >>" )
ResDef(QTFE_ERROR_CREATING_PIPE, QTFE_ERR_OFFSET + 153, "error creating pipe:")
ResDef(QTFE_OUTBOX_CONTAINS_MSG, QTFE_ERR_OFFSET + 156,
"Outbox folder contains an unsent\nmessage. Send it now?\n")
ResDef(QTFE_OUTBOX_CONTAINS_MSGS, QTFE_ERR_OFFSET + 157,
"Outbox folder contains %d unsent\nmessages. Send them now?\n")
ResDef(QTFE_NO_KEEP_ON_SERVER_WITH_MOVEMAIL, QTFE_ERR_OFFSET + 158,
"The ``Leave on Server'' option only works when\n\
using a POP3 server, not when using a local\n\
mail spool directory.  To retrieve your mail,\n\
first turn off that option in the Servers pane\n\
of the Mail and News Preferences.")
ResDef(QTFE_BACK, QTFE_ERR_OFFSET + 159, "Back")
ResDef(QTFE_BACK_IN_FRAME, QTFE_ERR_OFFSET + 160, "Back in Frame")
ResDef(QTFE_FORWARD, QTFE_ERR_OFFSET + 161, "Forward")
ResDef(QTFE_FORWARD_IN_FRAME, QTFE_ERR_OFFSET + 162, "Forward in Frame")
ResDef(QTFE_MAIL_SPOOL_UNKNOWN, QTFE_ERR_OFFSET + 163,
"Please set the $MAIL environment variable to the\n\
location of your mail-spool file.")
ResDef(QTFE_MOVEMAIL_NO_MESSAGES, QTFE_ERR_OFFSET + 164,
"No new messages.")
ResDef(QTFE_USER_DEFINED, QTFE_ERR_OFFSET + 165,
"User-Defined")
ResDef(QTFE_OTHER_LANGUAGE, QTFE_ERR_OFFSET + 166,
"Other")
ResDef(QTFE_COULDNT_FORK_FOR_DELIVERY, QTFE_ERR_OFFSET + 167,
"couldn't fork() for external message delivery")
ResDef(QTFE_CANNOT_READ_FILE, QTFE_ERR_OFFSET + 168,
"Cannot read file %s.")
ResDef(QTFE_CANNOT_SEE_FILE, QTFE_ERR_OFFSET + 169,
"%s does not exist.")
ResDef(QTFE_IS_A_DIRECTORY, QTFE_ERR_OFFSET + 170,
"%s is a directory.")
ResDef(QTFE_EKIT_LOCK_FILE_NOT_FOUND, QTFE_ERR_OFFSET + 171,
"Lock file not found.")
ResDef(QTFE_EKIT_CANT_OPEN, QTFE_ERR_OFFSET + 172,
"Can't open Netscape.lock file.")
ResDef(QTFE_EKIT_MODIFIED, QTFE_ERR_OFFSET + 173,
"Netscape.lock file has been modified.")
ResDef(QTFE_EKIT_FILESIZE, QTFE_ERR_OFFSET + 174,
"Could not determine lock file size.")
ResDef(QTFE_EKIT_CANT_READ, QTFE_ERR_OFFSET + 175,
"Could not read Netscape.lock data.")
ResDef(QTFE_ANIM_CANT_OPEN, QTFE_ERR_OFFSET + 176,
"Couldn't open animation file.")
ResDef(QTFE_ANIM_MODIFIED, QTFE_ERR_OFFSET + 177,
"Animation file modified.\nUsing default animation.")
ResDef(QTFE_ANIM_READING_SIZES, QTFE_ERR_OFFSET + 178,
"Couldn't read animation file size.\nUsing default animation.")
ResDef(QTFE_ANIM_READING_NUM_COLORS, QTFE_ERR_OFFSET + 179,
"Couldn't read number of animation colors.\nUsing default animation.")
ResDef(QTFE_ANIM_READING_COLORS, QTFE_ERR_OFFSET + 180,
"Couldn't read animation colors.\nUsing default animation.")
ResDef(QTFE_ANIM_READING_FRAMES, QTFE_ERR_OFFSET + 181,
"Couldn't read animation frames.\nUsing default animation.")
ResDef(QTFE_ANIM_NOT_AT_EOF, QTFE_ERR_OFFSET + 182,
"Ignoring extra bytes at end of animation file.")
ResDef(QTFE_LOOK_FOR_DOCUMENTS_THAT_HAVE_CHANGED_ON, QTFE_ERR_OFFSET + 183,
"Look for documents that have changed for:")
/* I'm not quite sure why these are resources?? ..djw */
ResDef(QTFE_CHARACTER, QTFE_ERR_OFFSET + 184, "Character") /* 26FEB96RCJ */
ResDef(QTFE_LINK, QTFE_ERR_OFFSET + 185, "Link")           /* 26FEB96RCJ */
ResDef(QTFE_PARAGRAPH, QTFE_ERR_OFFSET + 186, "Paragraph") /* 26FEB96RCJ */
ResDef(QTFE_IMAGE,     QTFE_ERR_OFFSET + 187, "Image")     /*  5MAR96RCJ */
ResDef(QTFE_REFRESH_FRAME, QTFE_ERR_OFFSET + 188, "Refresh Frame")
ResDef(QTFE_REFRESH, QTFE_ERR_OFFSET + 189, "Refresh")

ResDef(QTFE_MAIL_TITLE_FMT, QTFE_ERR_OFFSET + 190, "Mozilla Mail & Discussions: %.900s")
ResDef(QTFE_NEWS_TITLE_FMT, QTFE_ERR_OFFSET + 191, "Mozilla Discussions: %.900s")
ResDef(QTFE_TITLE_FMT, QTFE_ERR_OFFSET + 192, "Mozilla: %.900s")

ResDef(QTFE_PROTOCOLS, QTFE_ERR_OFFSET + 193, "Protocols")
ResDef(QTFE_LANG, QTFE_ERR_OFFSET + 194, "Languages")
ResDef(QTFE_CHANGE_PASSWORD, QTFE_ERR_OFFSET + 195, "Change Password")
ResDef(QTFE_SET_PASSWORD, QTFE_ERR_OFFSET + 196, "Set Password...")
ResDef(QTFE_NO_PLUGINS, QTFE_ERR_OFFSET + 197, "No Plugins")

/*
 * Messages for the dialog that warns before doing movemail.
 * DEM 4/30/96
 */
ResDef(QTFE_CONTINUE_MOVEMAIL, QTFE_ERR_OFFSET + 198, "Continue Movemail")
ResDef(QTFE_CANCEL_MOVEMAIL, QTFE_ERR_OFFSET + 199, "Cancel Movemail")
ResDef(QTFE_MOVEMAIL_EXPLANATION, QTFE_ERR_OFFSET + 200,
"Mozilla will move your mail from %s\n\
to %s/Inbox.\n\
\n\
Moving the mail will interfere with other mailers\n\
that expect already-read mail to remain in\n\
%s." )
ResDef(QTFE_SHOW_NEXT_TIME, QTFE_ERR_OFFSET + 201, "Show this Alert Next Time" )
ResDef(QTFE_EDITOR_TITLE_FMT, QTFE_ERR_OFFSET + 202, "Mozilla Composer: %.900s")

ResDef(QTFE_HELPERS_NETSCAPE, QTFE_ERR_OFFSET + 203, "Mozilla")
ResDef(QTFE_HELPERS_UNKNOWN, QTFE_ERR_OFFSET + 204, "Unknown:Prompt User")
ResDef(QTFE_HELPERS_SAVE, QTFE_ERR_OFFSET + 205, "Save To Disk")
ResDef(QTFE_HELPERS_PLUGIN, QTFE_ERR_OFFSET + 206, "Plug In : %s")
ResDef(QTFE_HELPERS_EMPTY_MIMETYPE, QTFE_ERR_OFFSET + 207,
"Mime type cannot be emtpy.")
ResDef(QTFE_HELPERS_LIST_HEADER, QTFE_ERR_OFFSET + 208, "Description|Handled By")
ResDef(QTFE_MOVEMAIL_CANT_DELETE_LOCK, QTFE_ERR_OFFSET + 209,
"Can't get new mail; a lock file %s exists.")
ResDef(QTFE_PLUGIN_NO_PLUGIN, QTFE_ERR_OFFSET + 210,
"No plugin %s. Reverting to save-to-disk for type %s.\n")
ResDef(QTFE_PLUGIN_CANT_LOAD, QTFE_ERR_OFFSET + 211,
"ERROR: %s\nCant load plugin %s. Ignored.\n")
ResDef(QTFE_HELPERS_PLUGIN_DESC_CHANGE, QTFE_ERR_OFFSET + 212,
"Plugin specifies different Description and/or Suffixes for mimetype %s.\n\
\n\
        Description = \"%s\"\n\
        Suffixes = \"%s\"\n\
\n\
Use plugin specified Description and Suffixes ?")
ResDef(QTFE_CANT_SAVE_PREFS, QTFE_ERR_OFFSET + 213, "error Saving options.")
ResDef(QTFE_VALUES_OUT_OF_RANGE, QTFE_ERR_OFFSET + 214,
	   "Some values are out of range:")
ResDef(QTFE_VALUE_OUT_OF_RANGE, QTFE_ERR_OFFSET + 215,
	   "The following value is out of range:")
ResDef(QTFE_EDITOR_TABLE_ROW_RANGE, QTFE_ERR_OFFSET + 216,
	   "You can have between 1 and 100 rows.")
ResDef(QTFE_EDITOR_TABLE_COLUMN_RANGE, QTFE_ERR_OFFSET + 217,
	   "You can have between 1 and 100 columns.")
ResDef(QTFE_EDITOR_TABLE_BORDER_RANGE, QTFE_ERR_OFFSET + 218,
	   "For border width, you can have 0 to 10000 pixels.")
ResDef(QTFE_EDITOR_TABLE_SPACING_RANGE, QTFE_ERR_OFFSET + 219,
	   "For cell spacing, you can have 0 to 10000 pixels.")
ResDef(QTFE_EDITOR_TABLE_PADDING_RANGE, QTFE_ERR_OFFSET + 220,
	   "For cell padding, you can have 0 to 10000 pixels.")
ResDef(QTFE_EDITOR_TABLE_WIDTH_RANGE, QTFE_ERR_OFFSET + 221,
	   "For width, you can have between 1 and 10000 pixels,\n"
	   "or between 1 and 100%.")
ResDef(QTFE_EDITOR_TABLE_HEIGHT_RANGE, QTFE_ERR_OFFSET + 222,
	   "For height, you can have between 1 and 10000 pixels,\n"
	   "or between 1 and 100%.")
ResDef(QTFE_EDITOR_TABLE_IMAGE_WIDTH_RANGE, QTFE_ERR_OFFSET + 223,
	   "For width, you can have between 1 and 10000 pixels.")
ResDef(QTFE_EDITOR_TABLE_IMAGE_HEIGHT_RANGE, QTFE_ERR_OFFSET + 224,
	   "For height, you can have between 1 and 10000 pixels.")
ResDef(QTFE_EDITOR_TABLE_IMAGE_SPACE_RANGE, QTFE_ERR_OFFSET + 225,
	   "For space, you can have between 1 and 10000 pixels.")
ResDef(QTFE_ENTER_NEW_VALUE, QTFE_ERR_OFFSET + 226,
	   "Please enter a new value and try again.")
ResDef(QTFE_ENTER_NEW_VALUES, QTFE_ERR_OFFSET + 227,
	   "Please enter new values and try again.")
ResDef(QTFE_EDITOR_LINK_TEXT_LABEL_NEW, QTFE_ERR_OFFSET + 228,
	   "Enter the text of the link:")
ResDef(QTFE_EDITOR_LINK_TEXT_LABEL_IMAGE, QTFE_ERR_OFFSET + 229,
	   "Linked image:")
ResDef(QTFE_EDITOR_LINK_TEXT_LABEL_TEXT, QTFE_ERR_OFFSET + 230,
	   "Linked text:")
ResDef(QTFE_EDITOR_LINK_TARGET_LABEL_NO_TARGETS, QTFE_ERR_OFFSET + 231,
	   "No targets in the selected document")
ResDef(QTFE_EDITOR_LINK_TARGET_LABEL_SPECIFIED, QTFE_ERR_OFFSET + 232,
	   "Link to a named target in the specified document (optional).")
ResDef(QTFE_EDITOR_LINK_TARGET_LABEL_CURRENT, QTFE_ERR_OFFSET + 233,
	   "Link to a named target in the current document (optional).")
ResDef(QTFE_EDITOR_WARNING_REMOVE_LINK, QTFE_ERR_OFFSET + 234,
	   "Do you want to remove the link?")
ResDef(QTFE_UNKNOWN, QTFE_ERR_OFFSET + 235,
	   "<unknown>")
ResDef(QTFE_EDITOR_TAG_UNOPENED, QTFE_ERR_OFFSET + 236,
	   "Unopened Tag: '<' was expected")
ResDef(QTFE_EDITOR_TAG_UNCLOSED, QTFE_ERR_OFFSET + 237,
	   "Unopened Tag:  '>' was expected")
ResDef(QTFE_EDITOR_TAG_UNTERMINATED_STRING, QTFE_ERR_OFFSET + 238,
	   "Unterminated String in tag: closing quote expected")
ResDef(QTFE_EDITOR_TAG_PREMATURE_CLOSE, QTFE_ERR_OFFSET + 239,
	   "Premature close of tag")
ResDef(QTFE_EDITOR_TAG_TAGNAME_EXPECTED, QTFE_ERR_OFFSET + 240,
	   "Tagname was expected")
ResDef(QTFE_EDITOR_TAG_UNKNOWN, QTFE_ERR_OFFSET + 241,
	   "Unknown tag error")
ResDef(QTFE_EDITOR_TAG_OK, QTFE_ERR_OFFSET + 242,
	   "Tag seems ok")
ResDef(QTFE_EDITOR_ALERT_FRAME_DOCUMENT, QTFE_ERR_OFFSET + 243,
	   "This document contains frames. Currently the editor\n"
	   "cannot edit documents which contain frames.")
ResDef(QTFE_EDITOR_ALERT_ABOUT_DOCUMENT, QTFE_ERR_OFFSET + 244,
	   "This document is an about document. The editor\n"
	   "cannot edit about documents.")
ResDef(QTFE_ALERT_SAVE_CHANGES, QTFE_ERR_OFFSET + 245,
	   "You must save this document locally before\n"
	   "continuing with the requested action.")
ResDef(QTFE_WARNING_SAVE_CHANGES, QTFE_ERR_OFFSET + 246,
	   "Do you want to save changes to:\n%.900s?")
ResDef(QTFE_ERROR_GENERIC_ERROR_CODE, QTFE_ERR_OFFSET + 247,
	   "The error code = (%d).")
ResDef(QTFE_EDITOR_COPY_DOCUMENT_BUSY, QTFE_ERR_OFFSET + 248,
	   "Cannot copy or cut at this time, try again later.")
ResDef(QTFE_EDITOR_COPY_SELECTION_EMPTY, QTFE_ERR_OFFSET + 249,
	   "Nothing is selected.")
ResDef(QTFE_EDITOR_COPY_SELECTION_CROSSES_TABLE_DATA_CELL, QTFE_ERR_OFFSET + 250,
	   "The selection includes a table cell boundary.\n"
	   "Deleting and copying are not allowed.")
ResDef(QTFE_COMMAND_EMPTY, QTFE_ERR_OFFSET + 251,
	   "Empty command specified!")
ResDef(QTFE_EDITOR_HTML_EDITOR_COMMAND_EMPTY, QTFE_ERR_OFFSET + 252,
	   "No html editor command has been specified in Editor Preferences.\n"
	   "Specify the file argument with %f. Mozilla will replace %f with\n"
	   "the correct file name. Example:\n"
	   "             xterm -e vi %f\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(QTFE_ACTION_SYNTAX_ERROR, QTFE_ERR_OFFSET + 253,
	   "Syntax error in action handler: %s")
ResDef(QTFE_ACTION_WRONG_CONTEXT, QTFE_ERR_OFFSET + 254,
	   "Invalid window type for action handler: %s")
ResDef(QTFE_EKIT_ABOUT_MESSAGE, QTFE_ERR_OFFSET + 255,
	   "Modified by the Mozilla Navigator Administration Kit.\n"
           "Version: %s\n"
           "User agent: C")

ResDef(QTFE_PRIVATE_MIMETYPE_RELOAD, QTFE_ERR_OFFSET + 256,
 "Private Mimetype File (%s) has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(QTFE_PRIVATE_MAILCAP_RELOAD, QTFE_ERR_OFFSET + 257,
 "Private Mailcap File (%s) has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(QTFE_PRIVATE_RELOADED_MIMETYPE, QTFE_ERR_OFFSET + 258,
 "Private Mimetype File (%s) has changed on disk and is being reloaded.")
ResDef(QTFE_PRIVATE_RELOADED_MAILCAP, QTFE_ERR_OFFSET + 259,
 "Private Mailcap File (%s) has changed on disk and is being reloaded.")
ResDef(QTFE_EDITOR_IMAGE_EDITOR_COMMAND_EMPTY, QTFE_ERR_OFFSET + 260,
	   "No image editor command has been specified in Editor Preferences.\n"
	   "Specify the file argument with %f. Mozilla will replace %f with\n"
	   "the correct file name. Example:\n"
	   "             xgifedit %f\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(QTFE_ERROR_COPYRIGHT_HINT, QTFE_ERR_OFFSET + 261,
 "You are about to download a remote\n"
 "document or image.\n"
 "You should get permission to use any\n"
 "copyrighted images or documents.")
ResDef(QTFE_ERROR_READ_ONLY, QTFE_ERR_OFFSET + 262,
	   "The file is marked read-only.")
ResDef(QTFE_ERROR_BLOCKED, QTFE_ERR_OFFSET + 263,
	   "The file is blocked at this time. Try again later.")
ResDef(QTFE_ERROR_BAD_URL, QTFE_ERR_OFFSET + 264,
	   "The file URL is badly formed.")
ResDef(QTFE_ERROR_FILE_OPEN, QTFE_ERR_OFFSET + 265,
	   "Error opening file for writing.")
ResDef(QTFE_ERROR_FILE_WRITE, QTFE_ERR_OFFSET + 266,
	   "Error writing to the file.")
ResDef(QTFE_ERROR_CREATE_BAKNAME, QTFE_ERR_OFFSET + 267,
	   "Error creating the temporary backup file.")
ResDef(QTFE_ERROR_DELETE_BAKFILE, QTFE_ERR_OFFSET + 268,
	   "Error deleting the temporary backup file.")
ResDef(QTFE_WARNING_SAVE_CONTINUE, QTFE_ERR_OFFSET + 269,
	   "Continue saving document?")
ResDef(QTFE_ERROR_WRITING_FILE, QTFE_ERR_OFFSET + 270,
	   "There was an error while saving the file:\n%.900s")
ResDef(QTFE_EDITOR_DOCUMENT_TEMPLATE_EMPTY, QTFE_ERR_OFFSET + 271,
	   "The new document template location is not set.\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(QTFE_EDITOR_AUTOSAVE_PERIOD_RANGE, QTFE_ERR_OFFSET + 272,
	   "Please enter an autosave period between 0 and 600 minutes.")
ResDef(QTFE_EDITOR_BROWSE_LOCATION_EMPTY, QTFE_ERR_OFFSET + 273,
	   "The default browse location is not set.\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(QTFE_EDITOR_PUBLISH_LOCATION_INVALID, QTFE_ERR_OFFSET + 274,
	   "Publish destination must begin with \"ftp://\", \"http://\", or \"https://\".\n"
	   "Please enter new values and try again.")
ResDef(QTFE_EDITOR_IMAGE_IS_REMOTE, QTFE_ERR_OFFSET + 275,
	   "Image is at a remote location.\n"
	   "Please save image locally before editing.")
ResDef(QTFE_COLORMAP_WARNING_TO_IGNORE, QTFE_ERR_OFFSET + 276,
	   "cannot allocate colormap")
ResDef(QTFE_UPLOADING_FILE, QTFE_ERR_OFFSET + 277,
	   "Uploading file to remote server:\n%.900s")
ResDef(QTFE_SAVING_FILE, QTFE_ERR_OFFSET + 278,
	   "Saving file to local disk:\n%.900s")
ResDef(QTFE_LOADING_IMAGE_FILE, QTFE_ERR_OFFSET + 279,
	   "Loading image file:\n%.900s")
ResDef(QTFE_FILE_N_OF_N, QTFE_ERR_OFFSET + 280,
	   "File %d of %d")
ResDef(QTFE_ERROR_SRC_NOT_FOUND, QTFE_ERR_OFFSET + 281,
	   "Source not found.")
ResDef(QTFE_WARNING_AUTO_SAVE_NEW_MSG, QTFE_ERR_OFFSET + 282,
	   "Press Cancel to turn off AutoSave until you\n"
	   "save this document later.")
ResDef(QTFE_ALL_NEWSGROUP_TAB, QTFE_ERR_OFFSET + 283,
	   "All Groups")
ResDef(QTFE_SEARCH_FOR_NEWSGROUP_TAB, QTFE_ERR_OFFSET + 284,
           "Search for a Group")
ResDef(QTFE_NEW_NEWSGROUP_TAB, QTFE_ERR_OFFSET + 285,
           "New Groups")
ResDef(QTFE_NEW_NEWSGROUP_TAB_INFO_MSG, QTFE_ERR_OFFSET + 286,
           "This list shows the accumulated list of new discussion groups since\n"
           "the last time you pressed the Clear New button.")
ResDef(QTFE_SILLY_NAME_FOR_SEEMINGLY_UNAMEABLE_THING, QTFE_ERR_OFFSET + 287,
           "Message Center for %s")
ResDef(QTFE_FOLDER_ON_LOCAL_MACHINE, QTFE_ERR_OFFSET + 288,
           "on local machine.")
ResDef(QTFE_FOLDER_ON_SERVER, QTFE_ERR_OFFSET + 289,
           "on server.")
ResDef(QTFE_MESSAGE, QTFE_ERR_OFFSET + 290,
       "Message:")
ResDef(QTFE_MESSAGE_SUBTITLE, QTFE_ERR_OFFSET + 291,
       "'%s' from %s in %s Folder")
ResDef(QTFE_WINDOW_TITLE_NEWSGROUP, QTFE_ERR_OFFSET + 292,
       "Mozilla Group - [ %s ]")
ResDef(QTFE_WINDOW_TITLE_FOLDER, QTFE_ERR_OFFSET + 293,
       "Mozilla Folder - [ %s ]")
ResDef(QTFE_MAIL_PRIORITY_LOWEST, QTFE_ERR_OFFSET + 294,
       "Lowest")
ResDef(QTFE_MAIL_PRIORITY_LOW, QTFE_ERR_OFFSET + 295,
       "Low")
ResDef(QTFE_MAIL_PRIORITY_NORMAL, QTFE_ERR_OFFSET + 296,
       "Normal")
ResDef(QTFE_MAIL_PRIORITY_HIGH, QTFE_ERR_OFFSET + 297,
       "High")
ResDef(QTFE_MAIL_PRIORITY_HIGHEST, QTFE_ERR_OFFSET + 298,
       "Highest")
ResDef(QTFE_SIZE_IN_BYTES, QTFE_ERR_OFFSET + 299,
       "Size")
ResDef(QTFE_SIZE_IN_LINES, QTFE_ERR_OFFSET + 300,
       "Lines")

ResDef(QTFE_AB_NAME_GENERAL_TAB, QTFE_ERR_OFFSET + 301,
       "Name")
ResDef(QTFE_AB_NAME_CONTACT_TAB, QTFE_ERR_OFFSET + 302,
       "Contact")
ResDef(QTFE_AB_NAME_SECURITY_TAB, QTFE_ERR_OFFSET + 303,
       "Security")
ResDef(QTFE_AB_NAME_COOLTALK_TAB, QTFE_ERR_OFFSET + 304,
       "Mozilla Conference")
ResDef(QTFE_AB_FIRSTNAME, QTFE_ERR_OFFSET + 305,
       "First Name:")
ResDef(QTFE_AB_LASTNAME, QTFE_ERR_OFFSET + 306,
       "Last Name:")
ResDef(QTFE_AB_EMAIL, QTFE_ERR_OFFSET + 307,
       "Email Address:")
ResDef(QTFE_AB_NICKNAME, QTFE_ERR_OFFSET + 308,
       "Nickname:")
ResDef(QTFE_AB_NOTES, QTFE_ERR_OFFSET + 309,
       "Notes:")
ResDef(QTFE_AB_PREFHTML, QTFE_ERR_OFFSET + 310,
       "Prefers to receive rich text (HTML) mail")
ResDef(QTFE_AB_COMPANY, QTFE_ERR_OFFSET + 311,
       "Organization:")
ResDef(QTFE_AB_TITLE, QTFE_ERR_OFFSET + 312,
       "Title:")
ResDef(QTFE_AB_ADDRESS, QTFE_ERR_OFFSET + 313,
       "Address:")
ResDef(QTFE_AB_CITY, QTFE_ERR_OFFSET + 314,
       "City:")
ResDef(QTFE_AB_STATE, QTFE_ERR_OFFSET + 315,
       "State:")
ResDef(QTFE_AB_ZIP, QTFE_ERR_OFFSET + 316,
       "Zip:")
ResDef(QTFE_AB_COUNTRY, QTFE_ERR_OFFSET + 317,
       "Country:")

ResDef(QTFE_AB_WORKPHONE, QTFE_ERR_OFFSET + 318,
       "Work:")
ResDef(QTFE_AB_FAX, QTFE_ERR_OFFSET + 319,
       "Fax:")
ResDef(QTFE_AB_HOMEPHONE, QTFE_ERR_OFFSET + 320,
       "Home:")

ResDef(QTFE_AB_SECUR_NO, QTFE_ERR_OFFSET + 321,
       "You do not have a Security Certificate for this person.\n"
	   "\n"
	   "This means that when you send this person email, it cannot be\n"
	   "encrypted. This will make it easier for other people to read your\n"
	   "message.\n"
	   "\n"
	   "To obtain Security Certificates for the recipient(s), they must\n"
	   "first obtain one for themselves and send you a signed email\n"
	   "message. The Security Certificate will automatically be remembered\n"
	   "once it is received.\n")
ResDef(QTFE_AB_SECUR_YES, QTFE_ERR_OFFSET + 322,
       "You have this person's Security Certificate.\n"
	   "\n"
	   "This means that when you send this person email, it can be encrypted.\n"
	   "Encrypting a message is like sending it in an envelope, rather than a\n"
	   "postcard. It makes it difficult for other peope to view your message.\n"
	   "\n"
	   "This person's Security Certificate will expire on %s. When it\n"
	   "expires, you will no longer be able to send encrypted mail to this\n"
	   "person until they send you a new one.")
ResDef(QTFE_AB_SECUR_EXPIRED, QTFE_ERR_OFFSET + 323,
       "This person's Security Certificate has Expired.\n"
	   "\n"
	   "You cannot send this person encrypted mail until you obtain a new\n"
	   "Security Certificate for them. To do this, have this person send you a\n"
	   "signed mail message. This will automatically include the new Security\n"
	   "Certificate.")
ResDef(QTFE_AB_SECUR_REVOKED, QTFE_ERR_OFFSET + 324,
       "This person's Security Certificate has been revoked. This means that\n"
	   "the Certificate may have been lost or stolen.\n"
	   "\n"
	   "You cannot send this person encrypted mail until you obtain a new\n"
	   "Security Certificate. To do this, have this person send you a signed\n"
	   "mail message. This will automatically include the new Security\n"
	   "Certificate.")
ResDef(QTFE_AB_SECUR_YOU_NO, QTFE_ERR_OFFSET + 325,
       "You do not have a Security Certificate for yourself.\n"
	   "This means that you cannot receive encrypted mail, which would\n"
	   "make it difficult for other people to eavesdrop on messages\n"
	   "sent to you.\n"
	   "\n"
	   "You also cannot digitally sign mail. A digital signature would\n"
	   "indicate that the message was from you, and would also prevent\n"
	   "other people from tampering with your message.\n"
	   "\n"
	   "To obtain a Security Certificate, press the Get Certificate\n"
	   "button. After you obtain a Certificate it will automatically\n"
	   "be sent out with your signed messages so that other people can\n"
	   "send you encrypted mail.")
ResDef(QTFE_AB_SECUR_YOU_YES, QTFE_ERR_OFFSET + 326,
       "You have a Security Certificate.\n"
	   "This means that you can receive encrypted mail. In order to be able to\n"
	   "do this, you must first send mail to a person and sign the message. By\n"
	   "doing so, you send them your certificate, which makes it possible for\n"
	   "them to send you encryped mail.\n"
	   "\n"
	   "Encrypting a message is like sending it in a envelope, rather than a\n"
	   "postcard. It makes it difficult for other peope to eavesdrop on your\n"
	   "message.\n"
	   "\n"
	   "Your Security Certificate will expire on %s. Before it expires,\n"
	   "you will have to obtain a new Certificate.")
ResDef(QTFE_AB_SECUR_YOU_EXPIRED, QTFE_ERR_OFFSET + 327,
       "Your Security Certificate has Expired.\n"
	   "\n"
	   "This means that you can no longer digitally sign messages with this\n"
	   "certificate. You can continue to receive encrypted mail, however.\n"
	   "\n"
	   "This means that you must obtain another Certificate. Click on Show\n"
	   "Certificate to do so.")
ResDef(QTFE_AB_SECUR_YOU_REVOKED, QTFE_ERR_OFFSET + 328,
       "Your Security Certificate has been revoked.\n"
	   "This means that you can no longer digitally sign messages with this\n"
	   "certificate. You can continue to receive encrypted mail, however.\n"
	   "\n"
	   "This means that you must obtain another Certificate.")

ResDef(QTFE_AB_SECUR_SHOW, QTFE_ERR_OFFSET + 329,
       "Show Certificate")

ResDef(QTFE_AB_SECUR_GET, QTFE_ERR_OFFSET + 330,
       "Get Certificate")


ResDef(QTFE_AB_MLIST_TITLE, QTFE_ERR_OFFSET + 331,
       "Mailing List Info")
ResDef(QTFE_AB_MLIST_LISTNAME, QTFE_ERR_OFFSET + 332,
       "List Name:")
ResDef(QTFE_AB_MLIST_NICKNAME, QTFE_ERR_OFFSET + 333,
       "List Nickname:")
ResDef(QTFE_AB_MLIST_DESCRIPTION, QTFE_ERR_OFFSET + 334,
       "Description:")
ResDef(QTFE_AB_MLIST_PROMPT, QTFE_ERR_OFFSET + 335,
       "To add entries to this mailing list, type names from the address book")

ResDef(QTFE_AB_HEADER_NAME, QTFE_ERR_OFFSET + 336,
       "Name")
ResDef(QTFE_AB_HEADER_CERTIFICATE, QTFE_ERR_OFFSET + 337,
       "")
ResDef(QTFE_AB_HEADER_EMAIL, QTFE_ERR_OFFSET + 338,
       "Email Address")
ResDef(QTFE_AB_HEADER_NICKNAME, QTFE_ERR_OFFSET + 339,
       "Nickname")
ResDef(QTFE_AB_HEADER_COMPANY, QTFE_ERR_OFFSET + 340,
       "Organization")
ResDef(QTFE_AB_HEADER_LOCALITY, QTFE_ERR_OFFSET + 341,
       "City")
ResDef(QTFE_AB_HEADER_STATE, QTFE_ERR_OFFSET + 342,
       "State")

ResDef(QTFE_MN_UNREAD_AND_TOTAL, QTFE_ERR_OFFSET + 343,
       "%d Unread, %d Total")

ResDef(QTFE_AB_SEARCH, QTFE_ERR_OFFSET + 344,
       "Search")
ResDef(QTFE_AB_STOP, QTFE_ERR_OFFSET + 345,
       "Stop")

ResDef(QTFE_AB_REMOVE, QTFE_ERR_OFFSET + 346,
       "Remove")

ResDef(QTFE_AB_ADDRESSEE_PROMPT, QTFE_ERR_OFFSET + 347,
       "This message will be sent to:")

ResDef(QTFE_OFFLINE_NEWS_ALL_MSGS, QTFE_ERR_OFFSET + 348,
       "all")
ResDef(QTFE_OFFLINE_NEWS_ONE_MONTH_AGO, QTFE_ERR_OFFSET + 349,
       "1 month ago")

ResDef(QTFE_MN_DELIVERY_IN_PROGRESS, QTFE_ERR_OFFSET + 350,
       "Attachment operation cannot be completed now.\nMessage delivery or attachment load is in progress.")
ResDef(QTFE_MN_ITEM_ALREADY_ATTACHED, QTFE_ERR_OFFSET + 351,
       "This item is already attached:\n%s")
ResDef(QTFE_MN_TOO_MANY_ATTACHMENTS, QTFE_ERR_OFFSET + 352,
       "Attachment panel is full - no more attachments can be added.")
ResDef(QTFE_DOWNLOADING_NEW_MESSAGES, QTFE_ERR_OFFSET + 353,
	   "Getting new messages...")

ResDef(QTFE_AB_FRAME_TITLE, QTFE_ERR_OFFSET + 354,
           "Communicator Address Book for %s")

ResDef(QTFE_AB_SHOW_CERTI, QTFE_ERR_OFFSET + 355,
           "Show Certificate")

ResDef(QTFE_LANG_COL_ORDER, QTFE_ERR_OFFSET + 356,
       "Order")
ResDef(QTFE_LANG_COL_LANG, QTFE_ERR_OFFSET + 357,
       "Language")

ResDef(QTFE_MFILTER_INFO, QTFE_ERR_OFFSET + 358,
	   "Filters will be applied to incoming mail in the\n"
	   "following order:")


ResDef(QTFE_AB_COOLTALK_INFO, QTFE_ERR_OFFSET + 359,
       "To call another person using Mozilla Conference, you must first\n"
       "choose the server you would like to use to look up that person's\n"
	   "address.")
ResDef(QTFE_AB_COOLTALK_DEF_SERVER, QTFE_ERR_OFFSET + 360,
       "Mozilla Conference DLS Server")
ResDef(QTFE_AB_COOLTALK_SERVER_IK_HOST, QTFE_ERR_OFFSET + 361,
       "Specific DLS Server")
ResDef(QTFE_AB_COOLTALK_DIRECTIP, QTFE_ERR_OFFSET + 362,
       "Hostname or IP Address")

ResDef(QTFE_AB_COOLTALK_ADDR_LABEL, QTFE_ERR_OFFSET + 363,
       "Address:")
ResDef(QTFE_AB_COOLTALK_ADDR_EXAMPLE, QTFE_ERR_OFFSET + 364,
       "(For example, %s)")

ResDef(QTFE_AB_NAME_CARD_FOR, QTFE_ERR_OFFSET + 365,
       "Card for <%s>")
ResDef(QTFE_AB_NAME_NEW_CARD, QTFE_ERR_OFFSET + 366,
       "New Card")

ResDef(QTFE_MARKBYDATE_CAPTION, QTFE_ERR_OFFSET + 367,
	   "Mark Messages Read")
ResDef(QTFE_MARKBYDATE, QTFE_ERR_OFFSET + 368,
	   "Mark messages read up to: (MM/DD/YY)")
ResDef(QTFE_DATE_MUST_BE_MM_DD_YY, QTFE_ERR_OFFSET + 369,
	   "The date must be valid,\nand must be in the form MM/DD/YY.")
ResDef(QTFE_THERE_ARE_N_ARTICLES, QTFE_ERR_OFFSET + 370,
	   "There are %d new message headers to download for\nthis discussion group.")
ResDef(QTFE_GET_NEXT_N_MSGS, QTFE_ERR_OFFSET + 371,
	   "Next %d")

ResDef(QTFE_OFFLINE_NEWS_UNREAD_MSGS, QTFE_ERR_OFFSET + 372,
       "unread")
ResDef(QTFE_OFFLINE_NEWS_YESTERDAY, QTFE_ERR_OFFSET + 373,
       "yesterday")
ResDef(QTFE_OFFLINE_NEWS_ONE_WK_AGO, QTFE_ERR_OFFSET + 374,
       "1 week ago")
ResDef(QTFE_OFFLINE_NEWS_TWO_WKS_AGO, QTFE_ERR_OFFSET + 375,
       "2 weeks ago")
ResDef(QTFE_OFFLINE_NEWS_SIX_MONTHS_AGO, QTFE_ERR_OFFSET + 376,
       "6 months ago")
ResDef(QTFE_OFFLINE_NEWS_ONE_YEAR_AGO, QTFE_ERR_OFFSET + 377,
       "1 year ago")

ResDef(QTFE_ADDR_ENTRY_ALREADY_EXISTS, QTFE_ERR_OFFSET + 378,
       "An Address Book entry with this name and email address already exists.")

ResDef (QTFE_ADDR_ADD_PERSON_TO_ABOOK, QTFE_ERR_OFFSET + 379,
		"Mailing lists can only contain entries from the Personal Address Book.\n"
		"Would you like to add %s to the address book?")

ResDef (QTFE_MAKE_SURE_SERVER_AND_PORT_ARE_VALID, QTFE_ERR_OFFSET + 380,
		"Make sure both the server name and port are filled in and are valid.")

ResDef(QTFE_UNKNOWN_EMAIL_ADDR, QTFE_ERR_OFFSET + 381,
       "unknown")

ResDef(QTFE_AB_PICKER_TO, QTFE_ERR_OFFSET + 382,
       "To:")

ResDef(QTFE_AB_PICKER_CC, QTFE_ERR_OFFSET + 383,
       "CC:")

ResDef(QTFE_AB_PICKER_BCC, QTFE_ERR_OFFSET + 384,
       "BCC:")

ResDef(QTFE_AB_SEARCH_PROMPT, QTFE_ERR_OFFSET + 385,
       "Type Name")

ResDef(QTFE_MN_NEXT_500, QTFE_ERR_OFFSET + 386,
       "Next %d")

ResDef(QTFE_MN_INVALID_ATTACH_URL, QTFE_ERR_OFFSET + 387,
       "This document cannot be attached to a message:\n%s")

ResDef(QTFE_PREFS_CR_CARD, QTFE_ERR_OFFSET + 388,
       "Mozilla Communicator cannot find your\n"
	   "Personal Card. Would you like to create a\n"
	   "Personal Card now?")

ResDef(QTFE_MN_FOLDER_TITLE, QTFE_ERR_OFFSET + 389,
       "Communicator Message Center for %s")

ResDef(QTFE_PREFS_LABEL_STYLE_PLAIN, QTFE_ERR_OFFSET + 390, "Plain")
ResDef(QTFE_PREFS_LABEL_STYLE_BOLD, QTFE_ERR_OFFSET + 391, "Bold")
ResDef(QTFE_PREFS_LABEL_STYLE_ITALIC, QTFE_ERR_OFFSET + 392, "Italic")
ResDef(QTFE_PREFS_LABEL_STYLE_BOLDITALIC, QTFE_ERR_OFFSET + 393, "Bold Italic")
ResDef(QTFE_PREFS_LABEL_SIZE_NORMAL, QTFE_ERR_OFFSET + 394, "Normal")
ResDef(QTFE_PREFS_LABEL_SIZE_BIGGER, QTFE_ERR_OFFSET + 395, "Bigger")
ResDef(QTFE_PREFS_LABEL_SIZE_SMALLER, QTFE_ERR_OFFSET + 396, "Smaller")
ResDef(QTFE_PREFS_MAIL_FOLDER_SENT, QTFE_ERR_OFFSET + 397, "Sent")

ResDef(QTFE_MNC_CLOSE_WARNING, QTFE_ERR_OFFSET + 398,
       "Message has not been sent. Do you want to\n"
       "save the message in the Drafts Folder?")

ResDef(QTFE_SEARCH_INVALID_DATE, QTFE_ERR_OFFSET + 399,
       "Invalid Date Value. No search is performed.")

ResDef(QTFE_SEARCH_INVALID_MONTH, QTFE_ERR_OFFSET + 400,
       "Invalid value for the MONTH field.\n"
       "Please enter month in 2 digits (1-12).\n"
       "Try again!")

ResDef(QTFE_SEARCH_INVALID_DAY, QTFE_ERR_OFFSET + 401,
       "Invalid value for the DAY of the month field.\n"
       "Please enter date in 2 digits (1-31).\n"
       "Try again!")

ResDef(QTFE_SEARCH_INVALID_YEAR, QTFE_ERR_OFFSET + 402,
       "Invalid value for the YEAR field.\n"
       "Please enter year in 4 digits (e.g. 1997).\n"
       "Year value has to be 1900 or later.\n"
       "Try again!")

ResDef(QTFE_MNC_ADDRESS_TO, QTFE_ERR_OFFSET + 403,
	"To:")
ResDef(QTFE_MNC_ADDRESS_CC, QTFE_ERR_OFFSET + 404,
	"Cc:")
ResDef(QTFE_MNC_ADDRESS_BCC, QTFE_ERR_OFFSET + 405,
	"Bcc:")
ResDef(QTFE_MNC_ADDRESS_NEWSGROUP, QTFE_ERR_OFFSET + 406,
	"Newsgroup:")
ResDef(QTFE_MNC_ADDRESS_REPLY, QTFE_ERR_OFFSET + 407,
	"Reply To:")
ResDef(QTFE_MNC_ADDRESS_FOLLOWUP, QTFE_ERR_OFFSET + 408,
	"Followup To:")

ResDef(QTFE_MNC_OPTION_HIGHEST, QTFE_ERR_OFFSET + 409,
	"Highest")
ResDef(QTFE_MNC_OPTION_HIGH, QTFE_ERR_OFFSET + 410,
	"High")
ResDef(QTFE_MNC_OPTION_NORMAL, QTFE_ERR_OFFSET + 411,
	"Normal")
ResDef(QTFE_MNC_OPTION_LOW, QTFE_ERR_OFFSET + 412,
	"Highest")
ResDef(QTFE_MNC_OPTION_LOWEST, QTFE_ERR_OFFSET + 413,
	"Lowest")
ResDef(QTFE_MNC_ADDRESS, QTFE_ERR_OFFSET + 414,
	"Address")
ResDef(QTFE_MNC_ATTACHMENT, QTFE_ERR_OFFSET + 415,
	"Attachment")
ResDef(QTFE_MNC_OPTION, QTFE_ERR_OFFSET + 416,
	"Option")


ResDef(QTFE_DLG_OK, QTFE_ERR_OFFSET + 417, "OK")
ResDef(QTFE_DLG_CLEAR, QTFE_ERR_OFFSET + 418, "Clear")
ResDef(QTFE_DLG_CANCEL, QTFE_ERR_OFFSET + 419, "Cancel")

ResDef(QTFE_PRI_URGENT, QTFE_ERR_OFFSET + 420, "Urgent")
ResDef(QTFE_PRI_IMPORTANT, QTFE_ERR_OFFSET + 421, "Important")
ResDef(QTFE_PRI_NORMAL, QTFE_ERR_OFFSET + 422, "Normal")
ResDef(QTFE_PRI_FYI, QTFE_ERR_OFFSET + 423, "FYI")
ResDef(QTFE_PRI_JUNK, QTFE_ERR_OFFSET + 424, "Junk")

ResDef(QTFE_PRI_PRIORITY, QTFE_ERR_OFFSET + 425, "Priority")
ResDef(QTFE_COMPOSE_LABEL, QTFE_ERR_OFFSET + 426, "%sLabel")
ResDef(QTFE_COMPOSE_ADDRESSING, QTFE_ERR_OFFSET + 427, "Addressing")
ResDef(QTFE_COMPOSE_ATTACHMENT, QTFE_ERR_OFFSET + 428, "Attachment")
ResDef(QTFE_COMPOSE_COMPOSE, QTFE_ERR_OFFSET + 429, "Compose")
ResDef(QTFE_SEARCH_ALLMAILFOLDERS, QTFE_ERR_OFFSET + 430, "All Mail Folders")
ResDef(QTFE_SEARCH_AllNEWSGROUPS, QTFE_ERR_OFFSET + 431, "All Groups")
ResDef(QTFE_SEARCH_LDAPDIRECTORY, QTFE_ERR_OFFSET + 432, "LDAP Directory")
ResDef(QTFE_SEARCH_LOCATION, QTFE_ERR_OFFSET + 433, "Location")
ResDef(QTFE_SEARCH_SUBJECT, QTFE_ERR_OFFSET + 434, "Subject")
ResDef(QTFE_SEARCH_SENDER, QTFE_ERR_OFFSET + 435, "Sender")
ResDef(QTFE_SEARCH_DATE, QTFE_ERR_OFFSET + 436, "Date")

ResDef(QTFE_PREPARE_UPLOAD, QTFE_ERR_OFFSET + 437,
	   "Preparing file to publish:\n%.900s")

	/* Bookmark outliner column headers */
ResDef(QTFE_BM_OUTLINER_COLUMN_NAME, QTFE_ERR_OFFSET + 438, "Name")
ResDef(QTFE_BM_OUTLINER_COLUMN_LOCATION, QTFE_ERR_OFFSET + 439, "Location")
ResDef(QTFE_BM_OUTLINER_COLUMN_LASTVISITED, QTFE_ERR_OFFSET + 440, "Last Visited")
ResDef(QTFE_BM_OUTLINER_COLUMN_LASTMODIFIED, QTFE_ERR_OFFSET + 441, "Last Modified")

	/* Folder outliner column headers */
ResDef(QTFE_FOLDER_OUTLINER_COLUMN_NAME, QTFE_ERR_OFFSET + 442, "Name")
ResDef(QTFE_FOLDER_OUTLINER_COLUMN_TOTAL, QTFE_ERR_OFFSET + 443, "Total")
ResDef(QTFE_FOLDER_OUTLINER_COLUMN_UNREAD, QTFE_ERR_OFFSET + 444, "Unread")

	/* Category outliner column headers */
ResDef(QTFE_CATEGORY_OUTLINER_COLUMN_NAME, QTFE_ERR_OFFSET + 445, "Category")

	/* Subscribe outliner column headers */
ResDef(QTFE_SUB_OUTLINER_COLUMN_NAME, QTFE_ERR_OFFSET + 446, "Group Name")
ResDef(QTFE_SUB_OUTLINER_COLUMN_POSTINGS, QTFE_ERR_OFFSET + 447, "Postings")

	/* Thread outliner column headers */
ResDef(QTFE_THREAD_OUTLINER_COLUMN_SUBJECT, QTFE_ERR_OFFSET + 448, "Subject")
ResDef(QTFE_THREAD_OUTLINER_COLUMN_DATE, QTFE_ERR_OFFSET + 449, "Date")
ResDef(QTFE_THREAD_OUTLINER_COLUMN_PRIORITY, QTFE_ERR_OFFSET + 450, "Priority")
ResDef(QTFE_THREAD_OUTLINER_COLUMN_STATUS, QTFE_ERR_OFFSET + 451, "Status")
ResDef(QTFE_THREAD_OUTLINER_COLUMN_SENDER, QTFE_ERR_OFFSET + 452, "Sender")
ResDef(QTFE_THREAD_OUTLINER_COLUMN_RECIPIENT, QTFE_ERR_OFFSET + 453, "Recipient")

ResDef(QTFE_FOLDER_MENU_FILE_HERE, QTFE_ERR_OFFSET + 454, "File Here")

	/* Splash screen messages */
ResDef(QTFE_SPLASH_REGISTERING_CONVERTERS, QTFE_ERR_OFFSET + 455, "Registering Converters")
ResDef(QTFE_SPLASH_INITIALIZING_SECURITY_LIBRARY, QTFE_ERR_OFFSET + 456, "Initializing Security Library")
ResDef(QTFE_SPLASH_INITIALIZING_NETWORK_LIBRARY, QTFE_ERR_OFFSET + 457, "Initializing Network Library")
ResDef(QTFE_SPLASH_INITIALIZING_MESSAGE_LIBRARY, QTFE_ERR_OFFSET + 458, "Initializing Message Library")
ResDef(QTFE_SPLASH_INITIALIZING_IMAGE_LIBRARY, QTFE_ERR_OFFSET + 459, "Initializing Image Library")
ResDef(QTFE_SPLASH_INITIALIZING_MOCHA, QTFE_ERR_OFFSET + 460, "Initializing Javascript")
ResDef(QTFE_SPLASH_INITIALIZING_PLUGINS, QTFE_ERR_OFFSET + 461, "Initializing Plugins")


	/* Display factory messages */
ResDef(QTFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR, QTFE_ERR_OFFSET + 462, "%s: installColormap: %s must be yes, no, or guess.\n")

ResDef(QTFE_BM_FRAME_TITLE, QTFE_ERR_OFFSET + 463,
           "Communicator Bookmarks for %s")

ResDef(QTFE_UNTITLED, QTFE_ERR_OFFSET + 464,"Untitled")

ResDef(QTFE_HTML_NEWSGROUP_MSG, QTFE_ERR_OFFSET + 465,
	   "Unchecking this option means that this group\n"
	   "and all discussion groups above it do not\n"
	   "receive HTML messages")
ResDef(QTFE_SEC_ENCRYPTED, QTFE_ERR_OFFSET + 466,
	   "Encrypted")
ResDef(QTFE_SEC_NONE, QTFE_ERR_OFFSET + 467,
	   "None")
ResDef(QTFE_SHOW_COLUMN, QTFE_ERR_OFFSET + 468,
	   "Show Column")
ResDef(QTFE_HIDE_COLUMN, QTFE_ERR_OFFSET + 469,
	   "Hide Column")

ResDef(QTFE_DISABLED_BY_ADMIN, QTFE_ERR_OFFSET + 470, "That functionality has been disabled")

ResDef(QTFE_EDITOR_NEW_DOCNAME, QTFE_ERR_OFFSET + 471, "file: Untitled")

ResDef(QTFE_EMPTY_DIR, QTFE_ERR_OFFSET + 472, "%s is not set.\n")
ResDef(QTFE_NEWS_DIR, QTFE_ERR_OFFSET + 473, "Discussion groups directory")
ResDef(QTFE_MAIL_DIR, QTFE_ERR_OFFSET + 474, "Local mail directory")
ResDef(QTFE_DIR_DOES_NOT_EXIST, QTFE_ERR_OFFSET + 475, "%s \"%.255s\" does not exist.\n")

ResDef(QTFE_SEARCH_NO_MATCHES, QTFE_ERR_OFFSET + 476, "No matches found")

ResDef(QTFE_EMPTY_EMAIL_ADDR, QTFE_ERR_OFFSET + 477,
	   "Please enter a valid email address (e.g. user@internet.com).\n")

ResDef(QTFE_HISTORY_FRAME_TITLE, QTFE_ERR_OFFSET + 478,
           "Communicator History for %s")

ResDef(QTFE_HISTORY_OUTLINER_COLUMN_TITLE, QTFE_ERR_OFFSET + 479,
       "Title")
ResDef(QTFE_HISTORY_OUTLINER_COLUMN_LOCATION, QTFE_ERR_OFFSET + 480,
       "Location")
ResDef(QTFE_HISTORY_OUTLINER_COLUMN_FIRSTVISITED, QTFE_ERR_OFFSET + 481,
       "First Visited")
ResDef(QTFE_HISTORY_OUTLINER_COLUMN_LASTVISITED, QTFE_ERR_OFFSET + 482,
       "Last Visited")
ResDef(QTFE_HISTORY_OUTLINER_COLUMN_EXPIRES, QTFE_ERR_OFFSET + 483,
       "Expires")
ResDef(QTFE_HISTORY_OUTLINER_COLUMN_VISITCOUNT, QTFE_ERR_OFFSET + 484,
       "Visit Count")

ResDef(QTFE_PLACE_CONFERENCE_CALL_TO, QTFE_ERR_OFFSET + 485,
           "Place a call with Mozilla Conference to ")

ResDef(QTFE_SEND_MSG_TO, QTFE_ERR_OFFSET + 486,
           "Send a message to ")

ResDef(QTFE_INBOX_DOESNT_EXIST, QTFE_ERR_OFFSET + 487,
           "Default Inbox folder does not exist.\n"
	       "You will not be able to get new messages!")

ResDef(QTFE_HELPERS_APP_TELNET, QTFE_ERR_OFFSET + 488, "telnet")
ResDef(QTFE_HELPERS_APP_TN3270, QTFE_ERR_OFFSET + 489, "TN3270 application")
ResDef(QTFE_HELPERS_APP_RLOGIN, QTFE_ERR_OFFSET + 490, "rlogin")
ResDef(QTFE_HELPERS_APP_RLOGIN_USER, QTFE_ERR_OFFSET + 491, "rlogin with user")
ResDef(QTFE_HELPERS_CANNOT_DEL_STATIC_APPS, QTFE_ERR_OFFSET + 492,
	   "You cannot delete this application from the preferences.")
ResDef(QTFE_HELPERS_EMPTY_APP, QTFE_ERR_OFFSET + 493,
	   "The application field is not set.")


ResDef(QTFE_JAVASCRIPT_APP,  QTFE_ERR_OFFSET + 494,
       "[JavaScript Application]")

ResDef(QTFE_PREFS_UPGRADE, QTFE_ERR_OFFSET + 495,
	   "The preferences you have, version %s, is incompatible with the\n"
	   "current version %s. Would you like to save new preferences now?")

ResDef(QTFE_PREFS_DOWNGRADE, QTFE_ERR_OFFSET + 496,
	   "Please be aware that the preferences you have, version %s,\n"
	   "is incompatible with the current version %s.")

ResDef(QTFE_MOZILLA_WRONG_RESOURCE_FILE_VERSION, QTFE_ERR_OFFSET + 497,
	   "%s: program is version %s, but resources are version %s.\n\n"
	   "\tThis means that there is an inappropriate `%s' file installed\n"
	   "\tin the app-defaults directory.  Check these environment variables\n"
	   "\tand the directories to which they point:\n\n"
	   "	$XAPPLRESDIR\n"
	   "	$XFILESEARCHPATH\n"
	   "	$XUSERFILESEARCHPATH\n\n"
	   "\tAlso check for this file in your home directory, or in the\n"
	   "\tdirectory called `app-defaults' somewhere under /usr/lib/.")

ResDef(QTFE_MOZILLA_CANNOT_FOND_RESOURCE_FILE, QTFE_ERR_OFFSET + 498,
	   "%s: couldn't find our resources?\n\n"
	   "\tThis could mean that there is an inappropriate `%s' file\n"
	   "\tinstalled in the app-defaults directory.  Check these environment\n"
	   "\tvariables and the directories to which they point:\n\n"
	   "	$XAPPLRESDIR\n"
	   "	$XFILESEARCHPATH\n"
	   "	$XUSERFILESEARCHPATH\n\n"
	   "\tAlso check for this file in your home directory, or in the\n"
	   "\tdirectory called `app-defaults' somewhere under /usr/lib/.")

ResDef(QTFE_MOZILLA_LOCALE_NOT_SUPPORTED_BY_XLIB, QTFE_ERR_OFFSET + 499,
	   "%s: locale `%s' not supported by Xlib; trying `C'.\n")

ResDef(QTFE_MOZILLA_LOCALE_C_NOT_SUPPORTED, QTFE_ERR_OFFSET + 500,
	   "%s: locale `C' not supported.\n")

ResDef(QTFE_MOZILLA_LOCALE_C_NOT_SUPPORTED_EITHER, QTFE_ERR_OFFSET + 501,
	   "%s: locale `C' not supported either.\n")

ResDef(QTFE_MOZILLA_NLS_LOSAGE, QTFE_ERR_OFFSET + 502,
	   "\n"
	   "	If the $XNLSPATH directory does not contain the proper config files,\n"
	   "	%s will crash the first time you try to paste into a text\n"
	   "	field.  (This is a bug in the X11R5 libraries against which this\n"
	   "	program was linked.)\n"
	   "\n"
	   "	Since neither X11R4 nor X11R6 come with these config files, we have\n"
	   "	included them with the %s distribution.  The normal place\n"
	   "	for these files is %s.\n"
	   "    If you can't create that\n"
	   "	directory, you should set the $XNLSPATH environment variable to\n"
	   "	point at the place where you installed the files.\n"
	   "\n")

ResDef(QTFE_MOZILLA_NLS_PATH_NOT_SET_CORRECTLY, QTFE_ERR_OFFSET + 503,
	   "	Perhaps the $XNLSPATH environment variable is not set correctly?\n")

ResDef(QTFE_MOZILLA_UNAME_FAILED, QTFE_ERR_OFFSET + 504,
	   " uname failed")

ResDef(QTFE_MOZILLA_UNAME_FAILED_CANT_DETERMINE_SYSTEM, QTFE_ERR_OFFSET + 505,
	   "%s: uname() failed; can't determine what system we're running on\n")

ResDef(QTFE_MOZILLA_TRYING_TO_RUN_SUNOS_ON_SOLARIS, QTFE_ERR_OFFSET + 506,
	   "%s: this is a SunOS 4.1.3 executable, and you are running it on\n"
	   "	SunOS %s (Solaris.)  It would be a very good idea for you to\n"
	   "	run the Solaris-specific binary instead.  Bad Things may happen.\n\n")

ResDef(QTFE_MOZILLA_FAILED_TO_INITIALIZE_EVENT_QUEUE, QTFE_ERR_OFFSET + 507,
	   "%s: failed to initialize mozilla_event_queue!\n")

ResDef(QTFE_MOZILLA_INVALID_REMOTE_OPTION, QTFE_ERR_OFFSET + 508,
	   "%s: invalid `-remote' option \"%s\"\n")

ResDef(QTFE_MOZILLA_ID_OPTION_MUST_PRECEED_REMOTE_OPTIONS, QTFE_ERR_OFFSET + 509,
	   "%s: the `-id' option must preceed all `-remote' options.\n")

ResDef(QTFE_MOZILLA_ONLY_ONE_ID_OPTION_CAN_BE_USED, QTFE_ERR_OFFSET + 510,
	   "%s: only one `-id' option may be used.\n")

ResDef(QTFE_MOZILLA_INVALID_OD_OPTION, QTFE_ERR_OFFSET + 511,
	   "%s: invalid `-id' option \"%s\"\n")

ResDef(QTFE_MOZILLA_ID_OPTION_CAN_ONLY_BE_USED_WITH_REMOTE,QTFE_ERR_OFFSET + 512,
	   "%s: the `-id' option may only be used with `-remote'.\n")

ResDef(QTFE_MOZILLA_XKEYSYMDB_SET_BUT_DOESNT_EXIST, QTFE_ERR_OFFSET + 513,
	   "%s: warning: $XKEYSYMDB is %s,\n"
	   "	but that file doesn't exist.\n")

ResDef(QTFE_MOZILLA_NO_XKEYSYMDB_FILE_FOUND, QTFE_ERR_OFFSET + 514,
	   "%s: warning: no XKeysymDB file in either\n"
	   "	%s, %s, or %s\n"
	   "	Please set $XKEYSYMDB to the correct XKeysymDB"
	   " file.\n")

ResDef(QTFE_MOZILLA_NOT_FOUND_IN_PATH, QTFE_ERR_OFFSET + 515,
	   "%s: not found in PATH!\n")

ResDef(QTFE_MOZILLA_RENAMING_SOMETHING_TO_SOMETHING, QTFE_ERR_OFFSET + 516,
	   "renaming %s to %s:")

ResDef(QTFE_COMMANDS_OPEN_URL_USAGE, QTFE_ERR_OFFSET + 517,
	   "%s: usage: OpenURL(url [ , new-window|window-name ] )\n")

ResDef(QTFE_COMMANDS_OPEN_FILE_USAGE, QTFE_ERR_OFFSET + 518,
	   "%s: usage: OpenFile(file)\n")

ResDef(QTFE_COMMANDS_PRINT_FILE_USAGE, QTFE_ERR_OFFSET + 519,
	   "%s: usage: print([filename])\n")

ResDef(QTFE_COMMANDS_SAVE_AS_USAGE, QTFE_ERR_OFFSET + 520,
	   "%s: usage: SaveAS(file, output-data-type)\n")

ResDef(QTFE_COMMANDS_SAVE_AS_USAGE_TWO, QTFE_ERR_OFFSET + 521,
	   "%s: usage: SaveAS(file, [output-data-type])\n")

ResDef(QTFE_COMMANDS_MAIL_TO_USAGE, QTFE_ERR_OFFSET + 522,
	   "%s: usage: mailto(address ...)\n")

ResDef(QTFE_COMMANDS_FIND_USAGE, QTFE_ERR_OFFSET + 523,
	   "%s: usage: find(string)\n")

ResDef(QTFE_COMMANDS_ADD_BOOKMARK_USAGE, QTFE_ERR_OFFSET + 524,
	   "%s: usage: addBookmark(url, title)\n")

ResDef(QTFE_COMMANDS_HTML_HELP_USAGE, QTFE_ERR_OFFSET + 525,
	   "%s: usage: htmlHelp(map-file, id, search-text)\n")

ResDef(QTFE_COMMANDS_UNPARSABLE_ENCODING_FILTER_SPEC, QTFE_ERR_OFFSET + 526,
	   "%s: unparsable encoding filter spec: %s\n")

ResDef(QTFE_COMMANDS_UPLOAD_FILE, QTFE_ERR_OFFSET + 527,
	   "Upload File")

ResDef(QTFE_MOZILLA_ERROR_SAVING_OPTIONS, QTFE_ERR_OFFSET + 528,
	   "error saving options")

ResDef(QTFE_URLBAR_FILE_HEADER, QTFE_ERR_OFFSET + 529,
	   "# Mozilla User History File\n"
	   "# Version: %s\n"
	   "# This is a generated file!  Do not edit.\n"
	   "\n")

ResDef(QTFE_LAY_TOO_MANY_ARGS_TO_ACTIVATE_LINK_ACTION, QTFE_ERR_OFFSET + 530,
	   "%s: too many args (%d) to ActivateLinkAction()\n")

ResDef(QTFE_LAY_UNKNOWN_PARAMETER_TO_ACTIVATE_LINK_ACTION, QTFE_ERR_OFFSET + 531,
	   "%s: unknown parameter (%s) to ActivateLinkAction()\n")

ResDef(QTFE_LAY_LOCAL_FILE_URL_UNTITLED, QTFE_ERR_OFFSET + 532,
	   "file:///Untitled")

ResDef(QTFE_DIALOGS_PRINTING, QTFE_ERR_OFFSET + 533,
	   "printing")

ResDef(QTFE_DIALOGS_DEFAULT_VISUAL_AND_COLORMAP, QTFE_ERR_OFFSET + 534,
	   "\nThis is the default visual and color map.")

ResDef(QTFE_DIALOGS_DEFAULT_VISUAL_AND_PRIVATE_COLORMAP, QTFE_ERR_OFFSET + 535,
	   "\nThis is the default visual with a private map.")

ResDef(QTFE_DIALOGS_NON_DEFAULT_VISUAL, QTFE_ERR_OFFSET + 536,
	   "\nThis is a non-default visual.")

ResDef(QTFE_DIALOGS_FROM_NETWORK, QTFE_ERR_OFFSET + 537,
	   "from network")

ResDef(QTFE_DIALOGS_FROM_DISK_CACHE, QTFE_ERR_OFFSET + 538,
	   "from disk cache")

ResDef(QTFE_DIALOGS_FROM_MEMORY_CACHE, QTFE_ERR_OFFSET + 539,
	   "from memory cache")

ResDef(QTFE_DIALOGS_DEFAULT, QTFE_ERR_OFFSET + 540,
	   "default")

ResDef(QTFE_HISTORY_TOO_FEW_ARGS_TO_HISTORY_ITEM, QTFE_ERR_OFFSET + 541,
	   "%s: too few args (%d) to HistoryItem()\n")

ResDef(QTFE_HISTORY_TOO_MANY_ARGS_TO_HISTORY_ITEM, QTFE_ERR_OFFSET + 542,
	   "%s: too many args (%d) to HistoryItem()\n")

ResDef(QTFE_HISTORY_UNKNOWN_PARAMETER_TO_HISTORY_ITEM, QTFE_ERR_OFFSET + 543,
	   "%s: unknown parameter (%s) to HistoryItem()\n")

ResDef(QTFE_REMOTE_S_UNABLE_TO_READ_PROPERTY, QTFE_ERR_OFFSET + 544,
	   "%s: unable to read %s property\n")

ResDef(QTFE_REMOTE_S_INVALID_DATA_ON_PROPERTY, QTFE_ERR_OFFSET + 545,
	   "%s: invalid data on %s of window 0x%x.\n")
	
ResDef(QTFE_REMOTE_S_509_INTERNAL_ERROR, QTFE_ERR_OFFSET + 546,
	   "509 internal error: unable to translate"
	   "window 0x%x to a widget")

ResDef(QTFE_REMOTE_S_500_UNPARSABLE_COMMAND, QTFE_ERR_OFFSET + 547,
	   "500 unparsable command: %s")

ResDef(QTFE_REMOTE_S_501_UNRECOGNIZED_COMMAND, QTFE_ERR_OFFSET + 548,
	   "501 unrecognized command: %s")
	
ResDef(QTFE_REMOTE_S_502_NO_APPROPRIATE_WINDOW, QTFE_ERR_OFFSET + 549,
	   "502 no appropriate window for %s")
	
ResDef(QTFE_REMOTE_S_200_EXECUTED_COMMAND, QTFE_ERR_OFFSET + 550,
	   "200 executed command: %s(")

ResDef(QTFE_REMOTE_200_EXECUTED_COMMAND, QTFE_ERR_OFFSET + 551,
	   "200 executed command: %s(")

ResDef(QTFE_SCROLL_WINDOW_GRAVITY_WARNING, QTFE_ERR_OFFSET + 552,
	   "%s: windowGravityWorks: %s must be yes, no, or guess.\n")

ResDef(QTFE_COULD_NOT_DUP_STDERR, QTFE_ERR_OFFSET + 553,
	   "could not dup() a stderr:")

ResDef(QTFE_COULD_NOT_FDOPEN_STDERR, QTFE_ERR_OFFSET + 554,
	   "could not fdopen() the new stderr:")

ResDef(QTFE_COULD_NOT_DUP_A_NEW_STDERR, QTFE_ERR_OFFSET + 555,
	   "could not dup() a new stderr:")

ResDef(QTFE_COULD_NOT_DUP_A_STDOUT, QTFE_ERR_OFFSET + 556,
	   "could not dup() a stdout:")

ResDef(QTFE_COULD_NOT_FDOPEN_THE_NEW_STDOUT, QTFE_ERR_OFFSET + 557,
	   "could not fdopen() the new stdout:")

ResDef(QTFE_COULD_NOT_DUP_A_NEW_STDOUT, QTFE_ERR_OFFSET + 558,
	   "could not dup() a new stdout:")

ResDef(QTFE_HPUX_VERSION_NONSENSE, QTFE_ERR_OFFSET + 559,
	   "\n%s:\n\nThis Mozilla Communicator binary does not run on %s %s.\n\n"
	   "Please visit http://home.netscape.com/ for a version of Communicator"
	   " that runs\non your system.\n\n")

ResDef(QTFE_BM_OUTLINER_COLUMN_CREATEDON, QTFE_ERR_OFFSET + 560,
       "Created On")

ResDef(QTFE_VIRTUALNEWSGROUP, QTFE_ERR_OFFSET + 561,
	"Virtual Discussion Group")

ResDef(QTFE_VIRTUALNEWSGROUPDESC, QTFE_ERR_OFFSET + 562,
       "Saving search criteria will create a Virtual Discussion Group\n"
       "based on that criteria. The Virtual Discussion Group will be \n"
       "available from the Message Center.\n" )

ResDef(QTFE_EXIT_CONFIRMATION, QTFE_ERR_OFFSET + 563,
       "Mozilla Exit Confirmation\n")

ResDef(QTFE_EXIT_CONFIRMATIONDESC, QTFE_ERR_OFFSET + 564,
       "Close all windows and exit Mozilla?\n")

ResDef(QTFE_MAIL_WARNING, QTFE_ERR_OFFSET + 565,
       "Mozilla Mail\n")

ResDef(QTFE_SEND_UNSENTMAIL, QTFE_ERR_OFFSET + 566,
       "Outbox folder contains unsent messages\n"
       "Send them now?")

ResDef(QTFE_PREFS_WRONG_POP_USER_NAME, QTFE_ERR_OFFSET + 567,
       "Your POP user name is just your user name (e.g. user),\n"
       "not your full POP address (e.g. user@internet.com).")

ResDef(QTFE_PREFS_ENTER_VALID_INFO, QTFE_ERR_OFFSET + 568,
       "Please enter valid information.")
ResDef(QTFE_CANNOT_EDIT_JS_MAILFILTERS, QTFE_ERR_OFFSET + 569,
	   "The editing of JavaScript message filters is not available\n"
	   "in this release of Communicator.")

ResDef(QTFE_AB_HEADER_PHONE, QTFE_ERR_OFFSET + 570,
       "Phone")

ResDef(QTFE_PURGING_NEWS_MESSAGES, QTFE_ERR_OFFSET + 571,
	   "Cleaning up news messages...")

ResDef(QTFE_PREFS_RESTART_FOR_FONT_CHANGES, QTFE_ERR_OFFSET + 572,
	   "Your font preferences will not take effect until you restart Communicator.")

ResDef(QTFE_DND_NO_EMAILADDRESS, QTFE_ERR_OFFSET + 573,
	   "One or more items in the selection you are dragging do not contain an email address\n"
	   "and was not added to the list. Please make sure each item in your selection includes\n"
	   "an email address.")

ResDef(QTFE_NEW_FOLDER_PROMPT, QTFE_ERR_OFFSET + 574, "New Folder Name:")
ResDef(QTFE_USAGE_MSG4, QTFE_ERR_OFFSET + 575, "\
       -component-bar            Show only the Component Bar.\n\
\n\
       -composer                 Open all command line URLs in Composer.\n\
       -edit                     Same as -composer.\n\
\n\
       -messenger                Show Messenger Mailbox (INBOX).\n\
       -mail                     Same as -messenger.\n\
\n\
       -discussions              Show Collabra Discussions.\n\
       -news                     Same as -discussions.\n\n" )
ResDef(QTFE_USAGE_MSG5, QTFE_ERR_OFFSET + 576, "\
       Arguments which are not switches are interpreted as either files or\n\
       URLs to be loaded.\n\n" )

ResDef(QTFE_SEARCH_DLG_PROMPT, QTFE_ERR_OFFSET + 577, "Searching:" )
ResDef(QTFE_SEARCH_AB_RESULT, QTFE_ERR_OFFSET + 578, "Search Results" )
ResDef(QTFE_SEARCH_AB_RESULT_FOR, QTFE_ERR_OFFSET + 579, "Search results for:" )

ResDef(QTFE_SEARCH_ATTRIB_NAME, QTFE_ERR_OFFSET + 580, "Name" )
ResDef(QTFE_SEARCH_ATTRIB_EMAIL, QTFE_ERR_OFFSET + 581, "Email" )
ResDef(QTFE_SEARCH_ATTRIB_ORGAN, QTFE_ERR_OFFSET + 582, "Organization" )
ResDef(QTFE_SEARCH_ATTRIB_ORGANU, QTFE_ERR_OFFSET + 583, "Department" )

ResDef(QTFE_SEARCH_RESULT_PROMPT, QTFE_ERR_OFFSET + 584, "Search results will appear in address book window" )

ResDef(QTFE_SEARCH_BASIC, QTFE_ERR_OFFSET + 585, "Basic Search >>" )
ResDef(QTFE_SEARCH_ADVANCED, QTFE_ERR_OFFSET + 586, "Advanced Search >>" )
ResDef(QTFE_SEARCH_MORE, QTFE_ERR_OFFSET + 587, "More" )
ResDef(QTFE_SEARCH_FEWER, QTFE_ERR_OFFSET + 588, "Fewer" )

ResDef(QTFE_SEARCH_BOOL_PROMPT, QTFE_ERR_OFFSET + 589, "Find items which" )
ResDef(QTFE_SEARCH_BOOL_AND_PROMPT, QTFE_ERR_OFFSET + 590, "Match all of the following" )
ResDef(QTFE_SEARCH_BOOL_OR_PROMPT, QTFE_ERR_OFFSET + 591, "Match any of the following" )
ResDef(QTFE_SEARCH_BOOL_WHERE, QTFE_ERR_OFFSET + 592, "where" )
ResDef(QTFE_SEARCH_BOOL_THE, QTFE_ERR_OFFSET + 593, "the" )
ResDef(QTFE_SEARCH_BOOL_AND_THE, QTFE_ERR_OFFSET + 594, "and the" )
ResDef(QTFE_SEARCH_BOOL_OR_THE, QTFE_ERR_OFFSET + 595, "or the" )

ResDef(QTFE_ABDIR_DESCRIPT, QTFE_ERR_OFFSET + 596, "Description:" )
ResDef(QTFE_ABDIR_SERVER, QTFE_ERR_OFFSET + 597, "LDAP Server:" )
ResDef(QTFE_ABDIR_SERVER_ROOT, QTFE_ERR_OFFSET + 598, "Server Root:" )
ResDef(QTFE_ABDIR_PORT_NUM, QTFE_ERR_OFFSET + 599, "Port Number:" )
ResDef(QTFE_ABDIR_MAX_HITS, QTFE_ERR_OFFSET + 600, "Maximum Number of Hits:" )
ResDef(QTFE_ABDIR_SECURE, QTFE_ERR_OFFSET + 601, "Secure" )
ResDef(QTFE_ABDIR_SAVE_PASSWD, QTFE_ERR_OFFSET + 602, "Save Password" )
ResDef(QTFE_ABDIR_DLG_TITLE, QTFE_ERR_OFFSET + 603, "Directory Info" )
ResDef(QTFE_AB2PANE_DIR_HEADER, QTFE_ERR_OFFSET + 604, "Directories" )
ResDef(QTFE_AB_SEARCH_DLG, QTFE_ERR_OFFSET + 605, "Search..." )

ResDef(QTFE_CUSTOM_HEADER, QTFE_ERR_OFFSET + 606, "Custom Header:" )
ResDef(QTFE_AB_DISPLAYNAME, QTFE_ERR_OFFSET + 607, "Display Name:")
ResDef(QTFE_AB_PAGER, QTFE_ERR_OFFSET + 608, "Pager:")
ResDef(QTFE_AB_CELLULAR, QTFE_ERR_OFFSET + 609, "Cellular:")

ResDef(QTFE_DND_MESSAGE_ERROR, QTFE_ERR_OFFSET + 610,
      "Cannot drop into the targeted destination folder."  )
ResDef(QTFE_ABDIR_USE_PASSWD, QTFE_ERR_OFFSET + 611,
	   "Login with name and password")

ResDef(NO_SPELL_SHLIB_FOUND, QTFE_ERR_OFFSET + 612,
       "No spellchk library found")

END_STR(mcom_cmd_qtfe_qtfe_err_h_strings)

#endif /* __QTFE_QTFE_ERR_H_ */
