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

#ifndef __GNOMEFE_GNOMEFE_ERR_H_
#define __GNOMEFE_GNOMEFE_ERR_H_

#include <resdef.h>

#define GNOMEFE_ERR_OFFSET 1000

RES_START
BEGIN_STR(mcom_cmd_gnome_gnome_err_h_strings)
ResDef(DEFAULT_ADD_BOOKMARK_ROOT, GNOMEFE_ERR_OFFSET + 0, "End of List")
ResDef(DEFAULT_MENU_BOOKMARK_ROOT, GNOMEFE_ERR_OFFSET + 1, "Entire List")
ResDef(GNOMEFE_SAVE_AS_TYPE_ENCODING, GNOMEFE_ERR_OFFSET + 3, "Save As... (type %.90s, encoding %.90s)")
ResDef(GNOMEFE_SAVE_AS_TYPE, GNOMEFE_ERR_OFFSET + 4, "Save As... (type %.90s)")
ResDef(GNOMEFE_SAVE_AS_ENCODING, GNOMEFE_ERR_OFFSET + 5, "Save As... (encoding %.90s)")
ResDef(GNOMEFE_SAVE_AS, GNOMEFE_ERR_OFFSET + 6, "Save As...")
ResDef(GNOMEFE_ERROR_OPENING_FILE, GNOMEFE_ERR_OFFSET + 7, "Error opening %.900s:")
ResDef(GNOMEFE_ERROR_DELETING_FILE, GNOMEFE_ERR_OFFSET + 8, "Error deleting %.900s:")
ResDef(GNOMEFE_LOG_IN_AS, GNOMEFE_ERR_OFFSET + 9, "When connected, log in as user `%.900s'")
ResDef(GNOMEFE_OUT_OF_MEMORY_URL, GNOMEFE_ERR_OFFSET + 10, "Out Of Memory -- Can't Open URL")
ResDef(GNOMEFE_COULD_NOT_LOAD_FONT, GNOMEFE_ERR_OFFSET + 11, "couldn't load:\n%s")
ResDef(GNOMEFE_USING_FALLBACK_FONT, GNOMEFE_ERR_OFFSET + 12, "%s\nNo other resources were reasonable!\nUsing fallback font \"%s\" instead.")
ResDef(GNOMEFE_NO_FALLBACK_FONT, GNOMEFE_ERR_OFFSET + 13, "%s\nNo other resources were reasonable!\nThe fallback font \"%s\" could not be loaded!\nGiving up.")
ResDef(GNOMEFE_DISCARD_BOOKMARKS, GNOMEFE_ERR_OFFSET + 14, "Bookmarks file has changed on disk: discard your changes?")
ResDef(GNOMEFE_RELOAD_BOOKMARKS, GNOMEFE_ERR_OFFSET + 15, "Bookmarks file has changed on disk: reload it?")
ResDef(GNOMEFE_NEW_ITEM, GNOMEFE_ERR_OFFSET + 16, "New Item")
ResDef(GNOMEFE_NEW_HEADER, GNOMEFE_ERR_OFFSET + 17, "New Header")
ResDef(GNOMEFE_REMOVE_SINGLE_ENTRY, GNOMEFE_ERR_OFFSET + 18, "Remove category \"%.900s\" and its %d entry?")
ResDef(GNOMEFE_REMOVE_MULTIPLE_ENTRIES, GNOMEFE_ERR_OFFSET + 19, "Remove category \"%.900s\" and its %d entries?")
ResDef(GNOMEFE_EXPORT_BOOKMARK, GNOMEFE_ERR_OFFSET + 20, "Export Bookmark")
ResDef(GNOMEFE_IMPORT_BOOKMARK, GNOMEFE_ERR_OFFSET + 21, "Import Bookmark")
ResDef(GNOMEFE_SECURITY_WITH, GNOMEFE_ERR_OFFSET + 22, "This version supports %s security with %s.")
ResDef(GNOMEFE_SECURITY_DISABLED, GNOMEFE_ERR_OFFSET + 23, "Security disabled")
ResDef(GNOMEFE_WELCOME_HTML, GNOMEFE_ERR_OFFSET + 24, "file:/usr/local/lib/netscape/docs/Welcome.html")
ResDef(GNOMEFE_DOCUMENT_DONE, GNOMEFE_ERR_OFFSET + 25, "Document: Done.")
ResDef(GNOMEFE_OPEN_FILE, GNOMEFE_ERR_OFFSET + 26, "Open File")
ResDef(GNOMEFE_ERROR_OPENING_PIPE, GNOMEFE_ERR_OFFSET + 27, "Error opening pipe to %.900s")
ResDef(GNOMEFE_WARNING, GNOMEFE_ERR_OFFSET + 28, "Warning:\n\n")
ResDef(GNOMEFE_FILE_DOES_NOT_EXIST, GNOMEFE_ERR_OFFSET + 29, "%s \"%.255s\" does not exist.\n")
ResDef(GNOMEFE_HOST_IS_UNKNOWN, GNOMEFE_ERR_OFFSET + 30, "%s \"%.255s\" is unknown.\n")
ResDef(GNOMEFE_NO_PORT_NUMBER, GNOMEFE_ERR_OFFSET + 31, "No port number specified for %s.\n")
ResDef(GNOMEFE_MAIL_HOST, GNOMEFE_ERR_OFFSET + 32, "Mail host")
ResDef(GNOMEFE_NEWS_HOST, GNOMEFE_ERR_OFFSET + 33, "News host")
ResDef(GNOMEFE_NEWS_RC_DIRECTORY, GNOMEFE_ERR_OFFSET + 34, "News RC directory")
ResDef(GNOMEFE_TEMP_DIRECTORY, GNOMEFE_ERR_OFFSET + 35, "Temporary directory")
ResDef(GNOMEFE_FTP_PROXY_HOST, GNOMEFE_ERR_OFFSET + 36, "FTP proxy host")
ResDef(GNOMEFE_GOPHER_PROXY_HOST, GNOMEFE_ERR_OFFSET + 37, "Gopher proxy host")
ResDef(GNOMEFE_HTTP_PROXY_HOST, GNOMEFE_ERR_OFFSET + 38, "HTTP proxy host")
ResDef(GNOMEFE_HTTPS_PROXY_HOST, GNOMEFE_ERR_OFFSET + 39, "HTTPS proxy host")
ResDef(GNOMEFE_WAIS_PROXY_HOST, GNOMEFE_ERR_OFFSET + 40, "WAIS proxy host")
ResDef(GNOMEFE_SOCKS_HOST, GNOMEFE_ERR_OFFSET + 41, "SOCKS host")
ResDef(GNOMEFE_GLOBAL_MIME_FILE, GNOMEFE_ERR_OFFSET + 42, "Global MIME types file")
ResDef(GNOMEFE_PRIVATE_MIME_FILE, GNOMEFE_ERR_OFFSET + 43, "Private MIME types file")
ResDef(GNOMEFE_GLOBAL_MAILCAP_FILE, GNOMEFE_ERR_OFFSET + 44, "Global mailcap file")
ResDef(GNOMEFE_PRIVATE_MAILCAP_FILE, GNOMEFE_ERR_OFFSET + 45, "Private mailcap file")
ResDef(GNOMEFE_BM_CANT_DEL_ROOT, GNOMEFE_ERR_OFFSET + 46, "Cannot delete toplevel bookmark")
ResDef(GNOMEFE_BM_CANT_CUT_ROOT, GNOMEFE_ERR_OFFSET + 47, "Cannot cut toplevel bookmark")
ResDef(GNOMEFE_BM_ALIAS, GNOMEFE_ERR_OFFSET + 48, "This is an alias to the following Bookmark:")
/* NPS i18n stuff */
ResDef(GNOMEFE_FILE_OPEN, GNOMEFE_ERR_OFFSET + 49, "File Open...")
ResDef(GNOMEFE_PRINTING_OF_FRAMES_IS_CURRENTLY_NOT_SUPPORTED, GNOMEFE_ERR_OFFSET + 50, "Printing of frames is currently not supported.")
ResDef(GNOMEFE_ERROR_SAVING_OPTIONS, GNOMEFE_ERR_OFFSET + 51, "error saving options")
ResDef(GNOMEFE_UNKNOWN_ESCAPE_CODE, GNOMEFE_ERR_OFFSET + 52,
"unknown %s escape code %%%c:\n%%h = host, %%p = port, %%u = user")
ResDef(GNOMEFE_COULDNT_FORK, GNOMEFE_ERR_OFFSET + 53, "couldn't fork():")
ResDef(GNOMEFE_EXECVP_FAILED, GNOMEFE_ERR_OFFSET + 54, "%s: execvp(%s) failed")
ResDef(GNOMEFE_SAVE_FRAME_AS, GNOMEFE_ERR_OFFSET + 55, "Save Frame as...")
ResDef(GNOMEFE_PRINT_FRAME, GNOMEFE_ERR_OFFSET + 57, "Print Frame...")
ResDef(GNOMEFE_PRINT, GNOMEFE_ERR_OFFSET + 58 , "Print...")
ResDef(GNOMEFE_DOWNLOAD_FILE, GNOMEFE_ERR_OFFSET + 59, "Download File: %s")
ResDef(GNOMEFE_COMPOSE_NO_SUBJECT, GNOMEFE_ERR_OFFSET + 60, "Compose: (No Subject)")
ResDef(GNOMEFE_COMPOSE, GNOMEFE_ERR_OFFSET + 61, "Compose: %s")
ResDef(GNOMEFE_NETSCAPE_UNTITLED, GNOMEFE_ERR_OFFSET + 62, "Mozilla: <untitled>")
ResDef(GNOMEFE_NETSCAPE, GNOMEFE_ERR_OFFSET + 63, "Mozilla: %s")
ResDef(GNOMEFE_NO_SUBJECT, GNOMEFE_ERR_OFFSET + 64, "(no subject)")
ResDef(GNOMEFE_UNKNOWN_ERROR_CODE, GNOMEFE_ERR_OFFSET + 65, "unknown error code %d")
ResDef(GNOMEFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST, GNOMEFE_ERR_OFFSET + 66,
"Invalid File attachment.\n%s: doesn't exist.\n")
ResDef(GNOMEFE_INVALID_FILE_ATTACHMENT_NOT_READABLE, GNOMEFE_ERR_OFFSET + 67,
"Invalid File attachment.\n%s: not readable.\n")
ResDef(GNOMEFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY, GNOMEFE_ERR_OFFSET + 68,
"Invalid File attachment.\n%s: is a directory.\n")
ResDef(GNOMEFE_COULDNT_FORK_FOR_MOVEMAIL, GNOMEFE_ERR_OFFSET + 69,
"couldn't fork() for movemail")
ResDef(GNOMEFE_PROBLEMS_EXECUTING, GNOMEFE_ERR_OFFSET + 70, "problems executing %s:")
ResDef(GNOMEFE_TERMINATED_ABNORMALLY, GNOMEFE_ERR_OFFSET + 71, "%s terminated abnormally:")
ResDef(GNOMEFE_UNABLE_TO_OPEN, GNOMEFE_ERR_OFFSET + 72, "Unable to open %.900s")
ResDef(GNOMEFE_PLEASE_ENTER_NEWS_HOST, GNOMEFE_ERR_OFFSET + 73,
"Please enter your news host in one\n\
of the following formats:\n\n\
    news://HOST,\n\
    news://HOST:PORT,\n\
    snews://HOST, or\n\
    snews://HOST:PORT\n\n" )

