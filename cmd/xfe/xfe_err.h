/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __XFE_XFE_ERR_H_
#define __XFE_XFE_ERR_H_

#include <resdef.h>

#define XFE_ERR_OFFSET 1000

RES_START
BEGIN_STR(mcom_cmd_xfe_xfe_err_h_strings)
ResDef(DEFAULT_ADD_BOOKMARK_ROOT, XFE_ERR_OFFSET + 0, "End of List")
ResDef(DEFAULT_MENU_BOOKMARK_ROOT, XFE_ERR_OFFSET + 1, "Entire List")
ResDef(XFE_SAVE_AS_TYPE_ENCODING, XFE_ERR_OFFSET + 3, "Save As... (type %.90s, encoding %.90s)")
ResDef(XFE_SAVE_AS_TYPE, XFE_ERR_OFFSET + 4, "Save As... (type %.90s)")
ResDef(XFE_SAVE_AS_ENCODING, XFE_ERR_OFFSET + 5, "Save As... (encoding %.90s)")
ResDef(XFE_SAVE_AS, XFE_ERR_OFFSET + 6, "Save As...")
ResDef(XFE_ERROR_OPENING_FILE, XFE_ERR_OFFSET + 7, "Error opening %.900s:")
ResDef(XFE_ERROR_DELETING_FILE, XFE_ERR_OFFSET + 8, "Error deleting %.900s:")
ResDef(XFE_LOG_IN_AS, XFE_ERR_OFFSET + 9, "When connected, log in as user `%.900s'")
ResDef(XFE_OUT_OF_MEMORY_URL, XFE_ERR_OFFSET + 10, "Out Of Memory -- Can't Open URL")
ResDef(XFE_COULD_NOT_LOAD_FONT, XFE_ERR_OFFSET + 11, "couldn't load:\n%s")
ResDef(XFE_USING_FALLBACK_FONT, XFE_ERR_OFFSET + 12, "%s\nNo other resources were reasonable!\nUsing fallback font \"%s\" instead.")
ResDef(XFE_NO_FALLBACK_FONT, XFE_ERR_OFFSET + 13, "%s\nNo other resources were reasonable!\nThe fallback font \"%s\" could not be loaded!\nGiving up.")
ResDef(XFE_DISCARD_BOOKMARKS, XFE_ERR_OFFSET + 14, "Bookmarks file has changed on disk: discard your changes?")
ResDef(XFE_RELOAD_BOOKMARKS, XFE_ERR_OFFSET + 15, "Bookmarks file has changed on disk: reload it?")
ResDef(XFE_NEW_ITEM, XFE_ERR_OFFSET + 16, "New Item")
ResDef(XFE_NEW_HEADER, XFE_ERR_OFFSET + 17, "New Header")
ResDef(XFE_REMOVE_SINGLE_ENTRY, XFE_ERR_OFFSET + 18, "Remove category \"%.900s\" and its %d entry?")
ResDef(XFE_REMOVE_MULTIPLE_ENTRIES, XFE_ERR_OFFSET + 19, "Remove category \"%.900s\" and its %d entries?")
ResDef(XFE_EXPORT_BOOKMARK, XFE_ERR_OFFSET + 20, "Export Bookmark")
ResDef(XFE_IMPORT_BOOKMARK, XFE_ERR_OFFSET + 21, "Import Bookmark")
ResDef(XFE_SECURITY_WITH, XFE_ERR_OFFSET + 22, "This version supports %s security with %s.")
ResDef(XFE_SECURITY_DISABLED, XFE_ERR_OFFSET + 23, "Security disabled")
ResDef(XFE_WELCOME_HTML, XFE_ERR_OFFSET + 24, "file:/usr/local/lib/netscape/docs/Welcome.html")
ResDef(XFE_DOCUMENT_DONE, XFE_ERR_OFFSET + 25, "Document: Done.")
ResDef(XFE_OPEN_FILE, XFE_ERR_OFFSET + 26, "Open File")
ResDef(XFE_ERROR_OPENING_PIPE, XFE_ERR_OFFSET + 27, "Error opening pipe to %.900s")
ResDef(XFE_WARNING, XFE_ERR_OFFSET + 28, "Warning:\n\n")
ResDef(XFE_FILE_DOES_NOT_EXIST, XFE_ERR_OFFSET + 29, "%s \"%.255s\" does not exist.\n")
ResDef(XFE_HOST_IS_UNKNOWN, XFE_ERR_OFFSET + 30, "%s \"%.255s\" is unknown.\n")
ResDef(XFE_NO_PORT_NUMBER, XFE_ERR_OFFSET + 31, "No port number specified for %s.\n")
ResDef(XFE_MAIL_HOST, XFE_ERR_OFFSET + 32, "Mail host")
ResDef(XFE_NEWS_HOST, XFE_ERR_OFFSET + 33, "News host")
ResDef(XFE_NEWS_RC_DIRECTORY, XFE_ERR_OFFSET + 34, "News RC directory")
ResDef(XFE_TEMP_DIRECTORY, XFE_ERR_OFFSET + 35, "Temporary directory")
ResDef(XFE_FTP_PROXY_HOST, XFE_ERR_OFFSET + 36, "FTP proxy host")
ResDef(XFE_GOPHER_PROXY_HOST, XFE_ERR_OFFSET + 37, "Gopher proxy host")
ResDef(XFE_HTTP_PROXY_HOST, XFE_ERR_OFFSET + 38, "HTTP proxy host")
ResDef(XFE_HTTPS_PROXY_HOST, XFE_ERR_OFFSET + 39, "HTTPS proxy host")
ResDef(XFE_WAIS_PROXY_HOST, XFE_ERR_OFFSET + 40, "WAIS proxy host")
ResDef(XFE_SOCKS_HOST, XFE_ERR_OFFSET + 41, "SOCKS host")
ResDef(XFE_GLOBAL_MIME_FILE, XFE_ERR_OFFSET + 42, "Global MIME types file")
ResDef(XFE_PRIVATE_MIME_FILE, XFE_ERR_OFFSET + 43, "Private MIME types file")
ResDef(XFE_GLOBAL_MAILCAP_FILE, XFE_ERR_OFFSET + 44, "Global mailcap file")
ResDef(XFE_PRIVATE_MAILCAP_FILE, XFE_ERR_OFFSET + 45, "Private mailcap file")
ResDef(XFE_BM_CANT_DEL_ROOT, XFE_ERR_OFFSET + 46, "Cannot delete toplevel bookmark")
ResDef(XFE_BM_CANT_CUT_ROOT, XFE_ERR_OFFSET + 47, "Cannot cut toplevel bookmark")
ResDef(XFE_BM_ALIAS, XFE_ERR_OFFSET + 48, "This is an alias to the following Bookmark:")
/* NPS i18n stuff */
ResDef(XFE_FILE_OPEN, XFE_ERR_OFFSET + 49, "File Open...")
ResDef(XFE_PRINTING_OF_FRAMES_IS_CURRENTLY_NOT_SUPPORTED, XFE_ERR_OFFSET + 50, "Printing of frames is currently not supported.")
ResDef(XFE_ERROR_SAVING_OPTIONS, XFE_ERR_OFFSET + 51, "error saving options")
ResDef(XFE_UNKNOWN_ESCAPE_CODE, XFE_ERR_OFFSET + 52,
"unknown %s escape code %%%c:\n%%h = host, %%p = port, %%u = user")
ResDef(XFE_COULDNT_FORK, XFE_ERR_OFFSET + 53, "couldn't fork():")
ResDef(XFE_EXECVP_FAILED, XFE_ERR_OFFSET + 54, "%s: execvp(%s) failed")
ResDef(XFE_SAVE_FRAME_AS, XFE_ERR_OFFSET + 55, "Save Frame as...")
ResDef(XFE_PRINT_FRAME, XFE_ERR_OFFSET + 57, "Print Frame...")
ResDef(XFE_PRINT, XFE_ERR_OFFSET + 58 , "Print...")
ResDef(XFE_DOWNLOAD_FILE, XFE_ERR_OFFSET + 59, "Download File: %s")
ResDef(XFE_COMPOSE_NO_SUBJECT, XFE_ERR_OFFSET + 60, "Compose: (No Subject)")
ResDef(XFE_COMPOSE, XFE_ERR_OFFSET + 61, "Compose: %s")
ResDef(XFE_NETSCAPE_UNTITLED, XFE_ERR_OFFSET + 62, "Mozilla: <untitled>")
ResDef(XFE_NETSCAPE, XFE_ERR_OFFSET + 63, "Mozilla: %s")
ResDef(XFE_NO_SUBJECT, XFE_ERR_OFFSET + 64, "(no subject)")
ResDef(XFE_UNKNOWN_ERROR_CODE, XFE_ERR_OFFSET + 65, "unknown error code %d")
ResDef(XFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST, XFE_ERR_OFFSET + 66,
"Invalid File attachment.\n%s: doesn't exist.\n")
ResDef(XFE_INVALID_FILE_ATTACHMENT_NOT_READABLE, XFE_ERR_OFFSET + 67,
"Invalid File attachment.\n%s: not readable.\n")
ResDef(XFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY, XFE_ERR_OFFSET + 68,
"Invalid File attachment.\n%s: is a directory.\n")
ResDef(XFE_COULDNT_FORK_FOR_MOVEMAIL, XFE_ERR_OFFSET + 69,
"couldn't fork() for movemail")
ResDef(XFE_PROBLEMS_EXECUTING, XFE_ERR_OFFSET + 70, "problems executing %s:")
ResDef(XFE_TERMINATED_ABNORMALLY, XFE_ERR_OFFSET + 71, "%s terminated abnormally:")
ResDef(XFE_UNABLE_TO_OPEN, XFE_ERR_OFFSET + 72, "Unable to open %.900s")
ResDef(XFE_PLEASE_ENTER_NEWS_HOST, XFE_ERR_OFFSET + 73,
"Please enter your news host in one\n\
of the following formats:\n\n\
    news://HOST,\n\
    news://HOST:PORT,\n\
    snews://HOST, or\n\
    snews://HOST:PORT\n\n" )