ResDef(GNOMEFE_MOVEMAIL_FAILURE_SUFFIX, GNOMEFE_ERR_OFFSET + 74,
       "For the internal movemail method to work, we must be able to create\n\
lock files in the mail spool directory.  On many systems, this is best\n\
accomplished by making that directory be mode 01777.  If that is not\n\
possible, then an external setgid/setuid movemail program must be used.\n\
Please see the Release Notes for more information.")
ResDef(GNOMEFE_CANT_MOVE_MAIL, GNOMEFE_ERR_OFFSET + 75, "Can't move mail from %.200s")
ResDef(GNOMEFE_CANT_GET_NEW_MAIL_LOCK_FILE_EXISTS, GNOMEFE_ERR_OFFSET + 76, "Can't get new mail; a lock file %.200s exists.")
ResDef(GNOMEFE_CANT_GET_NEW_MAIL_UNABLE_TO_CREATE_LOCK_FILE, GNOMEFE_ERR_OFFSET + 77, "Can't get new mail; unable to create lock file %.200s")
ResDef(GNOMEFE_CANT_GET_NEW_MAIL_SYSTEM_ERROR, GNOMEFE_ERR_OFFSET + 78, "Can't get new mail; a system error occurred.")
ResDef(GNOMEFE_CANT_MOVE_MAIL_UNABLE_TO_OPEN, GNOMEFE_ERR_OFFSET + 79, "Can't move mail; unable to open %.200s")
ResDef(GNOMEFE_CANT_MOVE_MAIL_UNABLE_TO_READ, GNOMEFE_ERR_OFFSET + 80, "Can't move mail; unable to read %.200s")
ResDef(GNOMEFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE, GNOMEFE_ERR_OFFSET + 81, "Can't move mail; unable to write to %.200s")
ResDef(GNOMEFE_THERE_WERE_PROBLEMS_MOVING_MAIL, GNOMEFE_ERR_OFFSET + 82, "There were problems moving mail")
ResDef(GNOMEFE_THERE_WERE_PROBLEMS_MOVING_MAIL_EXIT_STATUS, GNOMEFE_ERR_OFFSET + 83, "There were problems moving mail: exit status %d" )
ResDef(GNOMEFE_THERE_WERE_PROBLEMS_CLEANING_UP, GNOMEFE_ERR_OFFSET + 134, "There were problems cleaning up %s")
ResDef(GNOMEFE_USAGE_MSG1, GNOMEFE_ERR_OFFSET + 85, "%s\n\
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
ResDef(GNOMEFE_USAGE_MSG2, GNOMEFE_ERR_OFFSET + 154, "\
       -share                    when -install, cause each window to use the\n\
                                 same colormap instead of each window using\n\
                                 a new one.\n\
       -no-share                 cause each window to use the same colormap.\n")
ResDef(GNOMEFE_USAGE_MSG3, GNOMEFE_ERR_OFFSET + 86, "\
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
ResDef(GNOMEFE_VERSION_COMPLAINT_FORMAT, GNOMEFE_ERR_OFFSET + 87, "%s: program is version %s, but resources are version %s.\n\n\
\tThis means that there is an inappropriate `%s' file in\n\
\the system-wide app-defaults directory, or possibly in your\n\
\thome directory.  Check these environment variables and the\n\
\tdirectories to which they point:\n\n\
  $XAPPLRESDIR\n\
  $XFILESEARCHPATH\n\
  $XUSERFILESEARCHPATH\n\n\
\tAlso check for this file in your home directory, or in the\n\
\tdirectory called `app-defaults' somewhere under /usr/lib/." )
ResDef(GNOMEFE_INAPPROPRIATE_APPDEFAULT_FILE, GNOMEFE_ERR_OFFSET + 88, "%s: couldn't find our resources?\n\n\
\tThis could mean that there is an inappropriate `%s' file\n\
\tinstalled in the app-defaults directory.  Check these environment\n\
\tvariables and the directories to which they point:\n\n\
  $XAPPLRESDIR\n\
  $XFILESEARCHPATH\n\
  $XUSERFILESEARCHPATH\n\n\
\tAlso check for this file in your home directory, or in the\n\
\tdirectory called `app-defaults' somewhere under /usr/lib/." )
ResDef(GNOMEFE_INVALID_GEOMETRY_SPEC, GNOMEFE_ERR_OFFSET + 89, "%s: invalid geometry specification.\n\n\
 Apparently \"%s*geometry: %s\" or \"%s*geometry: %s\"\n\
 was specified in the resource database.  Specifying \"*geometry\"\n\
 will make %s (and most other X programs) malfunction in obscure\n\
 ways.  You should always use \".geometry\" instead.\n" )
ResDef(GNOMEFE_UNRECOGNISED_OPTION, GNOMEFE_ERR_OFFSET + 90, "%s: unrecognized option \"%s\"\n")
ResDef(GNOMEFE_APP_HAS_DETECTED_LOCK, GNOMEFE_ERR_OFFSET + 91, "%s has detected a %s\nfile.\n")
ResDef(GNOMEFE_ANOTHER_USER_IS_RUNNING_APP, GNOMEFE_ERR_OFFSET + 92, "\nThis may indicate that another user is running\n%s using your %s files.\n" )
ResDef(GNOMEFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID, GNOMEFE_ERR_OFFSET + 93,"It appears to be running on host %s\nunder process-ID %u.\n" )
ResDef(GNOMEFE_YOU_MAY_CONTINUE_TO_USE, GNOMEFE_ERR_OFFSET + 94, "\nYou may continue to use %s, but you will\n\
be unable to use the disk cache, global history,\n\
or your personal certificates.\n" )
ResDef(GNOMEFE_OTHERWISE_CHOOSE_CANCEL, GNOMEFE_ERR_OFFSET + 95, "\nOtherwise, you may choose Cancel, make sure that\n\
you are not running another %s Navigator,\n\
delete the %s file, and\n\
restart %s." )
ResDef(GNOMEFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY, GNOMEFE_ERR_OFFSET + 96, "%s: %s existed, but was not a directory.\n\
The old file has been renamed to %s\n\
and a directory has been created in its place.\n\n" )
ResDef(GNOMEFE_EXISTS_BUT_UNABLE_TO_RENAME, GNOMEFE_ERR_OFFSET + 97, "%s: %s exists but is not a directory,\n\
and we were unable to rename it!\n\
Please remove this file: it is in the way.\n\n" )
ResDef(GNOMEFE_UNABLE_TO_CREATE_DIRECTORY, GNOMEFE_ERR_OFFSET + 98,
"%s: unable to create the directory `%s'.\n%s\n\
Please create this directory.\n\n" )
ResDef(GNOMEFE_UNKNOWN_ERROR, GNOMEFE_ERR_OFFSET + 99, "unknown error")
ResDef(GNOMEFE_ERROR_CREATING, GNOMEFE_ERR_OFFSET + 100, "error creating %s")
ResDef(GNOMEFE_ERROR_WRITING, GNOMEFE_ERR_OFFSET + 101, "error writing %s")
ResDef(GNOMEFE_CREATE_CONFIG_FILES, GNOMEFE_ERR_OFFSET + 102,
"This version of %s uses different names for its config files\n\
than previous versions did.  Those config files which still use\n\
the same file formats have been copied to their new names, and\n\
those which don't will be recreated as necessary.\n%s\n\n\
Would you like us to delete the old files now?" )
ResDef(GNOMEFE_OLD_FILES_AND_CACHE, GNOMEFE_ERR_OFFSET + 103,
"\nThe old files still exist, including a disk cache directory\n\
(which might be large.)" )
ResDef(GNOMEFE_OLD_FILES, GNOMEFE_ERR_OFFSET + 104, "The old files still exist.")
ResDef(GNOMEFE_GENERAL, GNOMEFE_ERR_OFFSET + 105, "General")
ResDef(GNOMEFE_PASSWORDS, GNOMEFE_ERR_OFFSET + 106, "Passwords")
ResDef(GNOMEFE_PERSONAL_CERTIFICATES, GNOMEFE_ERR_OFFSET + 107, "Personal Certificates")
ResDef(GNOMEFE_SITE_CERTIFICATES, GNOMEFE_ERR_OFFSET + 108, "Site Certificates")
ResDef(GNOMEFE_ESTIMATED_TIME_REMAINING_CHECKED, GNOMEFE_ERR_OFFSET + 109,
"Checked %s (%d left)\n%d%% completed)\n\n\
Estimated time remaining: %s\n\
(Remaining time depends on the sites selected and\n\
the network traffic.)" )
ResDef(GNOMEFE_ESTIMATED_TIME_REMAINING_CHECKING, GNOMEFE_ERR_OFFSET + 110,
"Checking ... (%d left)\n%d%% completed)\n\n\
Estimated time remaining: %s\n\
(Remaining time depends on the sites selected and\n\
the network traffic.)" )
ResDef(GNOMEFE_RE, GNOMEFE_ERR_OFFSET + 111, "Re: ")
ResDef(GNOMEFE_DONE_CHECKING_ETC, GNOMEFE_ERR_OFFSET + 112,
"Done checking %d bookmarks.\n\
%d documents were reached.\n\
%d documents have changed and are marked as new." )
ResDef(GNOMEFE_APP_EXITED_WITH_STATUS, GNOMEFE_ERR_OFFSET + 115,"\"%s\" exited with status %d" )
ResDef(GNOMEFE_THE_MOTIF_KEYSYMS_NOT_DEFINED, GNOMEFE_ERR_OFFSET + 116,
"%s: The Motif keysyms seem not to be defined.\n\n\
 This is usually because the proper XKeysymDB file was not found.\n\
 You can set the $XKEYSYMDB environment variable to the location\n\
 of a file which contains the right keysyms.\n\n\
 Without the right XKeysymDB, many warnings will be generated,\n\
 and most keyboard accelerators will not work.\n\n\
 (An appropriate XKeysymDB file was included with the %s\n\
 distribution.)\n\n" )
ResDef(GNOMEFE_SOME_MOTIF_KEYSYMS_NOT_DEFINED, GNOMEFE_ERR_OFFSET + 117,
 "%s: Some of the Motif keysyms seem not to be defined.\n\n\
 This is usually because the proper XKeysymDB file was not found.\n\
 You can set the $XKEYSYMDB environment variable to the location\n\
 of a file which contains the right keysyms.\n\n\
 Without the right XKeysymDB, many warnings will be generated,\n\
 and some keyboard accelerators will not work.\n\n\
 (An appropriate XKeysymDB file was included with the %s\n\
 distribution.)\n\n" )
ResDef(GNOMEFE_VISUAL_GRAY_DIRECTCOLOR, GNOMEFE_ERR_OFFSET + 118,
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
ResDef(GNOMEFE_VISUAL_GRAY, GNOMEFE_ERR_OFFSET + 119,
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
ResDef(GNOMEFE_VISUAL_DIRECTCOLOR, GNOMEFE_ERR_OFFSET + 120,
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
ResDef(GNOMEFE_VISUAL_NORMAL, GNOMEFE_ERR_OFFSET + 121,
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
ResDef(GNOMEFE_WILL_BE_DISPLAYED_IN_MONOCHROME, GNOMEFE_ERR_OFFSET + 122,
"will be\ndisplayed in monochrome" )
ResDef(GNOMEFE_WILL_LOOK_BAD, GNOMEFE_ERR_OFFSET + 123, "will look bad")
ResDef(GNOMEFE_APPEARANCE, GNOMEFE_ERR_OFFSET + 124, "Appearance")
ResDef(GNOMEFE_BOOKMARKS, GNOMEFE_ERR_OFFSET + 125, "Bookmarks")
ResDef(GNOMEFE_COLORS, GNOMEFE_ERR_OFFSET + 126, "Colors")
ResDef(GNOMEFE_FONTS, GNOMEFE_ERR_OFFSET + 127, "Fonts" )
ResDef(GNOMEFE_APPLICATIONS, GNOMEFE_ERR_OFFSET + 128, "Applications")
ResDef(GNOMEFE_HELPERS, GNOMEFE_ERR_OFFSET + 155, "Helpers")
ResDef(GNOMEFE_IMAGES, GNOMEFE_ERR_OFFSET + 129, "Images")
ResDef(GNOMEFE_LANGUAGES, GNOMEFE_ERR_OFFSET + 130, "Languages")
ResDef(GNOMEFE_CACHE, GNOMEFE_ERR_OFFSET + 131, "Cache")
ResDef(GNOMEFE_CONNECTIONS, GNOMEFE_ERR_OFFSET + 132, "Connections")
ResDef(GNOMEFE_PROXIES, GNOMEFE_ERR_OFFSET + 133, "Proxies")
ResDef(GNOMEFE_COMPOSE_DLG, GNOMEFE_ERR_OFFSET + 135, "Compose")
ResDef(GNOMEFE_SERVERS, GNOMEFE_ERR_OFFSET + 136, "Servers")
ResDef(GNOMEFE_IDENTITY, GNOMEFE_ERR_OFFSET + 137, "Identity")
ResDef(GNOMEFE_ORGANIZATION, GNOMEFE_ERR_OFFSET + 138, "Organization")
ResDef(GNOMEFE_MAIL_FRAME, GNOMEFE_ERR_OFFSET + 139, "Mail Frame" )
ResDef(GNOMEFE_MAIL_DOCUMENT, GNOMEFE_ERR_OFFSET + 140, "Mail Document" )
ResDef(GNOMEFE_NETSCAPE_MAIL, GNOMEFE_ERR_OFFSET + 141, "Mozilla Mail & Discussions" )
ResDef(GNOMEFE_NETSCAPE_NEWS, GNOMEFE_ERR_OFFSET + 142, "Mozilla Discussions" )
ResDef(GNOMEFE_ADDRESS_BOOK, GNOMEFE_ERR_OFFSET + 143, "Address Book" )
ResDef(GNOMEFE_X_RESOURCES_NOT_INSTALLED_CORRECTLY, GNOMEFE_ERR_OFFSET + 144, 
"X resources not installed correctly!" )
ResDef(GNOMEFE_GG_EMPTY_LL, GNOMEFE_ERR_OFFSET + 145, "<< Empty >>")
ResDef(GNOMEFE_ERROR_SAVING_PASSWORD, GNOMEFE_ERR_OFFSET + 146, "error saving password")
ResDef(GNOMEFE_UNIMPLEMENTED, GNOMEFE_ERR_OFFSET + 147, "Unimplemented.")
ResDef(GNOMEFE_TILDE_USER_SYNTAX_DISALLOWED, GNOMEFE_ERR_OFFSET + 148, 
"%s: ~user/ syntax is not allowed in preferences file, only ~/\n" )
ResDef(GNOMEFE_UNRECOGNISED_VISUAL, GNOMEFE_ERR_OFFSET + 149, 
"%s: unrecognized visual \"%s\".\n" )
ResDef(GNOMEFE_NO_VISUAL_WITH_ID, GNOMEFE_ERR_OFFSET + 150,
"%s: no visual with id 0x%x.\n" )
ResDef(GNOMEFE_NO_VISUAL_OF_CLASS, GNOMEFE_ERR_OFFSET + 151,
"%s: no visual of class %s.\n" )
ResDef(GNOMEFE_STDERR_DIAGNOSTICS_HAVE_BEEN_TRUNCATED, GNOMEFE_ERR_OFFSET + 152, 
"\n\n<< stderr diagnostics have been truncated >>" )
ResDef(GNOMEFE_ERROR_CREATING_PIPE, GNOMEFE_ERR_OFFSET + 153, "error creating pipe:")
ResDef(GNOMEFE_OUTBOX_CONTAINS_MSG, GNOMEFE_ERR_OFFSET + 156,
"Outbox folder contains an unsent\nmessage. Send it now?\n")
ResDef(GNOMEFE_OUTBOX_CONTAINS_MSGS, GNOMEFE_ERR_OFFSET + 157,
"Outbox folder contains %d unsent\nmessages. Send them now?\n")
ResDef(GNOMEFE_NO_KEEP_ON_SERVER_WITH_MOVEMAIL, GNOMEFE_ERR_OFFSET + 158,
"The ``Leave on Server'' option only works when\n\
using a POP3 server, not when using a local\n\
mail spool directory.  To retrieve your mail,\n\
first turn off that option in the Servers pane\n\
of the Mail and News Preferences.")
ResDef(GNOMEFE_BACK, GNOMEFE_ERR_OFFSET + 159, "Back")
ResDef(GNOMEFE_BACK_IN_FRAME, GNOMEFE_ERR_OFFSET + 160, "Back in Frame")
ResDef(GNOMEFE_FORWARD, GNOMEFE_ERR_OFFSET + 161, "Forward")
ResDef(GNOMEFE_FORWARD_IN_FRAME, GNOMEFE_ERR_OFFSET + 162, "Forward in Frame")
ResDef(GNOMEFE_MAIL_SPOOL_UNKNOWN, GNOMEFE_ERR_OFFSET + 163,
"Please set the $MAIL environment variable to the\n\
location of your mail-spool file.")
ResDef(GNOMEFE_MOVEMAIL_NO_MESSAGES, GNOMEFE_ERR_OFFSET + 164,
"No new messages.")
ResDef(GNOMEFE_USER_DEFINED, GNOMEFE_ERR_OFFSET + 165,
"User-Defined")
ResDef(GNOMEFE_OTHER_LANGUAGE, GNOMEFE_ERR_OFFSET + 166,
"Other")
ResDef(GNOMEFE_COULDNT_FORK_FOR_DELIVERY, GNOMEFE_ERR_OFFSET + 167,
"couldn't fork() for external message delivery")
ResDef(GNOMEFE_CANNOT_READ_FILE, GNOMEFE_ERR_OFFSET + 168,
"Cannot read file %s.")
ResDef(GNOMEFE_CANNOT_SEE_FILE, GNOMEFE_ERR_OFFSET + 169,
"%s does not exist.")
ResDef(GNOMEFE_IS_A_DIRECTORY, GNOMEFE_ERR_OFFSET + 170,
"%s is a directory.")
ResDef(GNOMEFE_EKIT_LOCK_FILE_NOT_FOUND, GNOMEFE_ERR_OFFSET + 171,
"Lock file not found.")
ResDef(GNOMEFE_EKIT_CANT_OPEN, GNOMEFE_ERR_OFFSET + 172,
"Can't open Netscape.lock file.")
ResDef(GNOMEFE_EKIT_MODIFIED, GNOMEFE_ERR_OFFSET + 173,
"Netscape.lock file has been modified.")
ResDef(GNOMEFE_EKIT_FILESIZE, GNOMEFE_ERR_OFFSET + 174,
"Could not determine lock file size.")
ResDef(GNOMEFE_EKIT_CANT_READ, GNOMEFE_ERR_OFFSET + 175,
"Could not read Netscape.lock data.")
ResDef(GNOMEFE_ANIM_CANT_OPEN, GNOMEFE_ERR_OFFSET + 176,
"Couldn't open animation file.")
ResDef(GNOMEFE_ANIM_MODIFIED, GNOMEFE_ERR_OFFSET + 177,
"Animation file modified.\nUsing default animation.")
ResDef(GNOMEFE_ANIM_READING_SIZES, GNOMEFE_ERR_OFFSET + 178,
"Couldn't read animation file size.\nUsing default animation.")
ResDef(GNOMEFE_ANIM_READING_NUM_COLORS, GNOMEFE_ERR_OFFSET + 179,
"Couldn't read number of animation colors.\nUsing default animation.")
ResDef(GNOMEFE_ANIM_READING_COLORS, GNOMEFE_ERR_OFFSET + 180,
"Couldn't read animation colors.\nUsing default animation.")
ResDef(GNOMEFE_ANIM_READING_FRAMES, GNOMEFE_ERR_OFFSET + 181,
"Couldn't read animation frames.\nUsing default animation.")
ResDef(GNOMEFE_ANIM_NOT_AT_EOF, GNOMEFE_ERR_OFFSET + 182,
"Ignoring extra bytes at end of animation file.")
ResDef(GNOMEFE_LOOK_FOR_DOCUMENTS_THAT_HAVE_CHANGED_ON, GNOMEFE_ERR_OFFSET + 183,
"Look for documents that have changed for:")
/* I'm not quite sure why these are resources?? ..djw */
ResDef(GNOMEFE_CHARACTER, GNOMEFE_ERR_OFFSET + 184, "Character") /* 26FEB96RCJ */
ResDef(GNOMEFE_LINK, GNOMEFE_ERR_OFFSET + 185, "Link")           /* 26FEB96RCJ */
ResDef(GNOMEFE_PARAGRAPH, GNOMEFE_ERR_OFFSET + 186, "Paragraph") /* 26FEB96RCJ */
ResDef(GNOMEFE_IMAGE,     GNOMEFE_ERR_OFFSET + 187, "Image")     /*  5MAR96RCJ */
ResDef(GNOMEFE_REFRESH_FRAME, GNOMEFE_ERR_OFFSET + 188, "Refresh Frame")
ResDef(GNOMEFE_REFRESH, GNOMEFE_ERR_OFFSET + 189, "Refresh")

ResDef(GNOMEFE_MAIL_TITLE_FMT, GNOMEFE_ERR_OFFSET + 190, "Mozilla Mail & Discussions: %.900s")
ResDef(GNOMEFE_NEWS_TITLE_FMT, GNOMEFE_ERR_OFFSET + 191, "Mozilla Discussions: %.900s")
ResDef(GNOMEFE_TITLE_FMT, GNOMEFE_ERR_OFFSET + 192, "Mozilla: %.900s")

ResDef(GNOMEFE_PROTOCOLS, GNOMEFE_ERR_OFFSET + 193, "Protocols")
ResDef(GNOMEFE_LANG, GNOMEFE_ERR_OFFSET + 194, "Languages")
ResDef(GNOMEFE_CHANGE_PASSWORD, GNOMEFE_ERR_OFFSET + 195, "Change Password")
ResDef(GNOMEFE_SET_PASSWORD, GNOMEFE_ERR_OFFSET + 196, "Set Password...")
ResDef(GNOMEFE_NO_PLUGINS, GNOMEFE_ERR_OFFSET + 197, "No Plugins")

/*
 * Messages for the dialog that warns before doing movemail.
 * DEM 4/30/96
 */
ResDef(GNOMEFE_CONTINUE_MOVEMAIL, GNOMEFE_ERR_OFFSET + 198, "Continue Movemail")
ResDef(GNOMEFE_CANCEL_MOVEMAIL, GNOMEFE_ERR_OFFSET + 199, "Cancel Movemail")
ResDef(GNOMEFE_MOVEMAIL_EXPLANATION, GNOMEFE_ERR_OFFSET + 200,
"Mozilla will move your mail from %s\n\
to %s/Inbox.\n\
\n\
Moving the mail will interfere with other mailers\n\
that expect already-read mail to remain in\n\
%s." )
ResDef(GNOMEFE_SHOW_NEXT_TIME, GNOMEFE_ERR_OFFSET + 201, "Show this Alert Next Time" )
ResDef(GNOMEFE_EDITOR_TITLE_FMT, GNOMEFE_ERR_OFFSET + 202, "Mozilla Composer: %.900s")

ResDef(GNOMEFE_HELPERS_NETSCAPE, GNOMEFE_ERR_OFFSET + 203, "Mozilla")
ResDef(GNOMEFE_HELPERS_UNKNOWN, GNOMEFE_ERR_OFFSET + 204, "Unknown:Prompt User")
ResDef(GNOMEFE_HELPERS_SAVE, GNOMEFE_ERR_OFFSET + 205, "Save To Disk")
ResDef(GNOMEFE_HELPERS_PLUGIN, GNOMEFE_ERR_OFFSET + 206, "Plug In : %s")
ResDef(GNOMEFE_HELPERS_EMPTY_MIMETYPE, GNOMEFE_ERR_OFFSET + 207,
"Mime type cannot be emtpy.")
ResDef(GNOMEFE_HELPERS_LIST_HEADER, GNOMEFE_ERR_OFFSET + 208, "Description|Handled By")
ResDef(GNOMEFE_MOVEMAIL_CANT_DELETE_LOCK, GNOMEFE_ERR_OFFSET + 209,
"Can't get new mail; a lock file %s exists.")
ResDef(GNOMEFE_PLUGIN_NO_PLUGIN, GNOMEFE_ERR_OFFSET + 210,
"No plugin %s. Reverting to save-to-disk for type %s.\n")
ResDef(GNOMEFE_PLUGIN_CANT_LOAD, GNOMEFE_ERR_OFFSET + 211,
"ERROR: %s\nCant load plugin %s. Ignored.\n")
ResDef(GNOMEFE_HELPERS_PLUGIN_DESC_CHANGE, GNOMEFE_ERR_OFFSET + 212,
"Plugin specifies different Description and/or Suffixes for mimetype %s.\n\
\n\
        Description = \"%s\"\n\
        Suffixes = \"%s\"\n\
\n\
Use plugin specified Description and Suffixes ?")
ResDef(GNOMEFE_CANT_SAVE_PREFS, GNOMEFE_ERR_OFFSET + 213, "error Saving options.")
ResDef(GNOMEFE_VALUES_OUT_OF_RANGE, GNOMEFE_ERR_OFFSET + 214,
	   "Some values are out of range:")
ResDef(GNOMEFE_VALUE_OUT_OF_RANGE, GNOMEFE_ERR_OFFSET + 215,
	   "The following value is out of range:")
ResDef(GNOMEFE_EDITOR_TABLE_ROW_RANGE, GNOMEFE_ERR_OFFSET + 216,
	   "You can have between 1 and 100 rows.")
ResDef(GNOMEFE_EDITOR_TABLE_COLUMN_RANGE, GNOMEFE_ERR_OFFSET + 217,
	   "You can have between 1 and 100 columns.")
ResDef(GNOMEFE_EDITOR_TABLE_BORDER_RANGE, GNOMEFE_ERR_OFFSET + 218,
	   "For border width, you can have 0 to 10000 pixels.")
ResDef(GNOMEFE_EDITOR_TABLE_SPACING_RANGE, GNOMEFE_ERR_OFFSET + 219,
	   "For cell spacing, you can have 0 to 10000 pixels.")
ResDef(GNOMEFE_EDITOR_TABLE_PADDING_RANGE, GNOMEFE_ERR_OFFSET + 220,
	   "For cell padding, you can have 0 to 10000 pixels.")
ResDef(GNOMEFE_EDITOR_TABLE_WIDTH_RANGE, GNOMEFE_ERR_OFFSET + 221,
	   "For width, you can have between 1 and 10000 pixels,\n"
	   "or between 1 and 100%.")
ResDef(GNOMEFE_EDITOR_TABLE_HEIGHT_RANGE, GNOMEFE_ERR_OFFSET + 222,
	   "For height, you can have between 1 and 10000 pixels,\n"
	   "or between 1 and 100%.")
ResDef(GNOMEFE_EDITOR_TABLE_IMAGE_WIDTH_RANGE, GNOMEFE_ERR_OFFSET + 223,
	   "For width, you can have between 1 and 10000 pixels.")
ResDef(GNOMEFE_EDITOR_TABLE_IMAGE_HEIGHT_RANGE, GNOMEFE_ERR_OFFSET + 224,
	   "For height, you can have between 1 and 10000 pixels.")
ResDef(GNOMEFE_EDITOR_TABLE_IMAGE_SPACE_RANGE, GNOMEFE_ERR_OFFSET + 225,
	   "For space, you can have between 1 and 10000 pixels.")
ResDef(GNOMEFE_ENTER_NEW_VALUE, GNOMEFE_ERR_OFFSET + 226,
	   "Please enter a new value and try again.")
ResDef(GNOMEFE_ENTER_NEW_VALUES, GNOMEFE_ERR_OFFSET + 227,
	   "Please enter new values and try again.")
ResDef(GNOMEFE_EDITOR_LINK_TEXT_LABEL_NEW, GNOMEFE_ERR_OFFSET + 228,
	   "Enter the text of the link:")
ResDef(GNOMEFE_EDITOR_LINK_TEXT_LABEL_IMAGE, GNOMEFE_ERR_OFFSET + 229,
	   "Linked image:")
ResDef(GNOMEFE_EDITOR_LINK_TEXT_LABEL_TEXT, GNOMEFE_ERR_OFFSET + 230,
	   "Linked text:")
ResDef(GNOMEFE_EDITOR_LINK_TARGET_LABEL_NO_TARGETS, GNOMEFE_ERR_OFFSET + 231,
	   "No targets in the selected document")
ResDef(GNOMEFE_EDITOR_LINK_TARGET_LABEL_SPECIFIED, GNOMEFE_ERR_OFFSET + 232,
	   "Link to a named target in the specified document (optional).")
ResDef(GNOMEFE_EDITOR_LINK_TARGET_LABEL_CURRENT, GNOMEFE_ERR_OFFSET + 233,
	   "Link to a named target in the current document (optional).")
ResDef(GNOMEFE_EDITOR_WARNING_REMOVE_LINK, GNOMEFE_ERR_OFFSET + 234,
	   "Do you want to remove the link?")
ResDef(GNOMEFE_UNKNOWN, GNOMEFE_ERR_OFFSET + 235,
	   "<unknown>")
ResDef(GNOMEFE_EDITOR_TAG_UNOPENED, GNOMEFE_ERR_OFFSET + 236,
	   "Unopened Tag: '<' was expected")
ResDef(GNOMEFE_EDITOR_TAG_UNCLOSED, GNOMEFE_ERR_OFFSET + 237,
	   "Unopened Tag:  '>' was expected")
ResDef(GNOMEFE_EDITOR_TAG_UNTERMINATED_STRING, GNOMEFE_ERR_OFFSET + 238,
	   "Unterminated String in tag: closing quote expected")
ResDef(GNOMEFE_EDITOR_TAG_PREMATURE_CLOSE, GNOMEFE_ERR_OFFSET + 239,
	   "Premature close of tag")
ResDef(GNOMEFE_EDITOR_TAG_TAGNAME_EXPECTED, GNOMEFE_ERR_OFFSET + 240,
	   "Tagname was expected")
ResDef(GNOMEFE_EDITOR_TAG_UNKNOWN, GNOMEFE_ERR_OFFSET + 241,
	   "Unknown tag error")
ResDef(GNOMEFE_EDITOR_TAG_OK, GNOMEFE_ERR_OFFSET + 242,
	   "Tag seems ok")
ResDef(GNOMEFE_EDITOR_ALERT_FRAME_DOCUMENT, GNOMEFE_ERR_OFFSET + 243,
	   "This document contains frames. Currently the editor\n"
	   "cannot edit documents which contain frames.")
ResDef(GNOMEFE_EDITOR_ALERT_ABOUT_DOCUMENT, GNOMEFE_ERR_OFFSET + 244,
	   "This document is an about document. The editor\n"
	   "cannot edit about documents.")
ResDef(GNOMEFE_ALERT_SAVE_CHANGES, GNOMEFE_ERR_OFFSET + 245,
	   "You must save this document locally before\n"
	   "continuing with the requested action.")
ResDef(GNOMEFE_WARNING_SAVE_CHANGES, GNOMEFE_ERR_OFFSET + 246,
	   "Do you want to save changes to:\n%.900s?")
ResDef(GNOMEFE_ERROR_GENERIC_ERROR_CODE, GNOMEFE_ERR_OFFSET + 247,
	   "The error code = (%d).")
ResDef(GNOMEFE_EDITOR_COPY_DOCUMENT_BUSY, GNOMEFE_ERR_OFFSET + 248,
	   "Cannot copy or cut at this time, try again later.")
ResDef(GNOMEFE_EDITOR_COPY_SELECTION_EMPTY, GNOMEFE_ERR_OFFSET + 249,
	   "Nothing is selected.")
ResDef(GNOMEFE_EDITOR_COPY_SELECTION_CROSSES_TABLE_DATA_CELL, GNOMEFE_ERR_OFFSET + 250,
	   "The selection includes a table cell boundary.\n"
	   "Deleting and copying are not allowed.")
ResDef(GNOMEFE_COMMAND_EMPTY, GNOMEFE_ERR_OFFSET + 251,
	   "Empty command specified!")
ResDef(GNOMEFE_EDITOR_HTML_EDITOR_COMMAND_EMPTY, GNOMEFE_ERR_OFFSET + 252,
	   "No html editor command has been specified in Editor Preferences.\n"
	   "Specify the file argument with %f. Mozilla will replace %f with\n"
	   "the correct file name. Example:\n"
	   "             xterm -e vi %f\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(GNOMEFE_ACTION_SYNTAX_ERROR, GNOMEFE_ERR_OFFSET + 253,
	   "Syntax error in action handler: %s")
ResDef(GNOMEFE_ACTION_WRONG_CONTEXT, GNOMEFE_ERR_OFFSET + 254,
	   "Invalid window type for action handler: %s")
ResDef(GNOMEFE_EKIT_ABOUT_MESSAGE, GNOMEFE_ERR_OFFSET + 255,
	   "Modified by the Mozilla Navigator Administration Kit.\n"
           "Version: %s\n"
           "User agent: C")

ResDef(GNOMEFE_PRIVATE_MIMETYPE_RELOAD, GNOMEFE_ERR_OFFSET + 256,
 "Private Mimetype File (%s) has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(GNOMEFE_PRIVATE_MAILCAP_RELOAD, GNOMEFE_ERR_OFFSET + 257,
 "Private Mailcap File (%s) has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(GNOMEFE_PRIVATE_RELOADED_MIMETYPE, GNOMEFE_ERR_OFFSET + 258,
 "Private Mimetype File (%s) has changed on disk and is being reloaded.")
ResDef(GNOMEFE_PRIVATE_RELOADED_MAILCAP, GNOMEFE_ERR_OFFSET + 259,
 "Private Mailcap File (%s) has changed on disk and is being reloaded.")
ResDef(GNOMEFE_EDITOR_IMAGE_EDITOR_COMMAND_EMPTY, GNOMEFE_ERR_OFFSET + 260,
	   "No image editor command has been specified in Editor Preferences.\n"
	   "Specify the file argument with %f. Mozilla will replace %f with\n"
	   "the correct file name. Example:\n"
	   "             xgifedit %f\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(GNOMEFE_ERROR_COPYRIGHT_HINT, GNOMEFE_ERR_OFFSET + 261,
 "You are about to download a remote\n"
 "document or image.\n"
 "You should get permission to use any\n"
 "copyrighted images or documents.")
ResDef(GNOMEFE_ERROR_READ_ONLY, GNOMEFE_ERR_OFFSET + 262,
	   "The file is marked read-only.")
ResDef(GNOMEFE_ERROR_BLOCKED, GNOMEFE_ERR_OFFSET + 263,
	   "The file is blocked at this time. Try again later.")
ResDef(GNOMEFE_ERROR_BAD_URL, GNOMEFE_ERR_OFFSET + 264,
	   "The file URL is badly formed.")
ResDef(GNOMEFE_ERROR_FILE_OPEN, GNOMEFE_ERR_OFFSET + 265,
	   "Error opening file for writing.")
ResDef(GNOMEFE_ERROR_FILE_WRITE, GNOMEFE_ERR_OFFSET + 266,
	   "Error writing to the file.")
ResDef(GNOMEFE_ERROR_CREATE_BAKNAME, GNOMEFE_ERR_OFFSET + 267,
	   "Error creating the temporary backup file.")
ResDef(GNOMEFE_ERROR_DELETE_BAKFILE, GNOMEFE_ERR_OFFSET + 268,
	   "Error deleting the temporary backup file.")
ResDef(GNOMEFE_WARNING_SAVE_CONTINUE, GNOMEFE_ERR_OFFSET + 269,
	   "Continue saving document?")
ResDef(GNOMEFE_ERROR_WRITING_FILE, GNOMEFE_ERR_OFFSET + 270,
	   "There was an error while saving the file:\n%.900s")
ResDef(GNOMEFE_EDITOR_DOCUMENT_TEMPLATE_EMPTY, GNOMEFE_ERR_OFFSET + 271,
	   "The new document template location is not set.\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(GNOMEFE_EDITOR_AUTOSAVE_PERIOD_RANGE, GNOMEFE_ERR_OFFSET + 272,
	   "Please enter an autosave period between 0 and 600 minutes.")
ResDef(GNOMEFE_EDITOR_BROWSE_LOCATION_EMPTY, GNOMEFE_ERR_OFFSET + 273,
	   "The default browse location is not set.\n"
	   "Would you like to enter a value in Editor Preferences now?")
ResDef(GNOMEFE_EDITOR_PUBLISH_LOCATION_INVALID, GNOMEFE_ERR_OFFSET + 274,
	   "Publish destination must begin with \"ftp://\", \"http://\", or \"https://\".\n"
	   "Please enter new values and try again.")
ResDef(GNOMEFE_EDITOR_IMAGE_IS_REMOTE, GNOMEFE_ERR_OFFSET + 275,
	   "Image is at a remote location.\n"
	   "Please save image locally before editing.")
ResDef(GNOMEFE_COLORMAP_WARNING_TO_IGNORE, GNOMEFE_ERR_OFFSET + 276,
	   "cannot allocate colormap")
ResDef(GNOMEFE_UPLOADING_FILE, GNOMEFE_ERR_OFFSET + 277,
	   "Uploading file to remote server:\n%.900s")
ResDef(GNOMEFE_SAVING_FILE, GNOMEFE_ERR_OFFSET + 278,
	   "Saving file to local disk:\n%.900s")
ResDef(GNOMEFE_LOADING_IMAGE_FILE, GNOMEFE_ERR_OFFSET + 279,
	   "Loading image file:\n%.900s")
ResDef(GNOMEFE_FILE_N_OF_N, GNOMEFE_ERR_OFFSET + 280,
	   "File %d of %d")
ResDef(GNOMEFE_ERROR_SRC_NOT_FOUND, GNOMEFE_ERR_OFFSET + 281,
	   "Source not found.")
ResDef(GNOMEFE_WARNING_AUTO_SAVE_NEW_MSG, GNOMEFE_ERR_OFFSET + 282,
	   "Press Cancel to turn off AutoSave until you\n"
	   "save this document later.")
ResDef(GNOMEFE_ALL_NEWSGROUP_TAB, GNOMEFE_ERR_OFFSET + 283,
	   "All Groups")
ResDef(GNOMEFE_SEARCH_FOR_NEWSGROUP_TAB, GNOMEFE_ERR_OFFSET + 284,
           "Search for a Group")
ResDef(GNOMEFE_NEW_NEWSGROUP_TAB, GNOMEFE_ERR_OFFSET + 285,
           "New Groups")
ResDef(GNOMEFE_NEW_NEWSGROUP_TAB_INFO_MSG, GNOMEFE_ERR_OFFSET + 286,
           "This list shows the accumulated list of new discussion groups since\n"
           "the last time you pressed the Clear New button.")
ResDef(GNOMEFE_SILLY_NAME_FOR_SEEMINGLY_UNAMEABLE_THING, GNOMEFE_ERR_OFFSET + 287,
           "Message Center for %s")
ResDef(GNOMEFE_FOLDER_ON_LOCAL_MACHINE, GNOMEFE_ERR_OFFSET + 288,
           "on local machine.")
ResDef(GNOMEFE_FOLDER_ON_SERVER, GNOMEFE_ERR_OFFSET + 289,
           "on server.")
ResDef(GNOMEFE_MESSAGE, GNOMEFE_ERR_OFFSET + 290,
       "Message:")
ResDef(GNOMEFE_MESSAGE_SUBTITLE, GNOMEFE_ERR_OFFSET + 291,
       "'%s' from %s in %s Folder")
ResDef(GNOMEFE_WINDOW_TITLE_NEWSGROUP, GNOMEFE_ERR_OFFSET + 292,
       "Mozilla Group - [ %s ]")
ResDef(GNOMEFE_WINDOW_TITLE_FOLDER, GNOMEFE_ERR_OFFSET + 293,
       "Mozilla Folder - [ %s ]")
ResDef(GNOMEFE_MAIL_PRIORITY_LOWEST, GNOMEFE_ERR_OFFSET + 294,
       "Lowest")
ResDef(GNOMEFE_MAIL_PRIORITY_LOW, GNOMEFE_ERR_OFFSET + 295,
       "Low")
ResDef(GNOMEFE_MAIL_PRIORITY_NORMAL, GNOMEFE_ERR_OFFSET + 296,
       "Normal")
ResDef(GNOMEFE_MAIL_PRIORITY_HIGH, GNOMEFE_ERR_OFFSET + 297,
       "High")
ResDef(GNOMEFE_MAIL_PRIORITY_HIGHEST, GNOMEFE_ERR_OFFSET + 298,
       "Highest")
ResDef(GNOMEFE_SIZE_IN_BYTES, GNOMEFE_ERR_OFFSET + 299,
       "Size")
ResDef(GNOMEFE_SIZE_IN_LINES, GNOMEFE_ERR_OFFSET + 300,
       "Lines")

ResDef(GNOMEFE_AB_NAME_GENERAL_TAB, GNOMEFE_ERR_OFFSET + 301,
       "Name")
ResDef(GNOMEFE_AB_NAME_CONTACT_TAB, GNOMEFE_ERR_OFFSET + 302,
       "Contact")
ResDef(GNOMEFE_AB_NAME_SECURITY_TAB, GNOMEFE_ERR_OFFSET + 303,
       "Security")
ResDef(GNOMEFE_AB_NAME_COOLTALK_TAB, GNOMEFE_ERR_OFFSET + 304,
       "Mozilla Conference")
ResDef(GNOMEFE_AB_FIRSTNAME, GNOMEFE_ERR_OFFSET + 305,
       "First Name:")
ResDef(GNOMEFE_AB_LASTNAME, GNOMEFE_ERR_OFFSET + 306,
       "Last Name:")
ResDef(GNOMEFE_AB_EMAIL, GNOMEFE_ERR_OFFSET + 307,
       "Email Address:")
ResDef(GNOMEFE_AB_NICKNAME, GNOMEFE_ERR_OFFSET + 308,
       "Nickname:")
ResDef(GNOMEFE_AB_NOTES, GNOMEFE_ERR_OFFSET + 309,
       "Notes:")
ResDef(GNOMEFE_AB_PREFHTML, GNOMEFE_ERR_OFFSET + 310,
       "Prefers to receive rich text (HTML) mail")
ResDef(GNOMEFE_AB_COMPANY, GNOMEFE_ERR_OFFSET + 311,
       "Organization:")
ResDef(GNOMEFE_AB_TITLE, GNOMEFE_ERR_OFFSET + 312,
       "Title:")
ResDef(GNOMEFE_AB_ADDRESS, GNOMEFE_ERR_OFFSET + 313,
       "Address:")
ResDef(GNOMEFE_AB_CITY, GNOMEFE_ERR_OFFSET + 314,
       "City:")
ResDef(GNOMEFE_AB_STATE, GNOMEFE_ERR_OFFSET + 315,
       "State:")
ResDef(GNOMEFE_AB_ZIP, GNOMEFE_ERR_OFFSET + 316,
       "Zip:")
ResDef(GNOMEFE_AB_COUNTRY, GNOMEFE_ERR_OFFSET + 317,
       "Country:")

ResDef(GNOMEFE_AB_WORKPHONE, GNOMEFE_ERR_OFFSET + 318,
       "Work:")
ResDef(GNOMEFE_AB_FAX, GNOMEFE_ERR_OFFSET + 319,
       "Fax:")
ResDef(GNOMEFE_AB_HOMEPHONE, GNOMEFE_ERR_OFFSET + 320,
       "Home:")

ResDef(GNOMEFE_AB_SECUR_NO, GNOMEFE_ERR_OFFSET + 321,
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
ResDef(GNOMEFE_AB_SECUR_YES, GNOMEFE_ERR_OFFSET + 322,
       "You have this person's Security Certificate.\n"
	   "\n"
	   "This means that when you send this person email, it can be encrypted.\n"
	   "Encrypting a message is like sending it in an envelope, rather than a\n"
	   "postcard. It makes it difficult for other peope to view your message.\n"
	   "\n"
	   "This person's Security Certificate will expire on %s. When it\n"
	   "expires, you will no longer be able to send encrypted mail to this\n"
	   "person until they send you a new one.")
ResDef(GNOMEFE_AB_SECUR_EXPIRED, GNOMEFE_ERR_OFFSET + 323,
       "This person's Security Certificate has Expired.\n"
	   "\n"
	   "You cannot send this person encrypted mail until you obtain a new\n"
	   "Security Certificate for them. To do this, have this person send you a\n"
	   "signed mail message. This will automatically include the new Security\n"
	   "Certificate.")
ResDef(GNOMEFE_AB_SECUR_REVOKED, GNOMEFE_ERR_OFFSET + 324,
       "This person's Security Certificate has been revoked. This means that\n"
	   "the Certificate may have been lost or stolen.\n"
	   "\n"
	   "You cannot send this person encrypted mail until you obtain a new\n"
	   "Security Certificate. To do this, have this person send you a signed\n"
	   "mail message. This will automatically include the new Security\n"
	   "Certificate.")
ResDef(GNOMEFE_AB_SECUR_YOU_NO, GNOMEFE_ERR_OFFSET + 325,
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
ResDef(GNOMEFE_AB_SECUR_YOU_YES, GNOMEFE_ERR_OFFSET + 326,
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
ResDef(GNOMEFE_AB_SECUR_YOU_EXPIRED, GNOMEFE_ERR_OFFSET + 327,
       "Your Security Certificate has Expired.\n"
	   "\n"
	   "This means that you can no longer digitally sign messages with this\n"
	   "certificate. You can continue to receive encrypted mail, however.\n"
	   "\n"
	   "This means that you must obtain another Certificate. Click on Show\n"
	   "Certificate to do so.")
ResDef(GNOMEFE_AB_SECUR_YOU_REVOKED, GNOMEFE_ERR_OFFSET + 328,
       "Your Security Certificate has been revoked.\n"
	   "This means that you can no longer digitally sign messages with this\n"
	   "certificate. You can continue to receive encrypted mail, however.\n"
	   "\n"
	   "This means that you must obtain another Certificate.")

ResDef(GNOMEFE_AB_SECUR_SHOW, GNOMEFE_ERR_OFFSET + 329,
       "Show Certificate")

ResDef(GNOMEFE_AB_SECUR_GET, GNOMEFE_ERR_OFFSET + 330,
       "Get Certificate")


ResDef(GNOMEFE_AB_MLIST_TITLE, GNOMEFE_ERR_OFFSET + 331,
       "Mailing List Info")
ResDef(GNOMEFE_AB_MLIST_LISTNAME, GNOMEFE_ERR_OFFSET + 332,
       "List Name:")
ResDef(GNOMEFE_AB_MLIST_NICKNAME, GNOMEFE_ERR_OFFSET + 333,
       "List Nickname:")
ResDef(GNOMEFE_AB_MLIST_DESCRIPTION, GNOMEFE_ERR_OFFSET + 334,
       "Description:")
ResDef(GNOMEFE_AB_MLIST_PROMPT, GNOMEFE_ERR_OFFSET + 335,
       "To add entries to this mailing list, type names from the address book")

ResDef(GNOMEFE_AB_HEADER_NAME, GNOMEFE_ERR_OFFSET + 336,
       "Name")
ResDef(GNOMEFE_AB_HEADER_CERTIFICATE, GNOMEFE_ERR_OFFSET + 337,
       "")
ResDef(GNOMEFE_AB_HEADER_EMAIL, GNOMEFE_ERR_OFFSET + 338,
       "Email Address")
ResDef(GNOMEFE_AB_HEADER_NICKNAME, GNOMEFE_ERR_OFFSET + 339,
       "Nickname")
ResDef(GNOMEFE_AB_HEADER_COMPANY, GNOMEFE_ERR_OFFSET + 340,
       "Organization")
ResDef(GNOMEFE_AB_HEADER_LOCALITY, GNOMEFE_ERR_OFFSET + 341,
       "City")
ResDef(GNOMEFE_AB_HEADER_STATE, GNOMEFE_ERR_OFFSET + 342,
       "State")

ResDef(GNOMEFE_MN_UNREAD_AND_TOTAL, GNOMEFE_ERR_OFFSET + 343,
       "%d Unread, %d Total")

ResDef(GNOMEFE_AB_SEARCH, GNOMEFE_ERR_OFFSET + 344,
       "Search")
ResDef(GNOMEFE_AB_STOP, GNOMEFE_ERR_OFFSET + 345,
       "Stop")

ResDef(GNOMEFE_AB_REMOVE, GNOMEFE_ERR_OFFSET + 346,
       "Remove")

ResDef(GNOMEFE_AB_ADDRESSEE_PROMPT, GNOMEFE_ERR_OFFSET + 347,
       "This message will be sent to:")

ResDef(GNOMEFE_OFFLINE_NEWS_ALL_MSGS, GNOMEFE_ERR_OFFSET + 348,
       "all")
ResDef(GNOMEFE_OFFLINE_NEWS_ONE_MONTH_AGO, GNOMEFE_ERR_OFFSET + 349,
       "1 month ago")

ResDef(GNOMEFE_MN_DELIVERY_IN_PROGRESS, GNOMEFE_ERR_OFFSET + 350,
       "Attachment operation cannot be completed now.\nMessage delivery or attachment load is in progress.")
ResDef(GNOMEFE_MN_ITEM_ALREADY_ATTACHED, GNOMEFE_ERR_OFFSET + 351,
       "This item is already attached:\n%s")
ResDef(GNOMEFE_MN_TOO_MANY_ATTACHMENTS, GNOMEFE_ERR_OFFSET + 352,
       "Attachment panel is full - no more attachments can be added.")
ResDef(GNOMEFE_DOWNLOADING_NEW_MESSAGES, GNOMEFE_ERR_OFFSET + 353,
	   "Getting new messages...")

ResDef(GNOMEFE_AB_FRAME_TITLE, GNOMEFE_ERR_OFFSET + 354,
           "Communicator Address Book for %s")

ResDef(GNOMEFE_AB_SHOW_CERTI, GNOMEFE_ERR_OFFSET + 355,
           "Show Certificate")

ResDef(GNOMEFE_LANG_COL_ORDER, GNOMEFE_ERR_OFFSET + 356,
       "Order")
ResDef(GNOMEFE_LANG_COL_LANG, GNOMEFE_ERR_OFFSET + 357,
       "Language")

ResDef(GNOMEFE_MFILTER_INFO, GNOMEFE_ERR_OFFSET + 358,
	   "Filters will be applied to incoming mail in the\n"
	   "following order:")


ResDef(GNOMEFE_AB_COOLTALK_INFO, GNOMEFE_ERR_OFFSET + 359,
       "To call another person using Mozilla Conference, you must first\n"
       "choose the server you would like to use to look up that person's\n"
	   "address.")
ResDef(GNOMEFE_AB_COOLTALK_DEF_SERVER, GNOMEFE_ERR_OFFSET + 360,
       "Mozilla Conference DLS Server")
ResDef(GNOMEFE_AB_COOLTALK_SERVER_IK_HOST, GNOMEFE_ERR_OFFSET + 361,
       "Specific DLS Server")
ResDef(GNOMEFE_AB_COOLTALK_DIRECTIP, GNOMEFE_ERR_OFFSET + 362,
       "Hostname or IP Address")

ResDef(GNOMEFE_AB_COOLTALK_ADDR_LABEL, GNOMEFE_ERR_OFFSET + 363,
       "Address:")
ResDef(GNOMEFE_AB_COOLTALK_ADDR_EXAMPLE, GNOMEFE_ERR_OFFSET + 364,
       "(For example, %s)")

ResDef(GNOMEFE_AB_NAME_CARD_FOR, GNOMEFE_ERR_OFFSET + 365,
       "Card for <%s>")
ResDef(GNOMEFE_AB_NAME_NEW_CARD, GNOMEFE_ERR_OFFSET + 366,
       "New Card")

ResDef(GNOMEFE_MARKBYDATE_CAPTION, GNOMEFE_ERR_OFFSET + 367,
	   "Mark Messages Read")
ResDef(GNOMEFE_MARKBYDATE, GNOMEFE_ERR_OFFSET + 368,
	   "Mark messages read up to: (MM/DD/YY)")
ResDef(GNOMEFE_DATE_MUST_BE_MM_DD_YY, GNOMEFE_ERR_OFFSET + 369,
	   "The date must be valid,\nand must be in the form MM/DD/YY.")
ResDef(GNOMEFE_THERE_ARE_N_ARTICLES, GNOMEFE_ERR_OFFSET + 370,
	   "There are %d new message headers to download for\nthis discussion group.")
ResDef(GNOMEFE_GET_NEXT_N_MSGS, GNOMEFE_ERR_OFFSET + 371,
	   "Next %d")

ResDef(GNOMEFE_OFFLINE_NEWS_UNREAD_MSGS, GNOMEFE_ERR_OFFSET + 372,
       "unread")
ResDef(GNOMEFE_OFFLINE_NEWS_YESTERDAY, GNOMEFE_ERR_OFFSET + 373,
       "yesterday")
ResDef(GNOMEFE_OFFLINE_NEWS_ONE_WK_AGO, GNOMEFE_ERR_OFFSET + 374,
       "1 week ago")
ResDef(GNOMEFE_OFFLINE_NEWS_TWO_WKS_AGO, GNOMEFE_ERR_OFFSET + 375,
       "2 weeks ago")
ResDef(GNOMEFE_OFFLINE_NEWS_SIX_MONTHS_AGO, GNOMEFE_ERR_OFFSET + 376,
       "6 months ago")
ResDef(GNOMEFE_OFFLINE_NEWS_ONE_YEAR_AGO, GNOMEFE_ERR_OFFSET + 377,
       "1 year ago")

ResDef(GNOMEFE_ADDR_ENTRY_ALREADY_EXISTS, GNOMEFE_ERR_OFFSET + 378,
       "An Address Book entry with this name and email address already exists.")

ResDef (GNOMEFE_ADDR_ADD_PERSON_TO_ABOOK, GNOMEFE_ERR_OFFSET + 379, 
		"Mailing lists can only contain entries from the Personal Address Book.\n"
		"Would you like to add %s to the address book?")

ResDef (GNOMEFE_MAKE_SURE_SERVER_AND_PORT_ARE_VALID, GNOMEFE_ERR_OFFSET + 380,
		"Make sure both the server name and port are filled in and are valid.")

ResDef(GNOMEFE_UNKNOWN_EMAIL_ADDR, GNOMEFE_ERR_OFFSET + 381,
       "unknown")

ResDef(GNOMEFE_AB_PICKER_TO, GNOMEFE_ERR_OFFSET + 382,
       "To:")

ResDef(GNOMEFE_AB_PICKER_CC, GNOMEFE_ERR_OFFSET + 383,
       "CC:")

ResDef(GNOMEFE_AB_PICKER_BCC, GNOMEFE_ERR_OFFSET + 384,
       "BCC:")

ResDef(GNOMEFE_AB_SEARCH_PROMPT, GNOMEFE_ERR_OFFSET + 385,
       "Type Name")

ResDef(GNOMEFE_MN_NEXT_500, GNOMEFE_ERR_OFFSET + 386,
       "Next %d")

ResDef(GNOMEFE_MN_INVALID_ATTACH_URL, GNOMEFE_ERR_OFFSET + 387,
       "This document cannot be attached to a message:\n%s")

ResDef(GNOMEFE_PREFS_CR_CARD, GNOMEFE_ERR_OFFSET + 388,
       "Mozilla Communicator cannot find your\n"
	   "Personal Card. Would you like to create a\n"
	   "Personal Card now?")

ResDef(GNOMEFE_MN_FOLDER_TITLE, GNOMEFE_ERR_OFFSET + 389,
       "Communicator Message Center for %s")

ResDef(GNOMEFE_PREFS_LABEL_STYLE_PLAIN, GNOMEFE_ERR_OFFSET + 390, "Plain")
ResDef(GNOMEFE_PREFS_LABEL_STYLE_BOLD, GNOMEFE_ERR_OFFSET + 391, "Bold")
ResDef(GNOMEFE_PREFS_LABEL_STYLE_ITALIC, GNOMEFE_ERR_OFFSET + 392, "Italic")
ResDef(GNOMEFE_PREFS_LABEL_STYLE_BOLDITALIC, GNOMEFE_ERR_OFFSET + 393, "Bold Italic")
ResDef(GNOMEFE_PREFS_LABEL_SIZE_NORMAL, GNOMEFE_ERR_OFFSET + 394, "Normal")
ResDef(GNOMEFE_PREFS_LABEL_SIZE_BIGGER, GNOMEFE_ERR_OFFSET + 395, "Bigger")
ResDef(GNOMEFE_PREFS_LABEL_SIZE_SMALLER, GNOMEFE_ERR_OFFSET + 396, "Smaller")
ResDef(GNOMEFE_PREFS_MAIL_FOLDER_SENT, GNOMEFE_ERR_OFFSET + 397, "Sent")

ResDef(GNOMEFE_MNC_CLOSE_WARNING, GNOMEFE_ERR_OFFSET + 398,
       "Message has not been sent. Do you want to\n"
       "save the message in the Drafts Folder?")

ResDef(GNOMEFE_SEARCH_INVALID_DATE, GNOMEFE_ERR_OFFSET + 399,
       "Invalid Date Value. No search is performed.")

ResDef(GNOMEFE_SEARCH_INVALID_MONTH, GNOMEFE_ERR_OFFSET + 400,
       "Invalid value for the MONTH field.\n"
       "Please enter month in 2 digits (1-12).\n"
       "Try again!")

ResDef(GNOMEFE_SEARCH_INVALID_DAY, GNOMEFE_ERR_OFFSET + 401,
       "Invalid value for the DAY of the month field.\n"
       "Please enter date in 2 digits (1-31).\n"
       "Try again!")

ResDef(GNOMEFE_SEARCH_INVALID_YEAR, GNOMEFE_ERR_OFFSET + 402,
       "Invalid value for the YEAR field.\n"
       "Please enter year in 4 digits (e.g. 1997).\n"
       "Year value has to be 1900 or later.\n"
       "Try again!")

ResDef(GNOMEFE_MNC_ADDRESS_TO, GNOMEFE_ERR_OFFSET + 403,
	"To:")
ResDef(GNOMEFE_MNC_ADDRESS_CC, GNOMEFE_ERR_OFFSET + 404,
	"Cc:")
ResDef(GNOMEFE_MNC_ADDRESS_BCC, GNOMEFE_ERR_OFFSET + 405,
	"Bcc:")
ResDef(GNOMEFE_MNC_ADDRESS_NEWSGROUP, GNOMEFE_ERR_OFFSET + 406,
	"Newsgroup:")
ResDef(GNOMEFE_MNC_ADDRESS_REPLY, GNOMEFE_ERR_OFFSET + 407,
	"Reply To:")
ResDef(GNOMEFE_MNC_ADDRESS_FOLLOWUP, GNOMEFE_ERR_OFFSET + 408,
	"Followup To:")

ResDef(GNOMEFE_MNC_OPTION_HIGHEST, GNOMEFE_ERR_OFFSET + 409,
	"Highest")
ResDef(GNOMEFE_MNC_OPTION_HIGH, GNOMEFE_ERR_OFFSET + 410,
	"High")
ResDef(GNOMEFE_MNC_OPTION_NORMAL, GNOMEFE_ERR_OFFSET + 411,
	"Normal")
ResDef(GNOMEFE_MNC_OPTION_LOW, GNOMEFE_ERR_OFFSET + 412,
	"Highest")
ResDef(GNOMEFE_MNC_OPTION_LOWEST, GNOMEFE_ERR_OFFSET + 413,
	"Lowest")
ResDef(GNOMEFE_MNC_ADDRESS, GNOMEFE_ERR_OFFSET + 414,
	"Address")
ResDef(GNOMEFE_MNC_ATTACHMENT, GNOMEFE_ERR_OFFSET + 415,
	"Attachment")
ResDef(GNOMEFE_MNC_OPTION, GNOMEFE_ERR_OFFSET + 416,
	"Option")


ResDef(GNOMEFE_DLG_OK, GNOMEFE_ERR_OFFSET + 417, "OK")
ResDef(GNOMEFE_DLG_CLEAR, GNOMEFE_ERR_OFFSET + 418, "Clear")
ResDef(GNOMEFE_DLG_CANCEL, GNOMEFE_ERR_OFFSET + 419, "Cancel")

ResDef(GNOMEFE_PRI_URGENT, GNOMEFE_ERR_OFFSET + 420, "Urgent")
ResDef(GNOMEFE_PRI_IMPORTANT, GNOMEFE_ERR_OFFSET + 421, "Important")
ResDef(GNOMEFE_PRI_NORMAL, GNOMEFE_ERR_OFFSET + 422, "Normal")
ResDef(GNOMEFE_PRI_FYI, GNOMEFE_ERR_OFFSET + 423, "FYI")
ResDef(GNOMEFE_PRI_JUNK, GNOMEFE_ERR_OFFSET + 424, "Junk")

ResDef(GNOMEFE_PRI_PRIORITY, GNOMEFE_ERR_OFFSET + 425, "Priority")
ResDef(GNOMEFE_COMPOSE_LABEL, GNOMEFE_ERR_OFFSET + 426, "%sLabel")
ResDef(GNOMEFE_COMPOSE_ADDRESSING, GNOMEFE_ERR_OFFSET + 427, "Addressing")
ResDef(GNOMEFE_COMPOSE_ATTACHMENT, GNOMEFE_ERR_OFFSET + 428, "Attachment")
ResDef(GNOMEFE_COMPOSE_COMPOSE, GNOMEFE_ERR_OFFSET + 429, "Compose")
ResDef(GNOMEFE_SEARCH_ALLMAILFOLDERS, GNOMEFE_ERR_OFFSET + 430, "All Mail Folders")
ResDef(GNOMEFE_SEARCH_AllNEWSGROUPS, GNOMEFE_ERR_OFFSET + 431, "All Groups")
ResDef(GNOMEFE_SEARCH_LDAPDIRECTORY, GNOMEFE_ERR_OFFSET + 432, "LDAP Directory")
ResDef(GNOMEFE_SEARCH_LOCATION, GNOMEFE_ERR_OFFSET + 433, "Location")
ResDef(GNOMEFE_SEARCH_SUBJECT, GNOMEFE_ERR_OFFSET + 434, "Subject")
ResDef(GNOMEFE_SEARCH_SENDER, GNOMEFE_ERR_OFFSET + 435, "Sender")
ResDef(GNOMEFE_SEARCH_DATE, GNOMEFE_ERR_OFFSET + 436, "Date")

ResDef(GNOMEFE_PREPARE_UPLOAD, GNOMEFE_ERR_OFFSET + 437,
	   "Preparing file to publish:\n%.900s")

	/* Bookmark outliner column headers */
ResDef(GNOMEFE_BM_OUTLINER_COLUMN_NAME, GNOMEFE_ERR_OFFSET + 438, "Name")
ResDef(GNOMEFE_BM_OUTLINER_COLUMN_LOCATION, GNOMEFE_ERR_OFFSET + 439, "Location")
ResDef(GNOMEFE_BM_OUTLINER_COLUMN_LASTVISITED, GNOMEFE_ERR_OFFSET + 440, "Last Visited")
ResDef(GNOMEFE_BM_OUTLINER_COLUMN_LASTMODIFIED, GNOMEFE_ERR_OFFSET + 441, "Last Modified")

	/* Folder outliner column headers */
ResDef(GNOMEFE_FOLDER_OUTLINER_COLUMN_NAME, GNOMEFE_ERR_OFFSET + 442, "Name")
ResDef(GNOMEFE_FOLDER_OUTLINER_COLUMN_TOTAL, GNOMEFE_ERR_OFFSET + 443, "Total")
ResDef(GNOMEFE_FOLDER_OUTLINER_COLUMN_UNREAD, GNOMEFE_ERR_OFFSET + 444, "Unread")

	/* Category outliner column headers */
ResDef(GNOMEFE_CATEGORY_OUTLINER_COLUMN_NAME, GNOMEFE_ERR_OFFSET + 445, "Category")

	/* Subscribe outliner column headers */
ResDef(GNOMEFE_SUB_OUTLINER_COLUMN_NAME, GNOMEFE_ERR_OFFSET + 446, "Group Name")
ResDef(GNOMEFE_SUB_OUTLINER_COLUMN_POSTINGS, GNOMEFE_ERR_OFFSET + 447, "Postings")

	/* Thread outliner column headers */
ResDef(GNOMEFE_THREAD_OUTLINER_COLUMN_SUBJECT, GNOMEFE_ERR_OFFSET + 448, "Subject")
ResDef(GNOMEFE_THREAD_OUTLINER_COLUMN_DATE, GNOMEFE_ERR_OFFSET + 449, "Date")
ResDef(GNOMEFE_THREAD_OUTLINER_COLUMN_PRIORITY, GNOMEFE_ERR_OFFSET + 450, "Priority")
ResDef(GNOMEFE_THREAD_OUTLINER_COLUMN_STATUS, GNOMEFE_ERR_OFFSET + 451, "Status")
ResDef(GNOMEFE_THREAD_OUTLINER_COLUMN_SENDER, GNOMEFE_ERR_OFFSET + 452, "Sender")
ResDef(GNOMEFE_THREAD_OUTLINER_COLUMN_RECIPIENT, GNOMEFE_ERR_OFFSET + 453, "Recipient")

ResDef(GNOMEFE_FOLDER_MENU_FILE_HERE, GNOMEFE_ERR_OFFSET + 454, "File Here")

	/* Splash screen messages */
ResDef(GNOMEFE_SPLASH_REGISTERING_CONVERTERS, GNOMEFE_ERR_OFFSET + 455, "Registering Converters")
ResDef(GNOMEFE_SPLASH_INITIALIZING_SECURITY_LIBRARY, GNOMEFE_ERR_OFFSET + 456, "Initializing Security Library")
ResDef(GNOMEFE_SPLASH_INITIALIZING_NETWORK_LIBRARY, GNOMEFE_ERR_OFFSET + 457, "Initializing Network Library")
ResDef(GNOMEFE_SPLASH_INITIALIZING_MESSAGE_LIBRARY, GNOMEFE_ERR_OFFSET + 458, "Initializing Message Library")
ResDef(GNOMEFE_SPLASH_INITIALIZING_IMAGE_LIBRARY, GNOMEFE_ERR_OFFSET + 459, "Initializing Image Library")
ResDef(GNOMEFE_SPLASH_INITIALIZING_MOCHA, GNOMEFE_ERR_OFFSET + 460, "Initializing Javascript")
ResDef(GNOMEFE_SPLASH_INITIALIZING_PLUGINS, GNOMEFE_ERR_OFFSET + 461, "Initializing Plugins")


	/* Display factory messages */
ResDef(GNOMEFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR, GNOMEFE_ERR_OFFSET + 462, "%s: installColormap: %s must be yes, no, or guess.\n")

ResDef(GNOMEFE_BM_FRAME_TITLE, GNOMEFE_ERR_OFFSET + 463,
           "Communicator Bookmarks for %s")

ResDef(GNOMEFE_UNTITLED, GNOMEFE_ERR_OFFSET + 464,"Untitled")

ResDef(GNOMEFE_HTML_NEWSGROUP_MSG, GNOMEFE_ERR_OFFSET + 465,
	   "Unchecking this option means that this group\n"
	   "and all discussion groups above it do not\n"
	   "receive HTML messages")
ResDef(GNOMEFE_SEC_ENCRYPTED, GNOMEFE_ERR_OFFSET + 466,
	   "Encrypted")
ResDef(GNOMEFE_SEC_NONE, GNOMEFE_ERR_OFFSET + 467,
	   "None")
ResDef(GNOMEFE_SHOW_COLUMN, GNOMEFE_ERR_OFFSET + 468,
	   "Show Column")
ResDef(GNOMEFE_HIDE_COLUMN, GNOMEFE_ERR_OFFSET + 469,
	   "Hide Column")

ResDef(GNOMEFE_DISABLED_BY_ADMIN, GNOMEFE_ERR_OFFSET + 470, "That functionality has been disabled")

ResDef(GNOMEFE_EDITOR_NEW_DOCNAME, GNOMEFE_ERR_OFFSET + 471, "file: Untitled")

ResDef(GNOMEFE_EMPTY_DIR, GNOMEFE_ERR_OFFSET + 472, "%s is not set.\n")
ResDef(GNOMEFE_NEWS_DIR, GNOMEFE_ERR_OFFSET + 473, "Discussion groups directory")
ResDef(GNOMEFE_MAIL_DIR, GNOMEFE_ERR_OFFSET + 474, "Local mail directory")
ResDef(GNOMEFE_DIR_DOES_NOT_EXIST, GNOMEFE_ERR_OFFSET + 475, "%s \"%.255s\" does not exist.\n")

ResDef(GNOMEFE_SEARCH_NO_MATCHES, GNOMEFE_ERR_OFFSET + 476, "No matches found")

ResDef(GNOMEFE_EMPTY_EMAIL_ADDR, GNOMEFE_ERR_OFFSET + 477, 
	   "Please enter a valid email address (e.g. user@internet.com).\n")

ResDef(GNOMEFE_HISTORY_FRAME_TITLE, GNOMEFE_ERR_OFFSET + 478,
           "Communicator History for %s")

ResDef(GNOMEFE_HISTORY_OUTLINER_COLUMN_TITLE, GNOMEFE_ERR_OFFSET + 479,
       "Title")
ResDef(GNOMEFE_HISTORY_OUTLINER_COLUMN_LOCATION, GNOMEFE_ERR_OFFSET + 480,
       "Location")
ResDef(GNOMEFE_HISTORY_OUTLINER_COLUMN_FIRSTVISITED, GNOMEFE_ERR_OFFSET + 481,
       "First Visited")
ResDef(GNOMEFE_HISTORY_OUTLINER_COLUMN_LASTVISITED, GNOMEFE_ERR_OFFSET + 482,
       "Last Visited")
ResDef(GNOMEFE_HISTORY_OUTLINER_COLUMN_EXPIRES, GNOMEFE_ERR_OFFSET + 483,
       "Expires")
ResDef(GNOMEFE_HISTORY_OUTLINER_COLUMN_VISITCOUNT, GNOMEFE_ERR_OFFSET + 484,
       "Visit Count")

ResDef(GNOMEFE_PLACE_CONFERENCE_CALL_TO, GNOMEFE_ERR_OFFSET + 485,
           "Place a call with Mozilla Conference to ")

ResDef(GNOMEFE_SEND_MSG_TO, GNOMEFE_ERR_OFFSET + 486,
           "Send a message to ")

ResDef(GNOMEFE_INBOX_DOESNT_EXIST, GNOMEFE_ERR_OFFSET + 487,
           "Default Inbox folder does not exist.\n"
	       "You will not be able to get new messages!")

ResDef(GNOMEFE_HELPERS_APP_TELNET, GNOMEFE_ERR_OFFSET + 488, "telnet")
ResDef(GNOMEFE_HELPERS_APP_TN3270, GNOMEFE_ERR_OFFSET + 489, "TN3270 application")
ResDef(GNOMEFE_HELPERS_APP_RLOGIN, GNOMEFE_ERR_OFFSET + 490, "rlogin")
ResDef(GNOMEFE_HELPERS_APP_RLOGIN_USER, GNOMEFE_ERR_OFFSET + 491, "rlogin with user")
ResDef(GNOMEFE_HELPERS_CANNOT_DEL_STATIC_APPS, GNOMEFE_ERR_OFFSET + 492, 
	   "You cannot delete this application from the preferences.")
ResDef(GNOMEFE_HELPERS_EMPTY_APP, GNOMEFE_ERR_OFFSET + 493, 
	   "The application field is not set.")


ResDef(GNOMEFE_JAVASCRIPT_APP,  GNOMEFE_ERR_OFFSET + 494, 
       "[JavaScript Application]")

ResDef(GNOMEFE_PREFS_UPGRADE, GNOMEFE_ERR_OFFSET + 495, 
	   "The preferences you have, version %s, is incompatible with the\n"
	   "current version %s. Would you like to save new preferences now?")

ResDef(GNOMEFE_PREFS_DOWNGRADE, GNOMEFE_ERR_OFFSET + 496, 
	   "Please be aware that the preferences you have, version %s,\n"
	   "is incompatible with the current version %s.")

ResDef(GNOMEFE_MOZILLA_WRONG_RESOURCE_FILE_VERSION, GNOMEFE_ERR_OFFSET + 497, 
	   "%s: program is version %s, but resources are version %s.\n\n"
	   "\tThis means that there is an inappropriate `%s' file installed\n"
	   "\tin the app-defaults directory.  Check these environment variables\n"
	   "\tand the directories to which they point:\n\n"
	   "	$XAPPLRESDIR\n"
	   "	$XFILESEARCHPATH\n"
	   "	$XUSERFILESEARCHPATH\n\n"
	   "\tAlso check for this file in your home directory, or in the\n"
	   "\tdirectory called `app-defaults' somewhere under /usr/lib/.")

ResDef(GNOMEFE_MOZILLA_CANNOT_FOND_RESOURCE_FILE, GNOMEFE_ERR_OFFSET + 498, 
	   "%s: couldn't find our resources?\n\n"
	   "\tThis could mean that there is an inappropriate `%s' file\n"
	   "\tinstalled in the app-defaults directory.  Check these environment\n"
	   "\tvariables and the directories to which they point:\n\n"
	   "	$XAPPLRESDIR\n"
	   "	$XFILESEARCHPATH\n"
	   "	$XUSERFILESEARCHPATH\n\n"
	   "\tAlso check for this file in your home directory, or in the\n"
	   "\tdirectory called `app-defaults' somewhere under /usr/lib/.")

ResDef(GNOMEFE_MOZILLA_LOCALE_NOT_SUPPORTED_BY_XLIB, GNOMEFE_ERR_OFFSET + 499, 
	   "%s: locale `%s' not supported by Xlib; trying `C'.\n")

ResDef(GNOMEFE_MOZILLA_LOCALE_C_NOT_SUPPORTED, GNOMEFE_ERR_OFFSET + 500, 
	   "%s: locale `C' not supported.\n")

ResDef(GNOMEFE_MOZILLA_LOCALE_C_NOT_SUPPORTED_EITHER, GNOMEFE_ERR_OFFSET + 501, 
	   "%s: locale `C' not supported either.\n")

ResDef(GNOMEFE_MOZILLA_NLS_LOSAGE, GNOMEFE_ERR_OFFSET + 502, 
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

ResDef(GNOMEFE_MOZILLA_NLS_PATH_NOT_SET_CORRECTLY, GNOMEFE_ERR_OFFSET + 503,
	   "	Perhaps the $XNLSPATH environment variable is not set correctly?\n")

ResDef(GNOMEFE_MOZILLA_UNAME_FAILED, GNOMEFE_ERR_OFFSET + 504,
	   " uname failed")

ResDef(GNOMEFE_MOZILLA_UNAME_FAILED_CANT_DETERMINE_SYSTEM, GNOMEFE_ERR_OFFSET + 505,
	   "%s: uname() failed; can't determine what system we're running on\n")

ResDef(GNOMEFE_MOZILLA_TRYING_TO_RUN_SUNOS_ON_SOLARIS, GNOMEFE_ERR_OFFSET + 506,
	   "%s: this is a SunOS 4.1.3 executable, and you are running it on\n"
	   "	SunOS %s (Solaris.)  It would be a very good idea for you to\n"
	   "	run the Solaris-specific binary instead.  Bad Things may happen.\n\n")

ResDef(GNOMEFE_MOZILLA_FAILED_TO_INITIALIZE_EVENT_QUEUE, GNOMEFE_ERR_OFFSET + 507,
	   "%s: failed to initialize mozilla_event_queue!\n")

ResDef(GNOMEFE_MOZILLA_INVALID_REMOTE_OPTION, GNOMEFE_ERR_OFFSET + 508,
	   "%s: invalid `-remote' option \"%s\"\n")

ResDef(GNOMEFE_MOZILLA_ID_OPTION_MUST_PRECEED_REMOTE_OPTIONS, GNOMEFE_ERR_OFFSET + 509,
	   "%s: the `-id' option must preceed all `-remote' options.\n")

ResDef(GNOMEFE_MOZILLA_ONLY_ONE_ID_OPTION_CAN_BE_USED, GNOMEFE_ERR_OFFSET + 510,
	   "%s: only one `-id' option may be used.\n")

ResDef(GNOMEFE_MOZILLA_INVALID_OD_OPTION, GNOMEFE_ERR_OFFSET + 511,
	   "%s: invalid `-id' option \"%s\"\n")

ResDef(GNOMEFE_MOZILLA_ID_OPTION_CAN_ONLY_BE_USED_WITH_REMOTE,GNOMEFE_ERR_OFFSET + 512,
	   "%s: the `-id' option may only be used with `-remote'.\n")

ResDef(GNOMEFE_MOZILLA_XKEYSYMDB_SET_BUT_DOESNT_EXIST, GNOMEFE_ERR_OFFSET + 513,
	   "%s: warning: $XKEYSYMDB is %s,\n"
	   "	but that file doesn't exist.\n")

ResDef(GNOMEFE_MOZILLA_NO_XKEYSYMDB_FILE_FOUND, GNOMEFE_ERR_OFFSET + 514,
	   "%s: warning: no XKeysymDB file in either\n"
	   "	%s, %s, or %s\n"
	   "	Please set $XKEYSYMDB to the correct XKeysymDB"
	   " file.\n")

ResDef(GNOMEFE_MOZILLA_NOT_FOUND_IN_PATH, GNOMEFE_ERR_OFFSET + 515,
	   "%s: not found in PATH!\n")

ResDef(GNOMEFE_MOZILLA_RENAMING_SOMETHING_TO_SOMETHING, GNOMEFE_ERR_OFFSET + 516,
	   "renaming %s to %s:")

ResDef(GNOMEFE_COMMANDS_OPEN_URL_USAGE, GNOMEFE_ERR_OFFSET + 517,
	   "%s: usage: OpenURL(url [ , new-window|window-name ] )\n")

ResDef(GNOMEFE_COMMANDS_OPEN_FILE_USAGE, GNOMEFE_ERR_OFFSET + 518,
	   "%s: usage: OpenFile(file)\n")

ResDef(GNOMEFE_COMMANDS_PRINT_FILE_USAGE, GNOMEFE_ERR_OFFSET + 519,
	   "%s: usage: print([filename])\n")

ResDef(GNOMEFE_COMMANDS_SAVE_AS_USAGE, GNOMEFE_ERR_OFFSET + 520,
	   "%s: usage: SaveAS(file, output-data-type)\n")

ResDef(GNOMEFE_COMMANDS_SAVE_AS_USAGE_TWO, GNOMEFE_ERR_OFFSET + 521,
	   "%s: usage: SaveAS(file, [output-data-type])\n")

ResDef(GNOMEFE_COMMANDS_MAIL_TO_USAGE, GNOMEFE_ERR_OFFSET + 522,
	   "%s: usage: mailto(address ...)\n")

ResDef(GNOMEFE_COMMANDS_FIND_USAGE, GNOMEFE_ERR_OFFSET + 523,
	   "%s: usage: find(string)\n")

ResDef(GNOMEFE_COMMANDS_ADD_BOOKMARK_USAGE, GNOMEFE_ERR_OFFSET + 524,
	   "%s: usage: addBookmark(url, title)\n")

ResDef(GNOMEFE_COMMANDS_HTML_HELP_USAGE, GNOMEFE_ERR_OFFSET + 525,
	   "%s: usage: htmlHelp(map-file, id, search-text)\n")

ResDef(GNOMEFE_COMMANDS_UNPARSABLE_ENCODING_FILTER_SPEC, GNOMEFE_ERR_OFFSET + 526,
	   "%s: unparsable encoding filter spec: %s\n")

ResDef(GNOMEFE_COMMANDS_UPLOAD_FILE, GNOMEFE_ERR_OFFSET + 527,
	   "Upload File")

ResDef(GNOMEFE_MOZILLA_ERROR_SAVING_OPTIONS, GNOMEFE_ERR_OFFSET + 528,
	   "error saving options")

ResDef(GNOMEFE_URLBAR_FILE_HEADER, GNOMEFE_ERR_OFFSET + 529,
	   "# Mozilla User History File\n"
	   "# Version: %s\n"
	   "# This is a generated file!  Do not edit.\n"
	   "\n")

ResDef(GNOMEFE_LAY_TOO_MANY_ARGS_TO_ACTIVATE_LINK_ACTION, GNOMEFE_ERR_OFFSET + 530,
	   "%s: too many args (%d) to ActivateLinkAction()\n")

ResDef(GNOMEFE_LAY_UNKNOWN_PARAMETER_TO_ACTIVATE_LINK_ACTION, GNOMEFE_ERR_OFFSET + 531,
	   "%s: unknown parameter (%s) to ActivateLinkAction()\n")

ResDef(GNOMEFE_LAY_LOCAL_FILE_URL_UNTITLED, GNOMEFE_ERR_OFFSET + 532,
	   "file:///Untitled")

ResDef(GNOMEFE_DIALOGS_PRINTING, GNOMEFE_ERR_OFFSET + 533,
	   "printing")

ResDef(GNOMEFE_DIALOGS_DEFAULT_VISUAL_AND_COLORMAP, GNOMEFE_ERR_OFFSET + 534,
	   "\nThis is the default visual and color map.")

ResDef(GNOMEFE_DIALOGS_DEFAULT_VISUAL_AND_PRIVATE_COLORMAP, GNOMEFE_ERR_OFFSET + 535,
	   "\nThis is the default visual with a private map.")

ResDef(GNOMEFE_DIALOGS_NON_DEFAULT_VISUAL, GNOMEFE_ERR_OFFSET + 536,
	   "\nThis is a non-default visual.")

ResDef(GNOMEFE_DIALOGS_FROM_NETWORK, GNOMEFE_ERR_OFFSET + 537,
	   "from network")

ResDef(GNOMEFE_DIALOGS_FROM_DISK_CACHE, GNOMEFE_ERR_OFFSET + 538,
	   "from disk cache")

ResDef(GNOMEFE_DIALOGS_FROM_MEMORY_CACHE, GNOMEFE_ERR_OFFSET + 539,
	   "from memory cache")

ResDef(GNOMEFE_DIALOGS_DEFAULT, GNOMEFE_ERR_OFFSET + 540,
	   "default")

ResDef(GNOMEFE_HISTORY_TOO_FEW_ARGS_TO_HISTORY_ITEM, GNOMEFE_ERR_OFFSET + 541,
	   "%s: too few args (%d) to HistoryItem()\n")

ResDef(GNOMEFE_HISTORY_TOO_MANY_ARGS_TO_HISTORY_ITEM, GNOMEFE_ERR_OFFSET + 542,
	   "%s: too many args (%d) to HistoryItem()\n")

ResDef(GNOMEFE_HISTORY_UNKNOWN_PARAMETER_TO_HISTORY_ITEM, GNOMEFE_ERR_OFFSET + 543,
	   "%s: unknown parameter (%s) to HistoryItem()\n")

ResDef(GNOMEFE_REMOTE_S_UNABLE_TO_READ_PROPERTY, GNOMEFE_ERR_OFFSET + 544,
	   "%s: unable to read %s property\n")

ResDef(GNOMEFE_REMOTE_S_INVALID_DATA_ON_PROPERTY, GNOMEFE_ERR_OFFSET + 545,
	   "%s: invalid data on %s of window 0x%x.\n")
	   
ResDef(GNOMEFE_REMOTE_S_509_INTERNAL_ERROR, GNOMEFE_ERR_OFFSET + 546,
	   "509 internal error: unable to translate"
	   "window 0x%x to a widget")

ResDef(GNOMEFE_REMOTE_S_500_UNPARSABLE_COMMAND, GNOMEFE_ERR_OFFSET + 547,
	   "500 unparsable command: %s")

ResDef(GNOMEFE_REMOTE_S_501_UNRECOGNIZED_COMMAND, GNOMEFE_ERR_OFFSET + 548,
	   "501 unrecognized command: %s")
	   
ResDef(GNOMEFE_REMOTE_S_502_NO_APPROPRIATE_WINDOW, GNOMEFE_ERR_OFFSET + 549,
	   "502 no appropriate window for %s")
	   
ResDef(GNOMEFE_REMOTE_S_200_EXECUTED_COMMAND, GNOMEFE_ERR_OFFSET + 550,
	   "200 executed command: %s(")

ResDef(GNOMEFE_REMOTE_200_EXECUTED_COMMAND, GNOMEFE_ERR_OFFSET + 551,
	   "200 executed command: %s(")

ResDef(GNOMEFE_SCROLL_WINDOW_GRAVITY_WARNING, GNOMEFE_ERR_OFFSET + 552,
	   "%s: windowGravityWorks: %s must be yes, no, or guess.\n")

ResDef(GNOMEFE_COULD_NOT_DUP_STDERR, GNOMEFE_ERR_OFFSET + 553,
	   "could not dup() a stderr:")

ResDef(GNOMEFE_COULD_NOT_FDOPEN_STDERR, GNOMEFE_ERR_OFFSET + 554,
	   "could not fdopen() the new stderr:")

ResDef(GNOMEFE_COULD_NOT_DUP_A_NEW_STDERR, GNOMEFE_ERR_OFFSET + 555,
	   "could not dup() a new stderr:")

ResDef(GNOMEFE_COULD_NOT_DUP_A_STDOUT, GNOMEFE_ERR_OFFSET + 556,
	   "could not dup() a stdout:")

ResDef(GNOMEFE_COULD_NOT_FDOPEN_THE_NEW_STDOUT, GNOMEFE_ERR_OFFSET + 557,
	   "could not fdopen() the new stdout:")

ResDef(GNOMEFE_COULD_NOT_DUP_A_NEW_STDOUT, GNOMEFE_ERR_OFFSET + 558,
	   "could not dup() a new stdout:")

ResDef(GNOMEFE_HPUX_VERSION_NONSENSE, GNOMEFE_ERR_OFFSET + 559,
	   "\n%s:\n\nThis Mozilla Communicator binary does not run on %s %s.\n\n"
	   "Please visit http://home.netscape.com/ for a version of Communicator"
	   " that runs\non your system.\n\n")

ResDef(GNOMEFE_BM_OUTLINER_COLUMN_CREATEDON, GNOMEFE_ERR_OFFSET + 560,
       "Created On")

ResDef(GNOMEFE_VIRTUALNEWSGROUP, GNOMEFE_ERR_OFFSET + 561,
	"Virtual Discussion Group")

ResDef(GNOMEFE_VIRTUALNEWSGROUPDESC, GNOMEFE_ERR_OFFSET + 562,
       "Saving search criteria will create a Virtual Discussion Group\n"
       "based on that criteria. The Virtual Discussion Group will be \n"
       "available from the Message Center.\n" )

ResDef(GNOMEFE_EXIT_CONFIRMATION, GNOMEFE_ERR_OFFSET + 563,
       "Mozilla Exit Confirmation\n")

ResDef(GNOMEFE_EXIT_CONFIRMATIONDESC, GNOMEFE_ERR_OFFSET + 564,
       "Close all windows and exit Mozilla?\n")

ResDef(GNOMEFE_MAIL_WARNING, GNOMEFE_ERR_OFFSET + 565,
       "Mozilla Mail\n")

ResDef(GNOMEFE_SEND_UNSENTMAIL, GNOMEFE_ERR_OFFSET + 566,
       "Outbox folder contains unsent messages\n"
       "Send them now?")

ResDef(GNOMEFE_PREFS_WRONG_POP_USER_NAME, GNOMEFE_ERR_OFFSET + 567,
       "Your POP user name is just your user name (e.g. user),\n"
       "not your full POP address (e.g. user@internet.com).")

ResDef(GNOMEFE_PREFS_ENTER_VALID_INFO, GNOMEFE_ERR_OFFSET + 568,
       "Please enter valid information.")
ResDef(GNOMEFE_CANNOT_EDIT_JS_MAILFILTERS, GNOMEFE_ERR_OFFSET + 569,
	   "The editing of JavaScript message filters is not available\n"
	   "in this release of Communicator.")

ResDef(GNOMEFE_AB_HEADER_PHONE, GNOMEFE_ERR_OFFSET + 570,
       "Phone")

ResDef(GNOMEFE_PURGING_NEWS_MESSAGES, GNOMEFE_ERR_OFFSET + 571,
	   "Cleaning up news messages...")

ResDef(GNOMEFE_PREFS_RESTART_FOR_FONT_CHANGES, GNOMEFE_ERR_OFFSET + 572,
	   "Your font preferences will not take effect until you restart Communicator.")

ResDef(GNOMEFE_DND_NO_EMAILADDRESS, GNOMEFE_ERR_OFFSET + 573,
	   "One or more items in the selection you are dragging do not contain an email address\n"
	   "and was not added to the list. Please make sure each item in your selection includes\n"
	   "an email address.")

ResDef(GNOMEFE_NEW_FOLDER_PROMPT, GNOMEFE_ERR_OFFSET + 574, "New Folder Name:")
ResDef(GNOMEFE_USAGE_MSG4, GNOMEFE_ERR_OFFSET + 575, "\
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
ResDef(GNOMEFE_USAGE_MSG5, GNOMEFE_ERR_OFFSET + 576, "\
       Arguments which are not switches are interpreted as either files or\n\
       URLs to be loaded.\n\n" )

ResDef(GNOMEFE_SEARCH_DLG_PROMPT, GNOMEFE_ERR_OFFSET + 577, "Searching:" )
ResDef(GNOMEFE_SEARCH_AB_RESULT, GNOMEFE_ERR_OFFSET + 578, "Search Results" )
ResDef(GNOMEFE_SEARCH_AB_RESULT_FOR, GNOMEFE_ERR_OFFSET + 579, "Search results for:" )

ResDef(GNOMEFE_SEARCH_ATTRIB_NAME, GNOMEFE_ERR_OFFSET + 580, "Name" )
ResDef(GNOMEFE_SEARCH_ATTRIB_EMAIL, GNOMEFE_ERR_OFFSET + 581, "Email" )
ResDef(GNOMEFE_SEARCH_ATTRIB_ORGAN, GNOMEFE_ERR_OFFSET + 582, "Organization" )
ResDef(GNOMEFE_SEARCH_ATTRIB_ORGANU, GNOMEFE_ERR_OFFSET + 583, "Department" )

ResDef(GNOMEFE_SEARCH_RESULT_PROMPT, GNOMEFE_ERR_OFFSET + 584, "Search results will appear in address book window" )

ResDef(GNOMEFE_SEARCH_BASIC, GNOMEFE_ERR_OFFSET + 585, "Basic Search >>" )
ResDef(GNOMEFE_SEARCH_ADVANCED, GNOMEFE_ERR_OFFSET + 586, "Advanced Search >>" )
ResDef(GNOMEFE_SEARCH_MORE, GNOMEFE_ERR_OFFSET + 587, "More" )
ResDef(GNOMEFE_SEARCH_FEWER, GNOMEFE_ERR_OFFSET + 588, "Fewer" )

ResDef(GNOMEFE_SEARCH_BOOL_PROMPT, GNOMEFE_ERR_OFFSET + 589, "Find items which" )
ResDef(GNOMEFE_SEARCH_BOOL_AND_PROMPT, GNOMEFE_ERR_OFFSET + 590, "Match all of the following" )
ResDef(GNOMEFE_SEARCH_BOOL_OR_PROMPT, GNOMEFE_ERR_OFFSET + 591, "Match any of the following" )
ResDef(GNOMEFE_SEARCH_BOOL_WHERE, GNOMEFE_ERR_OFFSET + 592, "where" )
ResDef(GNOMEFE_SEARCH_BOOL_THE, GNOMEFE_ERR_OFFSET + 593, "the" )
ResDef(GNOMEFE_SEARCH_BOOL_AND_THE, GNOMEFE_ERR_OFFSET + 594, "and the" )
ResDef(GNOMEFE_SEARCH_BOOL_OR_THE, GNOMEFE_ERR_OFFSET + 595, "or the" )

ResDef(GNOMEFE_ABDIR_DESCRIPT, GNOMEFE_ERR_OFFSET + 596, "Description:" )
ResDef(GNOMEFE_ABDIR_SERVER, GNOMEFE_ERR_OFFSET + 597, "LDAP Server:" )
ResDef(GNOMEFE_ABDIR_SERVER_ROOT, GNOMEFE_ERR_OFFSET + 598, "Server Root:" )
ResDef(GNOMEFE_ABDIR_PORT_NUM, GNOMEFE_ERR_OFFSET + 599, "Port Number:" )
ResDef(GNOMEFE_ABDIR_MAX_HITS, GNOMEFE_ERR_OFFSET + 600, "Maximum Number of Hits:" )
ResDef(GNOMEFE_ABDIR_SECURE, GNOMEFE_ERR_OFFSET + 601, "Secure" )
ResDef(GNOMEFE_ABDIR_SAVE_PASSWD, GNOMEFE_ERR_OFFSET + 602, "Save Password" )
ResDef(GNOMEFE_ABDIR_DLG_TITLE, GNOMEFE_ERR_OFFSET + 603, "Directory Info" )
ResDef(GNOMEFE_AB2PANE_DIR_HEADER, GNOMEFE_ERR_OFFSET + 604, "Directories" )
ResDef(GNOMEFE_AB_SEARCH_DLG, GNOMEFE_ERR_OFFSET + 605, "Search..." )

ResDef(GNOMEFE_CUSTOM_HEADER, GNOMEFE_ERR_OFFSET + 606, "Custom Header:" )
ResDef(GNOMEFE_AB_DISPLAYNAME, GNOMEFE_ERR_OFFSET + 607, "Display Name:")
ResDef(GNOMEFE_AB_PAGER, GNOMEFE_ERR_OFFSET + 608, "Pager:")
ResDef(GNOMEFE_AB_CELLULAR, GNOMEFE_ERR_OFFSET + 609, "Cellular:")

ResDef(GNOMEFE_DND_MESSAGE_ERROR, GNOMEFE_ERR_OFFSET + 610, 
      "Cannot drop into the targeted destination folder."  )
ResDef(GNOMEFE_ABDIR_USE_PASSWD, GNOMEFE_ERR_OFFSET + 611, 
	   "Login with name and password")

ResDef(NO_SPELL_SHLIB_FOUND, GNOMEFE_ERR_OFFSET + 612,
       "No spellchk library found")

END_STR(mcom_cmd_gnome_gnome_err_h_strings)

#endif /* __GNOMEFE_GNOMEFE_ERR_H_ */