ResDef(XFE_MOVEMAIL_FAILURE_SUFFIX, XFE_ERR_OFFSET + 74,
       "For the internal movemail method to work, we must be able to create\n\
lock files in the mail spool directory.  On many systems, this is best\n\
accomplished by making that directory be mode 01777.  If that is not\n\
possible, then an external setgid/setuid movemail program must be used.\n\
Please see the Release Notes for more information.")
ResDef(XFE_CANT_MOVE_MAIL, XFE_ERR_OFFSET + 75, "Can't move mail from %.200s")
ResDef(XFE_CANT_GET_NEW_MAIL_LOCK_FILE_EXISTS, XFE_ERR_OFFSET + 76, "Can't get new mail; a lock file %.200s exists.")
ResDef(XFE_CANT_GET_NEW_MAIL_UNABLE_TO_CREATE_LOCK_FILE, XFE_ERR_OFFSET + 77, "Can't get new mail; unable to create lock file %.200s")
ResDef(XFE_CANT_GET_NEW_MAIL_SYSTEM_ERROR, XFE_ERR_OFFSET + 78, "Can't get new mail; a system error occurred.")
ResDef(XFE_CANT_MOVE_MAIL_UNABLE_TO_OPEN, XFE_ERR_OFFSET + 79, "Can't move mail; unable to open %.200s")
ResDef(XFE_CANT_MOVE_MAIL_UNABLE_TO_READ, XFE_ERR_OFFSET + 80, "Can't move mail; unable to read %.200s")
ResDef(XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE, XFE_ERR_OFFSET + 81, "Can't move mail; unable to write to %.200s")
ResDef(XFE_THERE_WERE_PROBLEMS_MOVING_MAIL, XFE_ERR_OFFSET + 82, "There were problems moving mail")
ResDef(XFE_THERE_WERE_PROBLEMS_MOVING_MAIL_EXIT_STATUS, XFE_ERR_OFFSET + 83, "There were problems moving mail: exit status %d" )
ResDef(XFE_THERE_WERE_PROBLEMS_CLEANING_UP, XFE_ERR_OFFSET + 134, "There were problems cleaning up %s")
ResDef(XFE_USAGE_MSG1, XFE_ERR_OFFSET + 85, "%s\n\
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
ResDef(XFE_USAGE_MSG2, XFE_ERR_OFFSET + 154, "\
       -share                    when -install, cause each window to use the\n\
                                 same colormap instead of each window using\n\
                                 a new one.\n\
       -no-share                 cause each window to use the same colormap.\n")
ResDef(XFE_USAGE_MSG3, XFE_ERR_OFFSET + 86, "\
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
ResDef(XFE_VERSION_COMPLAINT_FORMAT, XFE_ERR_OFFSET + 87, "%s: program is version %s, but resources are version %s.\n\n\
\tThis means that there is an inappropriate `%s' file in\n\
\the system-wide app-defaults directory, or possibly in your\n\
\thome directory.  Check these environment variables and the\n\
\tdirectories to which they point:\n\n\
  $XAPPLRESDIR\n\
  $XFILESEARCHPATH\n\
  $XUSERFILESEARCHPATH\n\n\
\tAlso check for this file in your home directory, or in the\n\
\tdirectory called `app-defaults' somewhere under /usr/lib/." )
ResDef(XFE_INAPPROPRIATE_APPDEFAULT_FILE, XFE_ERR_OFFSET + 88, "%s: couldn't find our resources?\n\n\
\tThis could mean that there is an inappropriate `%s' file\n\
\tinstalled in the app-defaults directory.  Check these environment\n\
\tvariables and the directories to which they point:\n\n\
  $XAPPLRESDIR\n\
  $XFILESEARCHPATH\n\
  $XUSERFILESEARCHPATH\n\n\
\tAlso check for this file in your home directory, or in the\n\
\tdirectory called `app-defaults' somewhere under /usr/lib/." )
ResDef(XFE_INVALID_GEOMETRY_SPEC, XFE_ERR_OFFSET + 89, "%s: invalid geometry specification.\n\n\
 Apparently \"%s*geometry: %s\" or \"%s*geometry: %s\"\n\
 was specified in the resource database.  Specifying \"*geometry\"\n\
 will make %s (and most other X programs) malfunction in obscure\n\
 ways.  You should always use \".geometry\" instead.\n" )
ResDef(XFE_UNRECOGNISED_OPTION, XFE_ERR_OFFSET + 90, "%s: unrecognized option \"%s\"\n")
ResDef(XFE_APP_HAS_DETECTED_LOCK, XFE_ERR_OFFSET + 91, "%s has detected a %s\nfile.\n")
ResDef(XFE_ANOTHER_USER_IS_RUNNING_APP, XFE_ERR_OFFSET + 92, "\nThis may indicate that another user is running\n%s using your %s files.\n" )
ResDef(XFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID, XFE_ERR_OFFSET + 93,"It appears to be running on host %s\nunder process-ID %u.\n" )
ResDef(XFE_YOU_MAY_CONTINUE_TO_USE, XFE_ERR_OFFSET + 94, "\nYou may continue to use %s, but you will\n\
be unable to use the disk cache, global history,\n\
or your personal certificates.\n" )
ResDef(XFE_OTHERWISE_CHOOSE_CANCEL, XFE_ERR_OFFSET + 95, "\nOtherwise, you may choose Cancel, make sure that\n\
you are not running another %s Navigator,\n\
delete the %s file, and\n\
restart %s." )
ResDef(XFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY, XFE_ERR_OFFSET + 96, "%s: %s existed, but was not a directory.\n\
The old file has been renamed to %s\n\
and a directory has been created in its place.\n\n" )
ResDef(XFE_EXISTS_BUT_UNABLE_TO_RENAME, XFE_ERR_OFFSET + 97, "%s: %s exists but is not a directory,\n\
and we were unable to rename it!\n\
Please remove this file: it is in the way.\n\n" )
ResDef(XFE_UNABLE_TO_CREATE_DIRECTORY, XFE_ERR_OFFSET + 98,
"%s: unable to create the directory `%s'.\n%s\n\
Please create this directory.\n\n" )
ResDef(XFE_UNKNOWN_ERROR, XFE_ERR_OFFSET + 99, "unknown error")
ResDef(XFE_ERROR_CREATING, XFE_ERR_OFFSET + 100, "error creating %s")
ResDef(XFE_ERROR_WRITING, XFE_ERR_OFFSET + 101, "error writing %s")
ResDef(XFE_CREATE_CONFIG_FILES, XFE_ERR_OFFSET + 102,
"This version of %s uses different names for its config files\n\
than previous versions did.  Those config files which still use\n\
the same file formats have been copied to their new names, and\n\
those which don't will be recreated as necessary.\n%s\n\n\
Would you like us to delete the old files now?" )
ResDef(XFE_OLD_FILES_AND_CACHE, XFE_ERR_OFFSET + 103,
"\nThe old files still exist, including a disk cache directory\n\
(which might be large.)" )
ResDef(XFE_OLD_FILES, XFE_ERR_OFFSET + 104, "The old files still exist.")
ResDef(XFE_GENERAL, XFE_ERR_OFFSET + 105, "General")
ResDef(XFE_PASSWORDS, XFE_ERR_OFFSET + 106, "Passwords")
ResDef(XFE_PERSONAL_CERTIFICATES, XFE_ERR_OFFSET + 107, "Personal Certificates")
ResDef(XFE_SITE_CERTIFICATES, XFE_ERR_OFFSET + 108, "Site Certificates")
ResDef(XFE_ESTIMATED_TIME_REMAINING_CHECKED, XFE_ERR_OFFSET + 109,
"Checked %s (%d left)\n%d%% completed)\n\n\
Estimated time remaining: %s\n\
(Remaining time depends on the sites selected and\n\
the network traffic.)" )
ResDef(XFE_ESTIMATED_TIME_REMAINING_CHECKING, XFE_ERR_OFFSET + 110,
"Checking ... (%d left)\n%d%% completed)\n\n\
Estimated time remaining: %s\n\
(Remaining time depends on the sites selected and\n\
the network traffic.)" )
ResDef(XFE_RE, XFE_ERR_OFFSET + 111, "Re: ")
ResDef(XFE_DONE_CHECKING_ETC, XFE_ERR_OFFSET + 112,
"Done checking %d bookmarks.\n\
%d documents were reached.\n\
%d documents have changed and are marked as new." )
ResDef(XFE_APP_EXITED_WITH_STATUS, XFE_ERR_OFFSET + 115,"\"%s\" exited with status %d" )
ResDef(XFE_THE_MOTIF_KEYSYMS_NOT_DEFINED, XFE_ERR_OFFSET + 116,
"%s: The Motif keysyms seem not to be defined.\n\n\
 This is usually because the proper XKeysymDB file was not found.\n\
 You can set the $XKEYSYMDB environment variable to the location\n\
 of a file which contains the right keysyms.\n\n\
 Without the right XKeysymDB, many warnings will be generated,\n\
 and most keyboard accelerators will not work.\n\n\
 (An appropriate XKeysymDB file was included with the %s\n\
 distribution.)\n\n" )
ResDef(XFE_SOME_MOTIF_KEYSYMS_NOT_DEFINED, XFE_ERR_OFFSET + 117,
 "%s: Some of the Motif keysyms seem not to be defined.\n\n\
 This is usually because the proper XKeysymDB file was not found.\n\
 You can set the $XKEYSYMDB environment variable to the location\n\
 of a file which contains the right keysyms.\n\n\
 Without the right XKeysymDB, many warnings will be generated,\n\
 and some keyboard accelerators will not work.\n\n\
 (An appropriate XKeysymDB file was included with the %s\n\
 distribution.)\n\n" )
ResDef(XFE_VISUAL_GRAY_DIRECTCOLOR, XFE_ERR_OFFSET + 118,
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
ResDef(XFE_VISUAL_GRAY, XFE_ERR_OFFSET + 119,
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
ResDef(XFE_VISUAL_DIRECTCOLOR, XFE_ERR_OFFSET + 120,
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
ResDef(XFE_VISUAL_NORMAL, XFE_ERR_OFFSET + 121,
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
ResDef(XFE_WILL_BE_DISPLAYED_IN_MONOCHROME, XFE_ERR_OFFSET + 122,
"will be\ndisplayed in monochrome" )
ResDef(XFE_WILL_LOOK_BAD, XFE_ERR_OFFSET + 123, "will look bad")
ResDef(XFE_APPEARANCE, XFE_ERR_OFFSET + 124, "Appearance")
ResDef(XFE_BOOKMARKS, XFE_ERR_OFFSET + 125, "Bookmarks")
ResDef(XFE_COLORS, XFE_ERR_OFFSET + 126, "Colors")
ResDef(XFE_FONTS, XFE_ERR_OFFSET + 127, "Fonts" )
ResDef(XFE_APPLICATIONS, XFE_ERR_OFFSET + 128, "Applications")
ResDef(XFE_HELPERS, XFE_ERR_OFFSET + 155, "Helpers")
ResDef(XFE_IMAGES, XFE_ERR_OFFSET + 129, "Images")
ResDef(XFE_LANGUAGES, XFE_ERR_OFFSET + 130, "Languages")
ResDef(XFE_CACHE, XFE_ERR_OFFSET + 131, "Cache")
ResDef(XFE_CONNECTIONS, XFE_ERR_OFFSET + 132, "Connections")
ResDef(XFE_PROXIES, XFE_ERR_OFFSET + 133, "Proxies")
ResDef(XFE_COMPOSE_DLG, XFE_ERR_OFFSET + 135, "Compose")
ResDef(XFE_SERVERS, XFE_ERR_OFFSET + 136, "Servers")
ResDef(XFE_IDENTITY, XFE_ERR_OFFSET + 137, "Identity")
ResDef(XFE_ORGANIZATION, XFE_ERR_OFFSET + 138, "Organization")
ResDef(XFE_MAIL_FRAME, XFE_ERR_OFFSET + 139, "Mail Frame" )
ResDef(XFE_MAIL_DOCUMENT, XFE_ERR_OFFSET + 140, "Mail Document" )
ResDef(XFE_NETSCAPE_MAIL, XFE_ERR_OFFSET + 141, "Mozilla Mail & Discussions" )
ResDef(XFE_NETSCAPE_NEWS, XFE_ERR_OFFSET + 142, "Mozilla Discussions" )
ResDef(XFE_ADDRESS_BOOK, XFE_ERR_OFFSET + 143, "Address Book" )
ResDef(XFE_X_RESOURCES_NOT_INSTALLED_CORRECTLY, XFE_ERR_OFFSET + 144, 
"X resources not installed correctly!" )
ResDef(XFE_GG_EMPTY_LL, XFE_ERR_OFFSET + 145, "<< Empty >>")
ResDef(XFE_ERROR_SAVING_PASSWORD, XFE_ERR_OFFSET + 146, "error saving password")
ResDef(XFE_UNIMPLEMENTED, XFE_ERR_OFFSET + 147, "Unimplemented.")
ResDef(XFE_TILDE_USER_SYNTAX_DISALLOWED, XFE_ERR_OFFSET + 148, 
"%s: ~user/ syntax is not allowed in preferences file, only ~/\n" )
ResDef(XFE_UNRECOGNISED_VISUAL, XFE_ERR_OFFSET + 149, 
"%s: unrecognized visual \"%s\".\n" )
ResDef(XFE_NO_VISUAL_WITH_ID, XFE_ERR_OFFSET + 150,
"%s: no visual with id 0x%x.\n" )
ResDef(XFE_NO_VISUAL_OF_CLASS, XFE_ERR_OFFSET + 151,
"%s: no visual of class %s.\n" )
ResDef(XFE_STDERR_DIAGNOSTICS_HAVE_BEEN_TRUNCATED, XFE_ERR_OFFSET + 152, 
"\n\n<< stderr diagnostics have been truncated >>" )
ResDef(XFE_ERROR_CREATING_PIPE, XFE_ERR_OFFSET + 153, "error creating pipe:")
ResDef(XFE_OUTBOX_CONTAINS_MSG, XFE_ERR_OFFSET + 156,
"Outbox folder contains an unsent\nmessage. Send it now?\n")
ResDef(XFE_OUTBOX_CONTAINS_MSGS, XFE_ERR_OFFSET + 157,
"Outbox folder contains %d unsent\nmessages. Send them now?\n")
ResDef(XFE_NO_KEEP_ON_SERVER_WITH_MOVEMAIL, XFE_ERR_OFFSET + 158,
"The ``Leave on Server'' option only works when\n\
using a POP3 server, not when using a local\n\
mail spool directory.  To retrieve your mail,\n\
first turn off that option in the Servers pane\n\
of the Mail and News Preferences.")
ResDef(XFE_BACK, XFE_ERR_OFFSET + 159, "Back")
ResDef(XFE_BACK_IN_FRAME, XFE_ERR_OFFSET + 160, "Back in Frame")
ResDef(XFE_FORWARD, XFE_ERR_OFFSET + 161, "Forward")
ResDef(XFE_FORWARD_IN_FRAME, XFE_ERR_OFFSET + 162, "Forward in Frame")
ResDef(XFE_MAIL_SPOOL_UNKNOWN, XFE_ERR_OFFSET + 163,
"Please set the $MAIL environment variable to the\n\
location of your mail-spool file.")
ResDef(XFE_MOVEMAIL_NO_MESSAGES, XFE_ERR_OFFSET + 164,
"No new messages.")
ResDef(XFE_USER_DEFINED, XFE_ERR_OFFSET + 165,
"User-Defined")
ResDef(XFE_OTHER_LANGUAGE, XFE_ERR_OFFSET + 166,
"Other")
ResDef(XFE_COULDNT_FORK_FOR_DELIVERY, XFE_ERR_OFFSET + 167,
"couldn't fork() for external message delivery")
ResDef(XFE_CANNOT_READ_FILE, XFE_ERR_OFFSET + 168,
"Cannot read file %s.")
ResDef(XFE_CANNOT_SEE_FILE, XFE_ERR_OFFSET + 169,
"%s does not exist.")
ResDef(XFE_IS_A_DIRECTORY, XFE_ERR_OFFSET + 170,
"%s is a directory.")
ResDef(XFE_EKIT_LOCK_FILE_NOT_FOUND, XFE_ERR_OFFSET + 171,
"Lock file not found.")
ResDef(XFE_EKIT_CANT_OPEN, XFE_ERR_OFFSET + 172,
"Can't open Netscape.lock file.")
ResDef(XFE_EKIT_MODIFIED, XFE_ERR_OFFSET + 173,
"Netscape.lock file has been modified.")
ResDef(XFE_EKIT_FILESIZE, XFE_ERR_OFFSET + 174,
"Could not determine lock file size.")
ResDef(XFE_EKIT_CANT_READ, XFE_ERR_OFFSET + 175,
"Could not read Netscape.lock data.")
ResDef(XFE_ANIM_CANT_OPEN, XFE_ERR_OFFSET + 176,
"Couldn't open animation file.")
ResDef(XFE_ANIM_MODIFIED, XFE_ERR_OFFSET + 177,
"Animation file modified.\nUsing default animation.")
ResDef(XFE_ANIM_READING_SIZES, XFE_ERR_OFFSET + 178,
"Couldn't read animation file size.\nUsing default animation.")
ResDef(XFE_ANIM_READING_NUM_COLORS, XFE_ERR_OFFSET + 179,
"Couldn't read number of animation colors.\nUsing default animation.")
ResDef(XFE_ANIM_READING_COLORS, XFE_ERR_OFFSET + 180,
"Couldn't read animation colors.\nUsing default animation.")
ResDef(XFE_ANIM_READING_FRAMES, XFE_ERR_OFFSET + 181,
"Couldn't read animation frames.\nUsing default animation.")
ResDef(XFE_ANIM_NOT_AT_EOF, XFE_ERR_OFFSET + 182,
"Ignoring extra bytes at end of animation file.")
ResDef(XFE_LOOK_FOR_DOCUMENTS_THAT_HAVE_CHANGED_ON, XFE_ERR_OFFSET + 183,
"Look for documents that have changed for:")
/* I'm not quite sure why these are resources?? ..djw */
ResDef(XFE_CHARACTER, XFE_ERR_OFFSET + 184, "Character") /* 26FEB96RCJ */
ResDef(XFE_LINK, XFE_ERR_OFFSET + 185, "Link")           /* 26FEB96RCJ */
ResDef(XFE_PARAGRAPH, XFE_ERR_OFFSET + 186, "Paragraph") /* 26FEB96RCJ */
ResDef(XFE_IMAGE,     XFE_ERR_OFFSET + 187, "Image")     /*  5MAR96RCJ */
ResDef(XFE_REFRESH_FRAME, XFE_ERR_OFFSET + 188, "Refresh Frame")
ResDef(XFE_REFRESH, XFE_ERR_OFFSET + 189, "Refresh")

ResDef(XFE_MAIL_TITLE_FMT, XFE_ERR_OFFSET + 190, "Mozilla Mail & Discussions: %.900s")
ResDef(XFE_NEWS_TITLE_FMT, XFE_ERR_OFFSET + 191, "Mozilla Discussions: %.900s")
ResDef(XFE_TITLE_FMT, XFE_ERR_OFFSET + 192, "Mozilla: %.900s")

ResDef(XFE_PROTOCOLS, XFE_ERR_OFFSET + 193, "Protocols")
ResDef(XFE_LANG, XFE_ERR_OFFSET + 194, "Languages")
ResDef(XFE_CHANGE_PASSWORD, XFE_ERR_OFFSET + 195, "Change Password")
ResDef(XFE_SET_PASSWORD, XFE_ERR_OFFSET + 196, "Set Password...")
ResDef(XFE_NO_PLUGINS, XFE_ERR_OFFSET + 197, "No Plugins")

/*
 * Messages for the dialog that warns before doing movemail.
 * DEM 4/30/96
 */
ResDef(XFE_CONTINUE_MOVEMAIL, XFE_ERR_OFFSET + 198, "Continue Movemail")
ResDef(XFE_CANCEL_MOVEMAIL, XFE_ERR_OFFSET + 199, "Cancel Movemail")
ResDef(XFE_MOVEMAIL_EXPLANATION, XFE_ERR_OFFSET + 200,
"Mozilla will move your mail from %s\n\
to %s/Inbox.\n\
\n\
Moving the mail will interfere with other mailers\n\
that expect already-read mail to remain in\n\
%s." )
ResDef(XFE_SHOW_NEXT_TIME, XFE_ERR_OFFSET + 201, "Show this Alert Next Time" )
ResDef(XFE_EDITOR_TITLE_FMT, XFE_ERR_OFFSET + 202, "Mozilla Composer: %.900s")

ResDef(XFE_HELPERS_NETSCAPE, XFE_ERR_OFFSET + 203, "Mozilla")
ResDef(XFE_HELPERS_UNKNOWN, XFE_ERR_OFFSET + 204, "Unknown:Prompt User")
ResDef(XFE_HELPERS_SAVE, XFE_ERR_OFFSET + 205, "Save To Disk")
ResDef(XFE_HELPERS_PLUGIN, XFE_ERR_OFFSET + 206, "Plug In : %s")
ResDef(XFE_HELPERS_EMPTY_MIMETYPE, XFE_ERR_OFFSET + 207,
"Mime type cannot be emtpy.")
ResDef(XFE_HELPERS_LIST_HEADER, XFE_ERR_OFFSET + 208, "Description|Handled By")
ResDef(XFE_MOVEMAIL_CANT_DELETE_LOCK, XFE_ERR_OFFSET + 209,
"Can't get new mail; a lock file %s exists.")
ResDef(XFE_PLUGIN_NO_PLUGIN, XFE_ERR_OFFSET + 210,
"No plugin %s. Reverting to save-to-disk for type %s.\n")
ResDef(XFE_PLUGIN_CANT_LOAD, XFE_ERR_OFFSET + 211,
"ERROR: %s\nCant load plugin %s. Ignored.\n")
ResDef(XFE_HELPERS_PLUGIN_DESC_CHANGE, XFE_ERR_OFFSET + 212,
"Plugin specifies different Description and/or Suffixes for mimetype %s.\n\
\n\
        Description = \"%s\"\n\
        Suffixes = \"%s\"\n\
\n\
Use plugin specified Description and Suffixes ?")
ResDef(XFE_CANT_SAVE_PREFS, XFE_ERR_OFFSET + 213, "error Saving options.")
ResDef(XFE_VALUES_OUT_OF_RANGE, XFE_ERR_OFFSET + 214,
	   "Some values are out of range:")
ResDef(XFE_VALUE_OUT_OF_RANGE, XFE_ERR_OFFSET + 215,
	   "The following value is out of range:")
ResDef(XFE_EDITOR_TABLE_ROW_RANGE, XFE_ERR_OFFSET + 216,
	   "You can have between 1 and 100 rows.")
ResDef(XFE_EDITOR_TABLE_COLUMN_RANGE, XFE_ERR_OFFSET + 217,
	   "You can have between 1 and 100 columns.")
ResDef(XFE_EDITOR_TABLE_BORDER_RANGE, XFE_ERR_OFFSET + 218,
	   "For border width, you can have 0 to 10000 pixels.")
ResDef(XFE_EDITOR_TABLE_SPACING_RANGE, XFE_ERR_OFFSET + 219,
	   "For cell spacing, you can have 0 to 10000 pixels.")
ResDef(XFE_EDITOR_TABLE_PADDING_RANGE, XFE_ERR_OFFSET + 220,
	   "For cell padding, you can have 0 to 10000 pixels.")
ResDef(XFE_EDITOR_TABLE_WIDTH_RANGE, XFE_ERR_OFFSET + 221,
	   "For width, you can have between 1 and 10000 pixels,\n"
	   "or between 1 and 100%.")
ResDef(XFE_EDITOR_TABLE_HEIGHT_RANGE, XFE_ERR_OFFSET + 222,
	   "For height, you can have between 1 and 10000 pixels,\n"
	   "or between 1 and 100%.")
ResDef(XFE_EDITOR_TABLE_IMAGE_WIDTH_RANGE, XFE_ERR_OFFSET + 223,
	   "For width, you can have between 1 and 10000 pixels.")
ResDef(XFE_EDITOR_TABLE_IMAGE_HEIGHT_RANGE, XFE_ERR_OFFSET + 224,
	   "For height, you can have between 1 and 10000 pixels.")
ResDef(XFE_EDITOR_TABLE_IMAGE_SPACE_RANGE, XFE_ERR_OFFSET + 225,
	   "For space, you can have between 1 and 10000 pixels.")
ResDef(XFE_ENTER_NEW_VALUE, XFE_ERR_OFFSET + 226,
	   "Please enter a new value and try again.")
ResDef(XFE_ENTER_NEW_VALUES, XFE_ERR_OFFSET + 227,
	   "Please enter new values and try again.")
ResDef(XFE_EDITOR_LINK_TEXT_LABEL_NEW, XFE_ERR_OFFSET + 228,
	   "Enter the text of the link:")
ResDef(XFE_EDITOR_LINK_TEXT_LABEL_IMAGE, XFE_ERR_OFFSET + 229,
	   "Linked image:")
ResDef(XFE_EDITOR_LINK_TEXT_LABEL_TEXT, XFE_ERR_OFFSET + 230,
	   "Linked text:")
ResDef(XFE_EDITOR_LINK_TARGET_LABEL_NO_TARGETS, XFE_ERR_OFFSET + 231,
	   "No targets in the selected document")
ResDef(XFE_EDITOR_LINK_TARGET_LABEL_SPECIFIED, XFE_ERR_OFFSET + 232,
	   "Link to a named target in the specified document (optional).")
ResDef(XFE_EDITOR_LINK_TARGET_LABEL_CURRENT, XFE_ERR_OFFSET + 233,
	   "Link to a named target in the current document (optional).")
ResDef(XFE_EDITOR_WARNING_REMOVE_LINK, XFE_ERR_OFFSET + 234,
	   "Do you want to remove the link?")
ResDef(XFE_UNKNOWN, XFE_ERR_OFFSET + 235,
	   "<unknown>")
ResDef(XFE_EDITOR_TAG_UNOPENED, XFE_ERR_OFFSET + 236,
	   "Unopened Tag: '<' was expected")
ResDef(XFE_EDITOR_TAG_UNCLOSED, XFE_ERR_OFFSET + 237,
	   "Unopened Tag:  '>' was expected")
ResDef(XFE_EDITOR_TAG_UNTERMINATED_STRING, XFE_ERR_OFFSET + 238,
	   "Unterminated String in tag: closing quote expected")
ResDef(XFE_EDITOR_TAG_PREMATURE_CLOSE, XFE_ERR_OFFSET + 239,
	   "Premature close of tag")
ResDef(XFE_EDITOR_TAG_TAGNAME_EXPECTED, XFE_ERR_OFFSET + 240,
	   "Tagname was expected")
ResDef(XFE_EDITOR_TAG_UNKNOWN, XFE_ERR_OFFSET + 241,
	   "Unknown tag error")
ResDef(XFE_EDITOR_TAG_OK, XFE_ERR_OFFSET + 242,
	   "Tag seems ok")
ResDef(XFE_EDITOR_ALERT_FRAME_DOCUMENT, XFE_ERR_OFFSET + 243,
	   "This document contains frames. Currently the editor\n"
	   "cannot edit documents which contain frames.")
ResDef(XFE_EDITOR_ALERT_ABOUT_DOCUMENT, XFE_ERR_OFFSET + 244,
	   "This document is an about document. The editor\n"
	   "cannot edit about documents.")
ResDef(XFE_ALERT_SAVE_CHANGES, XFE_ERR_OFFSET + 245,
	   "You must save this document locally before\n"
	   "continuing with the requested action.")
ResDef(XFE_WARNING_SAVE_CHANGES, XFE_ERR_OFFSET + 246,
	   "Do you want to save changes to:\n%.900s?")
ResDef(XFE_ERROR_GENERIC_ERROR_CODE, XFE_ERR_OFFSET + 247,
	   "The error code = (%d).")
ResDef(XFE_EDITOR_COPY_DOCUMENT_BUSY, XFE_ERR_OFFSET + 248,
	   "Cannot copy or cut at this time, try again later.")
ResDef(XFE_EDITOR_COPY_SELECTION_EMPTY, XFE_ERR_OFFSET + 249,
	   "Nothing is selected.")
ResDef(XFE_EDITOR_COPY_SELECTION_CROSSES_TABLE_DATA_CELL, XFE_ERR_OFFSET + 250,
	   "The selection includes a table cell boundary.\n"
	   "Deleting and copying are not allowed.")
ResDef(XFE_COMMAND_EMPTY, XFE_ERR_OFFSET + 251,
	   "Empty command specified!")
ResDef(XFE_EDITOR_HTML_EDITOR_COMMAND_EMPTY, XFE_ERR_OFFSET + 252,
	   "No html editor command has been specified in Editor Preferences.\n"
	   "Specify the file argument with %f. Mozilla will replace %f with\n"
	   "the correct file name. Example:\n"
	   "             xterm -e vi %f\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(XFE_ACTION_SYNTAX_ERROR, XFE_ERR_OFFSET + 253,
	   "Syntax error in action handler: %s")
ResDef(XFE_ACTION_WRONG_CONTEXT, XFE_ERR_OFFSET + 254,
	   "Invalid window type for action handler: %s")
ResDef(XFE_EKIT_ABOUT_MESSAGE, XFE_ERR_OFFSET + 255,
	   "Modified by the Mozilla Navigator Administration Kit.\n"
           "Version: %s\n"
           "User agent: C")

ResDef(XFE_PRIVATE_MIMETYPE_RELOAD, XFE_ERR_OFFSET + 256,
 "Private Mimetype File (%s) has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(XFE_PRIVATE_MAILCAP_RELOAD, XFE_ERR_OFFSET + 257,
 "Private Mailcap File (%s) has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(XFE_PRIVATE_RELOADED_MIMETYPE, XFE_ERR_OFFSET + 258,
 "Private Mimetype File (%s) has changed on disk and is being reloaded.")
ResDef(XFE_PRIVATE_RELOADED_MAILCAP, XFE_ERR_OFFSET + 259,
 "Private Mailcap File (%s) has changed on disk and is being reloaded.")
ResDef(XFE_EDITOR_IMAGE_EDITOR_COMMAND_EMPTY, XFE_ERR_OFFSET + 260,
	   "No image editor command has been specified in Editor Preferences.\n"
	   "Specify the file argument with %f. Mozilla will replace %f with\n"
	   "the correct file name. Example:\n"
	   "             xgifedit %f\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(XFE_ERROR_COPYRIGHT_HINT, XFE_ERR_OFFSET + 261,
 "You are about to download a remote\n"
 "document or image.\n"
 "You should get permission to use any\n"
 "copyrighted images or documents.")
ResDef(XFE_ERROR_READ_ONLY, XFE_ERR_OFFSET + 262,
	   "The file is marked read-only.")
ResDef(XFE_ERROR_BLOCKED, XFE_ERR_OFFSET + 263,
	   "The file is blocked at this time. Try again later.")
ResDef(XFE_ERROR_BAD_URL, XFE_ERR_OFFSET + 264,
	   "The file URL is badly formed.")
ResDef(XFE_ERROR_FILE_OPEN, XFE_ERR_OFFSET + 265,
	   "Error opening file for writing.")
ResDef(XFE_ERROR_FILE_WRITE, XFE_ERR_OFFSET + 266,
	   "Error writing to the file.")
ResDef(XFE_ERROR_CREATE_BAKNAME, XFE_ERR_OFFSET + 267,
	   "Error creating the temporary backup file.")
ResDef(XFE_ERROR_DELETE_BAKFILE, XFE_ERR_OFFSET + 268,
	   "Error deleting the temporary backup file.")
ResDef(XFE_WARNING_SAVE_CONTINUE, XFE_ERR_OFFSET + 269,
	   "Continue saving document?")
ResDef(XFE_ERROR_WRITING_FILE, XFE_ERR_OFFSET + 270,
	   "There was an error while saving the file:\n%.900s")
ResDef(XFE_EDITOR_DOCUMENT_TEMPLATE_EMPTY, XFE_ERR_OFFSET + 271,
	   "The new document template location is not set.\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(XFE_EDITOR_AUTOSAVE_PERIOD_RANGE, XFE_ERR_OFFSET + 272,
	   "Please enter an autosave period between 0 and 600 minutes.")
ResDef(XFE_EDITOR_BROWSE_LOCATION_EMPTY, XFE_ERR_OFFSET + 273,
	   "The default browse location is not set.\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(XFE_EDITOR_PUBLISH_LOCATION_INVALID, XFE_ERR_OFFSET + 274,
	   "Publish destination must begin with \"ftp://\", \"http://\", or \"https://\".\n"
	   "Please enter new values and try again.")
ResDef(XFE_EDITOR_IMAGE_IS_REMOTE, XFE_ERR_OFFSET + 275,
	   "Image is at a remote location.\n"
	   "Please save image locally before editing.")
ResDef(XFE_COLORMAP_WARNING_TO_IGNORE, XFE_ERR_OFFSET + 276,
	   "cannot allocate colormap")
ResDef(XFE_UPLOADING_FILE, XFE_ERR_OFFSET + 277,
	   "Uploading file to remote server:\n%.900s")
ResDef(XFE_SAVING_FILE, XFE_ERR_OFFSET + 278,
	   "Saving file to local disk:\n%.900s")
ResDef(XFE_LOADING_IMAGE_FILE, XFE_ERR_OFFSET + 279,
	   "Loading image file:\n%.900s")
ResDef(XFE_FILE_N_OF_N, XFE_ERR_OFFSET + 280,
	   "File %d of %d")
ResDef(XFE_ERROR_SRC_NOT_FOUND, XFE_ERR_OFFSET + 281,
	   "Source not found.")
ResDef(XFE_WARNING_AUTO_SAVE_NEW_MSG, XFE_ERR_OFFSET + 282,
	   "Press Cancel to turn off AutoSave until you\n"
	   "save this document later.")
ResDef(XFE_ALL_NEWSGROUP_TAB, XFE_ERR_OFFSET + 283,
	   "All Groups")
ResDef(XFE_SEARCH_FOR_NEWSGROUP_TAB, XFE_ERR_OFFSET + 284,
           "Search for a Group")
ResDef(XFE_NEW_NEWSGROUP_TAB, XFE_ERR_OFFSET + 285,
           "New Groups")
ResDef(XFE_NEW_NEWSGROUP_TAB_INFO_MSG, XFE_ERR_OFFSET + 286,
           "This list shows the accumulated list of new discussion groups since\n"
           "the last time you pressed the Clear New button.")
ResDef(XFE_SILLY_NAME_FOR_SEEMINGLY_UNAMEABLE_THING, XFE_ERR_OFFSET + 287,
           "Message Center for %s")
ResDef(XFE_FOLDER_ON_LOCAL_MACHINE, XFE_ERR_OFFSET + 288,
           "on local machine.")
ResDef(XFE_FOLDER_ON_SERVER, XFE_ERR_OFFSET + 289,
           "on server.")
ResDef(XFE_MESSAGE, XFE_ERR_OFFSET + 290,
       "Message:")
ResDef(XFE_MESSAGE_SUBTITLE, XFE_ERR_OFFSET + 291,
       "'%s' from %s in %s Folder")
ResDef(XFE_WINDOW_TITLE_NEWSGROUP, XFE_ERR_OFFSET + 292,
       "Mozilla Group - [ %s ]")
ResDef(XFE_WINDOW_TITLE_FOLDER, XFE_ERR_OFFSET + 293,
       "Mozilla Folder - [ %s ]")
ResDef(XFE_MAIL_PRIORITY_LOWEST, XFE_ERR_OFFSET + 294,
       "Lowest")
ResDef(XFE_MAIL_PRIORITY_LOW, XFE_ERR_OFFSET + 295,
       "Low")
ResDef(XFE_MAIL_PRIORITY_NORMAL, XFE_ERR_OFFSET + 296,
       "Normal")
ResDef(XFE_MAIL_PRIORITY_HIGH, XFE_ERR_OFFSET + 297,
       "High")
ResDef(XFE_MAIL_PRIORITY_HIGHEST, XFE_ERR_OFFSET + 298,
       "Highest")
ResDef(XFE_SIZE_IN_BYTES, XFE_ERR_OFFSET + 299,
       "Size")
ResDef(XFE_SIZE_IN_LINES, XFE_ERR_OFFSET + 300,
       "Lines")

ResDef(XFE_AB_NAME_GENERAL_TAB, XFE_ERR_OFFSET + 301,
       "Name")
ResDef(XFE_AB_NAME_CONTACT_TAB, XFE_ERR_OFFSET + 302,
       "Contact")
ResDef(XFE_AB_NAME_SECURITY_TAB, XFE_ERR_OFFSET + 303,
       "Security")
ResDef(XFE_AB_NAME_COOLTALK_TAB, XFE_ERR_OFFSET + 304,
       "Mozilla Conference")
ResDef(XFE_AB_FIRSTNAME, XFE_ERR_OFFSET + 305,
       "First Name:")
ResDef(XFE_AB_LASTNAME, XFE_ERR_OFFSET + 306,
       "Last Name:")
ResDef(XFE_AB_EMAIL, XFE_ERR_OFFSET + 307,
       "Email Address:")
ResDef(XFE_AB_NICKNAME, XFE_ERR_OFFSET + 308,
       "Nickname:")
ResDef(XFE_AB_NOTES, XFE_ERR_OFFSET + 309,
       "Notes:")
ResDef(XFE_AB_PREFHTML, XFE_ERR_OFFSET + 310,
       "Prefers to receive rich text (HTML) mail")
ResDef(XFE_AB_COMPANY, XFE_ERR_OFFSET + 311,
       "Organization:")
ResDef(XFE_AB_TITLE, XFE_ERR_OFFSET + 312,
       "Title:")
ResDef(XFE_AB_ADDRESS, XFE_ERR_OFFSET + 313,
       "Address:")
ResDef(XFE_AB_CITY, XFE_ERR_OFFSET + 314,
       "City:")
ResDef(XFE_AB_STATE, XFE_ERR_OFFSET + 315,
       "State:")
ResDef(XFE_AB_ZIP, XFE_ERR_OFFSET + 316,
       "Zip:")
ResDef(XFE_AB_COUNTRY, XFE_ERR_OFFSET + 317,
       "Country:")

ResDef(XFE_AB_WORKPHONE, XFE_ERR_OFFSET + 318,
       "Work:")
ResDef(XFE_AB_FAX, XFE_ERR_OFFSET + 319,
       "Fax:")
ResDef(XFE_AB_HOMEPHONE, XFE_ERR_OFFSET + 320,
       "Home:")

ResDef(XFE_AB_SECUR_NO, XFE_ERR_OFFSET + 321,
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
ResDef(XFE_AB_SECUR_YES, XFE_ERR_OFFSET + 322,
       "You have this person's Security Certificate.\n"
	   "\n"
	   "This means that when you send this person email, it can be encrypted.\n"
	   "Encrypting a message is like sending it in an envelope, rather than a\n"
	   "postcard. It makes it difficult for other peope to view your message.\n"
	   "\n"
	   "This person's Security Certificate will expire on %s. When it\n"
	   "expires, you will no longer be able to send encrypted mail to this\n"
	   "person until they send you a new one.")
ResDef(XFE_AB_SECUR_EXPIRED, XFE_ERR_OFFSET + 323,
       "This person's Security Certificate has Expired.\n"
	   "\n"
	   "You cannot send this person encrypted mail until you obtain a new\n"
	   "Security Certificate for them. To do this, have this person send you a\n"
	   "signed mail message. This will automatically include the new Security\n"
	   "Certificate.")
ResDef(XFE_AB_SECUR_REVOKED, XFE_ERR_OFFSET + 324,
       "This person's Security Certificate has been revoked. This means that\n"
	   "the Certificate may have been lost or stolen.\n"
	   "\n"
	   "You cannot send this person encrypted mail until you obtain a new\n"
	   "Security Certificate. To do this, have this person send you a signed\n"
	   "mail message. This will automatically include the new Security\n"
	   "Certificate.")
ResDef(XFE_AB_SECUR_YOU_NO, XFE_ERR_OFFSET + 325,
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
ResDef(XFE_AB_SECUR_YOU_YES, XFE_ERR_OFFSET + 326,
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
ResDef(XFE_AB_SECUR_YOU_EXPIRED, XFE_ERR_OFFSET + 327,
       "Your Security Certificate has Expired.\n"
	   "\n"
	   "This means that you can no longer digitally sign messages with this\n"
	   "certificate. You can continue to receive encrypted mail, however.\n"
	   "\n"
	   "This means that you must obtain another Certificate. Click on Show\n"
	   "Certificate to do so.")
ResDef(XFE_AB_SECUR_YOU_REVOKED, XFE_ERR_OFFSET + 328,
       "Your Security Certificate has been revoked.\n"
	   "This means that you can no longer digitally sign messages with this\n"
	   "certificate. You can continue to receive encrypted mail, however.\n"
	   "\n"
	   "This means that you must obtain another Certificate.")

ResDef(XFE_AB_SECUR_SHOW, XFE_ERR_OFFSET + 329,
       "Show Certificate")

ResDef(XFE_AB_SECUR_GET, XFE_ERR_OFFSET + 330,
       "Get Certificate")


ResDef(XFE_AB_MLIST_TITLE, XFE_ERR_OFFSET + 331,
       "Mailing List Info")
ResDef(XFE_AB_MLIST_LISTNAME, XFE_ERR_OFFSET + 332,
       "List Name:")
ResDef(XFE_AB_MLIST_NICKNAME, XFE_ERR_OFFSET + 333,
       "List Nickname:")
ResDef(XFE_AB_MLIST_DESCRIPTION, XFE_ERR_OFFSET + 334,
       "Description:")
ResDef(XFE_AB_MLIST_PROMPT, XFE_ERR_OFFSET + 335,
       "To add entries to this mailing list, type names from the address book")

ResDef(XFE_AB_HEADER_NAME, XFE_ERR_OFFSET + 336,
       "Name")
ResDef(XFE_AB_HEADER_CERTIFICATE, XFE_ERR_OFFSET + 337,
       "")
ResDef(XFE_AB_HEADER_EMAIL, XFE_ERR_OFFSET + 338,
       "Email Address")
ResDef(XFE_AB_HEADER_NICKNAME, XFE_ERR_OFFSET + 339,
       "Nickname")
ResDef(XFE_AB_HEADER_COMPANY, XFE_ERR_OFFSET + 340,
       "Organization")
ResDef(XFE_AB_HEADER_LOCALITY, XFE_ERR_OFFSET + 341,
       "City")
ResDef(XFE_AB_HEADER_STATE, XFE_ERR_OFFSET + 342,
       "State")

ResDef(XFE_MN_UNREAD_AND_TOTAL, XFE_ERR_OFFSET + 343,
       "%d Unread, %d Total")

ResDef(XFE_AB_SEARCH, XFE_ERR_OFFSET + 344,
       "Search")
ResDef(XFE_AB_STOP, XFE_ERR_OFFSET + 345,
       "Stop")

ResDef(XFE_AB_REMOVE, XFE_ERR_OFFSET + 346,
       "Remove")

ResDef(XFE_AB_ADDRESSEE_PROMPT, XFE_ERR_OFFSET + 347,
       "This message will be sent to:")

ResDef(XFE_OFFLINE_NEWS_ALL_MSGS, XFE_ERR_OFFSET + 348,
       "all")
ResDef(XFE_OFFLINE_NEWS_ONE_MONTH_AGO, XFE_ERR_OFFSET + 349,
       "1 month ago")

ResDef(XFE_MN_DELIVERY_IN_PROGRESS, XFE_ERR_OFFSET + 350,
       "Attachment operation cannot be completed now.\nMessage delivery or attachment load is in progress.")
ResDef(XFE_MN_ITEM_ALREADY_ATTACHED, XFE_ERR_OFFSET + 351,
       "This item is already attached:\n%s")
ResDef(XFE_MN_TOO_MANY_ATTACHMENTS, XFE_ERR_OFFSET + 352,
       "Attachment panel is full - no more attachments can be added.")
ResDef(XFE_DOWNLOADING_NEW_MESSAGES, XFE_ERR_OFFSET + 353,
	   "Getting new messages...")

ResDef(XFE_AB_FRAME_TITLE, XFE_ERR_OFFSET + 354,
           "Communicator Address Book for %s")

ResDef(XFE_AB_SHOW_CERTI, XFE_ERR_OFFSET + 355,
           "Show Certificate")

ResDef(XFE_LANG_COL_ORDER, XFE_ERR_OFFSET + 356,
       "Order")
ResDef(XFE_LANG_COL_LANG, XFE_ERR_OFFSET + 357,
       "Language")

ResDef(XFE_MFILTER_INFO, XFE_ERR_OFFSET + 358,
	   "Filters will be applied to incoming mail in the\n"
	   "following order:")


ResDef(XFE_AB_COOLTALK_INFO, XFE_ERR_OFFSET + 359,
       "To call another person using Mozilla Conference, you must first\n"
       "choose the server you would like to use to look up that person's\n"
	   "address.")
ResDef(XFE_AB_COOLTALK_DEF_SERVER, XFE_ERR_OFFSET + 360,
       "Mozilla Conference DLS Server")
ResDef(XFE_AB_COOLTALK_SERVER_IK_HOST, XFE_ERR_OFFSET + 361,
       "Specific DLS Server")
ResDef(XFE_AB_COOLTALK_DIRECTIP, XFE_ERR_OFFSET + 362,
       "Hostname or IP Address")

ResDef(XFE_AB_COOLTALK_ADDR_LABEL, XFE_ERR_OFFSET + 363,
       "Address:")
ResDef(XFE_AB_COOLTALK_ADDR_EXAMPLE, XFE_ERR_OFFSET + 364,
       "(For example, %s)")

ResDef(XFE_AB_NAME_CARD_FOR, XFE_ERR_OFFSET + 365,
       "Card for <%s>")
ResDef(XFE_AB_NAME_NEW_CARD, XFE_ERR_OFFSET + 366,
       "New Card")

ResDef(XFE_MARKBYDATE_CAPTION, XFE_ERR_OFFSET + 367,
	   "Mark Messages Read")
ResDef(XFE_MARKBYDATE, XFE_ERR_OFFSET + 368,
	   "Mark messages read up to: (MM/DD/YY)")
ResDef(XFE_DATE_MUST_BE_MM_DD_YY, XFE_ERR_OFFSET + 369,
	   "The date must be valid,\nand must be in the form MM/DD/YY.")
ResDef(XFE_THERE_ARE_N_ARTICLES, XFE_ERR_OFFSET + 370,
	   "There are %d new message headers to download for\nthis discussion group.")
ResDef(XFE_GET_NEXT_N_MSGS, XFE_ERR_OFFSET + 371,
	   "Next %d")

ResDef(XFE_OFFLINE_NEWS_UNREAD_MSGS, XFE_ERR_OFFSET + 372,
       "unread")
ResDef(XFE_OFFLINE_NEWS_YESTERDAY, XFE_ERR_OFFSET + 373,
       "yesterday")
ResDef(XFE_OFFLINE_NEWS_ONE_WK_AGO, XFE_ERR_OFFSET + 374,
       "1 week ago")
ResDef(XFE_OFFLINE_NEWS_TWO_WKS_AGO, XFE_ERR_OFFSET + 375,
       "2 weeks ago")
ResDef(XFE_OFFLINE_NEWS_SIX_MONTHS_AGO, XFE_ERR_OFFSET + 376,
       "6 months ago")
ResDef(XFE_OFFLINE_NEWS_ONE_YEAR_AGO, XFE_ERR_OFFSET + 377,
       "1 year ago")

ResDef(XFE_ADDR_ENTRY_ALREADY_EXISTS, XFE_ERR_OFFSET + 378,
       "An Address Book entry with this name and email address already exists.")

ResDef (XFE_ADDR_ADD_PERSON_TO_ABOOK, XFE_ERR_OFFSET + 379, 
		"Mailing lists can only contain entries from the Personal Address Book.\n"
		"Would you like to add %s to the address book?")

ResDef (XFE_MAKE_SURE_SERVER_AND_PORT_ARE_VALID, XFE_ERR_OFFSET + 380,
		"Make sure both the server name and port are filled in and are valid.")

ResDef(XFE_UNKNOWN_EMAIL_ADDR, XFE_ERR_OFFSET + 381,
       "unknown")

ResDef(XFE_AB_PICKER_TO, XFE_ERR_OFFSET + 382,
       "To:")

ResDef(XFE_AB_PICKER_CC, XFE_ERR_OFFSET + 383,
       "CC:")

ResDef(XFE_AB_PICKER_BCC, XFE_ERR_OFFSET + 384,
       "BCC:")

ResDef(XFE_AB_SEARCH_PROMPT, XFE_ERR_OFFSET + 385,
       "Type Name")

ResDef(XFE_MN_NEXT_500, XFE_ERR_OFFSET + 386,
       "Next %d")

ResDef(XFE_MN_INVALID_ATTACH_URL, XFE_ERR_OFFSET + 387,
       "This document cannot be attached to a message:\n%s")

ResDef(XFE_PREFS_CR_CARD, XFE_ERR_OFFSET + 388,
       "Mozilla Communicator cannot find your\n"
	   "Personal Card. Would you like to create a\n"
	   "Personal Card now?")

ResDef(XFE_MN_FOLDER_TITLE, XFE_ERR_OFFSET + 389,
       "Communicator Message Center for %s")

ResDef(XFE_PREFS_LABEL_STYLE_PLAIN, XFE_ERR_OFFSET + 390, "Plain")
ResDef(XFE_PREFS_LABEL_STYLE_BOLD, XFE_ERR_OFFSET + 391, "Bold")
ResDef(XFE_PREFS_LABEL_STYLE_ITALIC, XFE_ERR_OFFSET + 392, "Italic")
ResDef(XFE_PREFS_LABEL_STYLE_BOLDITALIC, XFE_ERR_OFFSET + 393, "Bold Italic")
ResDef(XFE_PREFS_LABEL_SIZE_NORMAL, XFE_ERR_OFFSET + 394, "Normal")
ResDef(XFE_PREFS_LABEL_SIZE_BIGGER, XFE_ERR_OFFSET + 395, "Bigger")
ResDef(XFE_PREFS_LABEL_SIZE_SMALLER, XFE_ERR_OFFSET + 396, "Smaller")
ResDef(XFE_PREFS_MAIL_FOLDER_SENT, XFE_ERR_OFFSET + 397, "Sent")

ResDef(XFE_MNC_CLOSE_WARNING, XFE_ERR_OFFSET + 398,
       "Message has not been sent. Do you want to\n"
       "save the message in the Drafts Folder?")

ResDef(XFE_SEARCH_INVALID_DATE, XFE_ERR_OFFSET + 399,
       "Invalid Date Value. No search is performed.")

ResDef(XFE_SEARCH_INVALID_MONTH, XFE_ERR_OFFSET + 400,
       "Invalid value for the MONTH field.\n"
       "Please enter month in 2 digits (1-12).\n"
       "Try again!")

ResDef(XFE_SEARCH_INVALID_DAY, XFE_ERR_OFFSET + 401,
       "Invalid value for the DAY of the month field.\n"
       "Please enter date in 2 digits (1-31).\n"
       "Try again!")

ResDef(XFE_SEARCH_INVALID_YEAR, XFE_ERR_OFFSET + 402,
       "Invalid value for the YEAR field.\n"
       "Please enter year in 4 digits (e.g. 1997).\n"
       "Year value has to be 1900 or later.\n"
       "Try again!")

ResDef(XFE_MNC_ADDRESS_TO, XFE_ERR_OFFSET + 403,
	"To:")
ResDef(XFE_MNC_ADDRESS_CC, XFE_ERR_OFFSET + 404,
	"Cc:")
ResDef(XFE_MNC_ADDRESS_BCC, XFE_ERR_OFFSET + 405,
	"Bcc:")
ResDef(XFE_MNC_ADDRESS_NEWSGROUP, XFE_ERR_OFFSET + 406,
	"Newsgroup:")
ResDef(XFE_MNC_ADDRESS_REPLY, XFE_ERR_OFFSET + 407,
	"Reply To:")
ResDef(XFE_MNC_ADDRESS_FOLLOWUP, XFE_ERR_OFFSET + 408,
	"Followup To:")

ResDef(XFE_MNC_OPTION_HIGHEST, XFE_ERR_OFFSET + 409,
	"Highest")
ResDef(XFE_MNC_OPTION_HIGH, XFE_ERR_OFFSET + 410,
	"High")
ResDef(XFE_MNC_OPTION_NORMAL, XFE_ERR_OFFSET + 411,
	"Normal")
ResDef(XFE_MNC_OPTION_LOW, XFE_ERR_OFFSET + 412,
	"Highest")
ResDef(XFE_MNC_OPTION_LOWEST, XFE_ERR_OFFSET + 413,
	"Lowest")
ResDef(XFE_MNC_ADDRESS, XFE_ERR_OFFSET + 414,
	"Address")
ResDef(XFE_MNC_ATTACHMENT, XFE_ERR_OFFSET + 415,
	"Attachment")
ResDef(XFE_MNC_OPTION, XFE_ERR_OFFSET + 416,
	"Option")


ResDef(XFE_DLG_OK, XFE_ERR_OFFSET + 417, "OK")
ResDef(XFE_DLG_CLEAR, XFE_ERR_OFFSET + 418, "Clear")
ResDef(XFE_DLG_CANCEL, XFE_ERR_OFFSET + 419, "Cancel")

ResDef(XFE_PRI_URGENT, XFE_ERR_OFFSET + 420, "Urgent")
ResDef(XFE_PRI_IMPORTANT, XFE_ERR_OFFSET + 421, "Important")
ResDef(XFE_PRI_NORMAL, XFE_ERR_OFFSET + 422, "Normal")
ResDef(XFE_PRI_FYI, XFE_ERR_OFFSET + 423, "FYI")
ResDef(XFE_PRI_JUNK, XFE_ERR_OFFSET + 424, "Junk")

ResDef(XFE_PRI_PRIORITY, XFE_ERR_OFFSET + 425, "Priority")
ResDef(XFE_COMPOSE_LABEL, XFE_ERR_OFFSET + 426, "%sLabel")
ResDef(XFE_COMPOSE_ADDRESSING, XFE_ERR_OFFSET + 427, "Addressing")
ResDef(XFE_COMPOSE_ATTACHMENT, XFE_ERR_OFFSET + 428, "Attachment")
ResDef(XFE_COMPOSE_COMPOSE, XFE_ERR_OFFSET + 429, "Compose")
ResDef(XFE_SEARCH_ALLMAILFOLDERS, XFE_ERR_OFFSET + 430, "All Mail Folders")
ResDef(XFE_SEARCH_AllNEWSGROUPS, XFE_ERR_OFFSET + 431, "All Groups")
ResDef(XFE_SEARCH_LDAPDIRECTORY, XFE_ERR_OFFSET + 432, "LDAP Directory")
ResDef(XFE_SEARCH_LOCATION, XFE_ERR_OFFSET + 433, "Location")
ResDef(XFE_SEARCH_SUBJECT, XFE_ERR_OFFSET + 434, "Subject")
ResDef(XFE_SEARCH_SENDER, XFE_ERR_OFFSET + 435, "Sender")
ResDef(XFE_SEARCH_DATE, XFE_ERR_OFFSET + 436, "Date")

ResDef(XFE_PREPARE_UPLOAD, XFE_ERR_OFFSET + 437,
	   "Preparing file to publish:\n%.900s")

	/* Bookmark outliner column headers */
ResDef(XFE_BM_OUTLINER_COLUMN_NAME, XFE_ERR_OFFSET + 438, "Name")
ResDef(XFE_BM_OUTLINER_COLUMN_LOCATION, XFE_ERR_OFFSET + 439, "Location")
ResDef(XFE_BM_OUTLINER_COLUMN_LASTVISITED, XFE_ERR_OFFSET + 440, "Last Visited")
ResDef(XFE_BM_OUTLINER_COLUMN_LASTMODIFIED, XFE_ERR_OFFSET + 441, "Last Modified")

	/* Folder outliner column headers */
ResDef(XFE_FOLDER_OUTLINER_COLUMN_NAME, XFE_ERR_OFFSET + 442, "Name")
ResDef(XFE_FOLDER_OUTLINER_COLUMN_TOTAL, XFE_ERR_OFFSET + 443, "Total")
ResDef(XFE_FOLDER_OUTLINER_COLUMN_UNREAD, XFE_ERR_OFFSET + 444, "Unread")

	/* Category outliner column headers */
ResDef(XFE_CATEGORY_OUTLINER_COLUMN_NAME, XFE_ERR_OFFSET + 445, "Category")

	/* Subscribe outliner column headers */
ResDef(XFE_SUB_OUTLINER_COLUMN_NAME, XFE_ERR_OFFSET + 446, "Group Name")
ResDef(XFE_SUB_OUTLINER_COLUMN_POSTINGS, XFE_ERR_OFFSET + 447, "Postings")

	/* Thread outliner column headers */
ResDef(XFE_THREAD_OUTLINER_COLUMN_SUBJECT, XFE_ERR_OFFSET + 448, "Subject")
ResDef(XFE_THREAD_OUTLINER_COLUMN_DATE, XFE_ERR_OFFSET + 449, "Date")
ResDef(XFE_THREAD_OUTLINER_COLUMN_PRIORITY, XFE_ERR_OFFSET + 450, "Priority")
ResDef(XFE_THREAD_OUTLINER_COLUMN_STATUS, XFE_ERR_OFFSET + 451, "Status")
ResDef(XFE_THREAD_OUTLINER_COLUMN_SENDER, XFE_ERR_OFFSET + 452, "Sender")
ResDef(XFE_THREAD_OUTLINER_COLUMN_RECIPIENT, XFE_ERR_OFFSET + 453, "Recipient")

ResDef(XFE_FOLDER_MENU_FILE_HERE, XFE_ERR_OFFSET + 454, "File Here")

	/* Splash screen messages */
ResDef(XFE_SPLASH_REGISTERING_CONVERTERS, XFE_ERR_OFFSET + 455, "Registering Converters")
ResDef(XFE_SPLASH_INITIALIZING_SECURITY_LIBRARY, XFE_ERR_OFFSET + 456, "Initializing Security Library")
ResDef(XFE_SPLASH_INITIALIZING_NETWORK_LIBRARY, XFE_ERR_OFFSET + 457, "Initializing Network Library")
ResDef(XFE_SPLASH_INITIALIZING_MESSAGE_LIBRARY, XFE_ERR_OFFSET + 458, "Initializing Message Library")
ResDef(XFE_SPLASH_INITIALIZING_IMAGE_LIBRARY, XFE_ERR_OFFSET + 459, "Initializing Image Library")
ResDef(XFE_SPLASH_INITIALIZING_MOCHA, XFE_ERR_OFFSET + 460, "Initializing Javascript")
ResDef(XFE_SPLASH_INITIALIZING_PLUGINS, XFE_ERR_OFFSET + 461, "Initializing Plugins")


	/* Display factory messages */
ResDef(XFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR, XFE_ERR_OFFSET + 462, "%s: installColormap: %s must be yes, no, or guess.\n")

ResDef(XFE_BM_FRAME_TITLE, XFE_ERR_OFFSET + 463,
           "Communicator Bookmarks for %s")

ResDef(XFE_UNTITLED, XFE_ERR_OFFSET + 464,"Untitled")

ResDef(XFE_HTML_NEWSGROUP_MSG, XFE_ERR_OFFSET + 465,
	   "Unchecking this option means that this group\n"
	   "and all discussion groups above it do not\n"
	   "receive HTML messages")
ResDef(XFE_SEC_ENCRYPTED, XFE_ERR_OFFSET + 466,
	   "Encrypted")
ResDef(XFE_SEC_NONE, XFE_ERR_OFFSET + 467,
	   "None")
ResDef(XFE_SHOW_COLUMN, XFE_ERR_OFFSET + 468,
	   "Show Column")
ResDef(XFE_HIDE_COLUMN, XFE_ERR_OFFSET + 469,
	   "Hide Column")

ResDef(XFE_DISABLED_BY_ADMIN, XFE_ERR_OFFSET + 470, "That functionality has been disabled")

ResDef(XFE_EDITOR_NEW_DOCNAME, XFE_ERR_OFFSET + 471, "file: Untitled")

ResDef(XFE_EMPTY_DIR, XFE_ERR_OFFSET + 472, "%s is not set.\n")
ResDef(XFE_NEWS_DIR, XFE_ERR_OFFSET + 473, "Discussion groups directory")
ResDef(XFE_MAIL_DIR, XFE_ERR_OFFSET + 474, "Local mail directory")
ResDef(XFE_DIR_DOES_NOT_EXIST, XFE_ERR_OFFSET + 475, "%s \"%.255s\" does not exist.\n")

ResDef(XFE_SEARCH_NO_MATCHES, XFE_ERR_OFFSET + 476, "No matches found")

ResDef(XFE_EMPTY_EMAIL_ADDR, XFE_ERR_OFFSET + 477, 
	   "Please enter a valid email address (e.g. user@internet.com).\n")

ResDef(XFE_HISTORY_FRAME_TITLE, XFE_ERR_OFFSET + 478,
           "Communicator History for %s")

ResDef(XFE_HISTORY_OUTLINER_COLUMN_TITLE, XFE_ERR_OFFSET + 479,
       "Title")
ResDef(XFE_HISTORY_OUTLINER_COLUMN_LOCATION, XFE_ERR_OFFSET + 480,
       "Location")
ResDef(XFE_HISTORY_OUTLINER_COLUMN_FIRSTVISITED, XFE_ERR_OFFSET + 481,
       "First Visited")
ResDef(XFE_HISTORY_OUTLINER_COLUMN_LASTVISITED, XFE_ERR_OFFSET + 482,
       "Last Visited")
ResDef(XFE_HISTORY_OUTLINER_COLUMN_EXPIRES, XFE_ERR_OFFSET + 483,
       "Expires")
ResDef(XFE_HISTORY_OUTLINER_COLUMN_VISITCOUNT, XFE_ERR_OFFSET + 484,
       "Visit Count")

ResDef(XFE_PLACE_CONFERENCE_CALL_TO, XFE_ERR_OFFSET + 485,
           "Place a call with Mozilla Conference to ")

ResDef(XFE_SEND_MSG_TO, XFE_ERR_OFFSET + 486,
           "Send a message to ")

ResDef(XFE_INBOX_DOESNT_EXIST, XFE_ERR_OFFSET + 487,
           "Default Inbox folder does not exist.\n"
	       "You will not be able to get new messages!")

ResDef(XFE_HELPERS_APP_TELNET, XFE_ERR_OFFSET + 488, "telnet")
ResDef(XFE_HELPERS_APP_TN3270, XFE_ERR_OFFSET + 489, "TN3270 application")
ResDef(XFE_HELPERS_APP_RLOGIN, XFE_ERR_OFFSET + 490, "rlogin")
ResDef(XFE_HELPERS_APP_RLOGIN_USER, XFE_ERR_OFFSET + 491, "rlogin with user")
ResDef(XFE_HELPERS_CANNOT_DEL_STATIC_APPS, XFE_ERR_OFFSET + 492, 
	   "You cannot delete this application from the preferences.")
ResDef(XFE_HELPERS_EMPTY_APP, XFE_ERR_OFFSET + 493, 
	   "The application field is not set.")


ResDef(XFE_JAVASCRIPT_APP,  XFE_ERR_OFFSET + 494, 
       "[JavaScript Application]")

ResDef(XFE_PREFS_UPGRADE, XFE_ERR_OFFSET + 495, 
	   "The preferences you have, version %s, is incompatible with the\n"
	   "current version %s. Would you like to save new preferences now?")

ResDef(XFE_PREFS_DOWNGRADE, XFE_ERR_OFFSET + 496, 
	   "Please be aware that the preferences you have, version %s,\n"
	   "is incompatible with the current version %s.")

ResDef(XFE_MOZILLA_WRONG_RESOURCE_FILE_VERSION, XFE_ERR_OFFSET + 497, 
	   "%s: program is version %s, but resources are version %s.\n\n"
	   "\tThis means that there is an inappropriate `%s' file installed\n"
	   "\tin the app-defaults directory.  Check these environment variables\n"
	   "\tand the directories to which they point:\n\n"
	   "	$XAPPLRESDIR\n"
	   "	$XFILESEARCHPATH\n"
	   "	$XUSERFILESEARCHPATH\n\n"
	   "\tAlso check for this file in your home directory, or in the\n"
	   "\tdirectory called `app-defaults' somewhere under /usr/lib/.")

ResDef(XFE_MOZILLA_CANNOT_FOND_RESOURCE_FILE, XFE_ERR_OFFSET + 498, 
	   "%s: couldn't find our resources?\n\n"
	   "\tThis could mean that there is an inappropriate `%s' file\n"
	   "\tinstalled in the app-defaults directory.  Check these environment\n"
	   "\tvariables and the directories to which they point:\n\n"
	   "	$XAPPLRESDIR\n"
	   "	$XFILESEARCHPATH\n"
	   "	$XUSERFILESEARCHPATH\n\n"
	   "\tAlso check for this file in your home directory, or in the\n"
	   "\tdirectory called `app-defaults' somewhere under /usr/lib/.")

ResDef(XFE_MOZILLA_LOCALE_NOT_SUPPORTED_BY_XLIB, XFE_ERR_OFFSET + 499, 
	   "%s: locale `%s' not supported by Xlib; trying `C'.\n")

ResDef(XFE_MOZILLA_LOCALE_C_NOT_SUPPORTED, XFE_ERR_OFFSET + 500, 
	   "%s: locale `C' not supported.\n")

ResDef(XFE_MOZILLA_LOCALE_C_NOT_SUPPORTED_EITHER, XFE_ERR_OFFSET + 501, 
	   "%s: locale `C' not supported either.\n")

ResDef(XFE_MOZILLA_NLS_LOSAGE, XFE_ERR_OFFSET + 502, 
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

ResDef(XFE_MOZILLA_NLS_PATH_NOT_SET_CORRECTLY, XFE_ERR_OFFSET + 503,
	   "	Perhaps the $XNLSPATH environment variable is not set correctly?\n")

ResDef(XFE_MOZILLA_UNAME_FAILED, XFE_ERR_OFFSET + 504,
	   " uname failed")

ResDef(XFE_MOZILLA_UNAME_FAILED_CANT_DETERMINE_SYSTEM, XFE_ERR_OFFSET + 505,
	   "%s: uname() failed; can't determine what system we're running on\n")

ResDef(XFE_MOZILLA_TRYING_TO_RUN_SUNOS_ON_SOLARIS, XFE_ERR_OFFSET + 506,
	   "%s: this is a SunOS 4.1.3 executable, and you are running it on\n"
	   "	SunOS %s (Solaris.)  It would be a very good idea for you to\n"
	   "	run the Solaris-specific binary instead.  Bad Things may happen.\n\n")

ResDef(XFE_MOZILLA_FAILED_TO_INITIALIZE_EVENT_QUEUE, XFE_ERR_OFFSET + 507,
	   "%s: failed to initialize mozilla_event_queue!\n")

ResDef(XFE_MOZILLA_INVALID_REMOTE_OPTION, XFE_ERR_OFFSET + 508,
	   "%s: invalid `-remote' option \"%s\"\n")

ResDef(XFE_MOZILLA_ID_OPTION_MUST_PRECEED_REMOTE_OPTIONS, XFE_ERR_OFFSET + 509,
	   "%s: the `-id' option must preceed all `-remote' options.\n")

ResDef(XFE_MOZILLA_ONLY_ONE_ID_OPTION_CAN_BE_USED, XFE_ERR_OFFSET + 510,
	   "%s: only one `-id' option may be used.\n")

ResDef(XFE_MOZILLA_INVALID_OD_OPTION, XFE_ERR_OFFSET + 511,
	   "%s: invalid `-id' option \"%s\"\n")

ResDef(XFE_MOZILLA_ID_OPTION_CAN_ONLY_BE_USED_WITH_REMOTE,XFE_ERR_OFFSET + 512,
	   "%s: the `-id' option may only be used with `-remote'.\n")

ResDef(XFE_MOZILLA_XKEYSYMDB_SET_BUT_DOESNT_EXIST, XFE_ERR_OFFSET + 513,
	   "%s: warning: $XKEYSYMDB is %s,\n"
	   "	but that file doesn't exist.\n")

ResDef(XFE_MOZILLA_NO_XKEYSYMDB_FILE_FOUND, XFE_ERR_OFFSET + 514,
	   "%s: warning: no XKeysymDB file in either\n"
	   "	%s, %s, or %s\n"
	   "	Please set $XKEYSYMDB to the correct XKeysymDB"
	   " file.\n")

ResDef(XFE_MOZILLA_NOT_FOUND_IN_PATH, XFE_ERR_OFFSET + 515,
	   "%s: not found in PATH!\n")

ResDef(XFE_MOZILLA_RENAMING_SOMETHING_TO_SOMETHING, XFE_ERR_OFFSET + 516,
	   "renaming %s to %s:")

ResDef(XFE_COMMANDS_OPEN_URL_USAGE, XFE_ERR_OFFSET + 517,
	   "%s: usage: OpenURL(url [ , new-window|window-name ] )\n")

ResDef(XFE_COMMANDS_OPEN_FILE_USAGE, XFE_ERR_OFFSET + 518,
	   "%s: usage: OpenFile(file)\n")

ResDef(XFE_COMMANDS_PRINT_FILE_USAGE, XFE_ERR_OFFSET + 519,
	   "%s: usage: print([filename])\n")

ResDef(XFE_COMMANDS_SAVE_AS_USAGE, XFE_ERR_OFFSET + 520,
	   "%s: usage: SaveAS(file, output-data-type)\n")

ResDef(XFE_COMMANDS_SAVE_AS_USAGE_TWO, XFE_ERR_OFFSET + 521,
	   "%s: usage: SaveAS(file, [output-data-type])\n")

ResDef(XFE_COMMANDS_MAIL_TO_USAGE, XFE_ERR_OFFSET + 522,
	   "%s: usage: mailto(address ...)\n")

ResDef(XFE_COMMANDS_FIND_USAGE, XFE_ERR_OFFSET + 523,
	   "%s: usage: find(string)\n")

ResDef(XFE_COMMANDS_ADD_BOOKMARK_USAGE, XFE_ERR_OFFSET + 524,
	   "%s: usage: addBookmark(url, title)\n")

ResDef(XFE_COMMANDS_HTML_HELP_USAGE, XFE_ERR_OFFSET + 525,
	   "%s: usage: htmlHelp(map-file, id, search-text)\n")

ResDef(XFE_COMMANDS_UNPARSABLE_ENCODING_FILTER_SPEC, XFE_ERR_OFFSET + 526,
	   "%s: unparsable encoding filter spec: %s\n")

ResDef(XFE_COMMANDS_UPLOAD_FILE, XFE_ERR_OFFSET + 527,
	   "Upload File")

ResDef(XFE_MOZILLA_ERROR_SAVING_OPTIONS, XFE_ERR_OFFSET + 528,
	   "error saving options")

ResDef(XFE_URLBAR_FILE_HEADER, XFE_ERR_OFFSET + 529,
	   "# Mozilla User History File\n"
	   "# Version: %s\n"
	   "# This is a generated file!  Do not edit.\n"
	   "\n")

ResDef(XFE_LAY_TOO_MANY_ARGS_TO_ACTIVATE_LINK_ACTION, XFE_ERR_OFFSET + 530,
	   "%s: too many args (%d) to ActivateLinkAction()\n")

ResDef(XFE_LAY_UNKNOWN_PARAMETER_TO_ACTIVATE_LINK_ACTION, XFE_ERR_OFFSET + 531,
	   "%s: unknown parameter (%s) to ActivateLinkAction()\n")

ResDef(XFE_LAY_LOCAL_FILE_URL_UNTITLED, XFE_ERR_OFFSET + 532,
	   "file:///Untitled")

ResDef(XFE_DIALOGS_PRINTING, XFE_ERR_OFFSET + 533,
	   "printing")

ResDef(XFE_DIALOGS_DEFAULT_VISUAL_AND_COLORMAP, XFE_ERR_OFFSET + 534,
	   "\nThis is the default visual and color map.")

ResDef(XFE_DIALOGS_DEFAULT_VISUAL_AND_PRIVATE_COLORMAP, XFE_ERR_OFFSET + 535,
	   "\nThis is the default visual with a private map.")

ResDef(XFE_DIALOGS_NON_DEFAULT_VISUAL, XFE_ERR_OFFSET + 536,
	   "\nThis is a non-default visual.")

ResDef(XFE_DIALOGS_FROM_NETWORK, XFE_ERR_OFFSET + 537,
	   "from network")

ResDef(XFE_DIALOGS_FROM_DISK_CACHE, XFE_ERR_OFFSET + 538,
	   "from disk cache")

ResDef(XFE_DIALOGS_FROM_MEMORY_CACHE, XFE_ERR_OFFSET + 539,
	   "from memory cache")

ResDef(XFE_DIALOGS_DEFAULT, XFE_ERR_OFFSET + 540,
	   "default")

ResDef(XFE_HISTORY_TOO_FEW_ARGS_TO_HISTORY_ITEM, XFE_ERR_OFFSET + 541,
	   "%s: too few args (%d) to HistoryItem()\n")

ResDef(XFE_HISTORY_TOO_MANY_ARGS_TO_HISTORY_ITEM, XFE_ERR_OFFSET + 542,
	   "%s: too many args (%d) to HistoryItem()\n")

ResDef(XFE_HISTORY_UNKNOWN_PARAMETER_TO_HISTORY_ITEM, XFE_ERR_OFFSET + 543,
	   "%s: unknown parameter (%s) to HistoryItem()\n")

ResDef(XFE_REMOTE_S_UNABLE_TO_READ_PROPERTY, XFE_ERR_OFFSET + 544,
	   "%s: unable to read %s property\n")

ResDef(XFE_REMOTE_S_INVALID_DATA_ON_PROPERTY, XFE_ERR_OFFSET + 545,
	   "%s: invalid data on %s of window 0x%x.\n")
	   
ResDef(XFE_REMOTE_S_509_INTERNAL_ERROR, XFE_ERR_OFFSET + 546,
	   "509 internal error: unable to translate"
	   "window 0x%x to a widget")

ResDef(XFE_REMOTE_S_500_UNPARSABLE_COMMAND, XFE_ERR_OFFSET + 547,
	   "500 unparsable command: %s")

ResDef(XFE_REMOTE_S_501_UNRECOGNIZED_COMMAND, XFE_ERR_OFFSET + 548,
	   "501 unrecognized command: %s")
	   
ResDef(XFE_REMOTE_S_502_NO_APPROPRIATE_WINDOW, XFE_ERR_OFFSET + 549,
	   "502 no appropriate window for %s")
	   
ResDef(XFE_REMOTE_S_200_EXECUTED_COMMAND, XFE_ERR_OFFSET + 550,
	   "200 executed command: %s(")

ResDef(XFE_REMOTE_200_EXECUTED_COMMAND, XFE_ERR_OFFSET + 551,
	   "200 executed command: %s(")

ResDef(XFE_SCROLL_WINDOW_GRAVITY_WARNING, XFE_ERR_OFFSET + 552,
	   "%s: windowGravityWorks: %s must be yes, no, or guess.\n")

ResDef(XFE_COULD_NOT_DUP_STDERR, XFE_ERR_OFFSET + 553,
	   "could not dup() a stderr:")

ResDef(XFE_COULD_NOT_FDOPEN_STDERR, XFE_ERR_OFFSET + 554,
	   "could not fdopen() the new stderr:")

ResDef(XFE_COULD_NOT_DUP_A_NEW_STDERR, XFE_ERR_OFFSET + 555,
	   "could not dup() a new stderr:")

ResDef(XFE_COULD_NOT_DUP_A_STDOUT, XFE_ERR_OFFSET + 556,
	   "could not dup() a stdout:")

ResDef(XFE_COULD_NOT_FDOPEN_THE_NEW_STDOUT, XFE_ERR_OFFSET + 557,
	   "could not fdopen() the new stdout:")

ResDef(XFE_COULD_NOT_DUP_A_NEW_STDOUT, XFE_ERR_OFFSET + 558,
	   "could not dup() a new stdout:")

ResDef(XFE_HPUX_VERSION_NONSENSE, XFE_ERR_OFFSET + 559,
	   "\n%s:\n\nThis Mozilla Communicator binary does not run on %s %s.\n\n"
	   "Please visit http://home.netscape.com/ for a version of Communicator"
	   " that runs\non your system.\n\n")

ResDef(XFE_BM_OUTLINER_COLUMN_CREATEDON, XFE_ERR_OFFSET + 560,
       "Created On")

ResDef(XFE_VIRTUALNEWSGROUP, XFE_ERR_OFFSET + 561,
	"Virtual Discussion Group")

ResDef(XFE_VIRTUALNEWSGROUPDESC, XFE_ERR_OFFSET + 562,
       "Saving search criteria will create a Virtual Discussion Group\n"
       "based on that criteria. The Virtual Discussion Group will be \n"
       "available from the Message Center.\n" )

ResDef(XFE_EXIT_CONFIRMATION, XFE_ERR_OFFSET + 563,
       "Mozilla Exit Confirmation\n")

ResDef(XFE_EXIT_CONFIRMATIONDESC, XFE_ERR_OFFSET + 564,
       "Close all windows and exit Mozilla?\n")

ResDef(XFE_MAIL_WARNING, XFE_ERR_OFFSET + 565,
       "Mozilla Mail\n")

ResDef(XFE_SEND_UNSENTMAIL, XFE_ERR_OFFSET + 566,
       "Outbox folder contains unsent messages\n"
       "Send them now?")

ResDef(XFE_PREFS_WRONG_POP_USER_NAME, XFE_ERR_OFFSET + 567,
       "Your POP user name is just your user name (e.g. user),\n"
       "not your full POP address (e.g. user@internet.com).")

ResDef(XFE_PREFS_ENTER_VALID_INFO, XFE_ERR_OFFSET + 568,
       "Please enter valid information.")
ResDef(XFE_CANNOT_EDIT_JS_MAILFILTERS, XFE_ERR_OFFSET + 569,
	   "The editing of JavaScript message filters is not available\n"
	   "in this release of Communicator.")

ResDef(XFE_AB_HEADER_PHONE, XFE_ERR_OFFSET + 570,
       "Phone")

ResDef(XFE_PURGING_NEWS_MESSAGES, XFE_ERR_OFFSET + 571,
	   "Cleaning up news messages...")

ResDef(XFE_PREFS_RESTART_FOR_FONT_CHANGES, XFE_ERR_OFFSET + 572,
	   "Your font preferences will not take effect until you restart Communicator.")

ResDef(XFE_DND_NO_EMAILADDRESS, XFE_ERR_OFFSET + 573,
	   "One or more items in the selection you are dragging do not contain an email address\n"
	   "and was not added to the list. Please make sure each item in your selection includes\n"
	   "an email address.")

ResDef(XFE_NEW_FOLDER_PROMPT, XFE_ERR_OFFSET + 574, "New Folder Name:")
ResDef(XFE_USAGE_MSG4, XFE_ERR_OFFSET + 575, "\
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
ResDef(XFE_USAGE_MSG5, XFE_ERR_OFFSET + 576, "\
       Arguments which are not switches are interpreted as either files or\n\
       URLs to be loaded.\n\n" )

ResDef(XFE_SEARCH_DLG_PROMPT, XFE_ERR_OFFSET + 577, "Searching:" )
ResDef(XFE_SEARCH_AB_RESULT, XFE_ERR_OFFSET + 578, "Search Results" )
ResDef(XFE_SEARCH_AB_RESULT_FOR, XFE_ERR_OFFSET + 579, "Search results for:" )

ResDef(XFE_SEARCH_ATTRIB_NAME, XFE_ERR_OFFSET + 580, "Name" )
ResDef(XFE_SEARCH_ATTRIB_EMAIL, XFE_ERR_OFFSET + 581, "Email" )
ResDef(XFE_SEARCH_ATTRIB_ORGAN, XFE_ERR_OFFSET + 582, "Organization" )
ResDef(XFE_SEARCH_ATTRIB_ORGANU, XFE_ERR_OFFSET + 583, "Department" )

ResDef(XFE_SEARCH_RESULT_PROMPT, XFE_ERR_OFFSET + 584, "Search results will appear in address book window" )

ResDef(XFE_SEARCH_BASIC, XFE_ERR_OFFSET + 585, "Basic Search >>" )
ResDef(XFE_SEARCH_ADVANCED, XFE_ERR_OFFSET + 586, "Advanced Search >>" )
ResDef(XFE_SEARCH_MORE, XFE_ERR_OFFSET + 587, "More" )
ResDef(XFE_SEARCH_FEWER, XFE_ERR_OFFSET + 588, "Fewer" )

ResDef(XFE_SEARCH_BOOL_PROMPT, XFE_ERR_OFFSET + 589, "Find items which" )
ResDef(XFE_SEARCH_BOOL_AND_PROMPT, XFE_ERR_OFFSET + 590, "Match all of the following" )
ResDef(XFE_SEARCH_BOOL_OR_PROMPT, XFE_ERR_OFFSET + 591, "Match any of the following" )
ResDef(XFE_SEARCH_BOOL_WHERE, XFE_ERR_OFFSET + 592, "where" )
ResDef(XFE_SEARCH_BOOL_THE, XFE_ERR_OFFSET + 593, "the" )
ResDef(XFE_SEARCH_BOOL_AND_THE, XFE_ERR_OFFSET + 594, "and the" )
ResDef(XFE_SEARCH_BOOL_OR_THE, XFE_ERR_OFFSET + 595, "or the" )

ResDef(XFE_ABDIR_DESCRIPT, XFE_ERR_OFFSET + 596, "Description:" )
ResDef(XFE_ABDIR_SERVER, XFE_ERR_OFFSET + 597, "LDAP Server:" )
ResDef(XFE_ABDIR_SERVER_ROOT, XFE_ERR_OFFSET + 598, "Server Root:" )
ResDef(XFE_ABDIR_PORT_NUM, XFE_ERR_OFFSET + 599, "Port Number:" )
ResDef(XFE_ABDIR_MAX_HITS, XFE_ERR_OFFSET + 600, "Maximum Number of Hits:" )
ResDef(XFE_ABDIR_SECURE, XFE_ERR_OFFSET + 601, "Secure" )
ResDef(XFE_ABDIR_SAVE_PASSWD, XFE_ERR_OFFSET + 602, "Save Password" )
ResDef(XFE_ABDIR_DLG_TITLE, XFE_ERR_OFFSET + 603, "Directory Info" )
ResDef(XFE_AB2PANE_DIR_HEADER, XFE_ERR_OFFSET + 604, "Directories" )
ResDef(XFE_AB_SEARCH_DLG, XFE_ERR_OFFSET + 605, "Search..." )

ResDef(XFE_CUSTOM_HEADER, XFE_ERR_OFFSET + 606, "Custom Header:" )
ResDef(XFE_AB_DISPLAYNAME, XFE_ERR_OFFSET + 607, "Display Name:")
ResDef(XFE_AB_PAGER, XFE_ERR_OFFSET + 608, "Pager:")
ResDef(XFE_AB_CELLULAR, XFE_ERR_OFFSET + 609, "Cellular:")

ResDef(XFE_DND_MESSAGE_ERROR, XFE_ERR_OFFSET + 610, 
      "Cannot drop into the targeted destination folder."  )
ResDef(XFE_ABDIR_USE_PASSWD, XFE_ERR_OFFSET + 611, 
	   "Login with name and password")

ResDef(NO_SPELL_SHLIB_FOUND, XFE_ERR_OFFSET + 612,
       "No spellchk library found")

ResDef(XFE_CHOOSE_FOLDER_INSTRUCT, XFE_ERR_OFFSET + 613,
       "Choose where you would like your %s messages to be stored:")
ResDef(XFE_FOLDER_ON_SERVER_FORMAT, XFE_ERR_OFFSET + 614,
       "Place a copy in folder: '%s' on '%s'")
ResDef(XFE_FOLDER_ON_FORMAT, XFE_ERR_OFFSET + 615,
       "Folder '%s' on")
ResDef(XFE_TEMPLATES_ON_SERVER, XFE_ERR_OFFSET + 616,
       "Keep templates in '%s' on '%s'")
ResDef(XFE_DRAFTS_ON_SERVER, XFE_ERR_OFFSET + 617,
       "Keep drafts in '%s' on '%s'")
ResDef(XFE_MAIL_SELF_FORMAT, XFE_ERR_OFFSET + 618,
       "BCC: %s")

ResDef(XFE_GENERAL_TAB, XFE_ERR_OFFSET + 619, "General")
ResDef(XFE_ADVANCED_TAB, XFE_ERR_OFFSET + 620, "Advanced")
ResDef(XFE_IMAP_TAB, XFE_ERR_OFFSET + 621, "IMAP")
ResDef(XFE_SHARING, XFE_ERR_OFFSET + 622, "Sharing")
ResDef(XFE_ACL_NOT_SUPPORTED, XFE_ERR_OFFSET + 623,
	   "This server does not support shared folders")
ResDef(XFE_ACL_PERMISSIONS, XFE_ERR_OFFSET + 624,
	   "You have the following permissions:")
ResDef(XFE_ACL_SHARE_PRIVILEGES, XFE_ERR_OFFSET + 625,
	   "Share this and other folders with network users \n\
        and display and set access privileges.")
ResDef(XFE_ACL_FOLDER_TYPE, XFE_ERR_OFFSET + 626,
	   "Folder Type:")

ResDef(XFE_POP_TAB, XFE_ERR_OFFSET+627, "POP")
     
ResDef(XFE_AUTOQUOTE_STYLE_ABOVE, XFE_ERR_OFFSET+628,"above the quoted text")
ResDef(XFE_AUTOQUOTE_STYLE_BELOW, XFE_ERR_OFFSET+629,"below the quoted text")
ResDef(XFE_AUTOQUOTE_STYLE_SELECT,XFE_ERR_OFFSET+630,"select the quoted text")

ResDef(XFE_NEWSSERVER_DEFAULT, XFE_ERR_OFFSET+631, " (default)")

ResDef(XFE_FORWARD_INLINE, XFE_ERR_OFFSET+632, "Inline")
ResDef(XFE_FORWARD_QUOTED, XFE_ERR_OFFSET+633, "Quoted")
ResDef(XFE_FORWARD_ATTACH, XFE_ERR_OFFSET+634, "As Attachment")
END_STR(mcom_cmd_xfe_xfe_err_h_strings)

#endif /* __XFE_XFE_ERR_H_ */
