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

#if defined(FEATURE_CUSTOM_BRAND)
#include "xp_brand.h"
#else
#define MOZ_NAME_PRODUCT    "Mozilla"
#define MOZ_NAME_BRAND      "Mozilla"
#define MOZ_NAME_FULL       "Mozilla"
#define MOZ_NAME_COMPANY    "Netscape Communications Corp"
#endif /* FEATURE_CUSTOM_BRAND */


/*
** Cross platform error codes. These are system errors, library errors,
** etc.  The ranges of error messages are displayed in the following table:
**
**       actual           7000 added        category
**    low      high      low     high
** -12288    -12276    -5288    -5276    sslerr (Mac, Unix)
**  -8192     -8171    -1192    -1171    secerr (Mac, Unix)
**  -7000     -5700        0     1300    Win, Mac FE
**  -4978     -4978     2022     2022    Mac FE
**   -436      -204     6564     6796    merrors
**      0       160     7000     7160    xp_error (Mac, Unix)
**   1000      1048     8000     8048    Unix FE
**  10000     11004    17000    18004    xp_error (Win)
**  14000     14020    21000    21020    Mac FE
**  15000     25000    22000    32000    xp_msg (Win, Mac, Unix)
**  25769     27500    32769    34500    Win FE
**  50000     58000    57000    65000    secerr, sslerr (Win)
**
**  JRM note 98/04/01.  The table above has little in common with reality.
**  moreover, as a scheme, it is quite unworkable on Macintosh, where the
**  legal range for resource ids is 128-0x7FFF.  This restricts the range
**  from the min to the max to be around 16000.  This  was exacerbated by
**  the security people staking out ranges such as -0x3000..-0x2000.  So
**  if anybody's wondering why I messed with these ranges for XP_MAC, now
**  you know.
*/
#ifndef _ALLXPSTR_H_
#define _ALLXPSTR_H_

#include "resdef.h"
/*
#ifdef XP_MAC
#define XP_MSG_BASE					1000
#define SEC_DIALOG_STRING_BASE		3000
#else */
#define XP_MSG_BASE			14000
#define SEC_DIALOG_STRING_BASE		(XP_MSG_BASE + 2000)
/*#endif*/

/* WARNING: DO NOT TAKE ERROR CODE -666, it is used internally
   by the message lib */


/* from merrors.h */
RES_START
BEGIN_STR(mcom_include_merrors_i_strings)

/* #define MK_HTTP_TYPE_CONFLICT           -204 */
ResDef(MK_HTTP_TYPE_CONFLICT, -204,
"A communications error occurred.\n\
 (TCP Error: %s)\n\n\
Try connecting again.")

/* #define MK_BAD_CONNECT                  -205 */
ResDef(MK_BAD_CONNECT,                  -205,
MOZ_NAME_BRAND" is unable to connect to the server at\n\
the location you have specified. The server may\n\
be down or busy.\n\n\
Try connecting again later.")

/* #define MK_TCP_ERROR                    -206 */
ResDef(MK_TCP_ERROR,            -206,
"A communications error occurred.\n\
 (TCP Error: %s)\n\n\
Try connecting again.")

#ifdef XP_MAC
/* #define MK_OUT_OF_MEMORY                -207 */
ResDef(MK_OUT_OF_MEMORY               , -207,
MOZ_NAME_BRAND" is out of memory.\n\n\
Try quitting "MOZ_NAME_PRODUCT" and increasing its minimum memory\n\
requirements through Get Info.")
#else
ResDef(MK_OUT_OF_MEMORY               , -207,
MOZ_NAME_BRAND" is out of memory.\n\n\
Try quitting some other applications or closing\n\
some windows.")

#endif


/* #define MK_MALFORMED_URL_ERROR          -209 */
ResDef(MK_MALFORMED_URL_ERROR,    -209,
"This Location (URL) is not recognized:\n\
  %.200s\n\n\
Check the Location and try again.")

/* #define MK_UNABLE_TO_USE_PASV_FTP       -211 */
ResDef(MK_UNABLE_TO_USE_PASV_FTP,       -211,
"Unable to use FTP passive mode")

/* #define MK_UNABLE_TO_CHANGE_FTP_MODE    -212 */
ResDef(MK_UNABLE_TO_CHANGE_FTP_MODE ,   -212,
MOZ_NAME_BRAND" is unable to set the FTP transfer mode with\n\
this server. You will not be able to download files.\n\n\
You should contact the administrator for this server\n\
or try again later.")


/* #define MK_UNABLE_TO_FTP_CWD            -213 */
ResDef(MK_UNABLE_TO_FTP_CWD,            -213,
MOZ_NAME_BRAND" is unable to send the change directory (cd)\n\
command, to the FTP server. You cannot view another\n\
directory.\n\n\
You should contact the administrator for this server\n\
or try again later.")


/* #define MK_UNABLE_TO_SEND_PORT_COMMAND  -214 */
ResDef(MK_UNABLE_TO_SEND_PORT_COMMAND,  -214,
MOZ_NAME_BRAND" is unable to send a port command to the FTP\n\
server to establish a data connection.\n\n\
You should contact the administrator for this server\n\
or try again later.")


/* #define MK_UNABLE_TO_LOCATE_FILE        -215 */
ResDef(MK_UNABLE_TO_LOCATE_FILE,        -215,
MOZ_NAME_BRAND" is unable to find the file or directory\n\
named %.200s.\n\n\
Check the name and try again.")


/* #define MK_BAD_NNTP_CONNECTION          -216 */
ResDef(MK_BAD_NNTP_CONNECTION,          -216,
"A News error occurred: Invalid NNTP connection\n\n\
Try connecting again.")

/* #define MK_NNTP_SERVER_ERROR            -217 */
ResDef(MK_NNTP_SERVER_ERROR,            -217,
"An error occurred with the News server.\n\n\
If you are unable to connect again, contact the\n\
administrator for this server.")

/* #define MK_SERVER_TIMEOUT               -218 */
ResDef(MK_SERVER_TIMEOUT,               -218,
"There was no response. The server could be down\n\
or is not responding.\n\n\
If you are unable to connect again later, contact\n\
the server's administrator.")


/* #define MK_UNABLE_TO_LOCATE_HOST        -219 */
ResDef(MK_UNABLE_TO_LOCATE_HOST,        -219,
MOZ_NAME_BRAND" is unable to locate the server:\n\
  %.200s\n\
The server does not have a DNS entry.\n\n\
Check the server name in the Location (URL)\n\
and try again.")

/* #define MK_SERVER_DISCONNECTED          -220 */
ResDef(MK_SERVER_DISCONNECTED,          -220,
"The server has disconnected.\n\
The server may have gone down or there may be a\n\
network problem.\n\n\
Try connecting again.")

/* #define MK_NEWS_ITEM_UNAVAILABLE        -221 */
ResDef(MK_NEWS_ITEM_UNAVAILABLE,        -221,
"The Newsgroup item is unavailable. It may have expired.\n\n\
Try retrieving another item.")

/* #define MK_UNABLE_TO_OPEN_NEWSRC        -222 */
ResDef(MK_UNABLE_TO_OPEN_NEWSRC,        -222,
MOZ_NAME_BRAND" is unable to open your News file (newsrc).\n\n\
Please verify that your Mail and News preferences are\n\
correct and try again.")


/* #define MK_UNABLE_TO_OPEN_FILE          -223 */
ResDef(MK_UNABLE_TO_OPEN_FILE,          -223,
MOZ_NAME_BRAND" is unable to open the file \n\
%.200s.\n\n\
Check the file name and try again.")

/* #define MK_TCP_WRITE_ERROR               -236    */
ResDef(MK_TCP_WRITE_ERROR, -236,
"A network error occurred while "MOZ_NAME_PRODUCT"\n\
was sending data.\n(Network Error: %s)\n\n\
Try connecting again.")

/* #define MK_COULD_NOT_LOGIN_TO_SMTP_SERVER -229   */
ResDef(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER, -229,
"An error occurred sending mail:\n\
"MOZ_NAME_PRODUCT" was unable to connect to the SMTP server.\n\
The server may be down or may be incorrectly configured.\n\n\
Please verify that your Mail preferences are correct\n\
and try again.")


/* #define MK_ERROR_SENDING_FROM_COMMAND    -230    */
ResDef(MK_ERROR_SENDING_FROM_COMMAND, -230,
"An error occurred while sending mail.\n\
The mail server responded:\n\
  %s\n\
Please verify that your email address is correct\n\
in your Mail preferences and try again.")


/* #define MK_ERROR_SENDING_RCPT_COMMAND    -231    */
ResDef(MK_ERROR_SENDING_RCPT_COMMAND,   -231,
"An error occurred while sending mail.\n\
The mail server responded:\n\
  %s\n\
Please check the message recipients and try again.")

/* #define MK_ERROR_SENDING_DATA_COMMAND    -232 */
ResDef(MK_ERROR_SENDING_DATA_COMMAND,   -232,
"An (SMTP) error occurred while sending mail.\nServer responded: %s")

/* #define MK_ERROR_SENDING_MESSAGE     -233    */
ResDef(MK_ERROR_SENDING_MESSAGE,        -233,
"An error occurred while sending mail.\n\
The mail server responded:\n\
  %s.\n\
Please check the message and try again.")

/* #define MK_SMTP_SERVER_ERROR         -234    */
ResDef(MK_SMTP_SERVER_ERROR,            -234,
"An error occurred sending mail: SMTP server error.\n\
The server responded:\n\
  %s\n\
Contact your mail administrator for assistance.")

/* #define MK_UNABLE_TO_CONNECT            -240    -- generic error --  */
ResDef(MK_UNABLE_TO_CONNECT,            -240,
"A network error occurred:\n\
unable to connect to server (TCP Error: %s)\n\
The server may be down or unreachable.\n\n\
Try connecting again later.")

/* #define MK_CONNECTION_TIMED_OUT         -241 */
ResDef(MK_CONNECTION_TIMED_OUT,     -241,
"There was no response. The server could be down\n\
or is not responding.\n\n\
If you are unable to connect again later, contact\n\
the server's administrator.")

/* #define MK_CONNECTION_REFUSED           -242 */
ResDef(MK_CONNECTION_REFUSED,           -242,
MOZ_NAME_BRAND"'s network connection was refused by the server \n\
%.200s.\n\
The server may not be accepting connections or\n\
may be busy.\n\n\
Try connecting again later.")


#ifdef XP_WIN
/* #define MK_UNABLE_TO_CREATE_SOCKET      -243 */
ResDef(MK_UNABLE_TO_CREATE_SOCKET   ,   -243,
MOZ_NAME_BRAND" was unable to create a network socket connection.\n\
There may be insufficient system resources or the network\n\
may be down.   (Reason: %s)\n\n\
Try connecting again later or try restarting "MOZ_NAME_PRODUCT". You\n\
can also try restarting Windows.")
#else
ResDef(MK_UNABLE_TO_CREATE_SOCKET   ,   -243,
MOZ_NAME_BRAND" was unable to create a network socket connection.\n\
There may be insufficient system resources or the network\n\
may be down.  (Reason: %s)\n\n\
Try connecting again later or try restarting "MOZ_NAME_PRODUCT".")
#endif


#ifdef XP_WIN
/* #define MK_UNABLE_TO_ACCEPT_SOCKET      -245     */
ResDef(MK_UNABLE_TO_ACCEPT_SOCKET,      -245,
MOZ_NAME_BRAND" is unable to complete a socket connection \n\
with this server. There may be insufficient system\n\
resources.\n\n\
Try restarting "MOZ_NAME_PRODUCT" or restarting Windows.")
#else
/* #define MK_UNABLE_TO_ACCEPT_SOCKET      -245     */
ResDef(MK_UNABLE_TO_ACCEPT_SOCKET,      -245,
MOZ_NAME_BRAND" is unable to complete a socket connection\n\
with this server. There may be insufficient system\n\
resources.\n\n\
Try restarting "MOZ_NAME_PRODUCT".")
#endif


/* #define MK_UNABLE_TO_CONNECT_TO_PROXY   -246 */
ResDef(MK_UNABLE_TO_CONNECT_TO_PROXY,   -246,
MOZ_NAME_BRAND" is unable to connect to your proxy server.\n\
The server may be down or may be incorrectly configured.\n\n\
Please verify that your Proxy preferences are correct\n\
and try again, or contact the server's administrator.")

/* #define MK_UNABLE_TO_LOCATE_PROXY    -247    */
ResDef(MK_UNABLE_TO_LOCATE_PROXY,    -247,
MOZ_NAME_BRAND" is unable to locate your proxy server.\n\
The server may be down or may be incorrectly configured.\n\n\
Please verify that your Proxy preferences are correct\n\
and try again, or contact the server's administrator.")



/* #define MK_ZERO_LENGTH_FILE              -251    */
ResDef(MK_ZERO_LENGTH_FILE, -251,
"The document contained no data.\n\
Try again later, or contact the server's administrator.")


/* #define MK_TCP_READ_ERROR               -252 */
ResDef(MK_TCP_READ_ERROR            ,   -252,
"A network error occurred while "MOZ_NAME_PRODUCT"\n\
was receiving data.\n(Network Error: %s)\n\n\
Try connecting again.")



/* #define MK_UNABLE_TO_OPEN_TMP_FILE      -253 */
ResDef(MK_UNABLE_TO_OPEN_TMP_FILE, -253,
MOZ_NAME_BRAND" is unable to open the temporary file\n\
%.200s.\n\n\
Check your `Temporary Directory' setting and try again.")

/* #define MK_COULD_NOT_GET_USERS_MAIL_ADDRESS -235 */
ResDef(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS, -235,
"An error occurred sending mail:\n\
the return mail address was invalid.\n\n\
Please verify that your email address is correct\n\
in your Mail preferences and try again.")

/* #define MK_DISK_FULL                    -250 */
ResDef(MK_DISK_FULL,                    -250,
"The disk is full. "MOZ_NAME_PRODUCT" is cancelling the file\n\
transfer and removing the file.\n\n\
Please remove some files and try again.")


/* #define MK_NNTP_AUTH_FAILED             -260  -- NNTP authinfo failure --    */
ResDef(MK_NNTP_AUTH_FAILED,             -260,
"An authorization error occurred:\n\n\
%s\n\n\
Please try entering your name and/or password again.")

/* #define MK_MIME_NO_SENDER                -266  */
ResDef(MK_MIME_NO_SENDER,               -266,  /* From: empty */
"No sender was specified.\n\
Please fill in your email address in the\n\
Mail and Newsgroup preferences.")

/* #define MK_MIME_NO_RECIPIENTS            -267  */
ResDef(MK_MIME_NO_RECIPIENTS,           -267,  /* To: and Newsgroups: empty */
"No recipients were specified.\n\
Please enter a recipient in a To: line,\n\
or a newsgroup in a Group: line..")

/* #define MK_MIME_NO_SUBJECT               -268    */
ResDef(MK_MIME_NO_SUBJECT,              -268,  /* Subject: empty */
"No subject was specified.")

/* #define MK_MIME_ERROR_WRITING_FILE       -269    */
ResDef(MK_MIME_ERROR_WRITING_FILE, -269,
"Error writing temporary file.")

/* #define MK_MIME_MULTIPART_BLURB          -275    */
ResDef(MK_MIME_MULTIPART_BLURB,         -275,  /* text preceding multiparts */
"This is a multi-part message in MIME format.")

/* #define MK_PRINT_LOSSAGE                -278 */
ResDef(MK_PRINT_LOSSAGE,                -278,
"Printing stopped.  A problem occurred while receiving\n\
the document.  Transmission may have been interrupted\n\
or there may be insufficient space to write the file.\n\n\
Try again. Check that space is available in the\n\
temporary directory or restart "MOZ_NAME_PRODUCT".")

/* #define MK_SIGNATURE_TOO_LONG           -279 */
ResDef(MK_SIGNATURE_TOO_LONG,           -279,
"Your signature exceeds the recommended four lines.")

/* #define MK_SIGNATURE_TOO_WIDE           -280 */
ResDef(MK_SIGNATURE_TOO_WIDE,           -280,
"Your signature exceeds the recommended 79 columns.\n\
For most readers, the lines will appear truncated, or\n\
will be wrapped unattractively.  \n\n\
Please edit it to keep the lines shorter than 80 characters.")

ResDef(MK_UNABLE_TO_CONNECT2,       -281,
"A network error occurred:\n\
    unable to connect to server\n\
The server may be down or unreachable.\n\n\
Try connecting again later.")

/* #define MK_CANT_LOAD_HELP_TOPIC          -282    */
ResDef(MK_CANT_LOAD_HELP_TOPIC,     -282,
"Unable to load the requested help topic")

/* #define MK_TIMEBOMB_MESSAGE              -301    */
ResDef(MK_TIMEBOMB_MESSAGE, -301,
"This copy of "MOZ_NAME_PRODUCT" has expired.\n\
This pre-release copy of "MOZ_NAME_FULL" has expired\n\
and can only be used to download a newer version of "MOZ_NAME_PRODUCT".")

/* #define MK_TIMEBOMB_URL_PROHIBIT     -302    */
ResDef(MK_TIMEBOMB_URL_PROHIBIT, -302,
"This trial or pre-release copy of "MOZ_NAME_FULL" has expired\n\
and can only be used to purchase or download a newer version of "MOZ_NAME_PRODUCT".")

/* #define MK_NO_WAIS_PROXY             -303    */
ResDef(MK_NO_WAIS_PROXY, -303,
"No WAIS proxy is configured.\n\n\
Check your Proxy preferences and try again.")

/* #define MK_NNTP_ERROR_MESSAGE            -304    */
ResDef(MK_NNTP_ERROR_MESSAGE, -304,
"A News (NNTP) error occurred:\n\
 %.100s")

/* #define MK_NNTP_NEWSGROUP_SCAN_ERROR     -305    */
ResDef(MK_NNTP_NEWSGROUP_SCAN_ERROR,    -305,
"A News error occurred. The scan of all newsgroups is incomplete.\n\
 \n\
Try to View All Newsgroups again.")

/* #define MK_CREATING_NEWSRC_FILE         -306 */
ResDef(MK_CREATING_NEWSRC_FILE,        -306,
MOZ_NAME_BRAND" could not find a News file (newsrc)\n\
and is creating one for you.")

/* #define MK_NNTP_SERVER_NOT_CONFIGURED   -307 */
ResDef(MK_NNTP_SERVER_NOT_CONFIGURED,-307,
"No NNTP server is configured.\n\n\
Check your Mail and News preferences and try again.")

/* #define MK_COMMUNICATIONS_ERROR          -308    */
ResDef(MK_COMMUNICATIONS_ERROR,         -308,
"A communications error occurred.\n\
Please try again.")


/* #define MK_SECURE_NEWS_PROXY_ERROR      -309 */
ResDef(MK_SECURE_NEWS_PROXY_ERROR,      -309,
MOZ_NAME_BRAND" was unable to connect to the secure news server\n\
because of a proxy error")


/* #define MK_POP3_SERVER_ERROR            -311  generic pop3 error code */
ResDef(MK_POP3_SERVER_ERROR,        -311,
"An error occurred with the POP3 mail server.\n\
You should contact the administrator for this server\n\
or try again later.")

/* #define MK_POP3_USERNAME_UNDEFINED      -312  no username defined */
ResDef(MK_POP3_USERNAME_UNDEFINED,  -312,
MOZ_NAME_BRAND" is unable to use the mail server because\n\
you have not provided a username.  Please provide\n\
one in the preferences and try again")

/* #define MK_POP3_PASSWORD_UNDEFINED      -313  no password defined */
ResDef(MK_POP3_PASSWORD_UNDEFINED,  -313,
"Error getting mail password.")

/* #define MK_POP3_USERNAME_FAILURE         -314  failure in USER step */
ResDef(MK_POP3_USERNAME_FAILURE,        -314,
"An error occurred while sending your user name to the mail server.\n\
You should contact the administrator for this server\n\
or try again later.")

/* #define MK_POP3_PASSWORD_FAILURE         -315  failure in PASS step */
ResDef(MK_POP3_PASSWORD_FAILURE,        -315,
"An error occurred while sending your password to the mail server.\n\
You should contact the administrator for this server\n\
or try again later.")

/* #define MK_POP3_NO_MESSAGES              -316  no mail messages on pop server */
ResDef(MK_POP3_NO_MESSAGES,             -316,
"There are no new messages on the server.")

/* #define MK_POP3_LIST_FAILURE         -317  LIST command failed */
ResDef(MK_POP3_LIST_FAILURE,            -317,
"An error occurred while listing messages on the POP3 mail server.\n\
You should contact the administrator for this server\n\
or try again later.")

/* #define MK_POP3_LAST_FAILURE         -318  LAST command failed */
ResDef(MK_POP3_LAST_FAILURE,            -318,
"An error occurred while querying the POP3 mail server for\n\
the last processed message.\n\
You should contact the administrator for this server\n\
or try again later.")


/* #define MK_POP3_RETR_FAILURE            -319  RETR command failed, continues */
ResDef(MK_POP3_RETR_FAILURE,        -319,
"An error occurred while getting messages from the POP3 mail server.\n\
You should contact the administrator for this server\n\
or try again later.")

/* #define MK_POP3_DELE_FAILURE         -320  DELE command failed */
ResDef(MK_POP3_DELE_FAILURE,            -320,
"An error occurred while removing messages from the POP3 mail server.\n\
You should contact the administrator for this server\n\
or try again later.")

/* #define MK_POP3_OUT_OF_DISK_SPACE        -321 */
ResDef(MK_POP3_OUT_OF_DISK_SPACE,       -321,
"There isn't enough room on the local disk to download\n\
your mail from the POP3 mail server.  Please make room and\n\
try again.  (The `Empty Trash' and `Compress This Folder'\n\
commands may recover some space.)")

/* #define MK_POP3_MESSAGE_WRITE_ERROR  -322 */
ResDef(MK_POP3_MESSAGE_WRITE_ERROR, -322,
"An error occurred while saving mail messages.")


/* #define MK_COULD_NOT_PUT_FILE           -325  FTP could not put file */
ResDef(MK_COULD_NOT_PUT_FILE,       -325,
"Could not post the file: %.80s.\n\
For reason:\n\
  %.200s\n\n\
You may not have permission to write to\n\
this directory.\n\
Check the permissions and try again.")

/* #define MK_TIMEBOMB_WARNING_MESSAGE              -326 */

/* original text for the message in this comment block.
   at some point, revert to this text when there is a better
   way of presenting dire alpha/beta software warning messages
   to the user. this is a gross and ugly hack that should be
   short lived. compaints go to cyeh@netscape.com 9/2/98
   
"This is a pre-release copy of "MOZ_NAME_FULL" that\n\
will expire at %s.\n\
To obtain a newer pre-release version or the latest full\n\
release of "MOZ_NAME_FULL" (which will not expire) \n\
choose Software from the Help menu.")
*/
ResDef(MK_TIMEBOMB_WARNING_MESSAGE, -326,
"This is a pre-release copy of "MOZ_NAME_FULL" that\n\
will expire at %s.\n\
This is ALPHA SOFTWARE.  It has not been tested at all.\n\
It may delete all your files and smoke your hard drive.\n\
YOU HAVE BEEN WARNED.\n")

/* #define MK_UNABLE_TO_DELETE_FILE */
ResDef(MK_UNABLE_TO_DELETE_FILE, -327,
"Could not delete file:\n\
    %s")

/* #define MK_UNABLE_TO_DELETE_DIRECTORY */
ResDef(MK_UNABLE_TO_DELETE_DIRECTORY, -328,
"Could not remove directory:\n\
    %s")

/* #define MK_MKDIR_FILE_ALREADY_EXISTS */
ResDef(MK_MKDIR_FILE_ALREADY_EXISTS, -329,
"Cannot create directory because a file or\n\
directory of that name already exists: \n\
    %s")

/* #define MK_COULD_NOT_CREATE_DIRECTORY */
ResDef(MK_COULD_NOT_CREATE_DIRECTORY, -330,
"Could not create directory:\n\
    %s")

/* #define MK_NOT_A_DIRECTORY */
ResDef(MK_NOT_A_DIRECTORY, -331,
"Object is not a directory:\n\
     %s")

/* define MK_COMPUSERVE_AUTH_FAILED   -332 */
ResDef(MK_COMPUSERVE_AUTH_FAILED, -332,
"Authorization failed")

/* #define MK_RELATIVE_TIMEBOMB_MESSAGE             -333    */
ResDef(MK_RELATIVE_TIMEBOMB_MESSAGE, -333,
"This trial copy of "MOZ_NAME_PRODUCT" has expired.\n\n\
To purchase a regular copy of "MOZ_NAME_FULL"\n(\
which will not expire) choose Software from the Help menu.")

/* #define MK_RELATIVE_TIMEBOMB_WARNING_MESSAGE             -334 */
ResDef(MK_RELATIVE_TIMEBOMB_WARNING_MESSAGE, -334,
"This trial copy of "MOZ_NAME_FULL" \n\
will expire at %s.\n\n\
To purchase a regular copy of "MOZ_NAME_FULL"\n(\
which will not expire) choose Software from the Help menu.")

/* #define MK_REDIRECT_ATTEMPT_NOT_ALLOWED  -335 */
ResDef(MK_REDIRECT_ATTEMPT_NOT_ALLOWED, -335,
"There was an attempt to redirect a url request,\n\
but the attempt was not allowed by the client.")

/* #define MK_BAD_GZIP_HEADER   -336 */
ResDef(MK_BAD_GZIP_HEADER, -336,
"Corruption was detected in the compressed GZip file that was requested")

/* Mail/News errors */

ResDef(MK_MSG_CANT_COPY_TO_SAME_FOLDER,     -401,
       "Can't move or copy messages to the folder they're already in.")

#ifdef XP_WIN16
ResDef(MK_MSG_CANT_COPY_TO_QUEUE_FOLDER,    -402,
       "Cannot copy messages into the 'Unsent' folder:\n\
That folder is only for storing messages\n\
to be sent later.")
#else
ResDef(MK_MSG_CANT_COPY_TO_QUEUE_FOLDER,    -402,
       "Cannot copy messages into the 'Unsent Messages' folder:\n\
That folder is only for storing messages\n\
to be sent later.")
#endif

ResDef(MK_MSG_CANT_COPY_TO_DRAFTS_FOLDER,   -403,
       "Cannot copy messages into the `Drafts' folder:\n\
That folder is only for holding drafts of messages which have\n\
not yet been sent.")

#ifdef XP_WIN
ResDef(MK_MSG_CANT_CREATE_FOLDER,           -404,
       "Couldn't create the folder! Your hard disk may be full\n\
or the folder pathname may be too long.")
#else
ResDef(MK_MSG_CANT_CREATE_FOLDER,           -404,
       "Couldn't create the folder! Your hard disk may be full.")
#endif

ResDef(MK_MSG_FOLDER_ALREADY_EXISTS,        -405,
       "A folder with that name already exists.")

ResDef(MK_MSG_FOLDER_NOT_EMPTY,             -406,
       "Can't delete a folder without first deleting the messages in it.")

ResDef(MK_MSG_CANT_DELETE_FOLDER,           -407,
       "Can't delete a folder without first deleting the messages in it.")

ResDef(MK_MSG_CANT_CREATE_INBOX,            -408,
       "Couldn't create default inbox folder!")

ResDef(MK_MSG_CANT_CREATE_MAIL_DIR,         -409,
       "Couldn't create a mail folder directory.  Mail will not work!")

ResDef(MK_MSG_NO_POP_HOST,                  -410,
       "No mail server has been specified. Please enter your mail \n\
server in the preferences (select Preferences from the Edit menu).")

ResDef(MK_MSG_MESSAGE_CANCELLED,            -414,
       "Message cancelled.")

ResDef(MK_MSG_COULDNT_OPEN_FCC_FILE,        -415,
       "Couldn't open Sent Mail file. \n\
       Please verify that your Mail preferences are correct.")

ResDef(MK_MSG_FOLDER_UNREADABLE,            -416,
       "Couldn't find the folder.")

ResDef(MK_MSG_FOLDER_SUMMARY_UNREADABLE,    -417,
       "Couldn't find the summary file.")

ResDef(MK_MSG_TMP_FOLDER_UNWRITABLE,        -418,
       "Couldn't open temporary folder file for output.")

ResDef(MK_MSG_ID_NOT_IN_FOLDER,             -419,
       "The specified message doesn't exist in that folder.\n\
It may have been deleted or moved into another folder.")

ResDef(MK_MSG_NEWSRC_UNPARSABLE,            -420,
       "A newsrc file exists but is unparsable.")

ResDef(MK_MSG_NO_RETURN_ADDRESS,            -421,
       "Your email address has not been specified.\n\
Before sending mail or news messages, you must specify a\n\
return address in Mail and News Preferences.")

#if 0
/* Windows and Mac resource only understand 254 chars per string */
ResDef(MK_MSG_NO_RETURN_ADDRESS_AT,         -421,
       "The return email address set in Preferences is: %s\n\
\n\
This does not appear to be complete (it contains no `@' and host name.)\n\
A valid email address will be of the form `USER@HOST', where USER is\n\
usually your login ID, and HOST is the fully-specified name of the\n\
machine to which your mail should be delivered.  A fully-specified host\n\
name will have at least one `.' in it, and will usually end in `.com',\n\
`.edu', `.net', or a two-letter country code.  If you are unsure what\n\
your full email address is, please consult your system administrator.")

/* Windows and Mac resource only understand 254 chars per string */
ResDef(MK_MSG_NO_RETURN_ADDRESS_DOT,        -422,
       "The return email address set in Preferences is: %s\n\
\n\
This does not appear to be complete (it contains no `.' in the host name.)\n\
A valid email address will be of the form `USER@HOST', where USER is\n\
usually your login ID, and HOST is the fully-specified name of the\n\
machine to which your mail should be delivered.  A fully-specified host\n\
name will have at least one `.' in it, and will usually end in `.com',\n\
`.edu', `.net', or a two-letter country code.  If you are unsure what\n\
your full email address is, please consult your system administrator.")
#else
ResDef(MK_MSG_NO_RETURN_ADDRESS_AT,         -423,
       "The return email address set in Preferences is: %s\n\
\n\
This appears to be incomplete (it contains no `@').  Examples of\n\
correct email addresses are `fred@xyz.com' and `sue@xyz.gov.au'.")

ResDef(MK_MSG_NO_RETURN_ADDRESS_DOT,        -424,
       "The return email address set in Preferences is: %s\n\
\n\
This appears to be incomplete (it contains no `.').  Examples of\n\
correct email addresses are `fred@xyz.com' and `sue@xyz.gov.au'.")
#endif

ResDef(MK_MSG_NO_SMTP_HOST,-425,
          "No outgoing mail (SMTP) server has been specified in Mail and News Preferences.")

ResDef(MK_NNTP_CANCEL_CONFIRM,          -426,
       "Are you sure you want to cancel this message?")

ResDef(MK_NNTP_CANCEL_DISALLOWED,       -427,
       "This message does not appear to be from you.\n\
You may only cancel your own posts, not those made by others.")

ResDef(MK_NNTP_CANCEL_ERROR,            -428,
       "Unable to cancel message!")

ResDef(MK_NNTP_NOT_CANCELLED,           -429,
       "Message not cancelled.")

ResDef(MK_NEWS_ERROR_FMT,               -430,
       "Error!\nNews server responded: %.512s\n")


ResDef(MK_MSG_NON_MAIL_FILE_READ_QUESTION,  -431,
       "%.300s does not appear to be a mail file.\n\
Attempt to read it anyway?")

ResDef(MK_MSG_NON_MAIL_FILE_WRITE_QUESTION, -432,
       "%.300s does not appear to be a mail file.\n\
Attempt to write it anyway?")

ResDef(MK_MSG_ERROR_WRITING_NEWSRC,         -433,
       "Error saving newsrc file!")

ResDef(MK_MSG_ERROR_WRITING_MAIL_FOLDER,    -434,
       "Error writing mail file!")

ResDef(MK_MSG_OFFER_COMPRESS,           -435,
       "At least one of your mail folders is wasting a lot\n\
of disk space.  If you compress your Mail folders now,\n\
you can recover %ld Kbytes of disk space. Compressing\n\
folders might take a while.\n\
\n\
Compress folders now?")

ResDef(MK_MSG_SEARCH_FAILED,            -436,
       "Not found.")

ResDef(MK_MSG_EMPTY_MESSAGE,            -437,
       "You haven't typed anything, and there is no attachment.\n\
Send anyway?")

ResDef(MK_MSG_DOUBLE_INCLUDE,           -438,
       "You have included the same document twice: first as a quoted\n\
document (meaning: with '>' at the beginning of each line), and\n\
then as an attachment (meaning: as a second part of the message,\n\
included after your new text).\n\
\n\
Send it anyway?")

#ifdef XP_WIN16
ResDef(MK_MSG_DELIVERY_FAILURE_1,                -439,
       "Delivery failed for 1 message.\n\n\
This message has been left in the Unsent folder.\n\
Before it can be delivered, the error must be\n\
corrected.")

ResDef(MK_MSG_DELIVERY_FAILURE_N,                -440,
       "Delivery failed for %d messages.\n\n\
These messages have been left in the Unsent folder.\n\
Before they can be delivered, the errors must be\n\
corrected.")
#else
ResDef(MK_MSG_DELIVERY_FAILURE_1,                -439,
       "Delivery failed for 1 message.\n\n\
This message has been left in the Unsent Messages folder.\n\
Before it can be delivered, the error must be\n\
corrected.")

ResDef(MK_MSG_DELIVERY_FAILURE_N,                -440,
       "Delivery failed for %d messages.\n\n\
These messages have been left in the Unsent Messages folder.\n\
Before they can be delivered, the errors must be\n\
corrected.")
#endif


ResDef(MK_MSG_MISSING_SUBJECT,          -441,
       "This message has no subject.  Send anyway?")

ResDef(MK_MSG_MIXED_SECURITY,           -442,
       "It will not be possible to send this message encrypted to all of the\n\
addressees.  Send it anyway?")

ResDef (MK_MSG_CAN_ONLY_DELETE_MAIL_FOLDERS,    -443,
    "Can only delete mail folders.")

ResDef(MK_MSG_FOLDER_BUSY,              -444,
"Can't copy messages because the mail folder is in use.\n\
Please wait until other copy operations are \n\
complete and try again.\n" )

ResDef(MK_MSG_PANES_OPEN_ON_FOLDER,     -445,
"Can't delete message folder '%s\' because you are viewing\n\
its contents. Please close those windows and try again.")

ResDef(MK_MSG_INCOMPLETE_NEWSGROUP_LIST,    -446,
"The complete list of newsgroups was not retrieved for\n\
this news server. Operations will not proceed normally\n\
until all newsgroups have been retrieved.\n\
\n\
Click on the 'All Newsgroups' tab to continue retrieving\n\
newsgroups.")

ResDef(MK_MSG_ONLINE_IMAP_WITH_NO_BODY,     -447,
"This message cannot be moved while Communicator is offline.\n\
It has not been downloaded for offline reading.\n\
Select Go Online from the File menu, then try again.")

ResDef(MK_MSG_ONLINE_COPY_FAILED,       -448,
"The IMAP message copy failed.  A source message was not found.")

ResDef(MK_MSG_ONLINE_MOVE_FAILED,       -449,
"The IMAP message move failed.\n\
The copy succeeded but a source message was not deleted.")

ResDef(MK_MSG_CONFIRM_CONTINUE_IMAP_SYNC,       -450,
"A problem has occurred uploading an offline change.\n\
 Continue uploading remaining offline changes (OK) \n\
 or try again later (Cancel)")

ResDef(MK_MSG_CANT_MOVE_INBOX,      -451,
"You cannot move your Inbox Folder.")

ResDef(MK_MSG_CANT_FIND_SNM,        -452,
"Could not find the summary information\n\
 for the %s IMAP folder.")

ResDef(MK_MSG_NO_UNDO_DURING_IMAP_FOLDER_LOAD,      -453,
"You cannot undo or redo a folder action while\n\
 the folder is loading.  Wait until the folder has\n\
 finished loading, then try again.")

ResDef(MK_MSG_MOVE_TARGET_NO_INFERIORS,     -454,
"The targeted destination folder does not allow subfolders.")

ResDef(MK_MSG_PARENT_TARGET_NO_INFERIORS,       -455,
"The selected parent folder does not allow subfolders.\n\
 Try selecting the server folder and typing\n\
 'parent/newFolder' to create a new hierarchy.")

ResDef(MK_MSG_COPY_TARGET_NO_SELECT,        -456,
"The targeted destination folder cannot hold messages.")

ResDef(MK_MSG_TRASH_NO_INFERIORS,       -457,
"This mail server cannot undo folder deletes, delete anyway?")

ResDef(MK_MSG_CANT_COPY_TO_QUEUE_FOLDER_OLD,    -458,
       "Cannot copy messages into the 'Outbox' folder:\n\
That folder is only for storing messages\n\
to be sent later.")

ResDef(MK_MSG_DELIVERY_FAILURE_1_OLD,                -459,
       "Delivery failed for 1 message.\n\n\
This message has been left in the Outbox folder.\n\
Before it can be delivered, the error must be\n\
corrected.")

ResDef(MK_MSG_DELIVERY_FAILURE_N_OLD,                -460,
       "Delivery failed for %d messages.\n\n\
These messages have been left in the Outbox folder.\n\
Before they can be delivered, the errors must be\n\
corrected.")

ResDef(MK_MSG_NO_MAIL_TO_NEWS,                -461,
       "You can't move a mail folder into a newsgroup.")

ResDef(MK_MSG_NO_NEWS_TO_MAIL,                -462,
       "You can't move a newsgroup into a mail folder.")

ResDef(MK_MSG_BOGUS_SERVER_MAILBOX_UID_STATE,                -463,
       "Mail Server Problem: The UID's for the messages in this\n\
       folder are not increasing.  Contact your system administrator.")

ResDef(MK_MSG_IMAP_SERVER_NOT_IMAP4,                -464,
       "This mail server is not an IMAP4 mail server.")

ResDef(MK_MSG_HTML_IMAP_NO_CACHED_BODY,                 -465,
"<TITLE>Go Online to View This Message</TITLE>\n\
The body of this message has not been downloaded from \n\
the server for reading offline. To read this message \n\
select 'Go Online' from the File menu and view the message again.")

ResDef(MK_MSG_COMPRESS_FAILED,                  -466,
       "Compress failed.")

ResDef(MK_MSG_LOTS_NEW_IMAP_FOLDERS,                  -467,
"At least 30 new IMAP folders have been found.\n\
\n\
Press <OK> to continue or <cancel> to change\n\
the IMAP server directory.")

ResDef(MK_MSG_IMAP_DIR_PROMPT,                  -468,
       "Enter IMAP server directory name.")

ResDef(MK_MSG_NO_POST_TO_DIFFERENT_HOSTS_ALLOWED,    -469,
"Posting to newsgroups on different hosts is not supported.")

ResDef(MK_MSG_CANT_MOVE_OFFLINE_MOVE_RESULT,    -470,
"This message was moved here while offline.  You have to \n\
go online to move it again.")

ResDef(MK_MSG_SEARCH_HITS_NOT_IN_DB,    -471,
"This IMAP folder is out of date.  Open it again for a more complete search.")

ResDef(MK_MSG_MAIL_DIRECTORY_CHANGED,    -472,
"Your new mail directory preference\n\
will take effect the next time\n\
you restart Communicator.")

ResDef(MK_MSG_UNABLE_TO_SAVE_DRAFT, -473,
MOZ_NAME_BRAND" is unable to save your message as draft.\n\
Please verify that your Mail preferences are correct\n\
and try again.")

ResDef(MK_MSG_UNABLE_TO_SAVE_TEMPLATE, -474,
MOZ_NAME_BRAND" is unable to save your message as template.\n\
Please verify that your Mail preferences are correct\n\
and try again.")

ResDef(MK_SENDMAIL_BAD_TLS,      -475,
"Your SMTP mail server could not start a secure connection.\n\
You have requested to send mail ONLY in secure mode and therefore \
the connection has been aborted. Please check your preferences.")

END_STR(mcom_include_merrors_i_strings)

/* WARNING: DO NOT TAKE ERROR CODE -666, it is used internally
   by the message lib */

/* General security error codes  */

#define NO_SECURITY_ERROR_ENUM
#include "secerr.h"

RES_START
BEGIN_STR(mcom_include_secerr_i_strings)

ResDef(SEC_ERROR_IO,                SEC_ERROR_BASE + 0,
"An I/O error occurred during security authorization.\n\
Please try your connection again")

ResDef(SEC_ERROR_LIBRARY_FAILURE,       SEC_ERROR_BASE + 1,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_BAD_DATA,          SEC_ERROR_BASE + 2,
"The security library has received bad data.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_OUTPUT_LEN,            SEC_ERROR_BASE + 3,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_INPUT_LEN,         SEC_ERROR_BASE + 4,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_INVALID_ARGS,          SEC_ERROR_BASE + 5,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_INVALID_ALGORITHM,     SEC_ERROR_BASE + 6,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_INVALID_AVA,           SEC_ERROR_BASE + 7,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_INVALID_TIME,          SEC_ERROR_BASE + 8,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_BAD_DER,           SEC_ERROR_BASE + 9,
"The security library has encountered an improperly formatted\n\
DER-encoded message.")

ResDef(SEC_ERROR_BAD_SIGNATURE,         SEC_ERROR_BASE + 10,
"The server's certificate has an invalid signature.\n\
You will not be able to connect to this site securely.")

ResDef(SEC_ERROR_EXPIRED_CERTIFICATE,       SEC_ERROR_BASE + 11,
"This operation cannot be performed because a required\n\
certificate has expired.  Click on the `Security' icon\n\
for more information about certificates.")

ResDef(SEC_ERROR_REVOKED_CERTIFICATE,       SEC_ERROR_BASE + 12,
"This operation cannot be performed because a required\n\
certificate has been revoked.  Click on the `Security'\n\
icon for more information about certificates.")

ResDef(SEC_ERROR_UNKNOWN_ISSUER,        SEC_ERROR_BASE + 13,
"The certificate issuer for this server is not recognized by\n\
"MOZ_NAME_PRODUCT". The security certificate may or may not be valid.\n\n\
"MOZ_NAME_PRODUCT" refuses to connect to this server.")

ResDef(SEC_ERROR_BAD_KEY,           SEC_ERROR_BASE + 14,
"The server's public key is invalid.\n\
You will not be able to connect to this site securely.")

ResDef(SEC_ERROR_BAD_PASSWORD,          SEC_ERROR_BASE + 15,
"The security password entered is incorrect.")

ResDef(SEC_ERROR_RETRY_PASSWORD,        SEC_ERROR_BASE + 16,
"You did not enter your new password correctly.  Please try again.")

ResDef(SEC_ERROR_NO_NODELOCK,           SEC_ERROR_BASE + 17,
"The security library has experienced an error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_BAD_DATABASE,          SEC_ERROR_BASE + 18,
"The security library has experienced a database error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_NO_MEMORY,         SEC_ERROR_BASE + 19,
"The security library has experienced an out of memory error.\n\
Please try to reconnect.")

ResDef(SEC_ERROR_UNTRUSTED_ISSUER,      SEC_ERROR_BASE + 20,
"The certificate issuer for this server has been marked as\n\
not trusted by the user.  "MOZ_NAME_PRODUCT" refuses to connect to this\n\
server.")

ResDef(SEC_ERROR_UNTRUSTED_CERT,        SEC_ERROR_BASE + 21,
"The certificate for this server has been marked as not\n\
trusted by the user.  "MOZ_NAME_PRODUCT" refuses to connect to this\n\
server.")

ResDef(SEC_ERROR_DUPLICATE_CERT,        (SEC_ERROR_BASE + 22),
"The Certificate that you are trying to download\n\
already exists in your database.")

ResDef(SEC_ERROR_DUPLICATE_CERT_NAME,       (SEC_ERROR_BASE + 23),
"You are trying to download a certificate whose name\n\
is the same as one that already exists in your database.\n\
If you want to download the new certificate you should\n\
delete the old one first.")

ResDef(SEC_ERROR_ADDING_CERT,           (SEC_ERROR_BASE + 24),
"Error adding certificate to your database")

ResDef(SEC_ERROR_FILING_KEY,            (SEC_ERROR_BASE + 25),
"Error refiling the key for this certificate")

ResDef(SEC_ERROR_NO_KEY,            (SEC_ERROR_BASE + 26),
"The Private Key for this certificate can\n\
not be found in your key database")

ResDef(SEC_ERROR_CERT_VALID,            (SEC_ERROR_BASE + 27),
"This certificate is valid.")

ResDef(SEC_ERROR_CERT_NOT_VALID,        (SEC_ERROR_BASE + 28),
"This certificate is not valid.")

ResDef(SEC_ERROR_CERT_NO_RESPONSE,      (SEC_ERROR_BASE + 29),
"No Response")

ResDef(SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,        (SEC_ERROR_BASE + 30),
"The certificate authority that issued this site's\n\
certificate has expired.\n\
Check your system date and time.")

ResDef(SEC_ERROR_CRL_EXPIRED,       (SEC_ERROR_BASE + 31),
"The certificate revocation list for this certificate authority\n\
that issued this site's certificate has expired.\n\
Reload a new certificate revocation list or check your system data and time.")

ResDef(SEC_ERROR_CRL_BAD_SIGNATURE,     (SEC_ERROR_BASE + 32),
"The certificate revocation list for this certificate authority\n\
that issued this site's certificate has an invalid signature.\n\
Reload a new certificate revocation list.")

ResDef(SEC_ERROR_CRL_INVALID,       (SEC_ERROR_BASE + 33),
"The certificate revocation list you are trying to load has\n\
an invalid format.")

ResDef(SEC_ERROR_EXTENSION_VALUE_INVALID,(SEC_ERROR_BASE + 34),
"Extension value is invalid.")

ResDef(SEC_ERROR_EXTENSION_NOT_FOUND,(SEC_ERROR_BASE + 35),
"Extension not found.")

ResDef(SEC_ERROR_CA_CERT_INVALID,(SEC_ERROR_BASE + 36),
"Issuer certificate is invalid.")

   
ResDef(SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID,(SEC_ERROR_BASE + 37),
"Certificate path length constraint is invalid.")

ResDef(SEC_ERROR_CERT_USAGES_INVALID,(SEC_ERROR_BASE + 38),
"Certificate usages is invalid.")

ResDef(SEC_INTERNAL_ONLY,       (SEC_ERROR_BASE + 39),
"**Internal ONLY module**")

ResDef(SEC_ERROR_INVALID_KEY,       (SEC_ERROR_BASE + 40),
"The system tried to use a key which does not support\n\
the requested operation.")

ResDef(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION,(SEC_ERROR_BASE + 41),
"Certificate contains unknown critical extension.")

ResDef(SEC_ERROR_OLD_CRL,       (SEC_ERROR_BASE + 42),
"The certificate revocation list you are trying to load is not\n\
later than the current one.")

ResDef(SEC_ERROR_NO_EMAIL_CERT,     (SEC_ERROR_BASE + 43),
"This message cannot be encrypted or signed because you do not\n\
yet have an email certificate.  Click on the `Security' icon for more\n\
information about certificates.")

ResDef(SEC_ERROR_NO_RECIPIENT_CERTS_QUERY,  (SEC_ERROR_BASE + 44),
"This message cannot be encrypted because you do not have\n\
certificates for each of the recipients.  Clicking on the\n\
`Security' icon will give you more information.\n\
\n\
Turn off encryption and send the message anyway?")

ResDef(SEC_ERROR_NOT_A_RECIPIENT,   (SEC_ERROR_BASE + 45),
"The data cannot be decrypted because you are not a recipient;\n\
either it was not intended for you, or a matching certificate or\n\
Private Key cannot be found in your local database.")

ResDef(SEC_ERROR_PKCS7_KEYALG_MISMATCH, (SEC_ERROR_BASE + 46),
"The data cannot be decrypted because the key encryption\n\
algorithm it used does not match that of your certificate.")

ResDef(SEC_ERROR_PKCS7_BAD_SIGNATURE,   (SEC_ERROR_BASE + 47),
"Signature verification failed due to no signer found,\n\
too many signers found, or improper or corrupted data.")

ResDef(SEC_ERROR_UNSUPPORTED_KEYALG,    (SEC_ERROR_BASE + 48),
"An unsupported or unknown key algorithm was encountered;\n\
the current operation cannot be completed.")

ResDef(SEC_ERROR_DECRYPTION_DISALLOWED, (SEC_ERROR_BASE + 49),
"The data cannot be decrypted because it was encrypted using an\n\
algorithm or key size which is not allowed by this configuration.")


#ifdef FORTEZZA
/* Fortezza Alerts */
ResDef(XP_SEC_FORTEZZA_BAD_CARD,        (SEC_ERROR_BASE + 50),
"The Fortezza Card in Socket %d has not been properly initialized.\
 Please remove it and return it to your issuer.")

ResDef(XP_SEC_FORTEZZA_NO_CARD,     (SEC_ERROR_BASE + 51),
"No cards Found")

ResDef(XP_SEC_FORTEZZA_NONE_SELECTED,       (SEC_ERROR_BASE + 52),
"No Card Selected")

ResDef(XP_SEC_FORTEZZA_MORE_INFO,       (SEC_ERROR_BASE + 53),
"Please Select a personality to get more info on")

ResDef(XP_SEC_FORTEZZA_PERSON_NOT_FOUND,        (SEC_ERROR_BASE + 54),
"Personality not found")

ResDef(XP_SEC_FORTEZZA_NO_MORE_INFO,        (SEC_ERROR_BASE + 55),
"No more information on that Personality")

ResDef(XP_SEC_FORTEZZA_BAD_PIN,     (SEC_ERROR_BASE + 56),
"Invalid Pin")

ResDef(XP_SEC_FORTEZZA_PERSON_ERROR,        (SEC_ERROR_BASE + 57),
"Couldn't initialize personalities")
#endif /* FORTEZZA */

ResDef(SEC_ERROR_NO_KRL,        (SEC_ERROR_BASE + 58),
"No key revocation list for this site's certificate has been found.\n\
You must load the key revocation list before continuing.")

ResDef(SEC_ERROR_KRL_EXPIRED,       (SEC_ERROR_BASE + 59),
"The key revocation list for this site's certificate has expired.\n\
Reload a new key revocation list.")

ResDef(SEC_ERROR_KRL_BAD_SIGNATURE,     (SEC_ERROR_BASE + 60),
"The key revocation list for this site's certificate has an invalid signature.\n\
Reload a new key revocation list.")

ResDef(SEC_ERROR_REVOKED_KEY,       (SEC_ERROR_BASE + 61),
"The key for this site's certificate has been revoked.\n\
You will be unable to access this site securely.")

ResDef(SEC_ERROR_KRL_INVALID,       (SEC_ERROR_BASE + 62),
"The key revocation list you are trying to load has\n\
an invalid format.")

ResDef(SEC_ERROR_NEED_RANDOM,           (SEC_ERROR_BASE + 63),
"The security library is out of random data.")

ResDef(SEC_ERROR_NO_MODULE,         (SEC_ERROR_BASE + 64),
"The security library could not find a security module which can\n\
perform the requested operation.")

ResDef(SEC_ERROR_NO_TOKEN,          (SEC_ERROR_BASE + 65),
"The security card or token does not exist, needs to be initialized\n\
or has been removed.")

ResDef(SEC_ERROR_READ_ONLY,         (SEC_ERROR_BASE + 66),
"The security library has experienced a database error.\n\
You will probably be unable to connect to this site securely.")

ResDef(SEC_ERROR_NO_SLOT_SELECTED,      (SEC_ERROR_BASE + 67),
"No slot or token was selected.")

ResDef(SEC_ERROR_CERT_NICKNAME_COLLISION,   (SEC_ERROR_BASE + 68),
"A certificate with the same name already exists.")

ResDef(SEC_ERROR_KEY_NICKNAME_COLLISION,    (SEC_ERROR_BASE + 69),
"A key with the same name already exists.")

ResDef(SEC_ERROR_SAFE_NOT_CREATED,      (SEC_ERROR_BASE + 70),
"An error occurred while creating safe object")

ResDef(SEC_ERROR_BAGGAGE_NOT_CREATED,       (SEC_ERROR_BASE + 71),
"An error occurred while creating safe object")

ResDef(XP_JAVA_REMOVE_PRINCIPAL_ERROR,      (SEC_ERROR_BASE + 72),
"Couldn't remove the principal")

ResDef(XP_JAVA_DELETE_PRIVILEGE_ERROR,      (SEC_ERROR_BASE + 73),
"Couldn't delete the privilege")

ResDef(XP_JAVA_CERT_NOT_EXISTS_ERROR,       (SEC_ERROR_BASE + 74),
"This principal doesn't have a certificate")

ResDef(SEC_ERROR_BAD_EXPORT_ALGORITHM,      (SEC_ERROR_BASE + 75),
"The operation cannot be performed because the required\n\
algorithm is not allowed by this configuration.")

ResDef(SEC_ERROR_EXPORTING_CERTIFICATES,    (SEC_ERROR_BASE + 76),
"Unable to export certificates.  An error occurred attempting to\n\
export the certificates.")

ResDef(SEC_ERROR_IMPORTING_CERTIFICATES,    (SEC_ERROR_BASE + 77),
"An error occurred attempting to import the certificates.")

ResDef(SEC_ERROR_PKCS12_DECODING_PFX,       (SEC_ERROR_BASE + 78),
"Unable to import certificates.  The file specified is either\n\
corrupt or is not a valid file.")

ResDef(SEC_ERROR_PKCS12_INVALID_MAC,        (SEC_ERROR_BASE + 79),
"Unable to import certificates.  Either the integrity password\n\
is incorrect or the data in the file specified has been tampered\n\
with or corrupted in some manner.")

ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM,  (SEC_ERROR_BASE + 80),
"Unable to import certificates.  The algorithm used to generate the\n\
integrity information for this file is not supported in the application.")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE, (SEC_ERROR_BASE + 81),
"Unable to import certificates.  "MOZ_NAME_PRODUCT" only supports password\n\
integrity and password privacy modes for importing certificates.")
#else
ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE, (SEC_ERROR_BASE + 81),
"Unable to import certificates.  Communicator only supports password\n\
integrity and password privacy modes for importing certificates.")
#endif

ResDef(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE,  (SEC_ERROR_BASE + 82),
"Unable to import certificates.  The file containing the certificates\n\
is corrupt.  Required information is either missing or invalid.")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM,  (SEC_ERROR_BASE + 83),
"Unable to import certificates.  The algorithm used to encrypt the\n\
contents is not supported by "MOZ_NAME_PRODUCT".")
#else
ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM,  (SEC_ERROR_BASE + 83),
"Unable to import certificates.  The algorithm used to encrypt the\n\
contents is not supported by Communicator.")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_VERSION,    (SEC_ERROR_BASE + 84),
"Unable to import certificates.  The file is a version not supported by\n\
"MOZ_NAME_PRODUCT".")
#else
ResDef(SEC_ERROR_PKCS12_UNSUPPORTED_VERSION,    (SEC_ERROR_BASE + 84),
"Unable to import certificates.  The file is a version not supported by\n\
Communicator.")
#endif

ResDef(SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT, (SEC_ERROR_BASE + 85),
"Unable to import certificates.  The privacy password specified is\n\
incorrect.")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_ERROR_PKCS12_CERT_COLLISION,     (SEC_ERROR_BASE + 86),
"Unable to import certificates.  A certificate with the same nickname,\n\
as one being imported already exists in your "MOZ_NAME_PRODUCT" database.")
#else
ResDef(SEC_ERROR_PKCS12_CERT_COLLISION,     (SEC_ERROR_BASE + 86),
"Unable to import certificates.  A certificate with the same nickname,\n\
as one being imported already exists in your Communicator database.")
#endif

ResDef(SEC_ERROR_USER_CANCELLED,        (SEC_ERROR_BASE + 87),
"The user pressed cancel.")

ResDef(SEC_ERROR_PKCS12_DUPLICATE_DATA,     (SEC_ERROR_BASE + 88),
"Certificates could not be imported since they already exist on \
the machine.")

ResDef(SEC_ERROR_MESSAGE_SEND_ABORTED,      (SEC_ERROR_BASE + 89),
"Message not sent.")

ResDef(SEC_ERROR_INADEQUATE_KEY_USAGE,  (SEC_ERROR_BASE + 90),
"The certificate is not approved for the attempted operation.")

ResDef(SEC_ERROR_INADEQUATE_CERT_TYPE,  (SEC_ERROR_BASE + 91),
"The certificate is not approved for the attempted application.")

ResDef(SEC_ERROR_CERT_ADDR_MISMATCH,        (SEC_ERROR_BASE + 92),
"The email address in the signing certificate does not match\n\
the email address in the message headers.  If these two\n\
addresses do not belong to the same person, then this could\n\
be an attempt at forgery.")

ResDef(SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY,   (SEC_ERROR_BASE + 93),
"Unable to import certificates.  An error occurred while attempting\n\
to import the Private Key associated with the certificate being imported.")

ResDef(SEC_ERROR_PKCS12_IMPORTING_CERT_CHAIN,   (SEC_ERROR_BASE + 94),
"Unable to import certificates.  An error occurred while attempting\n\
to import the certificate chain associated with the certificate\n\
being imported.")

ResDef(SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME,  (SEC_ERROR_BASE + 95),
"Unable to export certificates.  An error occurred while trying to locate\n\
a certificate or a key by its nickname.")

ResDef(SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY,   (SEC_ERROR_BASE + 96),
"Unable to export certificates.  The Private Key associated with a\n\
certificate could not be located or could not be exported from the\n\
key database.")

ResDef(SEC_ERROR_PKCS12_UNABLE_TO_WRITE,    (SEC_ERROR_BASE + 97),
"Unable to export certificates.  An error occurred while trying to write\n\
the export file.  Make sure the destination drive is not full and try\n\
exporting again.")

ResDef(SEC_ERROR_PKCS12_UNABLE_TO_READ,     (SEC_ERROR_BASE + 98),
"Unable to import certificates.  An error occurred while reading the\n\
import file.  Please make sure the file exists and is not corrupt and\n\
then try importing the file again.")

ResDef(SEC_ERROR_PKCS12_KEY_DATABASE_NOT_INITIALIZED,   (SEC_ERROR_BASE + 99),
"Unable to export certificates.  The database which contains\n\
Private Keys has not been initialized.  Either your key database\n\
is corrupt or has been deleted.  There is no key associated with\n\
this certificate.")

ResDef(SEC_ERROR_KEYGEN_FAIL,           (SEC_ERROR_BASE + 100),
"Unable to generate Public/Private Key Pair.")

ResDef(SEC_ERROR_INVALID_PASSWORD,      (SEC_ERROR_BASE + 101),
"The password you entered is invalid.  Please pick a different one.")

ResDef(SEC_ERROR_RETRY_OLD_PASSWORD,        (SEC_ERROR_BASE + 102),
"You did not enter your old password correctly.  Please try again.")

ResDef(SEC_ERROR_BAD_NICKNAME,          (SEC_ERROR_BASE + 103),
"The Certificate Name you entered is already in use by another certificate.")

ResDef(SEC_ERROR_NOT_FORTEZZA_ISSUER,           (SEC_ERROR_BASE + 104),
"Server FORTEZZA chain has a non-FORTEZZA Certificate. \n\
You will probably be unable to connect to this site securely.")


/* used in pk11dlgs.c */
ResDef(SEC_ERROR_UNKNOWN, (SEC_ERROR_BASE + 105),
       "Unknown")

/* define some error messages for lm_pkcs11.c */
ResDef(SEC_ERROR_JS_INVALID_MODULE_NAME, (SEC_ERROR_BASE + 106),
"Invalid module name.")

ResDef(SEC_ERROR_JS_INVALID_DLL, (SEC_ERROR_BASE + 107),
"Invalid module path/filename")

ResDef(SEC_ERROR_JS_ADD_MOD_FAILURE, (SEC_ERROR_BASE + 108),
"Unable to add module")

ResDef(SEC_ERROR_JS_DEL_MOD_FAILURE, (SEC_ERROR_BASE + 109),
 "Unable to delete module")

ResDef(SEC_ERROR_OLD_KRL,        (SEC_ERROR_BASE + 110),
"The key revocation list you are trying to load is not\n\
later than the current one.")
 
ResDef(SEC_ERROR_CKL_CONFLICT,       (SEC_ERROR_BASE + 111),
"The CKL you are trying to load has a different issuer\n\
than your current CKL.  You must first delete your\n\
current CKL.")

ResDef(SEC_ERROR_CERT_NOT_IN_NAME_SPACE, (SEC_ERROR_BASE + 112),
"The Certifying Authority for this certificate is not\n\
permited to issue a certificate with this name.")

ResDef(SEC_ERROR_KRL_NOT_YET_VALID, (SEC_ERROR_BASE + 113),
"The key revocation list for this site's certificate\n\
is not yet valid.  Reload a new key revocation list.")

ResDef(SEC_ERROR_CRL_NOT_YET_VALID, (SEC_ERROR_BASE + 114),
"The certificate revocation list for this site's\n\
certificate is not yet valid. Reload a new certificate\n\
revocation list.")

END_STR(mcom_include_secerr_i_strings)

/* HTML Dialog Box stuff.  Moved from secerr #'s to xp_msg #'s */
RES_START
BEGIN_STR(mcom_include_sec_dialog_strings)
/* NOTE - you can't use backslash-quote to get a quote in the html below.
 * you must use \042.  This is due to windows resource compiler hosage
 */
ResDef(XP_DIALOG_HEADER_STRINGS, (SEC_DIALOG_STRING_BASE + 0), "\
<head>%-styleinfo-%</head><body bgcolor=\042#bbbbbb\042><font face=\042PrimaSans BT\042>\
<form name=theform \
action=internal-dialog-handler method=post><input type=\042hidden\042 \
%-cont-%")

ResDef(XP_DIALOG_HEADER_STRINGS_1, (SEC_DIALOG_STRING_BASE + 1), "\
name=\042handle\042 value=\042%0%\042><input type=\042hidden\042 name=\042xxxbuttonxxx\042><font size=2>")

/* NOTE - dialogs and panels share the same footer strings !! */
ResDef(XP_DIALOG_FOOTER_STRINGS, (SEC_DIALOG_STRING_BASE + 2), "\
</font></form></font></body>%0%")

ResDef(XP_DIALOG_JS_HEADER_STRINGS, (SEC_DIALOG_STRING_BASE + 3), "\
<HTML><HEAD>%-styleinfo-%<TITLE>%0%</TITLE><SCRIPT LANGUAGE=\042JavaScript\042>\nvar dlgstring ='")

ResDef(XP_DIALOG_JS_MIDDLE_STRINGS, (SEC_DIALOG_STRING_BASE + 4), "\
';\nvar butstring ='")

ResDef(XP_DIALOG_JS_FOOTER_STRINGS, (SEC_DIALOG_STRING_BASE + 5), "\
';\nfunction drawdlg(win){\ncaptureEvents(Event.MOUSEDOWN);\n\
with(win.frames[0]) {\ndocument.write(parent.dlgstring);document.close();\n}\n\
with(win.frames[1]) {\nbutstring='<html><body bgcolor=\042#bbbbbb\042><font face=\042PrimaSans BT\042><form>'\
%-cont-%")

ResDef(XP_DIALOG_JS_FOOTER_STRINGS2, (SEC_DIALOG_STRING_BASE + 6), "\
+butstring+'</form></font></body></html>';document.write(parent.butstring);document.close();\n\
}\nreturn false;\n}\nfunction clicker(but,win)\n{\n\
with(win.frames[0].document.forms[0]) {\nxxxbuttonxxx.value=but.value;\n\
xxxbuttonxxx.name='button';\n\
%-cont-%")

ResDef(XP_DIALOG_JS_FOOTER_STRINGS3, (SEC_DIALOG_STRING_BASE + 7), "\
submit();\n}\n}\nfunction onMouseDown(e)\n{\nif ( e.which == 3 )\n\
return false;\nreturn true;\n}\n</SCRIPT></HEAD><FRAMESET ROWS=\042*,50\042\
ONLOAD=\042drawdlg(window)\042 BORDER=0>\n%-cont-%")

ResDef(XP_DIALOG_JS_FOOTER_STRINGS4, (SEC_DIALOG_STRING_BASE + 8), "\
<FRAME SRC=\042about:blank\042 MARGINWIDTH=5 MARGINHEIGHT=3 NORESIZE BORDER=NO>\n\
<FRAME SRC=\042about:blank\042 MARGINWIDTH=5 MARGINHEIGHT=0 NORESIZE SCROLLING=NO BORDER=NO>\n\
</FRAMESET></HTML>\n")

ResDef(XP_DIALOG_CANCEL_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 9), "\
<div align=right><input type=\042button\042 name=\042button\042 value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042 width=80></div>")

ResDef(XP_DIALOG_OK_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 10), "\
<div align=right><input type=\042button\042 name=\042button\042 value=\042%ok%\042 onclick=\042parent.clicker(this,window.parent)\042 width=80></div>")

ResDef(XP_DIALOG_CONTINUE_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 11), "\
<div align=right><input type=\042button\042 name=\042button\042 value=\042%continue%\042 onclick=\042parent.clicker(this,window.parent)\042 width=80>\
</div>")

ResDef(XP_DIALOG_CANCEL_OK_BUTTON_STRINGS, 
    (SEC_DIALOG_STRING_BASE + 12), "\
<div align=right><input type=\042button\042 value=\042%ok%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042>&nbsp;&nbsp;\
<input type=\042button\042 value=\042%cancel%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042></div>")

ResDef(XP_DIALOG_CANCEL_CONTINUE_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 13), "\
<div align=right><input type=\042button\042 name=\042button\042 value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042 width=80>&nbsp;&nbsp;\
<input type=\042button\042 name=\042button\042 value=\042%continue%\042 onclick=\042parent.clicker(this,window.parent)\042 width=80></div>")

ResDef(XP_PANEL_HEADER_STRINGS, (SEC_DIALOG_STRING_BASE + 14), "\
<head>%-styleinfo-%</head><body bgcolor=\042#bbbbbb\042><font face=\042PrimaSans BT\042><form name=theform \
action=internal-panel-handler method=post><input type=\042hidden\042 \
%-cont-%")

ResDef(XP_PANEL_HEADER_STRINGS_1, (SEC_DIALOG_STRING_BASE + 15), "\
name=\042handle\042 value=\042%0%\042><input type=\042hidden\042 name=\042xxxbuttonxxx\042>\
<font size=2>")

#ifdef XP_OS2_FIX
ResDef(XP_PANEL_FIRST_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 16), "\
<table border=0 width=\042100%%\042><tr><td width=\04233%%\042>\
value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042></td><td width=\04233%%\042 align=right><input \
%-cont-%")
#else
ResDef(XP_PANEL_FIRST_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 16), "\
<div align=right><input type=\042button\042 name=\042button\042 \
value=\042%next%\042 onclick=\042parent.clicker(this,window.parent)\042 \
width=80>&nbsp;&nbsp;<input \
%-cont-%")
#endif

#ifdef XP_OS2_FIX
ResDef(XP_PANEL_FIRST_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 17), "\
type=\042button\042 name=\042button\042 value=\042%next%\042 onclick=\042parent.clicker(this,window.parent)\042></td></tr></table>%0%")
#else
ResDef(XP_PANEL_FIRST_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 17), "\
type=\042button\042 name=\042button\042 value=\042%cancel%\042 \
onclick=\042parent.clicker(this,window.parent)\042 width=80></div>%0%")
#endif

#ifdef XP_OS2_FIX
ResDef(XP_PANEL_MIDDLE_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 18), "\
<table border=0 width=\042100%%\042><tr><td width=\04233%%\042>\
<td width=\04234%%\042 align=center><input type=\042button\042 name=\042button\042 \
value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042>\
</td><td width=\04233%%\042 align=right><input \
%-cont-%")
#else
ResDef(XP_PANEL_MIDDLE_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 18), "\
<div align=right><input type=\042button\042 name=\042button\042 \
value=\042%back%\042 onclick=\042parent.clicker(this,window.parent)\042 \
width=80><input type=\042button\042 name=\042button\042 value=\042%next%\042 \
%-cont-%")
#endif

#ifdef XP_OS2_FIX
ResDef(XP_PANEL_MIDDLE_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 19), "\
type=\042button\042 name=\042button\042 value=\042%back%\042 onclick=\042parent.clicker(this,window.parent)\042><input type=\042button\042 \
name=\042button\042 value=\042%next%\042 onclick=\042parent.clicker(this,window.parent)\042></td></tr></table>%0%")
#else
ResDef(XP_PANEL_MIDDLE_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 19), "\
onclick=\042parent.clicker(this,window.parent)\042 width=80>&nbsp;&nbsp;\
<input type=\042button\042 name=\042button\042 value=\042%cancel%\042 \
onclick=\042parent.clicker(this,window.parent)\042 width=80></div>%0%")
#endif

#ifdef XP_OS2_FIX
ResDef(XP_PANEL_LAST_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 20), "\
<table border=0 width=\042100%%\042><tr><td width=\04233%%\042>\
<td width=\04234%%\042 align=center><input type=\042button\042 name=\042button\042 \
value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042></td><td width=\04233%%\042 align=right><input \
%-cont-%")
#else
ResDef(XP_PANEL_LAST_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 20), "\
<dig align=right><input type=\042button\042 name=\042button\042 \
value=\042%back%\042 onclick=\042parent.clicker(this,window.parent)\042 \
width=80>%-cont-%")
#endif

#ifdef XP_OS2_FIX
ResDef(XP_PANEL_LAST_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 21), "\
type=\042button\042 name=\042button\042 value=\042%back%\042 onclick=\042parent.clicker(this,window.parent)\042><input type=\042button\042 \
name=\042button\042 value=\042%finished%\042 onclick=\042parent.clicker(this,window.parent)\042></td></tr></table>%0%")
#else
ResDef(XP_PANEL_LAST_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 21), "\
<input type=\042button\042 name=\042button\042 value=\042%finished%\042 \
onclick=\042parent.clicker(this,window.parent)\042 width=80>&nbsp;&nbsp;\
<input type=\042button\042 name=\042button\042 value=\042%cancel%\042 \
onclick=\042parent.clicker(this,window.parent)\042 width=80></div>%0%")
#endif

ResDef(XP_PLAIN_CERT_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 22), "\
%0%")

ResDef(XP_CERT_CONFIRM_STRINGS, (SEC_DIALOG_STRING_BASE + 23), "\
<title>%0%</title><b>%1%</b><hr>%2%<hr>%3%")

ResDef(XP_CERT_PAGE_STRINGS, (SEC_DIALOG_STRING_BASE + 24), "\
%0%%1%%2%")

ResDef(XP_SSL_SERVER_CERT1_STRINGS, (SEC_DIALOG_STRING_BASE + 25), "\
%sec-banner-begin%%0%%sec-banner-end%\
<b><font size=4>\
%1% is a site that uses encryption to protect transmitted information. \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT1_STRINGS_1, (SEC_DIALOG_STRING_BASE + 26), "\
However, "MOZ_NAME_PRODUCT" does not recognize the authority who signed its \
Certificate.</font></b><p>Although "MOZ_NAME_PRODUCT" does not recognize the \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT1_STRINGS_2, (SEC_DIALOG_STRING_BASE + 27), "\
signer of this Certificate, you may decide to accept it anyway so that \
you can connect to and exchange information with this site.<p>This \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT1_STRINGS_3, (SEC_DIALOG_STRING_BASE + 28), "\
assistant will help you decide whether or not you wish to accept this \
Certificate and to what extent.%2%")

ResDef(XP_SSL_SERVER_CERT2_STRINGS, (SEC_DIALOG_STRING_BASE + 29), "\
%sec-banner-begin%%0%%sec-banner-end%\
Here is the \
Certificate that is being presented:<hr><table><tr><td valign=top><font \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT2_STRINGS_1, (SEC_DIALOG_STRING_BASE + 30), "\
size=2>Certificate for: <br>Signed by: <br>Encryption: </font></td><td \
valign=top><font size=2>%1%<br>%2%<br>%3% Grade (%4% with %5%-bit secret \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT2_STRINGS_2, (SEC_DIALOG_STRING_BASE + 31), "\
key)</font></td><td valign=bottom><input type=\042submit\042 name=\042button\042 \
value=\042%moreinfo%\042></td></tr></table><hr>The signer of the \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT2_STRINGS_3, (SEC_DIALOG_STRING_BASE + 32), "\
Certificate promises you that the holder of this Certificate is who they \
say they are.  The encryption level is an indication of how difficult it \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT2_STRINGS_4, (SEC_DIALOG_STRING_BASE + 33), "\
would be for someone to eavesdrop on any information exchanged between \
you and this web site.%6%")

ResDef(XP_SSL_SERVER_CERT3_STRINGS, (SEC_DIALOG_STRING_BASE + 34), "\
%sec-banner-begin%%0%%sec-banner-end%\
Are you willing \
to accept this certificate for the purposes of receiving encrypted \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT3_STRINGS_1, (SEC_DIALOG_STRING_BASE + 35), "\
information from this web site?<p>This means that you will be able to \
browse through the site and receive documents from it and that all of \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT3_STRINGS_2, (SEC_DIALOG_STRING_BASE + 36), "\
these documents are protected from observation by a third party by \
encryption.<p><input type=radio name=accept value=session%1%>Accept this \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT3_STRINGS_3, (SEC_DIALOG_STRING_BASE + 37), "\
certificate for this session<br><input type=radio name=accept value=cancel%2%>\
Do not accept this certificate and do not connect\
<br><input type=radio name=accept %-cont-%")

ResDef(XP_SSL_SERVER_CERT3_STRINGS_4, (SEC_DIALOG_STRING_BASE + 38), "\
value=forever%3%>Accept this certificate forever (until it expires)<br>\
%4%")

ResDef(XP_SSL_SERVER_CERT4_STRINGS, (SEC_DIALOG_STRING_BASE + 39), "\
%sec-banner-begin%%0%%sec-banner-end%\
By accepting this \
certificate you are ensuring that all information you exchange with this site \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT4_STRINGS_1, (SEC_DIALOG_STRING_BASE + 40), "\
will be encrypted.  However, encryption will not protect you from \
fraud.<p>To protect yourself from fraud, do not send information \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT4_STRINGS_2, (SEC_DIALOG_STRING_BASE + 41), "(\
especially personal information, credit card numbers, or passwords) to \
this site if you have any doubt about the site's integrity.<p>For your \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT4_STRINGS_3, (SEC_DIALOG_STRING_BASE + 42), "\
own protection, "MOZ_NAME_PRODUCT" can remind you of this at the appropriate \
time.<p><center><input type=checkbox name=postwarn value=yes %1%>Warn me \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT4_STRINGS_4, (SEC_DIALOG_STRING_BASE + 43), "\
before I send information to this site</center><p>%2%")

ResDef(XP_SSL_SERVER_CERT5A_STRINGS, (SEC_DIALOG_STRING_BASE + 44), "\
%sec-banner-begin%%0%%sec-banner-end%\
<b>You have \
finished examining the certificate presented by:<br>%1%</b><p>You have \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT5A_STRINGS_1, (SEC_DIALOG_STRING_BASE + 45), "\
decided to refuse this ID. If, in the future, you change your mind about \
this decision, just visit this site again and this assistant will \
%-cont-%")

ResDef(XP_SSL_SERVER_CERT5A_STRINGS_2, (SEC_DIALOG_STRING_BASE + 46), "\
reappear.<p>Click on the Finish button to return to the document you were\
viewing before you attempted to connect to this site.%2%")

ResDef(XP_SSL_SERVER_CERT5B_STRINGS, (SEC_DIALOG_STRING_BASE + 47), "\
%sec-banner-begin%%0%%sec-banner-end%\
<b>You have \
finished examining the certificate presented by:<br>%1%</b><p>You have \
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_SSL_SERVER_CERT5B_STRINGS_1, (SEC_DIALOG_STRING_BASE + 48), "\
decided to accept this certificate and have asked that "MOZ_NAME_FULL" warn \
you before you send information to this site.<p>If you \
%-cont-%")
#else
ResDef(XP_SSL_SERVER_CERT5B_STRINGS_1, (SEC_DIALOG_STRING_BASE + 48), "\
decided to accept this certificate and have asked that "MOZ_NAME_PRODUCT" Communicator warn \
you before you send information to this site.<p>If you \
%-cont-%")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_SSL_SERVER_CERT5B_STRINGS_2, (SEC_DIALOG_STRING_BASE + 49), "\
change your mind, open Security Info from the "MOZ_NAME_PRODUCT" menu and edit \
Site Certificates.<p>Click on the Finish button to begin receiving\
documents.%2%")
#else
ResDef(XP_SSL_SERVER_CERT5B_STRINGS_2, (SEC_DIALOG_STRING_BASE + 49), "\
change your mind, open Security Info from the Communicator menu and edit \
Site Certificates.<p>Click on the Finish button to begin receiving\
documents.%2%")
#endif

ResDef(XP_SSL_SERVER_CERT5C_STRINGS, (SEC_DIALOG_STRING_BASE + 50), "\
%sec-banner-begin%%0%%sec-banner-end%\
<b>You have \
finished examining the certificate presented by:<br>%1%</b><p>You have \
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_SSL_SERVER_CERT5C_STRINGS_1, (SEC_DIALOG_STRING_BASE + 51), "\
decided to accept this certificate and have decided not to have "MOZ_NAME_PRODUCT" \
"MOZ_NAME_PRODUCT" warn you before you send information to this site.<p>If \
%-cont-%")
#else
ResDef(XP_SSL_SERVER_CERT5C_STRINGS_1, (SEC_DIALOG_STRING_BASE + 51), "\
decided to accept this certificate and have decided not to have "MOZ_NAME_PRODUCT" \
Communicator warn you before you send information to this site.<p>If \
%-cont-%")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_SSL_SERVER_CERT5C_STRINGS_2, (SEC_DIALOG_STRING_BASE + 52), "\
you change your mind, open Security Info from the "MOZ_NAME_PRODUCT" Menu \
edit Site Certificates.<p>Click on the Finish button\
to begin %-cont-%")
#else
ResDef(XP_SSL_SERVER_CERT5C_STRINGS_2, (SEC_DIALOG_STRING_BASE + 52), "\
you change your mind, open Security Info from the Communicator Menu \
edit Site Certificates.<p>Click on the Finish button\
to begin %-cont-%")
#endif

ResDef(XP_SSL_SERVER_CERT5C_STRINGS_3, (SEC_DIALOG_STRING_BASE + 53), "\
receiving documents.%2%")

ResDef(XP_SSL_CERT_DOMAIN_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 54), "\
%sec-banner-begin%%0%%sec-banner-end%\
The certificate \
that the site '%1%' has presented does not contain the correct site \
%-cont-%")

ResDef(XP_SSL_CERT_DOMAIN_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 55), "\
name. It is possible, though unlikely, that someone may be trying to \
intercept your communication with this site.  If you suspect the \
%-cont-%")

ResDef(XP_SSL_CERT_DOMAIN_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 56), "\
certificate shown below does not belong to the site you are connecting \
with, please cancel the connection and notify the site administrator. <p>\
%-cont-%")

ResDef(XP_SSL_CERT_DOMAIN_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 57), "\
Here is the Certificate that is being presented:<hr><table><tr><td \
valign=top><font size=2>Certificate for: <br>Signed by: <br>Encryption: \
%-cont-%")

ResDef(XP_SSL_CERT_DOMAIN_DIALOG_STRINGS_4, (SEC_DIALOG_STRING_BASE + 58), "\
</font></td><td valign=top><font size=2>%2%<br>%3%<br>%4% Grade (%5% \
with %6%-bit secret key)</font></td><td valign=bottom><input \
%-cont-%")

ResDef(XP_SSL_CERT_DOMAIN_DIALOG_STRINGS_5, (SEC_DIALOG_STRING_BASE + 59), "\
type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042></td></tr></table>\
<hr>%7%")

ResDef(XP_PW_ENTER_NEW_STRINGS, (SEC_DIALOG_STRING_BASE + 60), "\
%sec-banner-begin%%0%%sec-banner-end%\
Please enter your new password.  The safest passwords are a combination \
of letters \
%-cont-%")

ResDef(XP_PW_ENTER_NEW_STRINGS_1, (SEC_DIALOG_STRING_BASE + 61), "\
and numbers, are at least 8 characters long, and contain no words from a \
dictionary.<p>Password: <input type=password name=password1><p>Type in \
%-cont-%")

ResDef(XP_PW_ENTER_NEW_STRINGS_2, (SEC_DIALOG_STRING_BASE + 62), "\
your password, again, for verification:<p>Retype Password: <input \
type=password name=password2><p><b>Do not forget your password!  Your \
%-cont-%")

ResDef(XP_PW_ENTER_NEW_STRINGS_3, (SEC_DIALOG_STRING_BASE + 63), "\
password cannot be recovered. If you forget it, you will have to obtain \
new Certificates.</b>")

ResDef(XP_PW_RETRY_NEW_STRINGS, (SEC_DIALOG_STRING_BASE + 64), "\
%sec-banner-begin%%0%%sec-banner-end%\
You did not \
enter your password correctly.  Please try again:<p>Password: <input \
%-cont-%")

ResDef(XP_PW_RETRY_NEW_STRINGS_1, (SEC_DIALOG_STRING_BASE + 65), "\
type=password name=password1><p>Type in your password, again, for \
verification:<p>Retype Password: <input type=password name=password2><p>\
%-cont-%")

ResDef(XP_PW_RETRY_NEW_STRINGS_2, (SEC_DIALOG_STRING_BASE + 66), "\
<b>Do not forget your password!  Your password cannot be recovered. If \
you forget it, you will have to obtain new Certificates.</b> ")

ResDef(XP_PW_SETUP_STRINGS, (SEC_DIALOG_STRING_BASE + 67), "\
%sec-banner-being%%0%%sec-banner-end%\
%1%%2%%3%%4%\
%-cont-%")

ResDef(XP_PW_SETUP_STRINGS_1, (SEC_DIALOG_STRING_BASE + 68), "\
<table><tr><td>Password:</td><td><input type=password name=password1></td>\
</td></tr><tr><td>Type it again to confirm:</td><td><input type=password name=\
%-cont-%")

ResDef(XP_PW_SETUP_STRINGS_2, (SEC_DIALOG_STRING_BASE + 69), "\
password2></td><td valign=bottom></td></tr></table><B>Important: \
%-cont-%")

ResDef(XP_PW_SETUP_STRINGS_3, (SEC_DIALOG_STRING_BASE + 70), "\
Your password cannot be recovered.  If you forget it, you will lose all of your \
certificates.</B><P>%-cont-%")

ResDef(XP_PW_SETUP_STRINGS_4, (SEC_DIALOG_STRING_BASE + 71), "\
The safest passwords are at least 8 characters long, include \
both letters and numbers, and contain no words from a dictionary.<P>\
If you wish to change your password or other security \
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_PW_SETUP_STRINGS_5, (SEC_DIALOG_STRING_BASE + 72), "\
preferences, choose Security Info from the "MOZ_NAME_PRODUCT" menu.")
#else
ResDef(XP_PW_SETUP_STRINGS_5, (SEC_DIALOG_STRING_BASE + 72), "\
preferences, choose Security Info from the Communicator menu.")
#endif

/* unused               (SEC_DIALOG_STRING_BASE + 73) */

/* unused               (SEC_DIALOG_STRING_BASE + 74) */

/* unused               (SEC_DIALOG_STRING_BASE + 75) */

/* unused               (SEC_DIALOG_STRING_BASE + 76) */

/* unused               (SEC_DIALOG_STRING_BASE + 77) */

/* unused               (SEC_DIALOG_STRING_BASE + 78) */

/* unused               (SEC_DIALOG_STRING_BASE + 79) */

/* unused               (SEC_DIALOG_STRING_BASE + 80) */

/* unused               (SEC_DIALOG_STRING_BASE + 81) */

/* unused               (SEC_DIALOG_STRING_BASE + 82) */

ResDef(XP_PW_SETUP_REFUSED_STRINGS, (SEC_DIALOG_STRING_BASE + 83), "\
%sec-banner-begin%%0%%sec-banner-end%\
You have elected to operate without a password.<p>If you decide that you \
%-cont-%")

ResDef(XP_PW_SETUP_REFUSED_STRINGS_1, (SEC_DIALOG_STRING_BASE + 84), "\
would like to have a password to protect your Private Keys and Certificates, \
you can set up a password in Security Preferences.")

ResDef(XP_PW_CHANGE_STRINGS,        (SEC_DIALOG_STRING_BASE + 85), "\
%sec-banner-begin%%0%%sec-banner-end%\
Change the password for the %1%.<p>Enter your old password: <input \
%-cont-%")

ResDef(XP_PW_CHANGE_STRINGS_1,      (SEC_DIALOG_STRING_BASE + 86), "\
type=password name=password value=%2%><P><P>Enter your new password.  Leave \
the password fields blank if you don't want a password.<p><table><tr><td>\
%-cont-%")

ResDef(XP_PW_CHANGE_STRINGS_2,      (SEC_DIALOG_STRING_BASE + 87), "\
New Password:</td><td><input type=password name=password1></td></tr><tr>\
<td>Type it again to confirm:</td><td><input type=password name=password2>\
%-cont-%")

ResDef(XP_PW_CHANGE_STRINGS_3,      (SEC_DIALOG_STRING_BASE + 88), "\
</td></tr></table><p><B>Important: Your password cannot be recovered.  If \
you forget it, you will lose all of your certificates.</B>")

/* unused               (SEC_DIALOG_STRING_BASE + 89) */

ResDef(XP_PW_CHANGE_FAILURE_STRINGS,    (SEC_DIALOG_STRING_BASE + 90), "\
%sec-banner-begin%%0%%sec-banner-end%\
Your attempt to change your password failed.<p>This may be because your \
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_PW_CHANGE_FAILURE_STRINGS_1,  (SEC_DIALOG_STRING_BASE + 91), "\
key database is inaccessible (which can happen if you were already \
running a "MOZ_NAME_PRODUCT" when you started this one), or because of some other \
%-cont-%")
#else
ResDef(XP_PW_CHANGE_FAILURE_STRINGS_1,  (SEC_DIALOG_STRING_BASE + 91), "\
key database is inaccessible (which can happen if you were already \
running a Communicator when you started this one), or because of some other \
%-cont-%")
#endif

ResDef(XP_PW_CHANGE_FAILURE_STRINGS_2,  (SEC_DIALOG_STRING_BASE + 92), "\
error.<p>It may indicate that your key database file has been corrupted, \
in which case you should try to get it from of a backup, if possible. As \
%-cont-%")

ResDef(XP_PW_CHANGE_FAILURE_STRINGS_3,  (SEC_DIALOG_STRING_BASE + 93), "\
a last resort, you may need to delete your key database, after which you \
will have to obtain new personal Certificates.")

ResDef(XP_DIALOG_CANCEL_OK_MOREINFO_BUTTON_STRINGS, 
    (SEC_DIALOG_STRING_BASE + 94), "\
<table border=0 cellpadding=0 cellspacing=0 width=\042100%%\042><td>\
<input type=\042button\042 value=\042%moreinfo%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042></td>\
%-cont-%")

ResDef(XP_DIALOG_CANCEL_OK_MOREINFO_BUTTON_STRINGS_1,
    (SEC_DIALOG_STRING_BASE + 95), "\
<td align=\042right\042 nowrap><input type=\042button\042 value=\042%ok%\042 \
width=80 onclick=\042parent.clicker(this,window.parent)\042>&nbsp;&nbsp;\
%-cont-%")

ResDef(XP_DIALOG_CANCEL_OK_MOREINFO_BUTTON_STRINGS_2, 
    (SEC_DIALOG_STRING_BASE + 96),"\
<input type=\042button\042 value=\042%cancel%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042></td></table>")

/* unused               (SEC_DIALOG_STRING_BASE + 97) */

/* unused               (SEC_DIALOG_STRING_BASE + 98) */

ResDef(XP_PANEL_ONLY_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 99), "\
<div align=right><input type=\042button\042 name=\042button\042 \
value=\042%finished%\042 onclick=\042parent.clicker(this,window.parent)\042 \
width=80>&nbsp;&nbsp;\
%-cont-%")

ResDef(XP_PANEL_ONLY_BUTTON_STRINGS_1, (SEC_DIALOG_STRING_BASE + 100), "\
<input type=\042button\042 \
name=\042button\042 value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042 width=80></div>%0%")

ResDef(XP_NOT_IMPLEMENTED_STRINGS, (SEC_DIALOG_STRING_BASE + 101), "\
%sec-banner-begin%%0%%sec-banner-end%\
This function is \
not implemented:<br>%1%<br>Certificate name:<br>%2%")

ResDef(XP_CERT_VIEW_STRINGS, (SEC_DIALOG_STRING_BASE + 102), "\
%0%%1%")

ResDef(XP_DELETE_USER_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 103), "\
<b><FONT SIZE=4>WARNING: If you delete this Certificate you will not \
be able to read any E-mail that has been encrypted with it.</FONT></b><p>\
Are you sure that you want to delete this Personal Certificate?<p>%0%")

ResDef(XP_DELETE_SITE_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 104), "\
Are you sure that you want to delete this Site Certificate?<p>%0%")

ResDef(XP_DELETE_CA_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 105), "\
Are you sure that you want to delete this Certificate Authority?<p>%0%")

ResDef(XP_EDIT_SITE_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 106), "\
%0%<hr>This Certificate belongs to an SSL server site.<br><input \
type=radio name=allow value=yes %1%>Allow connections to this site<br>\
%-cont-%")

ResDef(XP_EDIT_SITE_CERT_STRINGS_1, (SEC_DIALOG_STRING_BASE + 107), "\
<input type=radio name=allow value=no %2%>Do not allow connections to \
this site<hr><input type=checkbox name=postwarn value=yes %3%>Warn \
%-cont-%")

ResDef(XP_EDIT_SITE_CERT_STRINGS_2, (SEC_DIALOG_STRING_BASE + 108), "\
before sending data to this site")

ResDef(XP_EDIT_CA_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 109), "\
%0%<hr>This Certificate belongs to a Certifying Authority<br> \
%-cont-%")

ResDef(XP_EDIT_CA_CERT_STRINGS_1, (SEC_DIALOG_STRING_BASE + 110), "\
%1%<br>%2%<br>%3%<hr><input \
%-cont-%")

ResDef(XP_EDIT_CA_CERT_STRINGS_2, (SEC_DIALOG_STRING_BASE + 111), "\
type=checkbox name=postwarn value=yes %4%>Warn before sending data to \
sites certified by this authority")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 112), "\
%sec-banner-begin%%0%%sec-banner-end%\
<b>Warning: You \
%-cont-%")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 113), "\
are about to send encrypted information to the site %1%.</b><p>It is \
card numbers, or passwords) to this site if you are in doubt about its \
%-cont-%")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 114), "\
safer not to send information (particularly personal information, credit \
Certificate or integrity.<br>Here is the Certificate for this site:<hr>\
%-cont-%")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 115), "\
<table><tr><td valign=top><font size=2>Certificate for: <br>Signed by: \
<br>Encryption: </font></td><td valign=top><font size=2>%2%<br>%3%<br>\
%-cont-%")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS_4, (SEC_DIALOG_STRING_BASE + 116), "\
%4% Grade (%5% with %6%-bit secret key)</font></td><td valign=bottom>\
<font size=2><input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\
%-cont-%")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS_5, (SEC_DIALOG_STRING_BASE + 117), "\
\042></font></td></tr></table><hr><input type=radio name=action \
value=sendandwarn checked>Send this information and warn again next \
%-cont-%")

ResDef(XP_SSL_CERT_POST_WARN_DIALOG_STRINGS_6, (SEC_DIALOG_STRING_BASE + 118), "\
time<br><input type=radio name=action value=send>Send this information \
and do not warn again<br><input type=radio name=action value=dontsend>Do \
not send information<br>%7%")

ResDef(XP_CA_CERT_DOWNLOAD1_STRINGS, (SEC_DIALOG_STRING_BASE + 119), "\
%sec-banner-begin%%0%%sec-banner-end%\
You are about to \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD1_STRINGS_1, (SEC_DIALOG_STRING_BASE + 120), "\
go through the process of accepting a Certificate Authority. This has \
serious implications on the security of future encryptions using \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD1_STRINGS_2, (SEC_DIALOG_STRING_BASE + 121), "\
"MOZ_NAME_PRODUCT". This assistant will help you decide whether or not you wish to \
accept this Certificate Authority.")

ResDef(XP_CA_CERT_DOWNLOAD2_STRINGS, (SEC_DIALOG_STRING_BASE + 122), "\
%sec-banner-begin%%0%%sec-banner-end%\
A Certificate \
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_CA_CERT_DOWNLOAD2_STRINGS_1, (SEC_DIALOG_STRING_BASE + 123), "\
Authority certifies the identity of sites on the internet. By accepting \
this Certificate Authority, you will allow "MOZ_NAME_FULL" to connect \
%-cont-%")
#else
ResDef(XP_CA_CERT_DOWNLOAD2_STRINGS_1, (SEC_DIALOG_STRING_BASE + 123), "\
Authority certifies the identity of sites on the internet. By accepting \
this Certificate Authority, you will allow "MOZ_NAME_PRODUCT" Communicator to connect \
%-cont-%")
#endif

ResDef(XP_CA_CERT_DOWNLOAD2_STRINGS_2, (SEC_DIALOG_STRING_BASE + 124), "\
to and receive information from any site that this authority certifies \
without prompting or warning you. <p>If you choose to refuse this \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD2_STRINGS_3, (SEC_DIALOG_STRING_BASE + 125), "\
Certificate Authority, you will be prompted before you connect to or \
receive information from any site that this authority certifies. ")

ResDef(XP_CA_CERT_DOWNLOAD3_STRINGS, (SEC_DIALOG_STRING_BASE + 126), "\
%sec-banner-begin%%0%%sec-banner-end%\
Here is the \
certificate for this Certificate Authority. Examine it carefully. The \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD3_STRINGS_1, (SEC_DIALOG_STRING_BASE + 127), "\
Certificate Fingerprint can be used to verify that this Authority is who \
they say they are. To do this, compare the Fingerprint against the \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD3_STRINGS_2, (SEC_DIALOG_STRING_BASE + 128), "\
Fingerprint published by this authority in other places.<hr> <table><tr>\
<td valign=top><font size=2>Certificate for: <br>Signed by: </font></td>\
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD3_STRINGS_3, (SEC_DIALOG_STRING_BASE + 129), "\
<td valign=top><font size=2>%1%<br>%2%</font></td><td valign=bottom>\
<font size=2><input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD3_STRINGS_4, (SEC_DIALOG_STRING_BASE + 130), "\
\042></font></td></tr></table><hr>")

ResDef(XP_CA_CERT_DOWNLOAD4_STRINGS, (SEC_DIALOG_STRING_BASE + 131), "\
%sec-banner-begin%%0%%sec-banner-end%\
Are you willing \
to accept this Certificate Authority for the purposes of certifying \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD4_STRINGS_1, (SEC_DIALOG_STRING_BASE + 132), "\
other internet sites, email users, or software developers?<p> \
%1%%2%%3%")

/* 133, 134 NOT USED */

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_CA_CERT_DOWNLOAD5_STRINGS, (SEC_DIALOG_STRING_BASE + 135), "\
%sec-banner-begin%%0%%sec-banner-end%\
By accepting this \
Certificate Authority, you have told "MOZ_NAME_FULL" \
%-cont-%")
#else
ResDef(XP_CA_CERT_DOWNLOAD5_STRINGS, (SEC_DIALOG_STRING_BASE + 135), "\
%sec-banner-begin%%0%%sec-banner-end%\
By accepting this \
Certificate Authority, you have told "MOZ_NAME_PRODUCT" Communicator to connect to \
%-cont-%")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_CA_CERT_DOWNLOAD5_STRINGS_1, (SEC_DIALOG_STRING_BASE + 136), "\
to connect to and receive information from any site that it certifies  \
without warning you or prompting you.<p>"MOZ_NAME_FULL" can, however, \
warn you before \
%-cont-%")
#else
ResDef(XP_CA_CERT_DOWNLOAD5_STRINGS_1, (SEC_DIALOG_STRING_BASE + 136), "\
to connect to and receive information from any site that it certifies  \
without warning you or prompting you.<p>"MOZ_NAME_PRODUCT" Communicator can, however, \
warn you before \
%-cont-%")
#endif

ResDef(XP_CA_CERT_DOWNLOAD5_STRINGS_2, (SEC_DIALOG_STRING_BASE + 137), "\
you send information to such a site.<p><input type=checkbox \
name=postwarn value=yes %1%>Warn me before sending information to sites \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD5_STRINGS_3, (SEC_DIALOG_STRING_BASE + 138), "\
certified by this Certificate Authority")

ResDef(XP_CA_CERT_DOWNLOAD6_STRINGS, (SEC_DIALOG_STRING_BASE + 139), "\
%sec-banner-begin%%0%%sec-banner-end%\
You have accepted \
this Certificate Authority.  You must now select a nickname that will be \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD6_STRINGS_1, (SEC_DIALOG_STRING_BASE + 140), "\
used to identify this Certificate Authority, for example <b>Mozilla's \
Certificate Shack</b>.<p>Nickname: <font size=4><input type=text \
%-cont-%")

ResDef(XP_CA_CERT_DOWNLOAD6_STRINGS_2, (SEC_DIALOG_STRING_BASE + 141), "\
name=nickname></font>")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_CA_CERT_DOWNLOAD7_STRINGS, (SEC_DIALOG_STRING_BASE + 142), "\
%sec-banner-begin%%0%%sec-banner-end%\
By rejecting this \
Certificate Authority, you have told "MOZ_NAME_FULL" not to connect \
%-cont-%")
#else
ResDef(XP_CA_CERT_DOWNLOAD7_STRINGS, (SEC_DIALOG_STRING_BASE + 142), "\
%sec-banner-begin%%0%%sec-banner-end%\
By rejecting this \
Certificate Authority, you have told "MOZ_NAME_BRAND" Communicator not to connect \
%-cont-%")
#endif

ResDef(XP_CA_CERT_DOWNLOAD7_STRINGS_1, (SEC_DIALOG_STRING_BASE + 143), "\
to and receive information from any site that it certifies without \
prompting you.")

ResDef(XP_EMPTY_STRINGS, (SEC_DIALOG_STRING_BASE + 144), "\
%0%")

ResDef(XP_WHICH_CERT_CLIENT_AUTH_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 145), "\
%sec-banner-begin%%0%%sec-banner-end%\
The site '%1%' \
has requested client authentication.<p>Here is the \
%-cont-%")

ResDef(XP_WHICH_CERT_CLIENT_AUTH_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 146), "\
site's certificate:<hr><table><tr><td valign=top><font size=2>Certificate \
for: <br>Signed by: <br>Encryption: </font></td><td valign=top><font \
%-cont-%")

ResDef(XP_WHICH_CERT_CLIENT_AUTH_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 147), "\
size=2>%2%<br>%3%<br>%4% Grade (%5% with %6%-bit secret key)</font></td>\
<td valign=bottom><input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\
%-cont-%")

ResDef(XP_WHICH_CERT_CLIENT_AUTH_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 148), "\
\042></td></tr></table><hr>Select Your Certificate:<select name=cert>\
%7%</select>%8%")

ResDef(XP_NO_CERT_CLIENT_AUTH_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 149), "\
%sec-banner-begin%%0%%sec-banner-end%\
The site '%1%' \
has requested client authentication, but you do not have a Personal \
%-cont-%")

ResDef(XP_NO_CERT_CLIENT_AUTH_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 150), "\
Certificate to authenticate yourself.  The site may choose not to give \
you access without one.%2%")

ResDef(XP_BROWSER_SEC_INFO_ENCRYPTED_STRINGS, (SEC_DIALOG_STRING_BASE + 151), "\
%sec-banner-begin%%0%%sec-banner-end%\
<B>All of the \
files that you have requested were encrypted.</B><p> This means that the \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_ENCRYPTED_STRINGS_1, (SEC_DIALOG_STRING_BASE + 152), "\
files that make up the document are sent to you encrypted for privacy \
while in transit.<p> For more details on the encryption of this \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_ENCRYPTED_STRINGS_2, (SEC_DIALOG_STRING_BASE + 153), "\
document, open Document Information.<p> <center><input type=\042submit\042 \
name=\042button\042 value=\042%ok%\042><input type=\042submit\042 name=\042button\042 \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_ENCRYPTED_STRINGS_3, (SEC_DIALOG_STRING_BASE + 154), "\
value=\042%showdocinfo%\042></center>%1%")

ResDef(XP_BROWSER_SEC_INFO_MIXED_STRINGS, (SEC_DIALOG_STRING_BASE + 155), "\
<img src=about:security?banner-mixed>%0%<br clear=all><p><B>Some of the \
files that you have requested were encrypted.</B><p> Some of these files \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_MIXED_STRINGS_1, (SEC_DIALOG_STRING_BASE + 156), "\
are sent to you encrypted for privacy while in transit. Others are not \
encrypted and can be observed by a third party while in transit.<p>To \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_MIXED_STRINGS_2, (SEC_DIALOG_STRING_BASE + 157), "\
find out exactly which files were encrypted and which were not, open \
Document Information.<p> <center><input type=\042submit\042 name=\042button\042 \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_MIXED_STRINGS_3, (SEC_DIALOG_STRING_BASE + 158), "\
value=\042%ok%\042><input type=\042submit\042 name=\042button\042 value=\042\
%showdocinfo%\042></center>%1%")

ResDef(XP_BROWSER_SEC_INFO_CLEAR_STRINGS, (SEC_DIALOG_STRING_BASE + 159), "\
<img src=about:security?banner-insecure>%0%<br clear=all><p><B>None of the \
files that you have requested are encrypted.</B><p>Unencrypted files can \
%-cont-%")

ResDef(XP_BROWSER_SEC_INFO_CLEAR_STRINGS_1, (SEC_DIALOG_STRING_BASE + 160), "\
be observed by a third party while in transit.<p> <center><input \
type=\042submit\042 name=\042button\042 value=\042%ok%\042></center>%1%")

ResDef( XP_SMIME_RC2_CBC_40,        (SEC_DIALOG_STRING_BASE + 161), \
"RC2 encryption in CBC mode with a 40-bit key")

ResDef( XP_SMIME_RC2_CBC_64,        (SEC_DIALOG_STRING_BASE + 162), \
"RC2 encryption in CBC mode with a 64-bit key")

ResDef( XP_SMIME_RC2_CBC_128,       (SEC_DIALOG_STRING_BASE + 163), \
"RC2 encryption in CBC mode with a 128-bit key")

ResDef( XP_SMIME_DES_CBC,       (SEC_DIALOG_STRING_BASE + 164), \
"DES encryption in CBC mode with a 56-bit key")

ResDef( XP_SMIME_DES_EDE3,      (SEC_DIALOG_STRING_BASE + 165), \
"DES EDE3 encryption in CBC mode with a 168-bit key")

ResDef( XP_SMIME_RC5PAD_64_16_40,   (SEC_DIALOG_STRING_BASE + 166), \
"RC5 encryption in CBC mode with a 40-bit key")

ResDef( XP_SMIME_RC5PAD_64_16_64,   (SEC_DIALOG_STRING_BASE + 167), \
"RC5 encryption in CBC mode with a 64-bit key")

ResDef( XP_SMIME_RC5PAD_64_16_128,  (SEC_DIALOG_STRING_BASE + 168), \
"RC5 encryption in CBC mode with a 128-bit key")


/* SEC_DIALOG_STRING_BASE + 169 thru SEC_DIALOG_STRING_BASE + 175 unused */
/* (They used to be strings for payment stuff.) */

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_CONFIG_NO_POLICY_STRINGS, (SEC_DIALOG_STRING_BASE + 176), \
"No valid encryption policy file was found for this English language \n\
version of "MOZ_NAME_PRODUCT".  All encryption and decryption will be disabled.")
#else
ResDef(XP_CONFIG_NO_POLICY_STRINGS, (SEC_DIALOG_STRING_BASE + 176), \
"No valid encryption policy file was found for this English language \n\
version of Communicator.  All encryption and decryption will be disabled.")
#endif

ResDef(XP_CONFIG_NO_CIPHERS_STRINGS, (SEC_DIALOG_STRING_BASE + 177), \
"  (No ciphers are permitted)")

ResDef(XP_CONFIG_MAYBE_CIPHERS_STRINGS, (SEC_DIALOG_STRING_BASE + 178), \
"  (When permitted)")

ResDef(XP_CONFIG_SMIME_CIPHERS_STRINGS, (SEC_DIALOG_STRING_BASE + 179), "\
%sec-banner-begin%%0%%sec-banner-end%\
<h3>Select ciphers to enable for S/MIME %1%</h3><ul>%2%</ul>%3%")

ResDef(XP_CONFIG_SSL2_CIPHERS_STRINGS, (SEC_DIALOG_STRING_BASE + 180), "\
%sec-banner-begin%%0%%sec-banner-end%\
<h3>Select ciphers to enable for SSL v2 %1%</h3><ul>%2%</ul>%3%")

ResDef(XP_CONFIG_SSL3_CIPHERS_STRINGS, (SEC_DIALOG_STRING_BASE + 181), "\
%sec-banner-begin%%0%%sec-banner-end%\
<h3>Select ciphers to enable for SSL v3 %1%</h3><ul>%2%</ul>%3%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_USER_CERT_DOWNLOAD_STRINGS, (SEC_DIALOG_STRING_BASE + 182), "\
%sec-banner-begin%%0%%sec-banner-end%\
You have received a new Certificate.  "MOZ_NAME_PRODUCT" will refer to this \
%-cont-%")
#else
ResDef(XP_USER_CERT_DOWNLOAD_STRINGS, (SEC_DIALOG_STRING_BASE + 182), "\
%sec-banner-begin%%0%%sec-banner-end%\
You have received a new Certificate.  Communicator will refer to this \
%-cont-%")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_USER_CERT_DOWNLOAD_STRINGS_1, (SEC_DIALOG_STRING_BASE + 183), "\
Certificate by the name shown below.  %1%<P>Click <B>OK</B> to install the \
certificate into "MOZ_NAME_PRODUCT" or click <B>Cancel</B> to refuse your new \
%-cont-%")
#else
ResDef(XP_USER_CERT_DOWNLOAD_STRINGS_1, (SEC_DIALOG_STRING_BASE + 183), "\
Certificate by the name shown below.  %1%<P>Click <B>OK</B> to install the \
certificate into Communicator or click <B>Cancel</B> to refuse your new \
%-cont-%")
#endif

ResDef(XP_USER_CERT_DOWNLOAD_STRINGS_2, (SEC_DIALOG_STRING_BASE + 184), "\
Certificate.<HR>Certificate Name: %2%<P><table width=97%%><tr><td>Certificate \
for: %3%<BR>Signed by: %4%</td><td align=right><input type=submit name=button \
%-cont-%")

ResDef(XP_USER_CERT_DOWNLOAD_STRINGS_3, (SEC_DIALOG_STRING_BASE + 185), "\
value=\042%showcert%\042></td></tr></table><HR><P>%5%</font>")

ResDef(XP_USER_CERT_NICKNAME_STRINGS,   (SEC_DIALOG_STRING_BASE + 186), "\
You can use the name provided or enter a new one.")

ResDef(XP_USER_CERT_DL_MOREINFO_STRINGS, (SEC_DIALOG_STRING_BASE + 187), "\
%sec-banner-begin%%0%%sec-banner-end%\
A Certificate is arriving from %1%.<p>This Certificate works in conjunction \
%-cont-%")

ResDef(XP_USER_CERT_DL_MOREINFO_STRINGS_1, (SEC_DIALOG_STRING_BASE + 188), "\
with the corresponding Private Key that was generated for you when you \
requested the Certificate.  Together they can identify you to Web sites and \
via Email.<p>Certificates and Private Keys are much more secure than \
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_USER_CERT_DL_MOREINFO_STRINGS_2, (SEC_DIALOG_STRING_BASE + 189), "\
traditional username and password security methods.  For more information \
about Certificates, choose <b>Security Info</b> from the "MOZ_NAME_PRODUCT" menu.")
#else
ResDef(XP_USER_CERT_DL_MOREINFO_STRINGS_2, (SEC_DIALOG_STRING_BASE + 189), "\
traditional username and password security methods.  For more information \
about Certificates, choose <b>Security Info</b> from the Communicator menu.")
#endif

ResDef(XP_USER_CERT_SAVE_STRINGS,   (SEC_DIALOG_STRING_BASE + 190), "\
%sec-banner-begin%%0%%sec-banner-end%\
You should make a copy of your new Certificate.<p>If you lose your Certificate \
%-cont-%")

ResDef(XP_USER_CERT_SAVE_STRINGS_1, (SEC_DIALOG_STRING_BASE + 191), "\
it <b>cannot be recovered</b>.  Only you hold your Private Key.  Without it \
you will not be able to read any email that you received using that \
%-cont-%")

ResDef(XP_USER_CERT_SAVE_STRINGS_2, (SEC_DIALOG_STRING_BASE + 192), "\
Certificate.<p>To make a copy, click <b>Save As</b> and decide where you \
would like to save your Certificate.  If possible, you should save it on a \
%-cont-%")

ResDef(XP_USER_CERT_SAVE_STRINGS_3, (SEC_DIALOG_STRING_BASE + 193), "\
floppy disk that you keep in a safe location.<p><input type=submit \
name=button value=\042%saveas%\042>")

ResDef(XP_USER_CERT_SAVE_TITLE,     (SEC_DIALOG_STRING_BASE + 194), "\
Save User Certificate")

/* unused               (SEC_DIALOG_STRING_BASE + 195) */

/* unused               (SEC_DIALOG_STRING_BASE + 196) */

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_KEY_GEN_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 197), "\
%sec-banner-begin%%0%%sec-banner-end%\
When you click OK, "MOZ_NAME_PRODUCT" will generate a Private Key for your \
%-cont-%")
#else
ResDef(XP_KEY_GEN_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 197), "\
%sec-banner-begin%%0%%sec-banner-end%\
When you click OK, Communicator will generate a Private Key for your \
%-cont-%")
#endif

ResDef(XP_KEY_GEN_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 198), "\
Certificate.  This may take a few minutes.<P><B>Important: If you \
interrupt this process, you will have to reapply for the Certificate.\
</B>%1% %2% %3%<P>")

ResDef(XP_KEY_GEN_MOREINFO_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 199), "\
Key Generation Info")

ResDef(XP_CERT_DL_MOREINFO_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 200), "\
Certificate Download Info")


ResDef(XP_CERT_EXPIRED_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 201), "\
%sec-banner-begin%%0%%sec-banner-end%\
%1% is a site \
that uses encryption to protect transmitted information.  However the \
%-cont-%")

ResDef(XP_CERT_EXPIRED_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 202), "\
digital Certificate that identifies this site has expired.  This may be \
because the certificate has actually expired, or because the date on \
%-cont-%")

ResDef(XP_CERT_EXPIRED_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 203), "\
your computer is wrong.<p>The certificate expires on %2%.<p>Your \
computer's date is set to %3%.  If this date is incorrect, then you \
%-cont-%")

ResDef(XP_CERT_EXPIRED_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 204), "\
should reset the date on your computer.<p>You may continue or cancel \
this connection.%4%")

ResDef(XP_CERT_NOT_YET_GOOD_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 205), "\
%sec-banner-begin%%0%%sec-banner-end%\
%1% is a site \
that uses encryption to protect transmitted information.  However the \
%-cont-%")

ResDef(XP_CERT_NOT_YET_GOOD_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 206), "\
digital Certificate that identifies this site is not yet valid.  This \
may be because the certificate was installed too soon by the site \
%-cont-%")

ResDef(XP_CERT_NOT_YET_GOOD_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 207), "\
administrator, or because the date on your computer is wrong.<p>The \
certificate is valid beginning %2%.<p>Your computer's date is set to \
%-cont-%")

ResDef(XP_CERT_NOT_YET_GOOD_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 208), "\
%3%.  If this date is incorrect, then you should reset the date on your \
computer.<p>You may continue or cancel this connection.%4%")

ResDef(XP_CA_EXPIRED_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 209), "\
%sec-banner-begin%%0%%sec-banner-end%\
%1% is a site \
that uses encryption to protect transmitted information.  However one of \
%-cont-%")

ResDef(XP_CA_EXPIRED_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 210), "\
the Certificate Authorities that identifies this site has expired.  This \
may be because a certificate has actually expired, or because the date \
%-cont-%")

ResDef(XP_CA_EXPIRED_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 211), "\
on your computer is wrong. Press the More Info button to see details of \
the expired certificate.<hr><table cellspacing=0 cellpadding=0><tr><td \
%-cont-%")

ResDef(XP_CA_EXPIRED_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 212), "\
valign=top><font size=2>Certificate Authority: <br>Expiration Date: \
</font></td><td valign=top><font size=2>%2%<br>%3%</font></td><td \
%-cont-%")

ResDef(XP_CA_EXPIRED_DIALOG_STRINGS_4, (SEC_DIALOG_STRING_BASE + 213), "\
valign=center align=right><input type=\042submit\042 name=\042button\042 \
value=\042%moreinfo%\042></td></tr></table><hr>Your computer's date is set \
%-cont-%")

ResDef(XP_CA_EXPIRED_DIALOG_STRINGS_5, (SEC_DIALOG_STRING_BASE + 214), "\
to %4%.  If this date is incorrect, then you should reset the date on \
your computer.<p>You may continue or cancel this connection.")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 215), "\
%sec-banner-begin%%0%%sec-banner-end%\
%1% is a site \
that uses encryption to protect transmitted information.  However one of \
%-cont-%")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 216), "\
the Certificate Authorities that identifies this site is not yet valid.  \
This may be because a certificate was install too soon by the site \
%-cont-%")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 217), "\
administrator, or because the date on your computer is wrong. Press the \
More Info button to see details of the expired certificate.<hr><table \
%-cont-%")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 218), "\
cellspacing=0 cellpadding=0><tr><td valign=top><font size=2>Certificate \
Authority: <br>Certificate Valid On: </font></td><td valign=top><font \
%-cont-%")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS_4, (SEC_DIALOG_STRING_BASE + 219), "\
size=2>%2%<br>%3%</font></td><td valign=center align=right><input \
type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042></td></tr></table>\
%-cont-%")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS_5, (SEC_DIALOG_STRING_BASE + 220), "\
<hr>Your computer's date is set to %4%.  If this date is incorrect, then \
you should reset the date on your computer.<p>You may continue or cancel \
%-cont-%")

ResDef(XP_CA_NOT_YET_GOOD_DIALOG_STRINGS_6, (SEC_DIALOG_STRING_BASE + 221), "\
this connection.")

ResDef(XP_SEC_CANCEL, (SEC_DIALOG_STRING_BASE + 222), "\
Cancel")

ResDef(XP_SEC_OK, (SEC_DIALOG_STRING_BASE + 223), "\
OK")

ResDef(XP_SEC_CONTINUE, (SEC_DIALOG_STRING_BASE + 224), "\
Continue")

/* These must match the kludge versions below */

ResDef(XP_SEC_NEXT, (SEC_DIALOG_STRING_BASE + 225), "\
Next&gt;")

ResDef(XP_SEC_BACK, (SEC_DIALOG_STRING_BASE + 226), "\
&lt;Back")

ResDef(XP_SEC_FINISHED, (SEC_DIALOG_STRING_BASE + 227), "\
Finish")

ResDef(XP_SEC_MOREINFO, (SEC_DIALOG_STRING_BASE + 228), "\
More Info...")

ResDef(XP_SEC_SHOWCERT, (SEC_DIALOG_STRING_BASE + 229), "\
Show Certificate")

ResDef(XP_SEC_SHOWORDER, (SEC_DIALOG_STRING_BASE + 230), "\
Show Order")

ResDef(XP_SEC_SHOWDOCINFO, (SEC_DIALOG_STRING_BASE + 231), "\
Show Document Info")

/* These must match the original versions above */

ResDef(XP_SEC_NEXT_KLUDGE, (SEC_DIALOG_STRING_BASE + 232), "\
Next>")

ResDef(XP_SEC_BACK_KLUDGE, (SEC_DIALOG_STRING_BASE + 233), "\
<Back")

ResDef(XP_SEC_SAVEAS,           (SEC_DIALOG_STRING_BASE + 234), "\
Save As...")

/* SEC_DIALOG_STRING_BASE + 235 unused */

ResDef(XP_ALERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 236), "\
Alert")

ResDef(XP_VIEWCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 237), "\
View A Certificate")

ResDef(XP_CERTDOMAIN_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 238), "\
Certificate Name Check")

ResDef(XP_CERTISEXP_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 239), "\
Certificate Is Expired")

ResDef(XP_CERTNOTYETGOOD_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 240), "\
Certificate Is Not Yet Good")

ResDef(XP_CAEXP_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 241), "\
Certificate Authority Is Expired")

ResDef(XP_CANOTYETGOOD_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 242), "\
Certificate Authority Is Not Yet Good")

ResDef(XP_ENCINFO_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 243), "\
Encryption Information")

ResDef(XP_VIEWPCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 244), "\
View A Personal Certificate")

ResDef(XP_DELPCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 245), "\
Delete A Personal Certificate")

ResDef(XP_DELSCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 246), "\
Delete A Site Certificate")

ResDef(XP_DELCACERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 247), "\
Delete A Certificate Authority")

ResDef(XP_EDITSCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 248), "\
Edit A Site Certificate")

ResDef(XP_EDITCACERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 249), "\
Edit A Certification Authority")

ResDef(XP_NOPCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 250), "\
No User Certificate")

ResDef(XP_SELPCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 251), "\
Select A Certificate")

ResDef(XP_SECINFO_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 252), "\
Security Information")

ResDef(XP_GENKEY_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 253), "\
Generate A Private Key")

ResDef(XP_NEWSCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 254), "\
New Site Certificate")

ResDef(XP_NEWCACERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 255), "\
New Certificate Authority")

ResDef(XP_NEWPCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 256), "\
New User Certificate")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_SETUPNSPW_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 257), "\
Setting Up Your "MOZ_NAME_PRODUCT" Password")
#else
ResDef(XP_SETUPNSPW_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 257), "\
Setting Up Your Communicator Password")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_CHANGENSPW_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 258), "\
Change Your "MOZ_NAME_PRODUCT" Password")
#else
ResDef(XP_CHANGENSPW_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 258), "\
Change Your Communicator Password")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_ENABLENSPW_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 259), "\
Enable Your "MOZ_NAME_PRODUCT" Password")
#else
ResDef(XP_ENABLENSPW_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 259), "\
Enable Your Communicator Password")
#endif

ResDef(XP_PWERROR_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 260), "\
Password Error")

ResDef(XP_CONFCIPHERS_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 261), "\
Configure Ciphers")

ResDef(XP_CLIENT_CERT_EXPIRED_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 262), "\
%sec-banner-begin%%0%%sec-banner-end%\
The certificate that you have selected has expired and may \
%-cont-%")

ResDef(XP_CLIENT_CERT_EXPIRED_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 263), "\
be rejected by the server.  You may press Continue to send it \
anyway, or Cancel to abort this connection.")

ResDef(XP_CLIENT_CERT_EXPIRED_RENEWAL_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 264), "\
%sec-banner-begin%%0%%sec-banner-end%\
<table><tr><td>The certificate that you have selected has expired and may \
%-cont-%")

ResDef(XP_CLIENT_CERT_EXPIRED_RENEWAL_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 265), "\
be rejected by the server.  You may press %continue% to send it \
anyway, or %cancel% to abort this connection.  To renew your \
%-cont-%")

ResDef(XP_CLIENT_CERT_EXPIRED_RENEWAL_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 266), "\
certificate press the %renew% button.</td><td><input type=submit \
name=button value=%renew%></td></tr></table>")

ResDef(XP_SEC_RENEW, (SEC_DIALOG_STRING_BASE + 267), "\
Renew")

ResDef(XP_CLIENT_CERT_NOT_YET_GOOD_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 268), "\
%sec-banner-begin%%0%%sec-banner-end%\
The certificate that you have selected is not yet valid and may \
be rejected by the server.  You may press Continue to send it \
anyway, or Cancel to abort this connection.")

ResDef(XP_CLCERTEXP_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 269), "\
Your Certificate Is Expired")

ResDef(XP_ASKUSER, (SEC_DIALOG_STRING_BASE + 270), "\
Ask every time")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_PICKAUTO, (SEC_DIALOG_STRING_BASE + 271), "\
Let "MOZ_NAME_PRODUCT" choose automatically")
#else
ResDef(XP_PICKAUTO, (SEC_DIALOG_STRING_BASE + 271), "\
Let Communicator choose automatically")
#endif

ResDef(XP_HIGH_GRADE, (SEC_DIALOG_STRING_BASE + 272), "\
1024 (High Grade)")

ResDef(XP_MEDIUM_GRADE, (SEC_DIALOG_STRING_BASE + 273), "\
 768 (Medium Grade)")

ResDef(XP_LOW_GRADE, (SEC_DIALOG_STRING_BASE + 274), "\
 512 (Low Grade)")

ResDef(XP_VIEW_CERT_POLICY, (SEC_DIALOG_STRING_BASE + 275), "\
View Certificate Policy")

ResDef(XP_CHECK_CERT_STATUS, (SEC_DIALOG_STRING_BASE + 276), "\
Check Certificate Status")

ResDef(XP_SSL2_RC4_128, (SEC_DIALOG_STRING_BASE + 277), "\
RC4 encryption with a 128-bit key")

ResDef(XP_SSL2_RC2_128, (SEC_DIALOG_STRING_BASE + 278), "\
RC2 encryption with a 128-bit key")

ResDef(XP_SSL2_DES_192_EDE3, (SEC_DIALOG_STRING_BASE + 279), "\
Triple DES encryption with a 168-bit key")

ResDef(XP_SSL2_DES_64, (SEC_DIALOG_STRING_BASE + 280), "\
DES encryption with a 56-bit key")

ResDef(XP_SSL2_RC4_40, (SEC_DIALOG_STRING_BASE + 281), "\
RC4 encryption with a 40-bit key")

ResDef(XP_SSL2_RC2_40, (SEC_DIALOG_STRING_BASE + 282), "\
RC2 encryption with a 40-bit key")

ResDef(XP_SSL3_RSA_RC4_128_MD5, (SEC_DIALOG_STRING_BASE + 283), "\
RC4 encryption with a 128-bit key and an MD5 MAC")

ResDef(XP_SSL3_RSA_3DES_SHA, (SEC_DIALOG_STRING_BASE + 284), "\
Triple DES encryption with a 168-bit key and a SHA-1 MAC")

ResDef(XP_SSL3_RSA_DES_SHA, (SEC_DIALOG_STRING_BASE + 285), "\
DES encryption with a 56-bit key and a SHA-1 MAC")

ResDef(XP_SSL3_RSA_RC4_40_MD5, (SEC_DIALOG_STRING_BASE + 286), "\
RC4 encryption with a 40-bit key and an MD5 MAC")

ResDef(XP_SSL3_RSA_RC2_40_MD5, (SEC_DIALOG_STRING_BASE + 287), "\
RC2 encryption with a 40-bit key and an MD5 MAC")

ResDef(XP_SSL3_RSA_NULL_MD5, (SEC_DIALOG_STRING_BASE + 288), "\
No encryption with an MD5 MAC")

ResDef(XP_DIALOG_SHOW_CRLS, (SEC_DIALOG_STRING_BASE + 289), "\
<h3>CRL List:</h3>\
<table><tr><td><select name=crl size=10>%0%</select>\
</td><td valign=\042middle\042>%-cont-%")

ResDef(XP_DIALOG_SHOW_CRLS_1, (SEC_DIALOG_STRING_BASE + 290), "\
<input type=\042submit\042 name=\042button\042 value=\042%new%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_SHOW_CRLS_2, (SEC_DIALOG_STRING_BASE + 291), "\
<input type=\042submit\042 name=\042button\042 value=\042%reload%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_SHOW_CRLS_3, (SEC_DIALOG_STRING_BASE + 292), "\
<input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_SHOW_CRLS_4, (SEC_DIALOG_STRING_BASE + 293), "\
<input type=\042submit\042 name=\042button\042 value=\042%delete%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_SHOW_CRLS_5, (SEC_DIALOG_STRING_BASE + 294), "\
</td></tr></table>")

ResDef(XP_SEC_NEW, (SEC_DIALOG_STRING_BASE + 295), "New/Edit ...")

ResDef(XP_SEC_RELOAD, (SEC_DIALOG_STRING_BASE + 296), "\
Reload")

ResDef(XP_SEC_DELETE, (SEC_DIALOG_STRING_BASE + 297), "\
Delete")

ResDef(XP_SSL3_RSA_FIPS_3DES_SHA, (SEC_DIALOG_STRING_BASE + 298), "\
FIPS 140-1 compliant triple DES encryption and SHA-1 MAC")

ResDef(XP_SSL3_RSA_FIPS_DES_SHA, (SEC_DIALOG_STRING_BASE + 299), "\
FIPS 140-1 compliant DES encryption and SHA-1 MAC")

ResDef(XP_DIALOG_EDIT_MODULE_TITLE, (SEC_DIALOG_STRING_BASE + 307), "\
Edit Security Module")

ResDef(XP_DIALOG_NEW_MODULE_TITLE, (SEC_DIALOG_STRING_BASE + 308), "\
Create a New Security Module")

/* Ciphers */
ResDef(XP_SSL3_FORTEZZA_SHA, (SEC_DIALOG_STRING_BASE + 309), "\
FORTEZZA encryption with a 80-bit key and an SHA-1 MAC")

ResDef(XP_SSL3_FORTEZZA_RC4_SHA, (SEC_DIALOG_STRING_BASE + 310), "\
FORTEZZA authentication with RC4 128-bit key and an SHA-1 MAC")

ResDef(XP_SSL3_FORTEZZA_NULL_SHA, (SEC_DIALOG_STRING_BASE + 311), "\
No encryption with FORTEZZA authentication and an SHA-1 MAC")


ResDef(XP_SEC_ENTER_PWD, (SEC_DIALOG_STRING_BASE + 312), "\
Please enter the password or the pin for\n\
%s.")

ResDef(XP_SSO_GET_PW_STRINGS, (SEC_DIALOG_STRING_BASE + 313), "\
%sec-banner-begin%%0%%sec-banner-end%\
The %1% has not been initialized with a User PIN or Password.  In order \
%-cont-%")

ResDef(XP_SSO_GET_PW_STRINGS_1, (SEC_DIALOG_STRING_BASE + 314), "\
to initialize this card, you must enter the Administration or Site Security \
Password.  If you do not know this password, please hit <B>cancel</B>, and \
%-cont-%")

ResDef(XP_SSO_GET_PW_STRINGS_2, (SEC_DIALOG_STRING_BASE + 315), "\
take this card back to your issuer to be initialized.<p>Enter adminstration \
Password for the %2%:<input type=password name=ssopassword>.")

ResDef(XP_PW_RETRY_SSO_STRINGS, (SEC_DIALOG_STRING_BASE + 316), "\
%sec-banner-begin%%0%%sec-banner-end%\
The administration password you entered for the %1% was incorrect.  \
%-cont-%")

ResDef(XP_PW_RETRY_SSO_STRINGS_1, (SEC_DIALOG_STRING_BASE + 317), "\
Many cards disable themselves after too many incorrect password attempts.  \
If you do not know this password, please hit <B>cancel</B>, and take this card \
%-cont-%")

ResDef(XP_PW_RETRY_SSO_STRINGS_2, (SEC_DIALOG_STRING_BASE + 318), "\
back to your issuer to be initialized.<p>Enter adminstration Password for \
the %2%:<input type=password name=ssopassword>.")

ResDef(XP_PW_SETUP_SSO_FAIL_STRINGS, (SEC_DIALOG_STRING_BASE + 319), "\
%sec-banner-begin%%0%%sec-banner-end%\
The %1% could not be initialized because of the following error:<p> \
%2%")

ResDef(XP_SEC_EDIT, (SEC_DIALOG_STRING_BASE + 320), "\
Edit Defaults...")

ResDef(XP_SEC_LOGIN, (SEC_DIALOG_STRING_BASE + 321), "\
Login")

ResDef(XP_SEC_LOGOUT, (SEC_DIALOG_STRING_BASE + 322), "\
Logout")

ResDef(XP_SECURITY_ADVISOR, (SEC_DIALOG_STRING_BASE + 323), "\
%0%%1%%2%%3%%4%%5%")

ResDef(XP_SECURITY_ADVISOR_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 324), "\
Security Info")

ResDef(XP_SEC_SETPASSWORD, (SEC_DIALOG_STRING_BASE + 325), "\
Set Password...")

ResDef(XP_SEC_NO_LOGIN_NEEDED, (SEC_DIALOG_STRING_BASE + 326), "\
Slot or Token does not require a login.")

ResDef(XP_SEC_ALREADY_LOGGED_IN, (SEC_DIALOG_STRING_BASE + 327), "\
Slot or Token is already logged in.")

ResDef(XP_KEY_GEN_TOKEN_SELECT, (SEC_DIALOG_STRING_BASE + 328), "\
<p>Select the card or database you wish to generate your key in:\
<SELECT name=\042tokenName\042>")

ResDef(XP_SEC_SLOT_READONLY, (SEC_DIALOG_STRING_BASE + 329), "\
Token %s is write protected, certs and keys cannot be deleted")

ResDef(PK11_COULD_NOT_INIT_TOKEN, (SEC_DIALOG_STRING_BASE + 331), "\
Slot failed to Initialize.")

ResDef(PK11_USER_SELECTED, (SEC_DIALOG_STRING_BASE + 332), "\
User has manually disabled this slot.")

ResDef(PK11_TOKEN_VERIFY_FAILED, (SEC_DIALOG_STRING_BASE + 333), "\
Token failed startup tests.")

ResDef(PK11_TOKEN_NOT_PRESENT, (SEC_DIALOG_STRING_BASE + 334), "\
Permanent Token not present.")

ResDef(XP_JAVA_SECURITY_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 335), "\
Java Security")

ResDef(XP_DELETE_JAVA_PRIV_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 336), "\
Java Security (Delete Privilege)")

ResDef(XP_EDIT_JAVA_PRIV_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 337), "\
Java Security (Edit Privileges)")

ResDef(XP_DELETE_JAVA_PRIN_STRINGS, (SEC_DIALOG_STRING_BASE + 338), "\
Are you sure that you want to delete all the privileges for all applets \
and scripts from <b>%0%</b>?")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS, (SEC_DIALOG_STRING_BASE + 339), "\
Allow applets and scripts from <b> %0% </b> to have the following access \
%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_1, (SEC_DIALOG_STRING_BASE + 340), "\
<table><tr><td colspan=3>Always:</td></tr> \
<tr><td><select name=target size=3> %1% </select></td> \
%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_2, (SEC_DIALOG_STRING_BASE + 341), "\
<td></td><td> <input type=\042submit\042 name=\042button\042 \
value=\042%delete%\042></input>%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_3, (SEC_DIALOG_STRING_BASE + 342), "\
<input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042>\
</input></td></tr></table>%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_4, (SEC_DIALOG_STRING_BASE + 343), "\
<table><tr><td colspan=3>For this session only:</td></tr> \
<tr><td><select name=target size=3> %2% </select></td> \
%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_5, (SEC_DIALOG_STRING_BASE + 344), "\
<td></td><td> <input type=\042submit\042 name=\042button\042 \
value=\042%delete%\042></input>%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_6, (SEC_DIALOG_STRING_BASE + 345), "\
<input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042>\
</input></td></tr></table>%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_7, (SEC_DIALOG_STRING_BASE + 346), "\
<table><tr><td colspan=3>Never:</td></tr> \
<tr><td><select name=target size=3> %3% </select></td> \
%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_8, (SEC_DIALOG_STRING_BASE + 347), "\
<td></td><td> <input type=\042submit\042 name=\042button\042 \
value=\042%delete%\042></input>%-cont-%")

ResDef(XP_EDIT_JAVA_PRIVILEGES_STRINGS_9, (SEC_DIALOG_STRING_BASE + 348), "\
<input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042>\
</input></td></tr></table>")

ResDef(XP_DELETE_JAVA_PRIV_STRINGS, (SEC_DIALOG_STRING_BASE + 349), "\
Are you sure that you want to delete the <b>%0%</b> privileges for \
all applets and scripts from <b>%1%</b>?")

ResDef(XP_MOREINFO_JAVA_PRIV_STRINGS, (SEC_DIALOG_STRING_BASE + 350), "\
<b> %0% </b> is a <b> %1% </b> access.<br> \
<ul>It consists of:<br><select name=details size=6> %2% </select></ul>")

/* 0 is cert name, 1 risk, 2 targets, 3 view cert button
 */
ResDef(XP_SIGNED_CERT_PRIV_STRINGS, (SEC_DIALOG_STRING_BASE + 351), "\
<form><table BORDER=0><font SIZE=3> \
JavaScript or a Java applet from \042<b>%0%</b>\042 is requesting additional \
privileges.<br><br><br><br> \
%-cont-%")

ResDef(XP_SIGNED_CERT_PRIV_STRINGS_1, (SEC_DIALOG_STRING_BASE + 352), "\
Granting the following is <b>%1% risk</b>:<br><br> \
%-cont-%")

ResDef(XP_SIGNED_CERT_PRIV_STRINGS_2, (SEC_DIALOG_STRING_BASE + 353), "\
<center><select multiple name=target size=4> %2%</select></center><br> \
%-cont-%")

ResDef(XP_SIGNED_CERT_PRIV_STRINGS_3, (SEC_DIALOG_STRING_BASE + 354), "\
<div align=right> \
<input type=\042submit\042 name=\042detailsbutton\042 value=\042%details%\042> \
</input></div><br><br> \
%-cont-%")

ResDef(XP_SIGNED_CERT_PRIV_STRINGS_4, (SEC_DIALOG_STRING_BASE + 355), "\
<input type=checkbox name=remember>Remember this decision<br> \
%-cont-%")

ResDef(XP_SIGNED_CERT_PRIV_STRINGS_5, (SEC_DIALOG_STRING_BASE + 356), "\
<br> \
<div align=right> \
<input type=\042button\042 value=\042%certificate%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042>&nbsp;&nbsp;\
<input type=\042button\042 value=\042%grant%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042>&nbsp;&nbsp;\
%-cont-%")

ResDef(XP_SIGNED_CERT_PRIV_STRINGS_6, (SEC_DIALOG_STRING_BASE + 357), "\
<input type=\042button\042 value=\042%deny%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042></div> \
<br></table></form>")

/* 0 is cert name, 1 risk, 2 targets.
 */
ResDef(XP_SIGNED_APPLET_PRIV_STRINGS, (SEC_DIALOG_STRING_BASE + 359), "\
<form><table BORDER=0><font SIZE=3> \
JavaScript or a Java applet from \042<b>%0%</b>\042 is requesting additional \
privileges.  It is <b>not digitally signed</b>.<br><br><br><br> \
%-cont-%")

ResDef(XP_SIGNED_APPLET_PRIV_STRINGS_1, (SEC_DIALOG_STRING_BASE + 360), "\
Granting the following is <b>%1% risk</b>:<br><br> \
%-cont-%")

ResDef(XP_SIGNED_APPLET_PRIV_STRINGS_2, (SEC_DIALOG_STRING_BASE + 361), "\
<center><select multiple name=target size=4> %2%</select></center><br> \
%-cont-%")

ResDef(XP_SIGNED_APPLET_PRIV_STRINGS_3, (SEC_DIALOG_STRING_BASE + 362), "\
<div align=right> \
<input type=\042submit\042 name=\042detailsbutton\042 value=\042%details%\042> \
</input></div><br><br> \
%-cont-%")

ResDef(XP_SIGNED_APPLET_PRIV_STRINGS_4, (SEC_DIALOG_STRING_BASE + 363), "\
<input type=checkbox name=remember>Remember this decision<br> \
%-cont-%")

ResDef(XP_SIGNED_APPLET_PRIV_STRINGS_5, (SEC_DIALOG_STRING_BASE + 364), "\
<br> \
<div align=right><input type=\042button\042 value=\042%grant%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042>&nbsp;&nbsp;\
<input type=\042button\042 value=\042%deny%\042 width=80 \
onclick=\042parent.clicker(this,window.parent)\042></div> \
%-cont-%")

ResDef(XP_SIGNED_APPLET_PRIV_STRINGS_6, (SEC_DIALOG_STRING_BASE + 365), "\
<br></table></form>")

ResDef(XP_DIALOG_NEW_MODULE, (SEC_DIALOG_STRING_BASE + 366), "\
<b>Security Module Name:</b> <input name=\042name\042><br>\
<b>Security Module File:</b> <input name=\042library\042><br>")

ResDef(XP_SEC_MODULE_NO_LIB, (SEC_DIALOG_STRING_BASE + 367), "\
You must specify a PKCS #11 Version 2.0 library to load\n")

ResDef(XP_DIALOG_SLOT_INFO, (SEC_DIALOG_STRING_BASE + 368), "\
<b>Slot Description:</b> %0%<br><b>Manufacturer:</b> %1%<br>\
<b>Version Number:</b> %2%<br><b>Firmware Version:</b> %3%<br>\
%-cont-%")

ResDef(XP_DIALOG_SLOT_INFO_1, (SEC_DIALOG_STRING_BASE + 369), "\
%4%<b>Token Name:</b> %5%<br><b>Token Manufacturer:</b>%6%<br>\
<b>Token Model:</b> %7%<br><b>Token Serial Number:</b>%8%<br>\
%-cont-%")

ResDef(XP_DIALOG_SLOT_INFO_2, (SEC_DIALOG_STRING_BASE + 370), "\
<b>Token Version:</b> %9%<br><b>Token Firmware Version:</b> %10%<br>\
<b>Login Type:</b> %11%<br><b>State:</b>%12%%13%%14%")

ResDef(XP_DIALOG_SLOT_INFO_TITLE, (SEC_DIALOG_STRING_BASE + 371), "\
Token/Slot Information")

ResDef(XP_SLOT_LOGIN_REQUIRED, (SEC_DIALOG_STRING_BASE + 372), "\
Login Required")

ResDef(XP_SLOT_NO_LOGIN_REQUIRED, (SEC_DIALOG_STRING_BASE + 373), "\
Public (no login required)")

ResDef(XP_SLOT_READY, (SEC_DIALOG_STRING_BASE + 374), "\
Ready")

ResDef(XP_SLOT_NOT_LOGGED_IN, (SEC_DIALOG_STRING_BASE + 375), "\
<font color=red>Not Logged In</font>")

ResDef(XP_SLOT_UNITIALIZED, (SEC_DIALOG_STRING_BASE + 376), "\
<font color=red>Uninitialized</font>")

ResDef(XP_SLOT_NOT_PRESENT, (SEC_DIALOG_STRING_BASE + 377), "\
<font color=red>Not Present</font>")

ResDef(XP_SLOT_DISABLED, (SEC_DIALOG_STRING_BASE + 378), "\
<font color=red>Disabled(")

ResDef(XP_SLOT_DISABLED_2, (SEC_DIALOG_STRING_BASE + 379), "\
)</font>")

ResDef(XP_SLOT_PASSWORD_INIT, (SEC_DIALOG_STRING_BASE + 380), "\
 Initialize Token")

ResDef(XP_SLOT_PASSWORD_CHANGE, (SEC_DIALOG_STRING_BASE + 381), "\
 Change Password ")

ResDef(XP_SLOT_PASSWORD_SET, (SEC_DIALOG_STRING_BASE + 382), "\
   Set Password  ")

ResDef(XP_SLOT_PASSWORD_NO, (SEC_DIALOG_STRING_BASE + 383), "\
   No Password   ")

ResDef(XP_DOWNLOAD_EMAIL_CERT_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 384), "\
%sec-banner-begin%%0%%sec-banner-end%\
You are downloading the e-mail certificate of another user.  After accepting \
%-cont-%")

ResDef(XP_DOWNLOAD_EMAIL_CERT_DIALOG_STRINGS_1, (SEC_DIALOG_STRING_BASE + 385), "\
this certificate you will be able to send encrypted e-mail to this user. \
Press the More Info button to see details of the e-mail certificate. \
%-cont-%")

ResDef(XP_DOWNLOAD_EMAIL_CERT_DIALOG_STRINGS_2, (SEC_DIALOG_STRING_BASE + 386), "\
<hr><table cellspacing=0 cellpadding=0><tr><td valign=top>\
<table cellspacing=0 cellpadding=0><tr><td><font size=2>Certificate For:\
%-cont-%")

ResDef(XP_DOWNLOAD_EMAIL_CERT_DIALOG_STRINGS_3, (SEC_DIALOG_STRING_BASE + 387), "\
</font></td><td><font size=2>%1%</font></td></tr><tr><td><font size=2>\
Email Address:</font></td><td><font size=2>%2%</font></td></tr><tr><td>\
%-cont-%")

ResDef(XP_DOWNLOAD_EMAIL_CERT_DIALOG_STRINGS_4, (SEC_DIALOG_STRING_BASE + 388), "\
<font size=2>Certified By:</font></td><td><font size=2>%3%</font></td></tr>\
</table></td><td valign=center align=right>\
<input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042>\
</td></tr></table><hr>")

ResDef(XP_EMAIL_CERT_DOWNLOAD_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 389), "\
Download an E-Mail Certificate")

ResDef(XP_CA_CERT_SSL_OK_STRING, (SEC_DIALOG_STRING_BASE + 390), "\
Accept this Certificate Authority for Certifying network sites")

ResDef(XP_CA_CERT_EMAIL_OK_STRING, (SEC_DIALOG_STRING_BASE + 391), "\
Accept this Certificate Authority for Certifying e-mail users")

ResDef(XP_CA_CERT_OBJECT_SIGNING_OK_STRING, (SEC_DIALOG_STRING_BASE + 392), "\
Accept this Certificate Authority for Certifying software developers")

ResDef(XP_CERT_MULTI_SUBJECT_SELECT_STRING, (SEC_DIALOG_STRING_BASE + 393), "\
%0%<p>%1%")

ResDef(XP_CERT_MULTI_SUBJECT_SELECT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 394), "\
Select A Certificate")

ResDef(XP_CERT_SELECT_EDIT_STRING, (SEC_DIALOG_STRING_BASE + 395), "\
Please select a certificate to edit:<p>")

ResDef(XP_CERT_SELECT_DEL_STRING, (SEC_DIALOG_STRING_BASE + 396), "\
Please select a certificate to delete:<p>")

ResDef(XP_CERT_SELECT_VIEW_STRING, (SEC_DIALOG_STRING_BASE + 397), "\
Please select a certificate to view:<p>")

ResDef(XP_CERT_SELECT_VERIFY_STRING, (SEC_DIALOG_STRING_BASE + 398), "\
Please select a certificate to verify:<p>")

ResDef(XP_DEL_EMAIL_CERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 399), "\
Delete An E-mail Certificate")

ResDef(XP_DELETE_EMAIL_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 400), "\
Are you sure that you want to delete this E-mail Certificate?<p>%0%")

ResDef(XP_SET_EMAIL_CERT_STRING, (SEC_DIALOG_STRING_BASE + 401), "\
<input type=checkbox name=useformail value=yes %s> \
Make this the default Certificate for signed and encrypted e-mail")

/*
 * NOTE to Translators: The following strings are part of a PKCS #11 standard
 * they must be exactly 32 bytes long (space padded). If they are not,
 * Communicator will revert to it's old strings */
ResDef(SEC_PK11_MANUFACTURER, (SEC_DIALOG_STRING_BASE + 402), "\
"MOZ_NAME_COMPANY"    ")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_PK11_LIBARARY, (SEC_DIALOG_STRING_BASE + 403), "\
"MOZ_NAME_PRODUCT" Internal Crypto Svc   ")
#else
ResDef(SEC_PK11_LIBARARY, (SEC_DIALOG_STRING_BASE + 403), "\
Communicator Internal Crypto Svc")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_PK11_TOKEN, (SEC_DIALOG_STRING_BASE + 404), "\
"MOZ_NAME_PRODUCT" Generic Crypto Svcs   ")
#else
ResDef(SEC_PK11_TOKEN, (SEC_DIALOG_STRING_BASE + 404), "\
Communicator Generic Crypto Svcs")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_PK11_PRIV_TOKEN, (SEC_DIALOG_STRING_BASE + 405), "\
"MOZ_NAME_PRODUCT" Certificate DB        ")
#else
ResDef(SEC_PK11_PRIV_TOKEN, (SEC_DIALOG_STRING_BASE + 405), "\
Communicator Certificate DB     ")
#endif

/*
 * NOTE to Translators: The following strings are part of a PKCS #11 standard
 * they must be exactly 64 bytes long (space padded). If they are not,
 * Communicator will revert to it's old strings */
#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_PK11_SLOT, (SEC_DIALOG_STRING_BASE + 406), "\
"MOZ_NAME_PRODUCT" Internal Cryptographic Services Version 4.0           ")
#else
ResDef(SEC_PK11_SLOT, (SEC_DIALOG_STRING_BASE + 406), "\
Communicator Internal Cryptographic Services Version 4.0        ")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SEC_PK11_PRIV_SLOT, (SEC_DIALOG_STRING_BASE + 407), "\
"MOZ_NAME_PRODUCT" User Private Key and Certificate Services             ")
#else
ResDef(SEC_PK11_PRIV_SLOT, (SEC_DIALOG_STRING_BASE + 407), "\
Communicator User Private Key and Certificate Services          ")
#endif

ResDef(SEC_PK11_FIPS_SLOT, (SEC_DIALOG_STRING_BASE + 408), "\
"MOZ_NAME_BRAND" Internal FIPS-140-1 Cryptographic Services             ")

ResDef(SEC_PK11_FIPS_PRIV_SLOT, (SEC_DIALOG_STRING_BASE + 409), "\
"MOZ_NAME_BRAND" FIPS-140-1 User Private Key Services                   ")

ResDef(XP_VERIFY_CERT_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 410), "\
Verification of the selected certificate failed for the following \
reasons:<p>%0%")

ResDef(XP_VERIFYCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 411), "\
Verify A Certificate")

ResDef(XP_VERIFY_CERT_OK_DIALOG_STRING, (SEC_DIALOG_STRING_BASE + 412), "\
The Certificate has been successfully verified.")

ResDef(XP_VERIFY_ERROR_EXPIRED, (SEC_DIALOG_STRING_BASE + 413), "\
Certificate has expired")

ResDef(XP_VERIFY_ERROR_NOT_CERTIFIED, (SEC_DIALOG_STRING_BASE + 414), "\
Not certified for %s")

ResDef(XP_VERIFY_ERROR_NOT_TRUSTED, (SEC_DIALOG_STRING_BASE + 415), "\
Certificate not trusted")

ResDef(XP_VERIFY_ERROR_NO_CA, (SEC_DIALOG_STRING_BASE + 416), "\
Unable to find Certificate Authority")

ResDef(XP_VERIFY_ERROR_BAD_SIG, (SEC_DIALOG_STRING_BASE + 417), "\
Certificate signature is invalid")

ResDef(XP_VERIFY_ERROR_BAD_CRL, (SEC_DIALOG_STRING_BASE + 418), "\
Certificate Revocation List is invalid")

ResDef(XP_VERIFY_ERROR_REVOKED, (SEC_DIALOG_STRING_BASE + 419), "\
Certificate has been revoked")

ResDef(XP_VERIFY_ERROR_NOT_CA, (SEC_DIALOG_STRING_BASE + 420), "\
Not a valid Certificate Authority")

ResDef(XP_VERIFY_ERROR_INTERNAL_ERROR, (SEC_DIALOG_STRING_BASE + 421), "\
Internal Error")

ResDef(XP_VERIFY_ERROR_SIGNING, (SEC_DIALOG_STRING_BASE + 422), "\
Digital Signing")

ResDef(XP_VERIFY_ERROR_ENCRYPTION, (SEC_DIALOG_STRING_BASE + 423), "\
Encryption")

ResDef(XP_VERIFY_ERROR_CERT_SIGNING, (SEC_DIALOG_STRING_BASE + 424), "\
Certificate Signing")

ResDef(XP_VERIFY_ERROR_CERT_UNKNOWN_USAGE, (SEC_DIALOG_STRING_BASE + 425), "\
Unknown Usage")

ResDef(XP_VERIFY_ERROR_EMAIL_CA, (SEC_DIALOG_STRING_BASE + 426), "\
E-Mail Certification")

ResDef(XP_VERIFY_ERROR_SSL_CA, (SEC_DIALOG_STRING_BASE + 427), "\
Internet Site Certification")

ResDef(XP_VERIFY_ERROR_OBJECT_SIGNING_CA, (SEC_DIALOG_STRING_BASE + 428), "\
Software Developer Certification")

ResDef(XP_VERIFY_ERROR_EMAIL, (SEC_DIALOG_STRING_BASE + 429), "\
E-Mail")

ResDef(XP_VERIFY_ERROR_SSL, (SEC_DIALOG_STRING_BASE + 430), "\
Internet Site")

ResDef(XP_VERIFY_ERROR_OBJECT_SIGNING, (SEC_DIALOG_STRING_BASE + 431), "\
Software Developer")

ResDef(XP_DIALOG_NULL_STRINGS, (SEC_DIALOG_STRING_BASE + 432), "\
%0%")

ResDef(XP_FIPS_MESSAGE_1, (SEC_DIALOG_STRING_BASE + 433), "\
This will replace the "MOZ_NAME_PRODUCT" internal module with the "MOZ_NAME_PRODUCT" FIPS-140-1 \
cryptographic module.\n\nThe FIPS-140-1 cryptographic module limits security ")

ResDef(XP_FIPS_MESSAGE_2, (SEC_DIALOG_STRING_BASE + 434), "\
functions to those approved by the United States Federal Government's internal \
standards.\n\nDo you wish to delete the internal module, anyway?")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_INT_MODULE_MESSAGE_1, (SEC_DIALOG_STRING_BASE + 435), "\
This will replace the FIPS-140-1 cryptographic module with the "MOZ_NAME_PRODUCT" \
internal module.\n\nThis means that "MOZ_NAME_PRODUCT" will no longer be FIPS-140-1 \
compliant (security ")
#else
ResDef(XP_INT_MODULE_MESSAGE_1, (SEC_DIALOG_STRING_BASE + 435), "\
This will replace the FIPS-140-1 cryptographic module with the "MOZ_NAME_PRODUCT" \
internal module.\n\nThis means that Communicator will no longer be FIPS-140-1 \
compliant (security ")
#endif

ResDef(XP_INT_MODULE_MESSAGE_2, (SEC_DIALOG_STRING_BASE + 436), "\
functions to those approved by the United States Federal Government's internal \
standards).\n\nDo you wish to delete the FIPS-140-1 module, anyway?")

ResDef(XP_SEC_FETCH, (SEC_DIALOG_STRING_BASE + 437), "\
Search")

ResDef(XP_DIALOG_FETCH_TITLE, (SEC_DIALOG_STRING_BASE + 438), "\
Search Directory for Certificates")

ResDef(XP_DIALOG_FETCH_CANCEL_BUTTON_STRINGS, (SEC_DIALOG_STRING_BASE + 439), "\
<div align=right><input type=\042button\042 name=\042button\042 \
value=\042%fetch%\042 onclick=\042parent.clicker(this,window.parent)\042 \
width=80>&nbsp;&nbsp;<input type=\042button\042 name=\042button\042 \
value=\042%cancel%\042 onclick=\042parent.clicker(this,window.parent)\042 \
width=80>%0%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_DIALOG_FETCH_STRINGS, (SEC_DIALOG_STRING_BASE + 440), "\
"MOZ_NAME_PRODUCT" will search network Directories for the Security Certificates \
that are used to send other people encrypted mail messages.<p>Enter the exact E-mail \
%-cont-%")
#else
ResDef(XP_DIALOG_FETCH_STRINGS, (SEC_DIALOG_STRING_BASE + 440), "\
Communicator will search network Directories for the Security Certificates \
that are used to send other people encrypted mail messages.<p>Enter the exact E-mail \
%-cont-%")
#endif

ResDef(XP_DIALOG_FETCH_STRINGS2, (SEC_DIALOG_STRING_BASE + 441), "\
addresses of the people you are looking for and press Search. \
<table border=0 cellspacing=0 cellpadding=5>\
<tr><td><b>Directory:</b></td>\
<td><select name=dirmenu>%0%\
%-cont-%")

ResDef(XP_DIALOG_FETCH_STRINGS3, (SEC_DIALOG_STRING_BASE + 442), "\
</select></td>\
</tr>\
%-cont-%")

ResDef(XP_DIALOG_FETCH_STRINGS4, (SEC_DIALOG_STRING_BASE + 443), "\
<tr>\
<td><b>E-mail Addresses:</b></td>\
<td><input type=text name=searchfor size=50></td>\
</tr>\
</table>")

ResDef(XP_DIALOG_ALL_DIRECTORIES, (SEC_DIALOG_STRING_BASE + 444), "\
All Directories")

ResDef(XP_DIALOG_FETCH_RESULTS_TITLE, (SEC_DIALOG_STRING_BASE + 445), "\
Search Results")

ResDef(XP_DIALOG_FETCH_RESULTS_STRINGS, (SEC_DIALOG_STRING_BASE + 446), "\
Press the <b>%ok%</b> button to save the Certificates that were found, or \
<b>%cancel%</b> to discard them.<p>\
Certificates for the following E-Mail users were found in the \
directory:<br> \
%0%<p>%1%%2%")

ResDef(XP_DIALOG_FETCH_NOT_FOUND_STRINGS, (SEC_DIALOG_STRING_BASE + 447), "\
Certificates for the following E-Mail users were not found in the \
directory:<br>")

ResDef(XP_DIALOG_PUBLISH_CERT_TITLE, (SEC_DIALOG_STRING_BASE + 448), "\
Send Your E-Mail Certificate To A Directory")

ResDef(XP_DIALOG_PUBLISH_CERT_STRINGS, (SEC_DIALOG_STRING_BASE + 449), "\
Select the Directory to send your \
Certificate to:<p>\
<select name=dirmenu>%0%\
</select><br>\
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_DIALOG_PUBLISH_CERT_STRINGS2, (SEC_DIALOG_STRING_BASE + 450), "\
"MOZ_NAME_PRODUCT" will send your Security Certificate to a network Directory \
so that other user's can easily find it to send you \
encrypted messages.")
#else
ResDef(XP_DIALOG_PUBLISH_CERT_STRINGS2, (SEC_DIALOG_STRING_BASE + 450), "\
Communicator will send your Security Certificate to a network Directory \
so that other user's can easily find it to send you \
encrypted messages.")
#endif

ResDef(XP_STATUS_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 451), "\
%0%%1%%2%%3%%4%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_DIALOG_FETCH2_STRINGS, (SEC_DIALOG_STRING_BASE + 452), "\
"MOZ_NAME_PRODUCT" will search a Directory for the Security \
Certificates that are needed to send this encrypted message. \
%-cont-%")
#else
ResDef(XP_DIALOG_FETCH2_STRINGS, (SEC_DIALOG_STRING_BASE + 452), "\
Communicator will search a Directory for the Security \
Certificates that are needed to send this encrypted message. \
%-cont-%")
#endif

ResDef(XP_DIALOG_FETCH2_STRINGS2, (SEC_DIALOG_STRING_BASE + 453), "\
<input type=hidden name=searchfor value=\042%0%\042>\
<table border=0 cellspacing=0 cellpadding=5>\
<tr><td><b>Select a Directory:</b></td>\
<td><select name=dirmenu>%1%\
%-cont-%")

ResDef(XP_DIALOG_FETCH2_STRINGS3, (SEC_DIALOG_STRING_BASE + 454), "\
</select></td>\
</tr>\
<tr>\
<td valign=top><b>Searching For:</b></td>\
<td>%2%</td>\
</tr>\
</table>")

ResDef(XP_SENDING_TO_DIRECTORY, (SEC_DIALOG_STRING_BASE + 455), "\
Sending to Directory")

ResDef(XP_SEARCHING_DIRECTORY, (SEC_DIALOG_STRING_BASE + 456), "\
Searching Directory")

ResDef(XP_DIRECTORY_PASSWORD, (SEC_DIALOG_STRING_BASE + 457), "\
Enter Password for Directory")

ResDef(XP_DIRECTORY_ERROR, (SEC_DIALOG_STRING_BASE + 458), "\
An error occurred when communicating with the Directory")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_KEY_GEN_MOREINFO_STRINGS, (SEC_DIALOG_STRING_BASE + 459), "\
%sec-banner-begin%%0%%sec-banner-end%\
"MOZ_NAME_PRODUCT" is about to generate a Private Key for you.  It will be used \
along with the Certificate you are now \
%-cont-%")
#else
ResDef(XP_KEY_GEN_MOREINFO_STRINGS, (SEC_DIALOG_STRING_BASE + 459), "\
%sec-banner-begin%%0%%sec-banner-end%\
Communicator is about to generate a Private Key for you.  It will be used \
along with the Certificate you are now \
%-cont-%")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_KEY_GEN_MOREINFO_STRINGS_1,   (SEC_DIALOG_STRING_BASE + 460), "\
requesting to identify you to Web\
Sites and via Email.  You Private Key never leaves your computer, and if you \
choose, will be protected by a "MOZ_NAME_PRODUCT" password.<P>\
%-cont-%")
#else
ResDef(XP_KEY_GEN_MOREINFO_STRINGS_1,   (SEC_DIALOG_STRING_BASE + 460), "\
requesting to identify you to Web\
Sites and via Email.  You Private Key never leaves your computer, and if you \
choose, will be protected by a Communicator password.<P>\
%-cont-%")
#endif

ResDef(XP_KEY_GEN_MOREINFO_STRINGS_2,   (SEC_DIALOG_STRING_BASE + 461), "\
Passwords are particularly important if you are in an environment where other \
people have access to your computer, either physically or over a network.  \
%-cont-%")

ResDef(XP_KEY_GEN_MOREINFO_STRINGS_3,   (SEC_DIALOG_STRING_BASE + 462), "\
Do not give others your password, because that would allow them to use your \
Certificate to impersonate you.<P>\
%-cont-%")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_KEY_GEN_MOREINFO_STRINGS_4,   (SEC_DIALOG_STRING_BASE + 463), "\
"MOZ_NAME_PRODUCT" uses a complex mathematical operation to generate your private \
key.  It may take up to severeal minutes to complete.  If you interrupt \
%-cont-%")
#else
ResDef(XP_KEY_GEN_MOREINFO_STRINGS_4,   (SEC_DIALOG_STRING_BASE + 463), "\
Communicator uses a complex mathematical operation to generate your private \
key.  It may take up to severeal minutes to complete.  If you interrupt \
%-cont-%")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_KEY_GEN_MOREINFO_STRINGS_5,   (SEC_DIALOG_STRING_BASE + 464), "\
"MOZ_NAME_PRODUCT" during this process, it will not create your key, and you will \
have to reapply for your Certificate.")
#else
ResDef(XP_KEY_GEN_MOREINFO_STRINGS_5,   (SEC_DIALOG_STRING_BASE + 464), "\
Communicator during this process, it will not create your key, and you will \
have to reapply for your Certificate.")
#endif

ResDef(XP_PW_MOREINFO_STRINGS,      (SEC_DIALOG_STRING_BASE + 465), "\
%sec-banner-begin%%0%%sec-banner-end%\
Passwords are particularly important if you are in an environment where other \
%-cont-%")

ResDef(XP_PW_MOREINFO_STRINGS_1,    (SEC_DIALOG_STRING_BASE + 466), "\
people have access to your computer, either physically or over a network.  \
Do not give others your password, because that would allow them to use your \
%-cont-%")

ResDef(XP_PW_MOREINFO_STRINGS_2,    (SEC_DIALOG_STRING_BASE + 467), "\
Certificate to impersonate you.<P>The safest passwords are at least 8 \
characters long, include both letters, and number or symbols, and contain \
no words found in a dictionary.")

ResDef(XP_PKCS12_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 468), "\
Select a Card or Database")

ResDef(XP_PKCS12_SELECT_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 469), "\
Select the card or database you wish to import certificates to: <BR> \
<SELECT NAME=\042tokenName\042 SIZE=10>%0%</SELECT>")

ResDef(XP_VERIFY_NOT_FORTEZZA_CA, (SEC_DIALOG_STRING_BASE + 470), "\
Not a valid FORTEZZA Certificate Authority")

ResDef(XP_VERIFY_NO_PUBLIC_KEY, (SEC_DIALOG_STRING_BASE + 471), "\
Certificate does not have a Recognized Public Key")

#ifdef XP_MAC
ResDef(XP_PREENC_SAVE_PROMPT, (SEC_DIALOG_STRING_BASE + 472), "\
Save this file encrypted [yes] or unencrypted [no]")
#else
ResDef(XP_PREENC_SAVE_PROMPT, (SEC_DIALOG_STRING_BASE + 472), "\
Save this file encrypted [ok] or unencrypted [cancel]")
#endif

ResDef(XP_SEC_ERROR_PWD, (SEC_DIALOG_STRING_BASE + 473), "\
Successive login failures may disable this card or database. \
Password is invalid. Retry?\n\
    %s\n")

ResDef(XP_VERIFY_NO_KRL, (SEC_DIALOG_STRING_BASE + 474),
"No key revocation list for the certificate has been found.\n\
You must load the key revocation list before continuing.")

ResDef(XP_VERIFY_KRL_EXPIRED, (SEC_DIALOG_STRING_BASE + 475),
"The key revocation list for the certificate has expired.\n\
Reload a new key revocation list.")

ResDef(XP_VERIFY_KRL_BAD_SIGNATURE, (SEC_DIALOG_STRING_BASE + 476),
"The key revocation list for the certificate has an invalid signature.\n\
Reload a new key revocation list.")

ResDef(XP_VERIFY_REVOKED_KEY, (SEC_DIALOG_STRING_BASE + 477),
"The key for the certificate has been revoked.")

ResDef(XP_VERIFY_KRL_INVALID, (SEC_DIALOG_STRING_BASE + 478),
"The key revocation list has an invalid format.")

ResDef(XP_VERIFY_BAD_CERT_DOMAIN, (SEC_DIALOG_STRING_BASE + 479),
MOZ_NAME_BRAND" is unable to communicate securely with this site\n\
because the domain to which you are attempting to connect\n\
does not match the domain name in the server's certificate.")

/* Fortezza SMIME strings */
ResDef(XP_SMIME_SKIPJACK, (SEC_DIALOG_STRING_BASE + 480), "\
FORTEZZA SKIPJACK encryption with an 80-bit key")

/* New Security Advisor->Crypto Modules->Edit menu */
ResDef(XP_DIALOG_EDIT_MODULE, (SEC_DIALOG_STRING_BASE + 481), "\
<b>Security Module Name:</b> %0%\
<input type=\042%1%\042 name=\042name\042 value=%2%><br>\
<b>Security Module File:</b> %3%\
<input type=\042%4%\042name=\042library\042 value=%5%><br>\
%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_0, (SEC_DIALOG_STRING_BASE + 482), "\
<b>Manufacturer:</b> %6%<br><b>Description:</b>%8%<br>\
<b>PKCS #11 Version:</b> %7%<br>\
<b>Library Version: </b> %9%<br>%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_1, (SEC_DIALOG_STRING_BASE + 483), "\
<center><table><tr><td><select name=slots size=10 onChange=\042setpass(this)\
\042>%10%</select></td><td valign=\042middle\042>\
<input type=\042submit\042 name=\042button\042 value=\042%moreinfo%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_2, (SEC_DIALOG_STRING_BASE + 484), "\
<input type=\042submit\042 name=\042button\042 value=\042%config%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_3, (SEC_DIALOG_STRING_BASE + 485), "\
<input type=\042submit\042 name=\042button\042 value=\042%login%\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_4, (SEC_DIALOG_STRING_BASE + 486), "\
<input type=\042submit\042 name=\042button\042 value=\042%logout%\042>\
</input><br>\
<input type=\042submit\042 name=\042button\042 value=\042Change Password\042>\
</input><br>%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_5, (SEC_DIALOG_STRING_BASE + 487), "\
</td></tr></table></center><script>\
var login_status = %11%\
function setpass(select){%-cont-%")

ResDef(XP_DIALOG_EDIT_MODULE_6, (SEC_DIALOG_STRING_BASE + 488), "\
for(var i=0;i<select.options.length;i++) {\
   if (select.options[i].selected) {\
    document.forms[0].elements[9].value = login_status[i];\
    return;\
   }\
  }%-cont-%")
 
 ResDef(XP_DIALOG_EDIT_MODULE_7, (SEC_DIALOG_STRING_BASE + 489), "\
  document.forms[0].elements[9].value = \042No Token Selected\042;\
 }\nfunction initarray() {\
  var numargs = initarray.arguments.length;\
  var a; a = new Array(numargs); %-cont-%")
 
 ResDef(XP_DIALOG_EDIT_MODULE_8, (SEC_DIALOG_STRING_BASE + 490), "\
  for ( var i = 0; i < numargs; i++ ) {\
    a[i] = initarray.arguments[i];\
  } return a;\
 }</script>")
 
 ResDef(XP_SEC_CONFIG, (SEC_DIALOG_STRING_BASE + 491), "\
 Config")
 
 /* Config Slot/Token */
 
 ResDef(XP_DIALOG_CONFIG_SLOT, (SEC_DIALOG_STRING_BASE + 492), "\
 <P><B>Module Name:</B> %0%\
 <BR><B>Slot Description:</B> %1%\
 <BR><B>Token Name: </B> %2%\
 <BR><B>Remarks: </B>%3%<P><HR>\
 %-cont-%")
 
 ResDef(XP_DIALOG_CONFIG_SLOT_0, (SEC_DIALOG_STRING_BASE + 493), "\
 <P>\
 <DT><INPUT TYPE=Radio %4% NAME=\042enabledisable\042 VALUE=\042disable\042> <B>\
 Disable this token. </B>\
 %-cont-%")
 
 ResDef(XP_DIALOG_CONFIG_SLOT_1, (SEC_DIALOG_STRING_BASE + 494), "\
 <FORM><P>\
 <DL>\
 <DT><INPUT TYPE=Radio %5% NAME=\042enabledisable\042 VALUE=\042enable\042>\
 <B>Enable this token and turn on the following functions: </B>\
 %6%\
 </FORM>")
 
 
 /* display this if try to configure a slot w/o token */
 ResDef(XP_DIALOG_CONFIG_SLOT_NO_SLOT, (SEC_DIALOG_STRING_BASE + 495), "\
 <P><B>Module Name:</B> %0%\
 <BR><B>Slot Description:</B> %1%\
 <BR><B>Token Name: </B> %2%\
 <P><HR>\
 %-cont-%")
 
 ResDef(XP_DIALOG_CONFIG_SLOT_NO_SLOT_0, (SEC_DIALOG_STRING_BASE + 496), "\
 <FORM> <P>\
 <FONT COLOR=\042FF0000\042 SIZE=+1> %3%<BR>%4%<BR>%5%\
 </FONT></FORM>")
 
 ResDef(XP_DIALOG_CONFIG_SLOT_TITLE, (SEC_DIALOG_STRING_BASE + 497), "\
 Configure Slot")

 /* define prompts for lm_pkcs11.c */
 /* squeezed in this space */
 ResDef(XP_JS_PKCS11_MOD_PROMPT, (SEC_DIALOG_STRING_BASE + 498), "Module Name: ")
 ResDef(XP_JS_PKCS11_DLL_PROMPT, (SEC_DIALOG_STRING_BASE + 499), "File: ")


 /* some Mechanism Type labels */
 ResDef(XP_CKM_RSA_PKCS_LABEL, (SEC_DIALOG_STRING_BASE + 500),\
        "RSA PKCS encryption")
 
 
 /* define some messages for lm_pkcs11.c */
 ResDef(XP_JS_PKCS11_EXTERNAL_DELETED, (SEC_DIALOG_STRING_BASE + 501),
        "External security module successfully deleted")
 ResDef(XP_JS_PKCS11_INTERNAL_DELETED, (SEC_DIALOG_STRING_BASE + 502),
        "Internal security module successfully deleted")
 ResDef(XP_JS_PKCS11_ADD_MOD_SUCCESS, (SEC_DIALOG_STRING_BASE + 503),
        "A new security module has been installed")
 ResDef(XP_JS_PKCS11_ADD_MOD_WARN, (SEC_DIALOG_STRING_BASE + 504),
        "Are you sure you want to install this security module?")
 ResDef(XP_JS_PKCS11_DEL_MOD_WARN, (SEC_DIALOG_STRING_BASE + 505),
        "Are you sure you want to delete this security module?")   

ResDef(XP_SEC_PROMPT_FOR_NICKNAME, (SEC_DIALOG_STRING_BASE + 506),
"Enter a nickname for the certificate:")

 /* Security Advisor->Signers->CRL's */

ResDef(XP_DIALOG_SHOW_CRLS_TITLE, (SEC_DIALOG_STRING_BASE + 507), "\
View/Edit CRL's")

ResDef(XP_DIALOG_SHOW_CRL, (SEC_DIALOG_STRING_BASE + 508), "\
<h3 align=center>%0%</h3>\
<b>URL:</b>%1%<br><b>Last Update:</b>%2%<br><b>Next Update:</b>%3% %4%<br>\
<b>Signed by:</b><ul>%5%</ul><br>\
<center><select name=bogus size=8>%6%</select></center>")

ResDef(XP_DIALOG_NEW_CRL, (SEC_DIALOG_STRING_BASE + 509), "\
Enter the URL of the new CRL/CKL to load:")

ResDef(XP_DIALOG_EDIT_CRL, (SEC_DIALOG_STRING_BASE + 510), "\
<h3 align=center>%0%</h3>\
Enter the URL of the Certificate Revocation List.<br><br>\
<b>URL:</b><input name=url size=70 type=text value=\042%1%\042>")

ResDef(XP_DIALOG_NO_CRL_SELECTED, (SEC_DIALOG_STRING_BASE + 511), "\
No CRL was selected. Please select a CRL from the list.")

ResDef(XP_DIALOG_NO_URL_FOR_CRL, (SEC_DIALOG_STRING_BASE + 512), "\
Selected CRL does not have a valid URL to load from.\n\
Use 'New/Edit...' to set the URL.")

ResDef(XP_DIALOG_CRL_EXPIRED_TAG, (SEC_DIALOG_STRING_BASE + 513), "\
<font color=red>Expired</font>")

ResDef(XP_DIALOG_NO_URL_GIVEN, (SEC_DIALOG_STRING_BASE + 514), "\
You did not enter a URL: No new CRL loaded.")

ResDef(XP_DIALOG_CRL_EDIT_TITLE, (SEC_DIALOG_STRING_BASE + 515), "\
Edit CRL")

ResDef(XP_DIALOG_CRLS_TITLE, (SEC_DIALOG_STRING_BASE + 516), "\
CRL")

ResDef(XP_DIALOG_JS_HEADER_STRINGS_WITH_UTF8_CHARSET, (SEC_DIALOG_STRING_BASE + 517), "\
<HTML><HEAD><meta http-equiv=\042Content-Type\042 content=\042text/html; charset=utf-8\042>\
<TITLE>%0%</TITLE><SCRIPT LANGUAGE=\042JavaScript\042>\nvar dlgstring ='")

ResDef(XP_SIGN_TEXT_TITLE_STRING,   (SEC_DIALOG_STRING_BASE + 518), "\
Digital Signature")

ResDef(XP_SIGN_TEXT_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 519), "\
The site '%0%' has requested that you sign the following message:\
<br><pre><dl><dd><tt>%1%</dl></tt></pre><br><b>If you agree to sign this \
message press %ok%, otherwise press %cancel%.</b>")

ResDef(XP_SIGN_TEXT_ASK_DIALOG_STRINGS, (SEC_DIALOG_STRING_BASE + 520), "\
The site '%0%' has requested that you sign the following message:\
<br><pre><dl><dd><tt>%1%</dl></tt></pre><br>Please select a certificate \
%-cont-%")

ResDef(XP_SIGN_TEXT_ASK_DIALOG_STRINGS2, (SEC_DIALOG_STRING_BASE + 521), "\
to use for signing:<br><SELECT NAME=certname>%2%</SELECT><br><b>If you \
agree to sign this message press %ok%, otherwise press %cancel%.</b>")

ResDef(XP_DIALOG_HEADER_STRINGS_WITH_UTF8_CHARSET, (SEC_DIALOG_STRING_BASE + 522), "\
<head><meta http-equiv=\042Content-Type\042 content=\042text/html; charset=utf-8\042></head><body bgcolor=\042#c0c0c0\042>\
<font face=\042PrimaSans BT\042><form name=theform \
action=internal-dialog-handler method=post><input type=\042hidden\042 \
%-cont-%")

ResDef(XP_DIALOG_HEADER_STRINGS_WITH_UTF8_CHARSET_1, (SEC_DIALOG_STRING_BASE + 523), "\
name=\042handle\042 value=\042%0%\042><input type=\042hidden\042 name=\042xxxbuttonxxx\042><font size=2>")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_PW_SETUP_RECOMMEND_STRING, (SEC_DIALOG_STRING_BASE + 524), "\
It is strongly recommended that you protect your Private Key with a \
"MOZ_NAME_PRODUCT" password.  If you do not want a password, leave the password \
field blank.<P>")
#else 
ResDef(XP_PW_SETUP_RECOMMEND_STRING, (SEC_DIALOG_STRING_BASE + 524), "\
It is strongly recommended that you protect your Private Key with a \
Communicator password.  If you do not want a password, leave the password \
field blank.<P>")
#endif

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_PW_SETUP_PW_REQUIRED_STRING, (SEC_DIALOG_STRING_BASE + 525), "\
Please choose a "MOZ_NAME_PRODUCT" password to protect your Private Keys.<P>")
#else
ResDef(XP_PW_SETUP_PW_REQUIRED_STRING, (SEC_DIALOG_STRING_BASE + 525), "\
Please choose a Communicator password to protect your Private Keys.<P>")
#endif

ResDef(XP_PW_SETUP_MIN_PWD_LENGTH_STRING_1, (SEC_DIALOG_STRING_BASE + 526), "\
Your password must be at least ")

ResDef(XP_PW_SETUP_MIN_PWD_LENGTH_STRING_2, (SEC_DIALOG_STRING_BASE + 527), "\
 characters long.<P>")

ResDef(XP_PW_CHANGE_RECOMMEND_STRING, (SEC_DIALOG_STRING_BASE + 528), "\
Leave the password fields blank if you don't want a password.<p>")

ResDef(XP_PW_CHANGE_REQUIRED_STRING, (SEC_DIALOG_STRING_BASE + 529), "\
Please choose a new password to protect your Private Keys.<P>")

ResDef(XP_BANNER_FONTFACE_INFO_STRING, (SEC_DIALOG_STRING_BASE + 530), "\
<font face=\042Impress BT, Verdana, Arial, Helvetica, sans-serif\042 \
point-size=16>")

ResDef(XP_DIALOG_STYLEINFO_STRING, (SEC_DIALOG_STRING_BASE + 531), "\
<style type=\042text/css\042>body {font-family:\042PrimaSans BT,  \
Verdana, Arial, Helvetica, sans-serif\042;} table {font-family: \
\042PrimaSans BT, Verdana, Arial, Helvetica, sans-serif\042;} \
td {font-family: \042PrimaSans BT, Verdana, Arial, Helvetica, sans-serif\042;} \
</style>")

ResDef(XP_DIALOG_CRL_NOT_YET_VALID_TAG, (SEC_DIALOG_STRING_BASE + 532), "\
<font color=red>Not Yet Valid</font>")

ResDef(XP_EDITPCERT_TITLE_STRING, (SEC_DIALOG_STRING_BASE + 533), "\
View/Edit A Personal Certificate")


ResDef(XP_EDIT_PCERT_STRINGS, (SEC_DIALOG_STRING_BASE + 534), "\
%0%<hr><b>This email user's certificate does not have a trusted issuer.</b> \
<br>You may decide to directly trust this certificate to permit the exchange \
%-cont-%")

ResDef(XP_EDIT_PCERT_STRINGS_1, (SEC_DIALOG_STRING_BASE + 535), "\
of<br>signed and encrypted e-mail with this user. <p>\
%-cont-%")

ResDef(XP_EDIT_PCERT_STRINGS_2, (SEC_DIALOG_STRING_BASE + 536), "\
To be safe, before deciding to trust this certificate, you should \
contact the<br>e-mail user and verify that the certificate fingerprint \
%-cont-%")

ResDef(XP_EDIT_PCERT_STRINGS_3, (SEC_DIALOG_STRING_BASE + 537), "\
listed above is the same<br>as the one he or she has.<p><input \
type=radio name=dirtrust value=no %1%>Do not trust this \
certificate.<br> %-cont-%")

ResDef(XP_EDIT_PCERT_STRINGS_4, (SEC_DIALOG_STRING_BASE + 538), "\
<input type=radio name=dirtrust value=yes %2%>Trust this \
certificate even though it does not have a trusted issuer.")

ResDef(XP_SEC_DETAILS, (SEC_DIALOG_STRING_BASE + 539), "\
Details")

ResDef(XP_SEC_GRANT, (SEC_DIALOG_STRING_BASE + 540), "\
Grant")

ResDef(XP_SEC_DENY, (SEC_DIALOG_STRING_BASE + 541), "\
Deny")

ResDef(XP_SEC_CERTIFICATE, (SEC_DIALOG_STRING_BASE + 542), "\
Certificate")


/*
 * NOTE - the SA_* strings are programmatically derived 
 *  from ns/security/lib/nav/secprefs.html.
 * To regenerate, run ns/security/lib/nav/convhtml.sh 
 * XXXXXXXXXXXXXXXXXXXXX    Do NOT edit by hand.   XXXXXXXXXXXXXXXXXXXX
 */

ResDef(SA_STRINGS_BEGIN, (SEC_DIALOG_STRING_BASE + 1000), "")
ResDef(SA_VIEW_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1001), "View")
ResDef(SA_EDIT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1002), "Edit")
ResDef(SA_VERIFY_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1003), "Verify")
ResDef(SA_DELETE_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1004), "Delete")
ResDef(SA_EXPORT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1005), "Export")
ResDef(SA_EXPIRED_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1006),
 "Delete Expired")
ResDef(SA_REMOVE_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1007), "Remove")
ResDef(SA_GET_CERT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1008),
 "Get a Certificate...")
ResDef(SA_GET_CERTS_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1009),
 "Get Certificates...")
ResDef(SA_IMPORT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1010),
 "Import a Certificate...")
ResDef(SA_VIEW_CERT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1011),
 "View Certificate")
ResDef(SA_EDIT_PRIVS_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1012),
 "Edit Privileges")
ResDef(SA_VIEW_EDIT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1013), "View/Edit")
ResDef(SA_ADD_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1014), "Add")
ResDef(SA_LOGOUT_ALL_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1015),
 "Logout All")
ResDef(SA_OK_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1016), "OK")
ResDef(SA_CANCEL_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1017), "Cancel")
ResDef(SA_HELP_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1018), "Help")
ResDef(SA_SEARCH_DIR_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1019),
 "Search Directory")
ResDef(SA_SEND_CERT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1020),
 "Send Certificate To Directory")
ResDef(SA_PAGE_INFO_LABEL, (SEC_DIALOG_STRING_BASE + 1021), "Open Page Info")
ResDef(SA_VIEW_CRL_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1022),
 "View/Edit CRL's")
ResDef(SA_SECINFO_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1023),
 "Security Info")
ResDef(SA_PASSWORDS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1024), "Passwords")
ResDef(SA_NAVIGATOR_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1025),MOZ_NAME_PRODUCT)
ResDef(SA_MESSENGER_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1026), "Messenger")
ResDef(SA_APPLETS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1027),
 "Java/JavaScript")
ResDef(SA_CERTS_INTRO_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1028),
 "Certificates")
ResDef(SA_YOURS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1029), "Yours")
ResDef(SA_PEOPLE_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1030), "People")
ResDef(SA_SITES_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1031), "Web Sites")
ResDef(SA_DEVELOPERS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1032),
 "Software Developers")
ResDef(SA_SIGNERS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1033), "Signers")
ResDef(SA_MODULES_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1034),
 "Cryptographic Modules")
ResDef(SA_SECINFO_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1035),
 "Security Info")
ResDef(SA_PASSWORDS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1036), "Passwords")
ResDef(SA_NAVIGATOR_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1037),MOZ_NAME_PRODUCT)
ResDef(SA_MESSENGER_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1038), "Messenger")
ResDef(SA_APPLETS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1039),
 "Java/JavaScript")
ResDef(SA_CERTS_INTRO_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1040),
 "Certificates")
ResDef(SA_YOURS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1041),
 "Your Certificates")
ResDef(SA_PEOPLE_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1042),
 "Other People's Certificates")
ResDef(SA_SITES_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1043),
 "Web Sites' Certificates")
ResDef(SA_DEVELOPERS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1044),
 "Software Developers' Certificates")
ResDef(SA_SIGNERS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1045),
 "Certificate Signers' Certificates")
ResDef(SA_MODULES_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1046),
 "Cryptographic Modules")
ResDef(SA_ENTER_SECURE_LABEL, (SEC_DIALOG_STRING_BASE + 1047),
 "Entering an encrypted site")
ResDef(SA_LEAVE_SECURE_LABEL, (SEC_DIALOG_STRING_BASE + 1048),
 "Leaving an encrypted site")
ResDef(SA_MIXED_SECURE_LABEL, (SEC_DIALOG_STRING_BASE + 1049),
 "Viewing a page with an encrypted/unencrypted mix")
ResDef(SA_SEND_CLEAR_LABEL, (SEC_DIALOG_STRING_BASE + 1050),
 "Sending unencrypted information to a Site")
ResDef(SA_SSL_CERT_LABEL, (SEC_DIALOG_STRING_BASE + 1051),
 "<B>Certificate to identify you to a web site:</B>")
ResDef(SA_PROXY_CERT_LABEL, (SEC_DIALOG_STRING_BASE + 1052),
 "<B>Certificate to use for Proxy Authentication:</B>")
ResDef(SA_NO_PROXY_AUTH_LABEL, (SEC_DIALOG_STRING_BASE + 1053),
 "No Proxy Authentication")
ResDef(SA_ASK_SA_EVERY_TIME_LABEL, (SEC_DIALOG_STRING_BASE + 1054),
 "Ask Every Time")
ResDef(SA_SELECT_AUTO_LABEL, (SEC_DIALOG_STRING_BASE + 1055),
 "Select Automatically")
ResDef(SA_SSL_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1056),
 "<B>Advanced Security (SSL) Configuration:</B>")
ResDef(SA_SSL2_ENABLE_LABEL, (SEC_DIALOG_STRING_BASE + 1057),
 "Enable SSL (Secure Sockets Layer) v2")
ResDef(SA_SSL2_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1058),
 "Configure SSL v2")
ResDef(SA_SSL3_ENABLE_LABEL, (SEC_DIALOG_STRING_BASE + 1059),
 "Enable SSL (Secure Sockets Layer) v3")
ResDef(SA_SSL3_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1060),
 "Configure SSL v3")
ResDef(SA_SMIME_CERT_LABEL, (SEC_DIALOG_STRING_BASE + 1061),
 "<B>Certificate for your Signed and Encrypted Messages:</B>")
ResDef(SA_SMIME_NO_CERTS_BLURB, (SEC_DIALOG_STRING_BASE + 1062),
 "<B>(You have no email certificates.)</B>")
ResDef(SA_SMIME_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1063),
 "Select S/MIME Ciphers")
ResDef(SA_CHANGE_PASSWORD_LABEL, (SEC_DIALOG_STRING_BASE + 1064),
 "Change Password")
ResDef(SA_SET_PASSWORD_LABEL, (SEC_DIALOG_STRING_BASE + 1065), "Set Password")
ResDef(SA_ASK_FOR_PASSWORD_LABEL, (SEC_DIALOG_STRING_BASE + 1066),
 "<B>Communicator will ask for this Password:</B>")
ResDef(SA_ONCE_PER_SESSION_LABEL, (SEC_DIALOG_STRING_BASE + 1067),
 "The first time your certificate is needed")
ResDef(SA_EVERY_TIME_LABEL, (SEC_DIALOG_STRING_BASE + 1068),
 "Every time your certificate is needed")
ResDef(SA_AFTER_LABEL, (SEC_DIALOG_STRING_BASE + 1069), "After")
ResDef(SA_MINUTES_LABEL, (SEC_DIALOG_STRING_BASE + 1070),
 "minutes of inactivity")
ResDef(SA_COMPOSE_ENCRYPT_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1071),
 "Encrypting Message")
ResDef(SA_COMPOSE_SIGN_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1072),
 "Signing Message")
ResDef(SA_MESSAGE_ENCRYPT_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1073),
 "Encrypted Message")
ResDef(SA_MESSAGE_SIGN_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1074),
 "Signed Message")
ResDef(SA_CAN_ENCRYPT, (SEC_DIALOG_STRING_BASE + 1075),
 "This message <B>can be encrypted</B> when it is sent.")
ResDef(SA_SEND_ENCRYPT_DESC, (SEC_DIALOG_STRING_BASE + 1076),
 "Sending an encrypted message is like sending your correspondence in an\
 envelope rather than a postcard; it makes it difficult for other\
 people to read your message.")
ResDef(SA_ENCRYPT_THIS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1077),
 "Encrypt this message.")
ResDef(SA_SEND_CLEAR_WARN_DESC, (SEC_DIALOG_STRING_BASE + 1078),
 "Sending an unencrypted message is like sending your correspondence in a \
postcard instead of an envelope. Other people may be able to read your\
 message.")
ResDef(SA_NO_RECIPIENTS_DESC, (SEC_DIALOG_STRING_BASE + 1079),
 "You must have at least one recipient entered for this message.")
ResDef(SA_CANNOT_ENCRYPT_HEAD, (SEC_DIALOG_STRING_BASE + 1080),
 "This message <B>cannot be encrypted</B> when it is sent because")
ResDef(SA_CERT_MISSING_TAIL, (SEC_DIALOG_STRING_BASE + 1081),
 " has no Security Certificate.")
ResDef(SA_CERT_HAS_EXP_TAIL, (SEC_DIALOG_STRING_BASE + 1082),
 " has an expired Security Certificate.")
ResDef(SA_CERT_HAS_REVOKE_TAIL, (SEC_DIALOG_STRING_BASE + 1083),
 " has a revoked Security Certificate.")
ResDef(SA_CERT_NO_ALIAS_TAIL, (SEC_DIALOG_STRING_BASE + 1084),
 " in Alias has no Security Certificate.")
ResDef(SA_CERT_NEWSGROUP_TAIL, (SEC_DIALOG_STRING_BASE + 1085),
 " is a Discussion Group.")
ResDef(SA_CERT_INVALID_TAIL, (SEC_DIALOG_STRING_BASE + 1086),
 " has an invalid Security Certificate.")
ResDef(SA_CERT_UNTRUSTED_TAIL, (SEC_DIALOG_STRING_BASE + 1087),
 " has a Security Certificate that is marked as Untrusted.")
ResDef(SA_CERT_ISSUER_UNTRUSTED_TAIL, (SEC_DIALOG_STRING_BASE + 1088),
 " has a Security Certificate Issuer that is marked as Untrusted.")
ResDef(SA_CERT_ISSUER_UNKNOWN_TAIL, (SEC_DIALOG_STRING_BASE + 1089),
 " has an unknown Security Certificate  Issuer.")
ResDef(SA_CERT_UNKNOWN_ERROR_TAIL, (SEC_DIALOG_STRING_BASE + 1090),
 ": unknown certificate error.")
ResDef(SA_HOW_TO_GET_THEIR_CERT_1, (SEC_DIALOG_STRING_BASE + 1091),
 "To obtain valid Security Certificates from a Directory, press <I>Get\
 Certificates.</I> Otherwise the recipients must first obtain a\
 Certificate for themselves and then ")
ResDef(SA_HOW_TO_GET_THEIR_CERT_2, (SEC_DIALOG_STRING_BASE + 1092),
 "send it to you in a signed email message. This new Security Certificate \
will automatically be remembered once it is received. <P>Discussion\
 groups cannot receive encrypted messages.")
ResDef(SA_CAN_BE_SIGNED, (SEC_DIALOG_STRING_BASE + 1093),
 "This message <B>can be signed</B> when it is sent.")
ResDef(SA_CANNOT_BE_SIGNED, (SEC_DIALOG_STRING_BASE + 1094),
 "This message <B>cannot be signed</B> when it is sent.")
ResDef(SA_CANNOT_SIGN_DESC_1, (SEC_DIALOG_STRING_BASE + 1095),
 "This is because you do not have a valid Security Certificate. When you\
 include your Security Certificate in a message, you ")
ResDef(SA_CANNOT_SIGN_DESC_2, (SEC_DIALOG_STRING_BASE + 1096),
 "also digitally sign that message. This makes it possible to verify that \
the message actually came from you.")
ResDef(SA_CAN_SIGN_DESC, (SEC_DIALOG_STRING_BASE + 1097),
 "When you digitally sign a message, you also include your Security\
 Certificate in that message. This makes it possible to verify that the \
message actually came from you.")
ResDef(SA_SIGN_DISCLAIMER, (SEC_DIALOG_STRING_BASE + 1098),
 "In some places this digital signature may be considered as legally\
 binding as your own written signature.")
ResDef(SA_SIGN_THIS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1099),
 "Include my Security Certificate in this message and use a digital\
 signature to sign this message ")
ResDef(SA_WAS_ENCRYPTED, (SEC_DIALOG_STRING_BASE + 1100),
 "This message <B>was encrypted</B> when it was sent. <P>This means that\
 it was hard for other people to eavesdrop on your message while it was \
being sent.")
ResDef(SA_WAS_NOT_ENCRYPTED, (SEC_DIALOG_STRING_BASE + 1101),
 "This message <B>was not encrypted</B> when it was sent. <P>This means\
 that it was possible for other people to view your message while it\
 was being sent.")
ResDef(SA_WAS_ENCRYPTED_FOR_OTHER_1, (SEC_DIALOG_STRING_BASE + 1102),
 "You cannot read this message because you do not have the Security\
 Certificate necessary to decrypt it. This could be because your ")
ResDef(SA_WAS_ENCRYPTED_FOR_OTHER_2, (SEC_DIALOG_STRING_BASE + 1103),
 "Security Certificate is on a different computer or it could be because\
 the message was encrypted with someone else's Security Certificate.")
ResDef(SA_ENCRYPTION_ALGORITHM_WAS, (SEC_DIALOG_STRING_BASE + 1104),
 "The algorithm used was ")
ResDef(SA_WAS_SIGNED_HEAD, (SEC_DIALOG_STRING_BASE + 1105),
 "This message <B>was digitally signed</B> by ")
ResDef(SA_INCLUDED_CERT_DESC_HEAD, (SEC_DIALOG_STRING_BASE + 1106),
 "This message included the Security Certificate for ")
ResDef(SA_WAS_SIGNED_AT, (SEC_DIALOG_STRING_BASE + 1107), " on ")
ResDef(SA_INCLUDED_CERT_DESC_SIGNED_AT, (SEC_DIALOG_STRING_BASE + 1108),
 ", and was signed on ")
ResDef(SA_WAS_SIGNED_DESC_TAIL_1, (SEC_DIALOG_STRING_BASE + 1109),
 "To check the Certificate, press the ``View/Edit'' button. <P>This\
 Certificate has automatically been added to your list of ")
ResDef(SA_WAS_SIGNED_ALT_DESC_TAIL_1, (SEC_DIALOG_STRING_BASE + 1110),
 "To check the Certificate or edit Certificate Trust Information, press\
 the ``View/Edit'' button. <P>This Certificate has automatically been\
 added to your list of ")
ResDef(SA_WAS_SIGNED_DESC_TAIL_2, (SEC_DIALOG_STRING_BASE + 1111),
 "People's Certificates to make it possible for you to send secure mail\
 to this person.")
ResDef(SA_WAS_NOT_SIGNED, (SEC_DIALOG_STRING_BASE + 1112),
 "This message <B>was not digitally signed</B>. <P>It is impossible to\
 verify that this message actually came from the sender.")
ResDef(SA_SIG_INVALID, (SEC_DIALOG_STRING_BASE + 1113),
 "<B>The Certificate that was used to digitally sign this message is\
 invalid</B>. <P>It is impossible to prove that this message actually\
 came from the sender.")
ResDef(SA_ENCRYPTION_INVALID, (SEC_DIALOG_STRING_BASE + 1114),
 "<B>This message cannot be decrypted.</B> ")
ResDef(SA_SIGN_ERROR_INTRO, (SEC_DIALOG_STRING_BASE + 1115), "The error was: ")
ResDef(SA_SIG_TAMPERED_1, (SEC_DIALOG_STRING_BASE + 1116),
 "Warning! In the time since the sender sent you this message and you\
 received it, the message appears to have been altered. Some ")
ResDef(SA_SIG_TAMPERED_2, (SEC_DIALOG_STRING_BASE + 1117),
 "or all of the content of this message has been changed. You should\
 contact the message sender and/or your system administrator.")
ResDef(SA_ADDR_MISMATCH_PART1, (SEC_DIALOG_STRING_BASE + 1118),
 "This message appears to have been sent from the email address <TT><B>")
ResDef(SA_ADDR_MISMATCH_PART2, (SEC_DIALOG_STRING_BASE + 1119),
 "</B></TT>, but the certificate which was used to sign this message\
 belongs to the email address <TT><B>")
ResDef(SA_ADDR_MISMATCH_PART3, (SEC_DIALOG_STRING_BASE + 1120),
 "</B></TT>. If these two email addresses don't belong to the same \
 person, this could be an attempt at forgery.")
ResDef(SA_NEWS_ENCRYPT_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1121),
 "Usenet News Security")
ResDef(SA_NEWS_ENCRYPTED_1, (SEC_DIALOG_STRING_BASE + 1122),
 "The connection to this news server <B>is encrypted</B>. This means that \
the messages you read are encrypted as they ")
ResDef(SA_NEWS_ENCRYPTED_2, (SEC_DIALOG_STRING_BASE + 1123),
 "are sent to you. This makes it difficult for other people to read the\
 messages while you are reading them. ")
ResDef(SA_NEWS_NOT_ENCRYPTED, (SEC_DIALOG_STRING_BASE + 1124),
 "The connection to this news server <B>is not encrypted</B>. <P>This\
 means that other people may be able to read these messages while you\
 are reading them.")
ResDef(SA_NAV_ENCRYPTION_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1125),
 "Encryption")
ResDef(SA_NAV_VERIFICATION_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1126),
 "Verification")
ResDef(SA_NAV_NO_ENCRYPT_DESC_1, (SEC_DIALOG_STRING_BASE + 1127),
 "This page <B>was not encrypted</B>. This means it was possible for\
 other people to view this page when it was loaded. It ")
ResDef(SA_NAV_NO_ENCRYPT_DESC_2, (SEC_DIALOG_STRING_BASE + 1128),
 "also means that you cannot check the identity of the web site. For\
 complete details on all the files on this page, click <B>Open Page\
 info</B>.")
ResDef(SA_NAV_ENCRYPT_DESC_1, (SEC_DIALOG_STRING_BASE + 1129),
 "This page <B>was encrypted</B>. This means it was difficult for other\
 people to view this page when it was loaded.<P>")
ResDef(SA_NAV_ENCRYPT_DESC_2, (SEC_DIALOG_STRING_BASE + 1130),
 "You can examine your copy of the certificate for this page and check\
 the identity of the web site. To see the certificate ")
ResDef(SA_NAV_ENCRYPT_DESC_3, (SEC_DIALOG_STRING_BASE + 1131),
 "for this web site, click <B>View Certificate</B>. For complete details\
 on all the files on this page and their certificates, click <B>Open\
 Page Info</B>.")
ResDef(SA_NAV_NO_ENCRYPT_MIX_DESC_1, (SEC_DIALOG_STRING_BASE + 1132),
 "This page <B>was not encrypted</B>, but some of the files it contains\
 were encrypted. This means it was difficult for ")
ResDef(SA_NAV_NO_ENCRYPT_MIX_DESC_2, (SEC_DIALOG_STRING_BASE + 1133),
 "other people to view the encrypted files when the page was loaded. It\
 also means that you cannot check the ")
ResDef(SA_NAV_NO_ENCRYPT_MIX_DESC_3, (SEC_DIALOG_STRING_BASE + 1134),
 "identity of the web site for the page.<P>For complete details on all\
 the files on this page, click <B>Open Page Info</B>.")
ResDef(SA_NAV_ENCRYPT_MIX_DESC_1, (SEC_DIALOG_STRING_BASE + 1135),
 "This page <B>was encrypted</B>. This means it was difficult for other\
 people to view this page when it was loaded. You can ")
ResDef(SA_NAV_ENCRYPT_MIX_DESC_2, (SEC_DIALOG_STRING_BASE + 1136),
 "examine your copy of the certificate for this page and the identity of\
 the web site. To see the certificate for ")
ResDef(SA_NAV_ENCRYPT_MIX_DESC_3, (SEC_DIALOG_STRING_BASE + 1137),
 "this web site, click <B>View Certificate</B>.<P>However, some of the\
 files on this page <B>were not ")
ResDef(SA_NAV_ENCRYPT_MIX_DESC_4, (SEC_DIALOG_STRING_BASE + 1138),
 "encrypted</B>. This means that it was possible for other people to view \
those files when they were loaded. For complete ")
ResDef(SA_NAV_ENCRYPT_MIX_DESC_5, (SEC_DIALOG_STRING_BASE + 1139),
 "details on all the files on this page and their certificates, click\
 <B>Open Page Info</B>.")
ResDef(SA_NAV_VERIFY_CERT_DESC, (SEC_DIALOG_STRING_BASE + 1140),
 "Take a look at the page's Certificate.")
ResDef(SA_NAV_VERIFY_DOMAIN_DESC, (SEC_DIALOG_STRING_BASE + 1141),
 "Make sure that this is the site you think it is. This page comes from\
 the site: ")
ResDef(SA_NAV_VERIFY_MISSING_DESC_1, (SEC_DIALOG_STRING_BASE + 1142),
 "The following elements are missing from this window: ")
ResDef(SA_NAV_VERIFY_MISSING_DESC_2, (SEC_DIALOG_STRING_BASE + 1143),
 ". This means that you may be missing important information.")
ResDef(SA_NAV_INFO_MENUBAR_NAME, (SEC_DIALOG_STRING_BASE + 1144),
 "the menubar")
ResDef(SA_NAV_INFO_TOOLBAR_NAME, (SEC_DIALOG_STRING_BASE + 1145),
 "the toolbar")
ResDef(SA_NAV_INFO_PERSONALBAR_NAME, (SEC_DIALOG_STRING_BASE + 1146),
 "the personal toolbar")
ResDef(SA_NAV_INFO_LOCATION_NAME, (SEC_DIALOG_STRING_BASE + 1147),
 "the location bar")
ResDef(SA_NAV_INFO_STATUS_NAME, (SEC_DIALOG_STRING_BASE + 1148),
 "the status bar")
ResDef(SA_NAV_VERIFY_JAVA_DESC_1, (SEC_DIALOG_STRING_BASE + 1149),
 "This window was created by a Java Application (from ")
ResDef(SA_NAV_VERIFY_JAVA_DESC_2, (SEC_DIALOG_STRING_BASE + 1150),
 "). This application has some control over this window and may influence \
what you see.")
ResDef(SA_NAV_VERIFY_JS_DESC_1, (SEC_DIALOG_STRING_BASE + 1151),
 "This window was created by a JavaScript Application (from ")
ResDef(SA_NAV_VERIFY_JS_DESC_2, (SEC_DIALOG_STRING_BASE + 1152),
 "). This application has some control over this window and may influence \
what you see.")
ResDef(SA_NOT_ME_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1153),
 "<B><H2>There is no Security Info for this window.</H2></B>")
ResDef(SA_NOT_ME_DESC_1, (SEC_DIALOG_STRING_BASE + 1154),
 "Security Info is available for Browser, Messenger Message, and\
 Discussion Message windows. ")
ResDef(SA_NOT_ME_DESC_2, (SEC_DIALOG_STRING_BASE + 1155),
 "<P>If you wish to change Security settings or preferences, use the tabs \
on the left to switch between different areas.")
ResDef(SA_CERTS_INTRO_STRING_1, (SEC_DIALOG_STRING_BASE + 1156),
 "This is an explanation of Security Certificates.<P> <B>Certificate:</B> \
A file that identifies a person or organization. Communicator uses\
 certificates to ")
ResDef(SA_CERTS_INTRO_STRING_2, (SEC_DIALOG_STRING_BASE + 1157),
 "encrypt information. You can use a certificate to check the identity of \
the certificate's owner. You should trust a certificate only if you\
 trust the person or organization that issued it. ")
ResDef(SA_CERTS_INTRO_STRING_3, (SEC_DIALOG_STRING_BASE + 1158),
 "<P>Your own certificates allow you to receive encrypted information.\
 A record is also kept of certificates from other people, web\
 sites, applets, and scripts. ")
ResDef(SA_CERTS_INTRO_STRING_4, (SEC_DIALOG_STRING_BASE + 1159),
 "<P><UL><B>Yours</B> lists your own certificates. <P><B>People</B> lists \
certificates from other people or organizations. ")
ResDef(SA_CERTS_INTRO_STRING_5, (SEC_DIALOG_STRING_BASE + 1160),
 "<P><B>Web Sites</B> lists certificates from web sites.\
 <P><B>Signers</B> lists certificates from certificate signers\
 (``Certificate Authorities''.) ")
ResDef(SA_CERTS_INTRO_STRING_6, (SEC_DIALOG_STRING_BASE + 1161),
 "<P><B>Software Developers</B> lists certificates that accompany signed\
 Java applets and JavaScript scripts.</UL>")
ResDef(SA_YOUR_CERTS_DESC_1, (SEC_DIALOG_STRING_BASE + 1162),
 "You can use any of these certificates to identify yourself to other\
 people and to web sites. Communicator uses your certificates ")
ResDef(SA_YOUR_CERTS_DESC_2, (SEC_DIALOG_STRING_BASE + 1163),
 "to decrypt information sent to you. Your certificates are signed by the \
organization that issued them. <P><B>These are your certificates:</B>")
ResDef(SA_YOUR_CERTS_DESC_TAIL_1, (SEC_DIALOG_STRING_BASE + 1164),
 "You should make a copy of your certificates and keep them in a safe\
 place. If you ever lose your certificates, you will be unable ")
ResDef(SA_YOUR_CERTS_DESC_TAIL_2, (SEC_DIALOG_STRING_BASE + 1165),
 "to read encrypted mail you have received, and you may have problems\
 identifying yourself to web sites.")
ResDef(SA_PEOPLE_CERTS_DESC_1, (SEC_DIALOG_STRING_BASE + 1166),
 "Other people have used these certificates to identify themselves to\
 you. Communicator can send encrypted ")
ResDef(SA_PEOPLE_CERTS_DESC_2, (SEC_DIALOG_STRING_BASE + 1167),
 "messages to anyone for whom you have a certificate. <P><B>These are\
 certificates from other people:</B>")
ResDef(SA_GET_CERTS_DESC, (SEC_DIALOG_STRING_BASE + 1168),
 "To get certificates from a network Directory press <I>Search\
 Directory</I>.<p>")
ResDef(SA_SITE_CERTS_DESC, (SEC_DIALOG_STRING_BASE + 1169),
 "<B>These are certificates that you have accepted from web sites:</B>")
ResDef(SA_SIGNERS_CERTS_DESC, (SEC_DIALOG_STRING_BASE + 1170),
 "<B>These certificates identify the certificate signers that you\
 accept:</B>")
ResDef(SA_SIGNERS_VIEW_CRL_DESC, (SEC_DIALOG_STRING_BASE + 1171),
 "To view or edit Certificate Revocation Lists press <I>View/Edit\
 CRL's</I>.<p>")
ResDef(SA_SSL_DESC, (SEC_DIALOG_STRING_BASE + 1172),
 "<B>These settings allow you to control "MOZ_NAME_PRODUCT" security settings.</B> \
<P>"MOZ_NAME_PRODUCT" security warnings can let you know before you do something \
that might be unsafe. <P><B>Show a warning before:")
ResDef(SA_SMIME_DESC, (SEC_DIALOG_STRING_BASE + 1173),
 "<B>These settings allow you to control Messenger security settings.</B> \
<P>Messenger Security warnings can let you know before you do something \
that might be unsafe.")
ResDef(SA_SMIME_PREF_DESC, (SEC_DIALOG_STRING_BASE + 1174),
 "<B>Sending Signed/Encrypted Mail:</B>")
ResDef(SA_ENCRYPT_ALWAYS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1175),
 "Encrypt mail messages, when it is possible")
ResDef(SA_SIGN_MAIL_ALWAYS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1176),
 "Sign mail messages, when it is possible")
ResDef(SA_SIGN_NEWS_ALWAYS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1177),
 "Sign discussion (news) messages, when it is possible")
ResDef(SA_SMIME_CERT_DESC, (SEC_DIALOG_STRING_BASE + 1178),
 "This certificate is included with every email message you <B>sign</B>.\
 When other people receive it, it makes it possible for them to send\
 you encrypted mail.")
ResDef(SA_SMIME_SEND_CERT_DESC, (SEC_DIALOG_STRING_BASE + 1179),
 "Other people could also obtain your certificate from a Directory:")
ResDef(SA_SMIME_NO_CERT_DESC_1, (SEC_DIALOG_STRING_BASE + 1180),
 "If you had one, this certificate would be included with every email\
 message you <B>signed</B>. When other people received it, ")
ResDef(SA_SMIME_NO_CERT_DESC_2, (SEC_DIALOG_STRING_BASE + 1181),
 "it would make it possible for them to send you encrypted mail. (To get\
 a certificate, see the ``Yours'' tab on the left.)")
ResDef(SA_SMIME_CIPHER_HEADING, (SEC_DIALOG_STRING_BASE + 1182),
 "<B>Advanced S/MIME Configuration:</B>")
ResDef(SA_SMIME_CIPHER_DESC, (SEC_DIALOG_STRING_BASE + 1183),
 "Cipher Preferences: ")
ResDef(SA_APPLETS_DESC_1, (SEC_DIALOG_STRING_BASE + 1184),
 "<B>These settings allow you to control access by Java applets and\
 JavaScript scripts.</B> <P>No applet or script is allowed to access\
 your computer or network without ")
ResDef(SA_APPLETS_DESC_2, (SEC_DIALOG_STRING_BASE + 1185),
 "your permission. You have explicitly granted or forbidden access for\
 all applets and scripts from the following vendors, distributors, or\
 web sites.")
ResDef(SA_PASSWORD_DESC_1, (SEC_DIALOG_STRING_BASE + 1186),
 "<B>Your Communicator password will be used to protect your\
 certificates.</B> <P>If you are in an environment where other ")
ResDef(SA_PASSWORD_DESC_2, (SEC_DIALOG_STRING_BASE + 1187),
 "people have access to your computer (either physically or over the\
 network) you should have a Communicator password. <P>")
ResDef(SA_MODULES_DESC, (SEC_DIALOG_STRING_BASE + 1188),
 "<B>Cryptographic Modules:</B>")
ResDef(SA_STRINGS_END, (SEC_DIALOG_STRING_BASE + 1189), "")

/* NOTE: THE FOLLOWING STRINGS SHOULD *NOT* BE LOCALIZED!

   These strings (from SA_VARNAMES_BEGIN to SA_VARNAMES_END)
   are not user-visible; changing them will break the Security
   Advisor dialog.  The only reason they are in this file is so
   that they will end up in the resource segment rather than
   static-data segment on Windows.  Probably this should be
   done in some other way, so that these don't clutter up the
   localization data, but for now...
 */
ResDef(SA_VARNAMES_BEGIN, (SEC_DIALOG_STRING_BASE + 1190), "")
ResDef(SA_VARNAME_VIEW_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1191), "sa_view_button_label")
ResDef(SA_VARNAME_EDIT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1192), "sa_edit_button_label")
ResDef(SA_VARNAME_VERIFY_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1193), "sa_verify_button_label")
ResDef(SA_VARNAME_DELETE_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1194), "sa_delete_button_label")
ResDef(SA_VARNAME_EXPORT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1195), "sa_export_button_label")
ResDef(SA_VARNAME_EXPIRED_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1196), "sa_expired_button_label")
ResDef(SA_VARNAME_REMOVE_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1197), "sa_remove_button_label")
ResDef(SA_VARNAME_GET_CERT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1198), "sa_get_cert_button_label")
ResDef(SA_VARNAME_GET_CERTS_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1199), "sa_get_certs_button_label")
ResDef(SA_VARNAME_IMPORT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1200), "sa_import_button_label")
ResDef(SA_VARNAME_VIEW_CERT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1201), "sa_view_cert_button_label")
ResDef(SA_VARNAME_EDIT_PRIVS_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1202), "sa_edit_privs_button_label")
ResDef(SA_VARNAME_VIEW_EDIT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1203), "sa_view_edit_button_label")
ResDef(SA_VARNAME_ADD_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1204), "sa_add_button_label")
ResDef(SA_VARNAME_LOGOUT_ALL_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1205), "sa_logout_all_button_label")
ResDef(SA_VARNAME_OK_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1206), "sa_ok_button_label")
ResDef(SA_VARNAME_CANCEL_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1207), "sa_cancel_button_label")
ResDef(SA_VARNAME_HELP_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1208), "sa_help_button_label")
ResDef(SA_VARNAME_SEARCH_DIR_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1209), "sa_search_dir_button_label")
ResDef(SA_VARNAME_SEND_CERT_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1210), "sa_send_cert_button_label")
ResDef(SA_VARNAME_PAGE_INFO_LABEL, (SEC_DIALOG_STRING_BASE + 1211), "sa_page_info_label")
ResDef(SA_VARNAME_VIEW_CRL_BUTTON_LABEL, (SEC_DIALOG_STRING_BASE + 1212), "sa_view_crl_button_label")
ResDef(SA_VARNAME_SECINFO_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1213), "sa_secinfo_index_label")
ResDef(SA_VARNAME_PASSWORDS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1214), "sa_passwords_index_label")
ResDef(SA_VARNAME_NAVIGATOR_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1215), "sa_navigator_index_label")
ResDef(SA_VARNAME_MESSENGER_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1216), "sa_messenger_index_label")
ResDef(SA_VARNAME_APPLETS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1217), "sa_applets_index_label")
ResDef(SA_VARNAME_CERTS_INTRO_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1218), "sa_certs_intro_index_label")
ResDef(SA_VARNAME_YOURS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1219), "sa_yours_index_label")
ResDef(SA_VARNAME_PEOPLE_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1220), "sa_people_index_label")
ResDef(SA_VARNAME_SITES_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1221), "sa_sites_index_label")
ResDef(SA_VARNAME_DEVELOPERS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1222), "sa_developers_index_label")
ResDef(SA_VARNAME_SIGNERS_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1223), "sa_signers_index_label")
ResDef(SA_VARNAME_MODULES_INDEX_LABEL, (SEC_DIALOG_STRING_BASE + 1224), "sa_modules_index_label")
ResDef(SA_VARNAME_SECINFO_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1225), "sa_secinfo_title_label")
ResDef(SA_VARNAME_PASSWORDS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1226), "sa_passwords_title_label")
ResDef(SA_VARNAME_NAVIGATOR_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1227), "sa_navigator_title_label")
ResDef(SA_VARNAME_MESSENGER_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1228), "sa_messenger_title_label")
ResDef(SA_VARNAME_APPLETS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1229), "sa_applets_title_label")
ResDef(SA_VARNAME_CERTS_INTRO_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1230), "sa_certs_intro_title_label")
ResDef(SA_VARNAME_YOURS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1231), "sa_yours_title_label")
ResDef(SA_VARNAME_PEOPLE_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1232), "sa_people_title_label")
ResDef(SA_VARNAME_SITES_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1233), "sa_sites_title_label")
ResDef(SA_VARNAME_DEVELOPERS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1234), "sa_developers_title_label")
ResDef(SA_VARNAME_SIGNERS_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1235), "sa_signers_title_label")
ResDef(SA_VARNAME_MODULES_TITLE_LABEL, (SEC_DIALOG_STRING_BASE + 1236), "sa_modules_title_label")
ResDef(SA_VARNAME_ENTER_SECURE_LABEL, (SEC_DIALOG_STRING_BASE + 1237), "sa_enter_secure_label")
ResDef(SA_VARNAME_LEAVE_SECURE_LABEL, (SEC_DIALOG_STRING_BASE + 1238), "sa_leave_secure_label")
ResDef(SA_VARNAME_MIXED_SECURE_LABEL, (SEC_DIALOG_STRING_BASE + 1239), "sa_mixed_secure_label")
ResDef(SA_VARNAME_SEND_CLEAR_LABEL, (SEC_DIALOG_STRING_BASE + 1240), "sa_send_clear_label")
ResDef(SA_VARNAME_SSL_CERT_LABEL, (SEC_DIALOG_STRING_BASE + 1241), "sa_ssl_cert_label")
ResDef(SA_VARNAME_PROXY_CERT_LABEL, (SEC_DIALOG_STRING_BASE + 1242), "sa_proxy_cert_label")
ResDef(SA_VARNAME_NO_PROXY_AUTH_LABEL, (SEC_DIALOG_STRING_BASE + 1243), "sa_no_proxy_auth_label")
ResDef(SA_VARNAME_ASK_SA_EVERY_TIME_LABEL, (SEC_DIALOG_STRING_BASE + 1244), "sa_ask_sa_every_time_label")
ResDef(SA_VARNAME_SELECT_AUTO_LABEL, (SEC_DIALOG_STRING_BASE + 1245), "sa_select_auto_label")
ResDef(SA_VARNAME_SSL_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1246), "sa_ssl_config_label")
ResDef(SA_VARNAME_SSL2_ENABLE_LABEL, (SEC_DIALOG_STRING_BASE + 1247), "sa_ssl2_enable_label")
ResDef(SA_VARNAME_SSL2_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1248), "sa_ssl2_config_label")
ResDef(SA_VARNAME_SSL3_ENABLE_LABEL, (SEC_DIALOG_STRING_BASE + 1249), "sa_ssl3_enable_label")
ResDef(SA_VARNAME_SSL3_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1250), "sa_ssl3_config_label")
ResDef(SA_VARNAME_SMIME_CERT_LABEL, (SEC_DIALOG_STRING_BASE + 1251), "sa_smime_cert_label")
ResDef(SA_VARNAME_SMIME_NO_CERTS_BLURB, (SEC_DIALOG_STRING_BASE + 1252), "sa_smime_no_certs_blurb")
ResDef(SA_VARNAME_SMIME_CONFIG_LABEL, (SEC_DIALOG_STRING_BASE + 1253), "sa_smime_config_label")
ResDef(SA_VARNAME_CHANGE_PASSWORD_LABEL, (SEC_DIALOG_STRING_BASE + 1254), "sa_change_password_label")
ResDef(SA_VARNAME_SET_PASSWORD_LABEL, (SEC_DIALOG_STRING_BASE + 1255), "sa_set_password_label")
ResDef(SA_VARNAME_ASK_FOR_PASSWORD_LABEL, (SEC_DIALOG_STRING_BASE + 1256), "sa_ask_for_password_label")
ResDef(SA_VARNAME_ONCE_PER_SESSION_LABEL, (SEC_DIALOG_STRING_BASE + 1257), "sa_once_per_session_label")
ResDef(SA_VARNAME_EVERY_TIME_LABEL, (SEC_DIALOG_STRING_BASE + 1258), "sa_every_time_label")
ResDef(SA_VARNAME_AFTER_LABEL, (SEC_DIALOG_STRING_BASE + 1259), "sa_after_label")
ResDef(SA_VARNAME_MINUTES_LABEL, (SEC_DIALOG_STRING_BASE + 1260), "sa_minutes_label")
ResDef(SA_VARNAME_COMPOSE_ENCRYPT_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1261), "sa_compose_encrypt_subtitle")
ResDef(SA_VARNAME_COMPOSE_SIGN_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1262), "sa_compose_sign_subtitle")
ResDef(SA_VARNAME_MESSAGE_ENCRYPT_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1263), "sa_message_encrypt_subtitle")
ResDef(SA_VARNAME_MESSAGE_SIGN_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1264), "sa_message_sign_subtitle")
ResDef(SA_VARNAME_CAN_ENCRYPT, (SEC_DIALOG_STRING_BASE + 1265), "sa_can_encrypt")
ResDef(SA_VARNAME_SEND_ENCRYPT_DESC, (SEC_DIALOG_STRING_BASE + 1266), "sa_send_encrypt_desc")
ResDef(SA_VARNAME_ENCRYPT_THIS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1267), "sa_encrypt_this_checkbox_label")
ResDef(SA_VARNAME_SEND_CLEAR_WARN_DESC, (SEC_DIALOG_STRING_BASE + 1268), "sa_send_clear_warn_desc")
ResDef(SA_VARNAME_NO_RECIPIENTS_DESC, (SEC_DIALOG_STRING_BASE + 1269), "sa_no_recipients_desc")
ResDef(SA_VARNAME_CANNOT_ENCRYPT_HEAD, (SEC_DIALOG_STRING_BASE + 1270), "sa_cannot_encrypt_head")
ResDef(SA_VARNAME_CERT_MISSING_TAIL, (SEC_DIALOG_STRING_BASE + 1271), "sa_cert_missing_tail")
ResDef(SA_VARNAME_CERT_HAS_EXP_TAIL, (SEC_DIALOG_STRING_BASE + 1272), "sa_cert_has_exp_tail")
ResDef(SA_VARNAME_CERT_HAS_REVOKE_TAIL, (SEC_DIALOG_STRING_BASE + 1273), "sa_cert_has_revoke_tail")
ResDef(SA_VARNAME_CERT_NO_ALIAS_TAIL, (SEC_DIALOG_STRING_BASE + 1274), "sa_cert_no_alias_tail")
ResDef(SA_VARNAME_CERT_NEWSGROUP_TAIL, (SEC_DIALOG_STRING_BASE + 1275), "sa_cert_newsgroup_tail")
ResDef(SA_VARNAME_CERT_INVALID_TAIL, (SEC_DIALOG_STRING_BASE + 1276), "sa_cert_invalid_tail")
ResDef(SA_VARNAME_CERT_UNTRUSTED_TAIL, (SEC_DIALOG_STRING_BASE + 1277), "sa_cert_untrusted_tail")
ResDef(SA_VARNAME_CERT_ISSUER_UNTRUSTED_TAIL, (SEC_DIALOG_STRING_BASE + 1278), "sa_cert_issuer_untrusted_tail")
ResDef(SA_VARNAME_CERT_ISSUER_UNKNOWN_TAIL, (SEC_DIALOG_STRING_BASE + 1279), "sa_cert_issuer_unknown_tail")
ResDef(SA_VARNAME_CERT_UNKNOWN_ERROR_TAIL, (SEC_DIALOG_STRING_BASE + 1280), "sa_cert_unknown_error_tail")
ResDef(SA_VARNAME_HOW_TO_GET_THEIR_CERT_1, (SEC_DIALOG_STRING_BASE + 1281), "sa_how_to_get_their_cert_1")
ResDef(SA_VARNAME_HOW_TO_GET_THEIR_CERT_2, (SEC_DIALOG_STRING_BASE + 1282), "sa_how_to_get_their_cert_2")
ResDef(SA_VARNAME_CAN_BE_SIGNED, (SEC_DIALOG_STRING_BASE + 1283), "sa_can_be_signed")
ResDef(SA_VARNAME_CANNOT_BE_SIGNED, (SEC_DIALOG_STRING_BASE + 1284), "sa_cannot_be_signed")
ResDef(SA_VARNAME_CANNOT_SIGN_DESC_1, (SEC_DIALOG_STRING_BASE + 1285), "sa_cannot_sign_desc_1")
ResDef(SA_VARNAME_CANNOT_SIGN_DESC_2, (SEC_DIALOG_STRING_BASE + 1286), "sa_cannot_sign_desc_2")
ResDef(SA_VARNAME_CAN_SIGN_DESC, (SEC_DIALOG_STRING_BASE + 1287), "sa_can_sign_desc")
ResDef(SA_VARNAME_SIGN_DISCLAIMER, (SEC_DIALOG_STRING_BASE + 1288), "sa_sign_disclaimer")
ResDef(SA_VARNAME_SIGN_THIS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1289), "sa_sign_this_checkbox_label")
ResDef(SA_VARNAME_WAS_ENCRYPTED, (SEC_DIALOG_STRING_BASE + 1290), "sa_was_encrypted")
ResDef(SA_VARNAME_WAS_NOT_ENCRYPTED, (SEC_DIALOG_STRING_BASE + 1291), "sa_was_not_encrypted")
ResDef(SA_VARNAME_WAS_ENCRYPTED_FOR_OTHER_1, (SEC_DIALOG_STRING_BASE + 1292), "sa_was_encrypted_for_other_1")
ResDef(SA_VARNAME_WAS_ENCRYPTED_FOR_OTHER_2, (SEC_DIALOG_STRING_BASE + 1293), "sa_was_encrypted_for_other_2")
ResDef(SA_VARNAME_ENCRYPTION_ALGORITHM_WAS, (SEC_DIALOG_STRING_BASE + 1294), "sa_encryption_algorithm_was")
ResDef(SA_VARNAME_WAS_SIGNED_HEAD, (SEC_DIALOG_STRING_BASE + 1295), "sa_was_signed_head")
ResDef(SA_VARNAME_INCLUDED_CERT_DESC_HEAD, (SEC_DIALOG_STRING_BASE + 1296), "sa_included_cert_desc_head")
ResDef(SA_VARNAME_WAS_SIGNED_AT, (SEC_DIALOG_STRING_BASE + 1297), "sa_was_signed_at")
ResDef(SA_VARNAME_INCLUDED_CERT_DESC_SIGNED_AT, (SEC_DIALOG_STRING_BASE + 1298), "sa_included_cert_desc_signed_at")
ResDef(SA_VARNAME_WAS_SIGNED_DESC_TAIL_1, (SEC_DIALOG_STRING_BASE + 1299), "sa_was_signed_desc_tail_1")
ResDef(SA_VARNAME_WAS_SIGNED_ALT_DESC_TAIL_1, (SEC_DIALOG_STRING_BASE + 1300), "sa_was_signed_alt_desc_tail_1")
ResDef(SA_VARNAME_WAS_SIGNED_DESC_TAIL_2, (SEC_DIALOG_STRING_BASE + 1301), "sa_was_signed_desc_tail_2")
ResDef(SA_VARNAME_WAS_NOT_SIGNED, (SEC_DIALOG_STRING_BASE + 1302), "sa_was_not_signed")
ResDef(SA_VARNAME_SIG_INVALID, (SEC_DIALOG_STRING_BASE + 1303), "sa_sig_invalid")
ResDef(SA_VARNAME_ENCRYPTION_INVALID, (SEC_DIALOG_STRING_BASE + 1304), "sa_encryption_invalid")
ResDef(SA_VARNAME_SIGN_ERROR_INTRO, (SEC_DIALOG_STRING_BASE + 1305), "sa_sign_error_intro")
ResDef(SA_VARNAME_SIG_TAMPERED_1, (SEC_DIALOG_STRING_BASE + 1306), "sa_sig_tampered_1")
ResDef(SA_VARNAME_SIG_TAMPERED_2, (SEC_DIALOG_STRING_BASE + 1307), "sa_sig_tampered_2")
ResDef(SA_VARNAME_ADDR_MISMATCH_PART1, (SEC_DIALOG_STRING_BASE + 1308), "sa_addr_mismatch_part1")
ResDef(SA_VARNAME_ADDR_MISMATCH_PART2, (SEC_DIALOG_STRING_BASE + 1309), "sa_addr_mismatch_part2")
ResDef(SA_VARNAME_ADDR_MISMATCH_PART3, (SEC_DIALOG_STRING_BASE + 1310), "sa_addr_mismatch_part3")
ResDef(SA_VARNAME_NEWS_ENCRYPT_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1311), "sa_news_encrypt_subtitle")
ResDef(SA_VARNAME_NEWS_ENCRYPTED_1, (SEC_DIALOG_STRING_BASE + 1312), "sa_news_encrypted_1")
ResDef(SA_VARNAME_NEWS_ENCRYPTED_2, (SEC_DIALOG_STRING_BASE + 1313), "sa_news_encrypted_2")
ResDef(SA_VARNAME_NEWS_NOT_ENCRYPTED, (SEC_DIALOG_STRING_BASE + 1314), "sa_news_not_encrypted")
ResDef(SA_VARNAME_NAV_ENCRYPTION_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1315), "sa_nav_encryption_subtitle")
ResDef(SA_VARNAME_NAV_VERIFICATION_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1316), "sa_nav_verification_subtitle")
ResDef(SA_VARNAME_NAV_NO_ENCRYPT_DESC_1, (SEC_DIALOG_STRING_BASE + 1317), "sa_nav_no_encrypt_desc_1")
ResDef(SA_VARNAME_NAV_NO_ENCRYPT_DESC_2, (SEC_DIALOG_STRING_BASE + 1318), "sa_nav_no_encrypt_desc_2")
ResDef(SA_VARNAME_NAV_ENCRYPT_DESC_1, (SEC_DIALOG_STRING_BASE + 1319), "sa_nav_encrypt_desc_1")
ResDef(SA_VARNAME_NAV_ENCRYPT_DESC_2, (SEC_DIALOG_STRING_BASE + 1320), "sa_nav_encrypt_desc_2")
ResDef(SA_VARNAME_NAV_ENCRYPT_DESC_3, (SEC_DIALOG_STRING_BASE + 1321), "sa_nav_encrypt_desc_3")
ResDef(SA_VARNAME_NAV_NO_ENCRYPT_MIX_DESC_1, (SEC_DIALOG_STRING_BASE + 1322), "sa_nav_no_encrypt_mix_desc_1")
ResDef(SA_VARNAME_NAV_NO_ENCRYPT_MIX_DESC_2, (SEC_DIALOG_STRING_BASE + 1323), "sa_nav_no_encrypt_mix_desc_2")
ResDef(SA_VARNAME_NAV_NO_ENCRYPT_MIX_DESC_3, (SEC_DIALOG_STRING_BASE + 1324), "sa_nav_no_encrypt_mix_desc_3")
ResDef(SA_VARNAME_NAV_ENCRYPT_MIX_DESC_1, (SEC_DIALOG_STRING_BASE + 1325), "sa_nav_encrypt_mix_desc_1")
ResDef(SA_VARNAME_NAV_ENCRYPT_MIX_DESC_2, (SEC_DIALOG_STRING_BASE + 1326), "sa_nav_encrypt_mix_desc_2")
ResDef(SA_VARNAME_NAV_ENCRYPT_MIX_DESC_3, (SEC_DIALOG_STRING_BASE + 1327), "sa_nav_encrypt_mix_desc_3")
ResDef(SA_VARNAME_NAV_ENCRYPT_MIX_DESC_4, (SEC_DIALOG_STRING_BASE + 1328), "sa_nav_encrypt_mix_desc_4")
ResDef(SA_VARNAME_NAV_ENCRYPT_MIX_DESC_5, (SEC_DIALOG_STRING_BASE + 1329), "sa_nav_encrypt_mix_desc_5")
ResDef(SA_VARNAME_NAV_VERIFY_CERT_DESC, (SEC_DIALOG_STRING_BASE + 1330), "sa_nav_verify_cert_desc")
ResDef(SA_VARNAME_NAV_VERIFY_DOMAIN_DESC, (SEC_DIALOG_STRING_BASE + 1331), "sa_nav_verify_domain_desc")
ResDef(SA_VARNAME_NAV_VERIFY_MISSING_DESC_1, (SEC_DIALOG_STRING_BASE + 1332), "sa_nav_verify_missing_desc_1")
ResDef(SA_VARNAME_NAV_VERIFY_MISSING_DESC_2, (SEC_DIALOG_STRING_BASE + 1333), "sa_nav_verify_missing_desc_2")
ResDef(SA_VARNAME_NAV_INFO_MENUBAR_NAME, (SEC_DIALOG_STRING_BASE + 1334), "sa_nav_info_menubar_name")
ResDef(SA_VARNAME_NAV_INFO_TOOLBAR_NAME, (SEC_DIALOG_STRING_BASE + 1335), "sa_nav_info_toolbar_name")
ResDef(SA_VARNAME_NAV_INFO_PERSONALBAR_NAME, (SEC_DIALOG_STRING_BASE + 1336), "sa_nav_info_personalbar_name")
ResDef(SA_VARNAME_NAV_INFO_LOCATION_NAME, (SEC_DIALOG_STRING_BASE + 1337), "sa_nav_info_location_name")
ResDef(SA_VARNAME_NAV_INFO_STATUS_NAME, (SEC_DIALOG_STRING_BASE + 1338), "sa_nav_info_status_name")
ResDef(SA_VARNAME_NAV_VERIFY_JAVA_DESC_1, (SEC_DIALOG_STRING_BASE + 1339), "sa_nav_verify_java_desc_1")
ResDef(SA_VARNAME_NAV_VERIFY_JAVA_DESC_2, (SEC_DIALOG_STRING_BASE + 1340), "sa_nav_verify_java_desc_2")
ResDef(SA_VARNAME_NAV_VERIFY_JS_DESC_1, (SEC_DIALOG_STRING_BASE + 1341), "sa_nav_verify_js_desc_1")
ResDef(SA_VARNAME_NAV_VERIFY_JS_DESC_2, (SEC_DIALOG_STRING_BASE + 1342), "sa_nav_verify_js_desc_2")
ResDef(SA_VARNAME_NOT_ME_SUBTITLE, (SEC_DIALOG_STRING_BASE + 1343), "sa_not_me_subtitle")
ResDef(SA_VARNAME_NOT_ME_DESC_1, (SEC_DIALOG_STRING_BASE + 1344), "sa_not_me_desc_1")
ResDef(SA_VARNAME_NOT_ME_DESC_2, (SEC_DIALOG_STRING_BASE + 1345), "sa_not_me_desc_2")
ResDef(SA_VARNAME_CERTS_INTRO_STRING_1, (SEC_DIALOG_STRING_BASE + 1346), "sa_certs_intro_string_1")
ResDef(SA_VARNAME_CERTS_INTRO_STRING_2, (SEC_DIALOG_STRING_BASE + 1347), "sa_certs_intro_string_2")
ResDef(SA_VARNAME_CERTS_INTRO_STRING_3, (SEC_DIALOG_STRING_BASE + 1348), "sa_certs_intro_string_3")
ResDef(SA_VARNAME_CERTS_INTRO_STRING_4, (SEC_DIALOG_STRING_BASE + 1349), "sa_certs_intro_string_4")
ResDef(SA_VARNAME_CERTS_INTRO_STRING_5, (SEC_DIALOG_STRING_BASE + 1350), "sa_certs_intro_string_5")
ResDef(SA_VARNAME_CERTS_INTRO_STRING_6, (SEC_DIALOG_STRING_BASE + 1351), "sa_certs_intro_string_6")
ResDef(SA_VARNAME_YOUR_CERTS_DESC_1, (SEC_DIALOG_STRING_BASE + 1352), "sa_your_certs_desc_1")
ResDef(SA_VARNAME_YOUR_CERTS_DESC_2, (SEC_DIALOG_STRING_BASE + 1353), "sa_your_certs_desc_2")
ResDef(SA_VARNAME_YOUR_CERTS_DESC_TAIL_1, (SEC_DIALOG_STRING_BASE + 1354), "sa_your_certs_desc_tail_1")
ResDef(SA_VARNAME_YOUR_CERTS_DESC_TAIL_2, (SEC_DIALOG_STRING_BASE + 1355), "sa_your_certs_desc_tail_2")
ResDef(SA_VARNAME_PEOPLE_CERTS_DESC_1, (SEC_DIALOG_STRING_BASE + 1356), "sa_people_certs_desc_1")
ResDef(SA_VARNAME_PEOPLE_CERTS_DESC_2, (SEC_DIALOG_STRING_BASE + 1357), "sa_people_certs_desc_2")
ResDef(SA_VARNAME_GET_CERTS_DESC, (SEC_DIALOG_STRING_BASE + 1358), "sa_get_certs_desc")
ResDef(SA_VARNAME_SITE_CERTS_DESC, (SEC_DIALOG_STRING_BASE + 1359), "sa_site_certs_desc")
ResDef(SA_VARNAME_SIGNERS_CERTS_DESC, (SEC_DIALOG_STRING_BASE + 1360), "sa_signers_certs_desc")
ResDef(SA_VARNAME_SIGNERS_VIEW_CRL_DESC, (SEC_DIALOG_STRING_BASE + 1361), "sa_signers_view_crl_desc")
ResDef(SA_VARNAME_SSL_DESC, (SEC_DIALOG_STRING_BASE + 1362), "sa_ssl_desc")
ResDef(SA_VARNAME_SMIME_DESC, (SEC_DIALOG_STRING_BASE + 1363), "sa_smime_desc")
ResDef(SA_VARNAME_SMIME_PREF_DESC, (SEC_DIALOG_STRING_BASE + 1364), "sa_smime_pref_desc")
ResDef(SA_VARNAME_ENCRYPT_ALWAYS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1365), "sa_encrypt_always_checkbox_label")
ResDef(SA_VARNAME_SIGN_MAIL_ALWAYS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1366), "sa_sign_mail_always_checkbox_label")
ResDef(SA_VARNAME_SIGN_NEWS_ALWAYS_CHECKBOX_LABEL, (SEC_DIALOG_STRING_BASE + 1367), "sa_sign_news_always_checkbox_label")
ResDef(SA_VARNAME_SMIME_CERT_DESC, (SEC_DIALOG_STRING_BASE + 1368), "sa_smime_cert_desc")
ResDef(SA_VARNAME_SMIME_SEND_CERT_DESC, (SEC_DIALOG_STRING_BASE + 1369), "sa_smime_send_cert_desc")
ResDef(SA_VARNAME_SMIME_NO_CERT_DESC_1, (SEC_DIALOG_STRING_BASE + 1370), "sa_smime_no_cert_desc_1")
ResDef(SA_VARNAME_SMIME_NO_CERT_DESC_2, (SEC_DIALOG_STRING_BASE + 1371), "sa_smime_no_cert_desc_2")
ResDef(SA_VARNAME_SMIME_CIPHER_HEADING, (SEC_DIALOG_STRING_BASE + 1372), "sa_smime_cipher_heading")
ResDef(SA_VARNAME_SMIME_CIPHER_DESC, (SEC_DIALOG_STRING_BASE + 1373), "sa_smime_cipher_desc")
ResDef(SA_VARNAME_APPLETS_DESC_1, (SEC_DIALOG_STRING_BASE + 1374), "sa_applets_desc_1")
ResDef(SA_VARNAME_APPLETS_DESC_2, (SEC_DIALOG_STRING_BASE + 1375), "sa_applets_desc_2")
ResDef(SA_VARNAME_PASSWORD_DESC_1, (SEC_DIALOG_STRING_BASE + 1376), "sa_password_desc_1")
ResDef(SA_VARNAME_PASSWORD_DESC_2, (SEC_DIALOG_STRING_BASE + 1377), "sa_password_desc_2")
ResDef(SA_VARNAME_MODULES_DESC, (SEC_DIALOG_STRING_BASE + 1378), "sa_modules_desc")
ResDef(SA_VARNAMES_END, (SEC_DIALOG_STRING_BASE + 1379), "")

/* Don't add new strings here... add them above SA_STRINGS_BEGIN... You don't
 * want to start renumbering them every time the security advisor changes!!!
 */
END_STR(mcom_include_sec_dialog_strings)


/* WARNING: DO NOT TAKE ERROR CODE -666, it is used internally
   by the message lib */

/* SSL-specific security error codes  */

#include "sslerr.h"

RES_START
BEGIN_STR(mcom_include_sslerr_i_strings)

ResDef(SSL_ERROR_EXPORT_ONLY_SERVER,    SSL_ERROR_BASE + 0,
MOZ_NAME_BRAND" is unable to communicate securely with this site\n\
because the server does not support high-grade encryption.")

ResDef(SSL_ERROR_US_ONLY_SERVER,    SSL_ERROR_BASE + 1,
MOZ_NAME_BRAND" is unable to communicate securely with this site\n\
because the server requires the use of high-grade encryption.\n\n\
This version of "MOZ_NAME_PRODUCT" does not support high-grade\n\
encryption, probably due to U.S. export restrictions.")

ResDef(SSL_ERROR_NO_CYPHER_OVERLAP, SSL_ERROR_BASE + 2,
MOZ_NAME_BRAND" and this server cannot communicate securely\n\
because they have no common encryption algorithm(s).")

ResDef(SSL_ERROR_NO_CERTIFICATE,    SSL_ERROR_BASE + 3,
MOZ_NAME_BRAND" is unable to find the certificate or key necessary\n\
for authentication.")

ResDef(SSL_ERROR_BAD_CERTIFICATE,   SSL_ERROR_BASE + 4,
MOZ_NAME_BRAND" is unable to communicate securely with this site\n\
because the server's certificate was rejected.")

/* unused               (SSL_ERROR_BASE + 5),   */

ResDef(SSL_ERROR_BAD_CLIENT,        SSL_ERROR_BASE + 6,
"The server has encountered bad data from the client.")

ResDef(SSL_ERROR_BAD_SERVER,        SSL_ERROR_BASE + 7,
MOZ_NAME_BRAND" has encountered bad data from the server.")

ResDef(SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE,  SSL_ERROR_BASE + 8,
MOZ_NAME_BRAND" has encountered an unsupported type of certificate.\n\n\
A newer version of "MOZ_NAME_PRODUCT" may solve this problem.")

ResDef(SSL_ERROR_UNSUPPORTED_VERSION,   SSL_ERROR_BASE + 9,
"The server is using an unsupported version of the security\n\
protocol.\n\n\
A newer version of "MOZ_NAME_PRODUCT" may solve this problem.")

/* unused               (SSL_ERROR_BASE + 10),  */

ResDef(SSL_ERROR_WRONG_CERTIFICATE, SSL_ERROR_BASE + 11,
"Client authentication failed due to mismatch between private\n\
key found in client key database and public key found in client\n\
certificate database.")

ResDef(SSL_ERROR_BAD_CERT_DOMAIN,   SSL_ERROR_BASE + 12,
MOZ_NAME_BRAND" is unable to communicate securely with this site\n\
because the domain to which you are attempting to connect\n\
does not match the domain name in the server's certificate.")

/* SSL_ERROR_POST_WARNING       (SSL_ERROR_BASE + 13),
   defined in sslerr.h
*/

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SSL_ERROR_SSL2_DISABLED,     (SSL_ERROR_BASE + 14),
"This site only supports SSL version 2.  You can enable\n\
support for SSL version 2 by selecting Security Info from\n\
the "MOZ_NAME_PRODUCT" menu and opening the "MOZ_NAME_PRODUCT" section.")
#else
ResDef(SSL_ERROR_SSL2_DISABLED,     (SSL_ERROR_BASE + 14),
"This site only supports SSL version 2.  You can enable\n\
support for SSL version 2 by selecting Security Info from\n\
the Communicator menu and opening the "MOZ_NAME_PRODUCT" section.")
#endif

ResDef(SSL_ERROR_BAD_MAC_READ,      (SSL_ERROR_BASE + 15),
"SSL has received a record with an incorrect Message\n\
Authentication Code.  This could indicate a network error,\n\
a bad server implementation, or a security violation.")

ResDef(SSL_ERROR_BAD_MAC_ALERT,     (SSL_ERROR_BASE + 16),
"SSL has received an error from the server indicating an\n\
incorrect Message Authentication Code.  This could indicate\n\
a network error, a bad server implementation, or a\n\
security violation.")

ResDef(SSL_ERROR_BAD_CERT_ALERT,    (SSL_ERROR_BASE + 17),
"The server cannot verify your certificate.")

ResDef(SSL_ERROR_REVOKED_CERT_ALERT,    (SSL_ERROR_BASE + 18),
"The server has rejected your certificate as revoked.")

ResDef(SSL_ERROR_EXPIRED_CERT_ALERT,    (SSL_ERROR_BASE + 19),
"The server has rejected your certificate as expired.")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(SSL_ERROR_SSL_DISABLED,      (SSL_ERROR_BASE + 20),
"You cannot connect to an encrypted website because SSL\n\
has been disabled.  You can enable SSL by selecting\n\
Security Info from the "MOZ_NAME_PRODUCT" menu and opening the\n\
"MOZ_NAME_PRODUCT" section.")
#else
ResDef(SSL_ERROR_SSL_DISABLED,      (SSL_ERROR_BASE + 20),
"You cannot connect to an encrypted website because SSL\n\
has been disabled.  You can enable SSL by selecting\n\
Security Info from the Communicator menu and opening the\n\
"MOZ_NAME_PRODUCT" section.")
#endif

ResDef(SSL_ERROR_FORTEZZA_PQG,      (SSL_ERROR_BASE + 21),
"The server is in another FORTEZZA domain,\n\
you cannot connect to it.")

END_STR(mcom_include_sslerr_i_strings)


/* from xp_error.h */

#include <errno.h>


/* System error codes */

#ifdef XP_WIN

#include "winsock.h"

RES_START
BEGIN_STR(mcom_include_xp_error_i_strings)
ResDef(XP_ERRNO_NOTINITIALISED  ,WSANOTINITIALISED  ,"Winsock is uninitialized")
ResDef(XP_ERRNO_EWOULDBLOCK     ,WSAEWOULDBLOCK     ,"Operation would block")
ResDef(XP_ERRNO_ECONNREFUSED    ,WSAECONNREFUSED    ,"Connection refused")
ResDef(XP_ERRNO_EINVAL          ,WSAEINVAL          ,"Invalid argument")
#ifndef RESOURCE_STR            /* ID conflicts */
ResDef(XP_ERRNO_EIO             ,WSAECONNREFUSED    ,"I/O error")
#endif
ResDef(XP_ERRNO_ENOMEM          ,WSAENOBUFS         ,"Not enough memory")
ResDef(XP_ERRNO_EBADF           ,WSAEBADF           ,"Bad file number")
#ifndef RESOURCE_STR            /* ID conflicts  */
ResDef(XP_ERRNO_HANDSHAKE       ,WSAECONNREFUSED    ,"Handshake failed")
#endif
ResDef(XP_ERRNO_EISCONN         ,WSAEISCONN         ,"Socket is already connected")
ResDef(XP_ERRNO_ETIMEDOUT       ,WSAETIMEDOUT       ,"Connection timed out")
ResDef(XP_ERRNO_EINPROGRESS     ,WSAEINPROGRESS     ,"operation now in progress")
#ifndef RESOURCE_STR            /* ID conflicts */
ResDef(XP_ERRNO_EAGAIN          ,WSAEWOULDBLOCK     ,"EAGAIN")
#endif
ResDef(XP_ERRNO_EALREADY        ,WSAEALREADY        ,"EALREADY")
ResDef(XP_ERRNO_EADDRINUSE      ,WSAEADDRINUSE      ,"Address already in use")

/* new by lou 2-21-95 */
ResDef(XP_ERRNO_EINTR           ,WSAEINTR           ,"interrupted system call")
ResDef(XP_ERRNO_EACCES          ,WSAEACCES          ,"Permission denied")
ResDef(XP_ERRNO_EADDRNOTAVAIL   ,WSAEADDRNOTAVAIL   ,"Can't assign requested address")
ResDef(XP_ERRNO_ENETDOWN        ,WSAENETDOWN        ,"Network is down")
ResDef(XP_ERRNO_ENETUNREACH     ,WSAENETUNREACH     ,"Network is unreachable")
ResDef(XP_ERRNO_ENETRESET       ,WSAENETRESET       ,"Network dropped connection because of reset")
ResDef(XP_ERRNO_ECONNABORTED    ,WSAECONNABORTED    ,"Connection aborted")
ResDef(XP_ERRNO_ECONNRESET      ,WSAECONNRESET      ,"Connection reset by peer")
ResDef(XP_ERRNO_ENOTCONN        ,WSAENOTCONN        ,"Socket is not connected")
ResDef(XP_ERRNO_EHOSTDOWN       ,WSAEHOSTDOWN       ,"Host is down")
ResDef(XP_ERRNO_EHOSTUNREACH    ,WSAEHOSTUNREACH    ,"No route to host")
END_STR(mcom_include_xp_error_i_strings)

#else
#ifdef XP_MAC
#include "xp_sock.h"
#endif
RES_START
BEGIN_STR(mcom_include_xp_error_i_strings)
ResDef(XP_ERRNO_EPIPE           ,EPIPE              ,"Broken pipe")
ResDef(XP_ERRNO_ECONNREFUSED    ,ECONNREFUSED       ,"Connection refused")
ResDef(XP_ERRNO_EINVAL          ,EINVAL             ,"Invalid argument")
ResDef(XP_ERRNO_EIO             ,EIO                ,"I/O error")
ResDef(XP_ERRNO_ENOMEM          ,ENOMEM             ,"Not enough memory")
ResDef(XP_ERRNO_EBADF           ,EBADF              ,"Bad file number")
#ifndef RESOURCE_STR            /* ID conflicts */
ResDef(XP_ERRNO_HANDSHAKE       ,EIO                ,"Handshake failed")
#endif
ResDef(XP_ERRNO_EWOULDBLOCK     ,EWOULDBLOCK        ,"Operation would block")
ResDef(XP_ERRNO_EISCONN         ,EISCONN            ,"Socket is already connected")
ResDef(XP_ERRNO_ETIMEDOUT       ,ETIMEDOUT          ,"Connection timed out")
ResDef(XP_ERRNO_EINPROGRESS     ,EINPROGRESS        ,"operation now in progress")
#ifndef RESOURCE_STR            /* ID conflicts */
ResDef(XP_ERRNO_EAGAIN          ,EAGAIN             ,"EAGAIN")
#endif
ResDef(XP_ERRNO_EALREADY        ,EALREADY           ,"EALREADY")
ResDef(XP_ERRNO_EADDRINUSE      ,EADDRINUSE         ,"Address already in use")

/* new by lou 2-21-95 */
ResDef(XP_ERRNO_EINTR           ,EINTR              ,"interrupted system call")
ResDef(XP_ERRNO_EACCES          ,EACCES             ,"Permission denied")
#ifdef NOTDEF                       /* ID conflicts */
ResDef(XP_ERRNO_EADDRINUSE      ,EADDRINUSE         ,"Can't assign requested address")
#endif
ResDef(XP_ERRNO_EADDRNOTAVAIL   ,EADDRNOTAVAIL      ,"Can't assign requested address")
ResDef(XP_ERRNO_ENETDOWN        ,ENETDOWN           ,"Network is down")
ResDef(XP_ERRNO_ENETUNREACH     ,ENETUNREACH        ,"Network is unreachable")
ResDef(XP_ERRNO_ENETRESET       ,ENETRESET          ,"Network dropped connection because of reset")
ResDef(XP_ERRNO_ECONNABORTED    ,ECONNABORTED       ,"Connection aborted")
ResDef(XP_ERRNO_ECONNRESET      ,ECONNRESET         ,"Connection reset by peer")
ResDef(XP_ERRNO_ENOTCONN        ,ENOTCONN           ,"Socket is not connected")
ResDef(XP_ERRNO_EHOSTDOWN       ,EHOSTDOWN          ,"Host is down")
ResDef(XP_ERRNO_EHOSTUNREACH    ,EHOSTUNREACH       ,"No route to host")
END_STR(mcom_include_xp_error_i_strings)
#endif


/* WARNING: DO NOT TAKE ERROR CODE -666, it is used internally
   by the message lib */


RES_START
BEGIN_STR(mcom_include_xp_msg_i_strings)
ResDef(XP_DOCINFO_1,    XP_MSG_BASE+1,  " (unrecognized)")
ResDef(XP_DOCINFO_2,    XP_MSG_BASE+2,      " (autoselect)"             )
ResDef(XP_DOCINFO_3,    XP_MSG_BASE+3,  " (default)")
ResDef(XP_DOCINFO_4,    XP_MSG_BASE+4,  " (not found)" )
ResDef(XP_IMAGE_1,      XP_MSG_BASE+10, "%s image %dx%d pixels")
ResDef(XP_IMAGE_2,      XP_MSG_BASE+11, "Images are %d bits deep with %d cells allocated.")
ResDef(XP_IMAGE_3,      XP_MSG_BASE+12, "Images are monochrome.")
ResDef(XP_IMAGE_4,      XP_MSG_BASE+13, "Images are %d bit greyscale.")
ResDef(XP_IMAGE_5,      XP_MSG_BASE+14, "Images are %d bit truecolor.")
ResDef(XP_LAYFORM_1,    XP_MSG_BASE+20, "This is a searchable index. Enter search keywords: ")
ResDef(XP_HISTORY_2,    XP_MSG_BASE+31, "Main Hotlist")
ResDef(XP_HISTORY_3,    XP_MSG_BASE+32,
"<!-- This is an automatically generated file.\n\
    It will be read and overwritten.\n\
    Do Not Edit! -->\n")
ResDef(XP_HISTORY_4,                    XP_MSG_BASE+33,     "---End of History List---\n")
ResDef(XP_HISTORY_SAVE,                 XP_MSG_BASE+34,     "Save History List")
ResDef(XP_PROGRESS_LOOKUPHOST,          XP_MSG_BASE+40,     "Connect: Looking up host: %.256s...")
ResDef(XP_PROGRESS_CONTACTHOST,         XP_MSG_BASE+41,     "Connect: Contacting host: %.256s...")
ResDef(XP_PROGRESS_NOCONNECTION,        XP_MSG_BASE+42,     "Error: Could not make connection non-blocking.")
ResDef(XP_PROGRESS_UNABLELOCATE,        XP_MSG_BASE+43,     "Unable to locate host %.256s.")
ResDef(XP_PROGRESS_5,                   XP_MSG_BASE+44,     "Unable to locate host %.256s.")
ResDef(XP_PROGRESS_READFILE,            XP_MSG_BASE+45,     "Reading file...")
ResDef(XP_PROGRESS_FILEZEROLENGTH,      XP_MSG_BASE+46,     "Reading file...Error Zero Length")
ResDef(XP_PROGRESS_READDIR,             XP_MSG_BASE+47,     "Reading directory...")
ResDef(XP_PROGRESS_FILEDONE,            XP_MSG_BASE+48,     "Reading file...Done")
ResDef(XP_PROGRESS_DIRDONE,             XP_MSG_BASE+49,     "Reading directory...Done")
ResDef(XP_PROGRESS_RECEIVE_FTPFILE,     XP_MSG_BASE+50,     "Receiving FTP file")
ResDef(XP_PROGRESS_RECEIVE_FTPDIR,      XP_MSG_BASE+51,     "Receiving FTP directory")
ResDef(XP_PROGRESS_RECEIVE_DATA,        XP_MSG_BASE+52,     "Receiving data.")
ResDef(XP_PROGRESS_TRANSFER_DATA,       XP_MSG_BASE+53,     "Transferring data from %.256s")
ResDef(XP_PROGRESS_WAIT_REPLY,          XP_MSG_BASE+54,     "Connect: Host %.256s contacted. Waiting for reply...")
ResDef(XP_PROGRESS_TRYAGAIN,            XP_MSG_BASE+55,     "Connect: Trying again (HTTP 0.9)...")
ResDef(XP_PROGRESS_WAITREPLY_GOTHER,    XP_MSG_BASE+56,     "Connect: Host contacted. Waiting for reply (Gopher)")
ResDef(XP_PROGRESS_MAILSENT,            XP_MSG_BASE+57,     "Mail sent successfully")
ResDef(XP_PROGRESS_RECEIVE_NEWSGROUP,   XP_MSG_BASE+58,     "Receiving newsgroups...")
ResDef(XP_PROGRESS_20,                  XP_MSG_BASE+59,     "Receiving newsgroups...")
ResDef(XP_PROGRESS_RECEIVE_ARTICLE,     XP_MSG_BASE+60,     "Receiving articles...")
ResDef(XP_PROGRESS_RECEIVE_LISTARTICLES,XP_MSG_BASE+61,     "Receiving articles...")
ResDef(XP_PROGRESS_READ_NEWSGROUPLIST,  XP_MSG_BASE+62,     "Reading newsgroup list")
ResDef(XP_PROGRESS_READ_NEWSGROUPINFO,  XP_MSG_BASE+63,     "Reading newsgroup overview information")
ResDef(XP_PROGRESS_SORT_ARTICLES,       XP_MSG_BASE+64,     "Sorting articles...")
ResDef(XP_PROGRESS_STARTING_JAVA,   XP_MSG_BASE+65, "Starting Java...")
ResDef(XP_PROGRESS_STARTING_JAVA_DONE,  XP_MSG_BASE+66, "Starting Java...done")
ResDef(XP_ALERT_UNABLE_INVOKEVIEWER,    XP_MSG_BASE+101,    "Unable to invoke external viewer")
ResDef(XP_ALERT_2,                      XP_MSG_BASE+102,    "Proxy is requiring an authentication scheme that is not supported.")
ResDef(XP_ALERT_OUTMEMORY,              XP_MSG_BASE+103,    "Out of memory error in HTTP Load routine!")
ResDef(XP_ALERT_UNKNOWN_STATUS,         XP_MSG_BASE+104,    "Unknown status reply from server: %d!")
ResDef(XP_ALERT_URLS_LESSZERO,          XP_MSG_BASE+105,    "Warning! Non-critical application error: NET_TotalNumberOfProcessingURLs < 0")
ResDef(XP_ALERT_CONNECTION_LESSZERO,    XP_MSG_BASE+106,    "Warning! Non-critical application error: NET_TotalNumberOfOpenConnections < 0")
ResDef(XP_ALERT_URN_USEHTTP,            XP_MSG_BASE+107,    "URN's not internally supported, use an HTTP proxy server: ")
ResDef(XP_ALERT_INTERRUPT_WINDOW,       XP_MSG_BASE+108,    "reentrant call to Interrupt window")
ResDef(XP_ALERT_BADMSG_NUMBER,          XP_MSG_BASE+109,    "Bad message number")
ResDef(XP_ALERT_ARTICLE_OUTRANGE,       XP_MSG_BASE+110,    "Article number out of range")
ResDef(XP_ALERT_CANTLOAD_MAILBOX,       XP_MSG_BASE+111,    "Could not load mailbox")
ResDef(XP_ALERT_SMTPERROR_SENDINGMAIL,  XP_MSG_BASE+113,    "SMTP Error sending mail. Server responded: %.256s")
ResDef(XP_ALERT_UNRECOGNIZED_ENCODING,  XP_MSG_BASE+114,    "Warning: unrecognized encoding: `")
ResDef(XP_ALERT_CANTFIND_CONVERTER,     XP_MSG_BASE+115,    "Alert! did not find a converter or decoder")
ResDef(XP_ALERT_CANT_FORMHOTLIST,       XP_MSG_BASE+116,    "Cannot add the result of a form submission to the hotlist")
#ifdef XP_UNIX
ResDef(XP_UNKNOWN_HTTP_PROXY,           XP_MSG_BASE+117,
"Warning: an HTTP proxy host was specified\n(\
%.2048s), but that host is unknown.\n\
\n\
This means that external hosts will be unreachable.\n\
Perhaps there is a problem with your name server?\n\
Consult your system administrator.")
ResDef(XP_UNKNOWN_SOCKS_HOST,           XP_MSG_BASE+118,
"Warning: a SOCKS host was specified (%.2048s)\n\
but that host is unknown.\n\
\n\
This means that external hosts will be unreachable.\n\
\n\
Perhaps there is a problem with your name server?\n")
ResDef(XP_SOCKS_NS_ENV_VAR,             XP_MSG_BASE+119,
"If your site must use a non-root name server, you will\n\
need to set the $SOCKS_NS environment variable to\n\
point at the appropriate name server.  It may (or\n\
may not) be necessary to set this variable, or the\n\
SOCKS host preference, to the IP address of the host\n\
in question rather than its name.\n\
\n")
ResDef(XP_CONSULT_SYS_ADMIN,            XP_MSG_BASE+120,
"Consult your system administrator.")
ResDef(XP_UNKNOWN_HOSTS,                XP_MSG_BASE+121,
"Warning: the following hosts are unknown:\n\n")
ResDef(XP_UNKNOWN_HOST,                 XP_MSG_BASE+122,
"Warning: the host %.256s is unknown.\n")
ResDef(XP_SOME_HOSTS_UNREACHABLE,       XP_MSG_BASE+123,
"\n\
This means that some or all hosts will be unreachable.\n\
\n\
Perhaps there is a problem with your name server?\n")
ResDef(XP_THIS_IS_DNS_VERSION,          XP_MSG_BASE+124,
"On SunOS 4 systems, there are two %s executables,\n\
one for sites using DNS, and one for sites using YP/NIS.\n\
This is the DNS executable.  Perhaps you need to use\n\
the other one?\n\n")
ResDef(XP_THIS_IS_YP_VERSION,           XP_MSG_BASE+125,
"On SunOS 4 systems, there are two %s executables,\n\
one for sites using DNS, and one for sites using YP/NIS.\n\
This is the YP/NIS executable.  Perhaps you need to use\n\
the other one?\n\n")

ResDef(XP_CONFIRM_EXEC_UNIXCMD_ARE, XP_MSG_BASE+130,
"Warning: this is an executable `%.1024s' script!\n\n\
You are about to execute arbitrary system commands\n\
on your local machine.  This is a security risk.\n\
Unless you completely understand this script, it\n\
is strongly recommended you not do this.\n\n\
Execute the script?")

ResDef(XP_CONFIRM_EXEC_UNIXCMD_MAYBE,   XP_MSG_BASE+131,
"Warning: this is an executable `%.1024s' script!\n\n\
You may be about to execute arbitrary system commands\n\
on your local machine.  This is a security risk.\n\
Unless you completely understand this script, it\n\
is strongly recommended you not do this.\n\n\
Execute the script?")

#endif /* XP_UNIX */
ResDef(XP_CONFIRM_AUTHORIZATION_FAIL,   XP_MSG_BASE+133,    "Authorization failed.  Retry?")
ResDef(XP_CONFIRM_PROXYAUTHOR_FAIL, XP_MSG_BASE+134,    "Proxy authorization failed.  Retry?")
ResDef(XP_CONFIRM_REPOST_FORMDATA,      XP_MSG_BASE+135,    "Repost form data?")
ResDef(XP_CONFIRM_SAVE_NEWSGROUPS,      XP_MSG_BASE+136,
"Before viewing all newsgroups, "MOZ_NAME_PRODUCT" saves\n\
a copy of the newsgroup list.\n\n\
On a modem or slow connection, this may take a\n\
few minutes. You can choose New Window from the\n\
File menu to continue browsing.  Proceed?")
ResDef(XP_CONFIRM_MAILTO_POST_1,        XP_MSG_BASE+137,
"This form is being submitted via e-mail.\n\
Submitting the form via e-mail will reveal\n\
your e-mail address to the recipient, and\n\
will send the form data without encrypting\n")
ResDef(XP_CONFIRM_MAILTO_POST_2,        XP_MSG_BASE+139,
"it for privacy.  You may not want to submit\n\
sensitive or private information via this\n\
form.  You may continue or cancel this\n\
submission.  ")

ResDef(XP_ABORT_INVALID_SPECIFIER,      XP_MSG_BASE+151,    "message: invalid specifier `%c'\n")
ResDef(XP_ABORT_2,                      XP_MSG_BASE+152,    "Implement more temp name")
ResDef(XP_ABORT_3,                      XP_MSG_BASE+153,    "Implement more XPStats")


ResDef(XP_PROMPT_ENTER_USERNAME,        XP_MSG_BASE+160,    "Please enter a username for news server access")
ResDef(XP_PROMPT_ENTER_PASSWORD,        XP_MSG_BASE+161,    "Enter password for user %s:")

/* XP_HTML_NEWS unused                  XP_MSG_BASE+200, */
/* XP_HTML_SUBSCRIBE_1 unused               XP_MSG_BASE+201, */
/* XP_HTML_SUBSCRIBE_2 unused               XP_MSG_BASE+202, */
/* XP_HTML_SUBSCRIBE_3 unused               XP_MSG_BASE+203, */

ResDef(XP_HTML_NEWS_ERROR,              XP_MSG_BASE+204,
"<TITLE>Error!</TITLE>\n\
<H1>Error!</H1> newsgroup server responded: <b>%.256s</b><p>\n")

ResDef(XP_HTML_ARTICLE_EXPIRED,         XP_MSG_BASE+205,
"<b><p>Perhaps the article has expired</b><p>\n")

/* XP_HTML_NEWSGROUP_LIST unused         XP_MSG_BASE+206, */
/* XP_HTML_LISTNEW_NEWS unused          XP_MSG_BASE+207, */
/* XP_HTML_NEWSERROR_MAILADDRESS unused             XP_MSG_BASE+209, */

ResDef(XP_HTML_FTPERROR_NOLOGIN,        XP_MSG_BASE+218,
"<TITLE>FTP Error</TITLE>\n<H1>FTP Error</H1>\n<h2>Could not login to FTP server</h2>\n<PRE>")

ResDef(XP_HTML_FTPERROR_TRANSFER,       XP_MSG_BASE+219,
"<TITLE>FTP Error</TITLE>\n<H1>FTP Error</H1>\n<h2>FTP Transfer failed:</h2>\n<PRE>")

ResDef(XP_HTML_GOPHER_INDEX,       XP_MSG_BASE+220,
"<TITLE>Gopher Index %.256s</TITLE><H1>%.256s <BR>Gopher Search</H1>\nThis is a searchable Gopher index.\n\
Use the search function of your browser to enter search terms.\n<ISINDEX>")

ResDef(XP_HTML_CSO_SEARCH,       XP_MSG_BASE+221,
"<TITLE>CSO Search of %.256s</TITLE><H1>%.256s CSO Search</H1>\nA CSO database usually contains a phonebook or directory.\n\
Use the search function of your browser to enter search terms.\n<ISINDEX>")

ResDef(XP_HTML_MISSING_REPLYDATA,       XP_MSG_BASE+222,
"<TITLE>Missing Post reply data</TITLE>\n<H1>Data Missing</H1>\nThis document resulted from a POST operation and has expired from the\n\
cache.  If you wish you can repost the form data to recreate the\ndocument by pressing the <b>reload</b> button.\n")


ResDef(XP_SEC_GOT_RSA,                  XP_MSG_BASE+250,
"RSA Public Key Cryptography")

ResDef(XP_SEC_INTERNATIONAL,            XP_MSG_BASE+251,
"International")

#ifdef XP_UNIX
ResDef(XP_SEC_NO_MESSAGE,               XP_MSG_BASE+252,
 "This is an insecure document that is not encrypted and offers no security\nprotection.")
#else
ResDef(XP_SEC_NO_MESSAGE,               XP_MSG_BASE+252,
 "This is an insecure document that is not encrypted and offers no security protection.")
#endif


ResDef(XP_NEWS_NONEWGROUP,              XP_MSG_BASE+253,
"<h3>No new newsgroups</h3>")

/* XP_NEWS_NOARTICLES unused                XP_MSG_BASE+254, */
/* XP_NEWS_NOARTICLES_INRANGE unused        XP_MSG_BASE+255, */

ResDef(XP_ACCESS_ENTER_USERNAME,        XP_MSG_BASE+256,
"Enter username for %.200s at %.200s:")

ResDef(XP_NEWS_PROMPT_ADD_NEWSGROUP,        XP_MSG_BASE+257,
"Type in a newsgroup to add to the list:")

#ifdef XP_UNIX
ResDef(XP_SEC_LOW_MESSAGE,              XP_MSG_BASE+260,
 "This is a secure document that uses a medium-grade encryption key suited\nfor U.S. export")
ResDef(XP_SEC_HIGH_MESSAGE,             XP_MSG_BASE+261,
 "This is a secure document that uses a high-grade encryption key for U.S.\ndomestic use only")
#else
ResDef(XP_SEC_LOW_MESSAGE,              XP_MSG_BASE+260,
 "This is a secure document that uses a medium-grade encryption key suited for U.S. export")
ResDef(XP_SEC_HIGH_MESSAGE,             XP_MSG_BASE+261,
 "This is a secure document that uses a high-grade encryption key for U.S. domestic use only")
#endif

ResDef(XP_BKMKS_BOOKMARKS_CHANGED,          XP_MSG_BASE+270,
 "Bookmarks have changed on disk and are being reloaded.")
ResDef(XP_BKMKS_ADDRESSBOOK_CHANGED,            XP_MSG_BASE+271,
 "The address book has changed on disk and is being reloaded.")
ResDef(XP_BKMKS_BOOKMARKS_CONFLICT,         XP_MSG_BASE+272,
 "Bookmarks have changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(XP_BKMKS_ADDRESSBOOK_CONFLICT,           XP_MSG_BASE+273,
 "The address book has changed on disk.  Discard your unsaved changes\n\
and reload?")
ResDef(XP_BKMKS_CANT_WRITE_BOOKMARKS,           XP_MSG_BASE+274,
 "Error saving bookmarks file!")
ResDef(XP_BKMKS_CANT_WRITE_ADDRESSBOOK,         XP_MSG_BASE+275,
 "Error saving address book file!")


/* BEGIN HARD CODED STRING EXTRACTION */


ResDef(XP_LAYOUT_DEF_ISINDEX_PROMPT,            XP_MSG_BASE+276,
 "This is a searchable index. Enter search keywords: ")

ResDef(XP_LAYOUT_DEF_RESET_BUTTON_TEXT,         XP_MSG_BASE+277,
 "Reset")

ResDef(XP_LAYOUT_DEF_SUBMIT_BUTTON_TEXT,        XP_MSG_BASE+278,
 "Submit Query")

ResDef(XP_LAYOUT_NO_DOC_INFO_WHILE_LOADING,     XP_MSG_BASE+279,
 "<H3>No info while document is loading</H3>\n")

ResDef(XP_LAYOUT_LI_ENCODING,                   XP_MSG_BASE+280,
 "<LI>Encoding: ")

ResDef(XP_LAYOUT_FORM_LIST,                     XP_MSG_BASE+281,
 "<b>Form %d:</b><UL>")

ResDef(XP_BKMKS_HOURS_AGO,                      XP_MSG_BASE+282,
 "%ld hours ago")

ResDef(XP_BKMKS_DAYS_AGO,                       XP_MSG_BASE+283,
 "%ld days ago")

ResDef(XP_BKMKS_COUNTALIASES_MANY,              XP_MSG_BASE+284,
 "There are %ld aliases to %s")

ResDef(XP_BKMKS_COUNTALIASES_ONE,               XP_MSG_BASE+285,
 "There is 1 alias to %s")

ResDef(XP_BKMKS_COUNTALIASES_NONE,              XP_MSG_BASE+286,
 "There are no aliases to %s")

ResDef(XP_BKMKS_INVALID_NICKNAME,               XP_MSG_BASE+287,
 "Nicknames may only have letters and numbers\n\
in them.  The nickname has not been changed.")

ResDef(XP_BKMKS_NICKNAME_ALREADY_EXISTS,        XP_MSG_BASE+288,
 "An entry with this nickname already exists.\n\
The nickname has not been changed.")

ResDef(XP_BKMKS_REMOVE_THIS_ITEMS_ALIASES,        XP_MSG_BASE+289,
 "This item has %d alias(es). These aliases \n\
will be removed, as well.")

ResDef(XP_BKMKS_REMOVE_SOME_ITEMS_ALIASES,        XP_MSG_BASE+290,
 "Some of the items you are about to remove \n\
have one or more aliases. The aliases will \n\
be removed, as well.")

ResDef(XP_BKMKS_AUTOGENERATED_FILE,             XP_MSG_BASE+291,
 "<!-- This is an automatically generated file.")

ResDef(XP_BKMKS_READ_AND_OVERWRITE,             XP_MSG_BASE+292,
 "It will be read and overwritten.")

ResDef(XP_BKMKS_DO_NOT_EDIT,                    XP_MSG_BASE+293,
 "Do Not Edit! -->")

ResDef(XP_BKMKS_NEW_HEADER,                     XP_MSG_BASE+294,
 "New Folder")

ResDef(XP_BKMKS_NEW_BOOKMARK,                   XP_MSG_BASE+295,
 "New Bookmark")

ResDef(XP_BKMKS_NOT_FOUND,                      XP_MSG_BASE+296,
 "Not Found")

ResDef(XP_BKMKS_OPEN_BKMKS_FILE,                XP_MSG_BASE+297,
 "Open bookmarks file")

ResDef(XP_BKMKS_IMPORT_BKMKS_FILE,              XP_MSG_BASE+298,
"Import bookmarks file")

ResDef(XP_BKMKS_SAVE_BKMKS_FILE,                XP_MSG_BASE+299,
 "Save bookmarks file")

ResDef(XP_BKMKS_LESS_THAN_ONE_HOUR_AGO,         XP_MSG_BASE+300,
 "Less than one hour ago")

ResDef(XP_GLHIST_DATABASE_CLOSED,               XP_MSG_BASE+301,
 "The global history database is currently closed")

ResDef(XP_GLHIST_UNKNOWN,                       XP_MSG_BASE+302,
 "Unknown")

ResDef(XP_GLHIST_DATABASE_EMPTY,                XP_MSG_BASE+303,
"The global history database is currently empty")

ResDef(XP_GLHIST_HTML_DATE,                     XP_MSG_BASE+304,
 "<BR>\n<TT>Date:</TT> %s<P>")

ResDef(XP_GLHIST_HTML_TOTAL_ENTRIES,            XP_MSG_BASE+305,
 "\n<HR>\n<TT>Total entries:</TT> %ld<P>")

/* this isn't used, I think.  Shall we remove it? */
ResDef(XP_HOTLIST_DEF_NAME,                     XP_MSG_BASE+306,
 "Personal Bookmarks")

ResDef(XP_HOTLIST_AUTOGENERATED_FILE,           XP_MSG_BASE+307,
 "<!-- This is an automatically generated file.\n\
It will be read and overwritten.\n\
Do Not Edit! -->\n")

ResDef(XP_PLUGIN_LOADING_PLUGIN,                XP_MSG_BASE+308,
 "Loading plug-in")

ResDef(XP_THERMO_BYTE_RATE_FORMAT,                   XP_MSG_BASE+309,
 "at %ld bytes/sec")

ResDef(XP_THERMO_K_RATE_FORMAT,                 XP_MSG_BASE+310,
 "at %.1fK/sec")

ResDef(XP_THERMO_M_RATE_FORMAT,                 XP_MSG_BASE+311,
 "at %.1fM/sec")

ResDef(XP_THERMO_STALLED_FORMAT,                XP_MSG_BASE+312,
 "stalled")

ResDef(XP_THERMO_BYTE_FORMAT,                   XP_MSG_BASE+313,
 "%lu")

ResDef(XP_THERMO_KBYTE_FORMAT,                   XP_MSG_BASE+314,
 "%luK")

ResDef(XP_THERMO_MBYTE_FORMAT,                   XP_MSG_BASE+315,
 "%3.2fM")

ResDef(XP_THERMO_HOURS_FORMAT,                   XP_MSG_BASE+316,
 "%02ld:%02ld:%02ld remaining")

ResDef(XP_THERMO_MINUTES_FORMAT,                 XP_MSG_BASE+317,
 "%02ld:%02ld remaining")

ResDef(XP_THERMO_SECONDS_FORMAT,                 XP_MSG_BASE+318,
 "%ld sec%s remaining")


ResDef(XP_SEC_VERSION,                           XP_MSG_BASE+319,
 "Version: ")

ResDef(XP_SEC_SERIAL_NUMBER,                     XP_MSG_BASE+320,
 "Serial Number: ")

ResDef(XP_SEC_ISSUER,                            XP_MSG_BASE+321,
 "Issuer: ")

ResDef(XP_SEC_SUBJECT,                           XP_MSG_BASE+322,
 "Subject: ")

ResDef(XP_PRETTY_CERT_INF0,                      XP_MSG_BASE+323,
 "Version: %s%sSerial Number: %s%sIssuer:  %s%sSubject: %s%sNot Valid Before: %s%sNot Valid After: %s%s")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(XP_SEC_ENTER_KEYFILE_PWD,                 XP_MSG_BASE+324,
 "Enter Your "MOZ_NAME_PRODUCT" Password:")
#else
ResDef(XP_SEC_ENTER_KEYFILE_PWD,                 XP_MSG_BASE+324,
 "Enter Your Communicator Password:")
#endif

ResDef(XP_PLUGIN_NOT_FOUND,                      XP_MSG_BASE+325,
 "A plugin for the mime type %s\nwas not found.")

ResDef(XP_PLUGIN_CANT_LOAD_PLUGIN,               XP_MSG_BASE+326,
  "Could not load the plug-in '%s' for the MIME type '%s'.  \n\n\
  Make sure enough memory is available and that the plug-in is installed correctly.")


ResDef(XP_JAVA_NO_CLASSES,                      XP_MSG_BASE+327,
"Unable to start a java applet: Can't find the system classes in\n\
your CLASSPATH. Read the release notes and install them\n\
properly before restarting.\n")

ResDef(XP_JAVA_GENERAL_FAILURE,                 XP_MSG_BASE+328,
"Java virtual machine failure. Error %d.\n")

ResDef(XP_JAVA_STARTUP_FAILED,          XP_MSG_BASE+329,
"Java reported the following error on startup:\n\n%s\n")

ResDef(XP_JAVA_DEBUGGER_FAILED,         XP_MSG_BASE+330,
"Failed to start the Java debugger.\n")

ResDef(XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD,XP_MSG_BASE+331,
"The system has been locked to prevent access to restricted sites. Please enter \
your password if you will be changing these settings:")

ResDef(XP_ALERT_PROMPT_JAVA_CAPIBILITIES_PASSWORD_FAILED_ONCE,XP_MSG_BASE+332,
"The previously entered password was not correct. \
Please enter your password if you'll be changing these restrictions settings \
during this session:")

/*
 * UNUSED:
 *  XP_MSG_BASE + 333
 *  .
 *  .
 *  .
 *  XP_MSG_BASE + 334
 */

ResDef(XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_HOST,   XP_MSG_BASE +  335,
 "Connect: Please enter password for host...")

ResDef(XP_PROXY_REQUIRES_UNSUPPORTED_AUTH_SCHEME,   XP_MSG_BASE + 336,
 "Proxy is requiring an authentication scheme that is not supported." )


ResDef(XP_LOOPING_OLD_NONCES,   XP_MSG_BASE + 337,
  "Proxy nonces appear to expire immediately.\n\
This is either a problem in the proxy's authentication \n\
implementation, or you have mistyped your password.\n\
Do you want to re-enter your username and password?" )


ResDef(XP_UNIDENTIFIED_PROXY_SERVER,    XP_MSG_BASE + 338,
 "unidentified proxy server")

ResDef(XP_PROXY_AUTH_REQUIRED_FOR,  XP_MSG_BASE + 339,
 "Proxy authentication required for %.250s at %.250s:" )

ResDef(XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_PROXY,  XP_MSG_BASE + 340,
 "Connect: Please enter password for proxy...")

ResDef(XP_BAD_KEYWORD_IN_PROXY_AUTOCFG, XP_MSG_BASE + 341,
 "Bad keyword in proxy automatic configuration: %s." )

ResDef(XP_RETRY_AGAIN_PROXY,    XP_MSG_BASE + 342,
 "Try connecting to the proxy again?" )

ResDef(XP_RETRY_AGAIN_SOCKS,    XP_MSG_BASE + 343,
 "Try connecting to SOCKS again?" )

ResDef(XP_RETRY_AGAIN_PROXY_OR_SOCKS,   XP_MSG_BASE + 344,
 "Try connecting to proxy / SOCKS again?" )

ResDef(XP_PROXY_UNAVAILABLE_TRY_AGAIN,  XP_MSG_BASE + 345,
 "Proxy server is unavailable.\n\n\
Try connecting to proxy %s again?")

ResDef(XP_ALL_PROXIES_DOWN_TRY_AGAIN,   XP_MSG_BASE + 346,
 "All proxy servers are unavailable.\n\n\
Try connecting to %s again?" )

ResDef(XP_ALL_SOCKS_DOWN,   XP_MSG_BASE + 347,
 "SOCKS is unavailable.  Try connecting to SOCKS %s again?" )

ResDef(XP_ALL_DOWN_MIX, XP_MSG_BASE + 348,
 "SOCKS and proxies are unavailable.  Try\n\
connecting to %s again?" )

ResDef(XP_OVERRIDE_PROXY,   XP_MSG_BASE + 349,
 "All proxies are unavailable. Do you wish to temporarily\n\
override proxies by connecting directly until proxies\n\
are available again?" )

ResDef(XP_OVERRIDE_SOCKS,   XP_MSG_BASE + 350,
 "SOCKS is unavailable. Do you wish to temporarily\n\
override SOCKS by connecting directly until SOCKS\n\
are available again?" )

ResDef(XP_OVERRIDE_MIX, XP_MSG_BASE + 351,
 "Both proxies and SOCKS are unavailable. Do you wish to\n\
temporarily override them by connecting directly until they\n\
are available again?" )

ResDef(XP_STILL_OVERRIDE_PROXY, XP_MSG_BASE + 352,
 "All proxies are still down.\n\
Continue with direct connections?" )

ResDef(XP_STILL_OVERRIDE_SOCKS, XP_MSG_BASE + 353,
 "SOCKS is still down.\n\n\
Continue with direct connections?" )

ResDef(XP_STILL_OVERRIDE_MIX,   XP_MSG_BASE + 354,
 "SOCKS and proxies are still down.\n\n\
Continue with direct connections?" )

ResDef(XP_NO_CONFIG_RECIEVED,   XP_MSG_BASE + 355,
 "No proxy automatic configuration file was received.\n\n\
No proxies will be used." )

ResDef(XP_EMPTY_CONFIG_USE_PREV,    XP_MSG_BASE + 356,
 "The automatic configuration file is empty:\n\n    %s\n\n\
Use the configuration from the previous session instead?" )

ResDef(XP_BAD_CONFIG_USE_PREV,  XP_MSG_BASE + 357,
 "The automatic configuration file has errors:\n\n        %s\n\n\
Use the configuration from the previous session instead?" )

ResDef(XP_BAD_CONFIG_IGNORED,   XP_MSG_BASE + 358,
 "The proxy automatic configuration file has errors:\n\n        %s\n\n\
No proxies will be used." )

ResDef(XP_BAD_TYPE_USE_PREV,    XP_MSG_BASE + 359,
 "The automatic configuration file is not of the correct type:\n\n\
        %s\n\n\
Expected MIME type of application/x-javascript-config or application/x-ns-proxy-autoconfig.\n\n\
Use the configuration from the previous session instead?" )

ResDef(XP_CONF_LOAD_FAILED_IGNORED, XP_MSG_BASE + 360,
 "The proxy automatic configuration file could not be loaded.\n\n\
Check the proxy automatic configuration URL in preferences.\n\n\
No proxies will be used." )

ResDef(XP_CONF_LOAD_FAILED_USE_PREV,    XP_MSG_BASE + 361,
   "The automatic configuration file could not be loaded.\n\n\
Use the configuration from the previous session instead?" )

ResDef(XP_EVEN_SAVED_IS_BAD,    XP_MSG_BASE + 362,
 "The backup proxy automatic configuration file had errors.\n\n\
No proxies will be used." )

ResDef(XP_CONFIG_LOAD_ABORTED,  XP_MSG_BASE + 363,
 "Proxy automatic configuration load was cancelled.\n\n\
No proxies will be used." )

ResDef(XP_CONFIG_BLAST_WARNING, XP_MSG_BASE + 364,
 "Warning:\n\n\
Server sent an unrequested proxy automatic\n\
configuration file to "MOZ_NAME_PRODUCT":\n\n        %s\n\n\
Configuration file will be ignored." )

ResDef(XP_RECEIVING_PROXY_AUTOCFG,  XP_MSG_BASE + 365,
 "Receiving proxy auto-configuration file...")

ResDef(XP_CACHE_CLEANUP,    XP_MSG_BASE + 366,
 "Cache cleanup: removing %d files...")

ResDef(XP_DATABASE_CANT_BE_VALIDATED_MISSING_NAME_ENTRY,    XP_MSG_BASE + 367,
 "The database selected is valid, but cannot\n\
be validated as the correct database because\n\
it is missing a name entry.  Do you wish to\n\
use this database anyway?")

ResDef(XP_DB_SELECTED_DB_NAMED, XP_MSG_BASE + 368,
 "The database selected is named:\n\
%.900s\n\
The database requested was named:\n\
%.900s\n\
Do you wish to use this database anyway?" )

ResDef(XP_REQUEST_EXTERNAL_CACHE,   XP_MSG_BASE + 369,
 "The page currently loading has requested an external\n\
cache.  Using a read-only external cache can improve\n\
network file retrieval time.\n\
\n\
If you do not have the external cache requested,\n\
select \042Cancel\042 in the file selection box.")

ResDef(XP_BAD_TYPE_CONFIG_IGNORED,  XP_MSG_BASE + 370,
 "The proxy automatic configuration file is not of the correct type:\n\n\
        %s\n\n\
Expected the MIME type of application/x-ns-proxy-autoconfig.\n\n\
No proxies will be used." )


ResDef(XP_READING_SEGMENT,  XP_MSG_BASE + 371,
 "Reading segment...Done" )

ResDef(XP_TITLE_DIRECTORY_LISTING,  XP_MSG_BASE + 372,
 "<TITLE>Directory listing of %.1024s</TITLE>\n" )

ResDef(XP_H1_DIRECTORY_LISTING, XP_MSG_BASE + 373,
 "<H1>Directory listing of %.1024s</H1>\n<PRE>" )

ResDef( XP_UPTO_HIGHER_LEVEL_DIRECTORY, XP_MSG_BASE + 374,
 "\042>Up to higher level directory</A><BR>")

ResDef(XP_COULD_NOT_LOGIN_TO_FTP_SERVER ,   XP_MSG_BASE + 375,
 "Could not login to FTP server")

ResDef( XP_ERROR_COULD_NOT_MAKE_CONNECTION_NON_BLOCKING,    XP_MSG_BASE + 376,
 "Error: Could not make connection non-blocking.")

ResDef( XP_POSTING_FILE,    XP_MSG_BASE + 377,
 "Posting file %.256s..." )

ResDef(XP_TITLE_DIRECTORY_OF_ETC ,  XP_MSG_BASE + 378,
 "<TITLE>Directory of %.512s</TITLE>\n\
 <H2>Current directory is %.512s</H2>\n\
 <PRE>")

ResDef( XP_URLS_WAITING_FOR_AN_OPEN_SOCKET, XP_MSG_BASE + 379,
 "%d URL's waiting for an open socket (limit %d)\n" )

ResDef( XP_URLS_WAITING_FOR_FEWER_ACTIVE_URLS,  XP_MSG_BASE + 380,
"%d URL's waiting for fewer active URL's\n" )

ResDef( XP_CONNECTIONS_OPEN,    XP_MSG_BASE + 381,
 "%d Connections Open\n" )

ResDef( XP_ACTIVE_URLS, XP_MSG_BASE + 382,
 "%d Active URL's\n" )

ResDef( XP_USING_PREVIOUSLY_CACHED_COPY_INSTEAD,    XP_MSG_BASE + 383,
"\n\nUsing previously cached copy instead")

ResDef( XP_SERVER_RETURNED_NO_DATA, XP_MSG_BASE + 384,
"Server returned no data")

ResDef( XP_HR_TRANSFER_INTERRUPTED, XP_MSG_BASE + 385,
">\n<HR><H3>Transfer interrupted!</H3>\n")

ResDef( XP_TRANSFER_INTERRUPTED,    XP_MSG_BASE + 386,
"\n\nTransfer interrupted!\n")

ResDef( XP_MAIL_READING_FOLDER, XP_MSG_BASE + 387,
"Mail: Reading folder %s..." )

ResDef( XP_MAIL_READING_MESSAGE,    XP_MSG_BASE + 388,
"Mail: Reading message...")

ResDef( XP_MAIL_EMPTYING_TRASH, XP_MSG_BASE + 389,
"Mail: Emptying trash...")

ResDef( XP_COMPRESSING_FOLDER,  XP_MSG_BASE + 390,
"Mail: Compressing folder %s..." )

ResDef( XP_MAIL_DELIVERING_QUEUED_MESSAGES, XP_MSG_BASE + 391,
"Mail: Delivering queued messages...")

ResDef( XP_READING_MESSAGE_DONE,    XP_MSG_BASE + 392,
"Mail: Reading message...Done")

ResDef( XP_MAIL_READING_FOLDER_DONE,    XP_MSG_BASE + 393,
"Mail: Reading folder...Done")

ResDef( XP_MAIL_EMPTYING_TRASH_DONE,    XP_MSG_BASE + 394,
"Mail: Emptying trash...Done")

ResDef( XP_MAIL_COMPRESSING_FOLDER_DONE,    XP_MSG_BASE + 395,
"Mail: Compressing folder...Done")

ResDef( XP_MAIL_DELIVERING_QUEUED_MESSAGES_DONE,    XP_MSG_BASE + 396,
"Mail: Delivering queued messages...Done")

/* XP_UNKNOWN_ERROR unused  XP_MSG_BASE + 397, */

ResDef( XP_CONNECT_NEWS_HOST_CONTACTED_WAITING_FOR_REPLY, XP_MSG_BASE + 398,
 "Connect: News server contacted. Waiting for reply...")

ResDef( XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS,  XP_MSG_BASE + 399,
 "Please enter a password for news server access")

ResDef( XP_MESSAGE_SENT_WAITING_NEWS_REPLY, XP_MSG_BASE + 400,
"Message sent; waiting for reply...")

ResDef( XP_NO_ANSWER,   XP_MSG_BASE + 401,
 "No Answer")

ResDef( XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC,   XP_MSG_BASE + 402,
 "The POP3 server (%s) does not\n\
 support UIDL, which "MOZ_NAME_PRODUCT" Mail needs to implement\n\
 the ``Leave on Server'' and ``Maximum Message Size''\n\
 options.\n\n\
 To download your mail, turn off these options in the\n\
 Mail Server panel of Preferences.")

ResDef( XP_RECEIVING_MESSAGE_OF,    XP_MSG_BASE + 403,
"Receiving: message %lu of %lu" )

ResDef( XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND,  XP_MSG_BASE + 404,
 "The POP3 mail server (%s) does not\n\
support the TOP command.\n\n\
Without server support for this, we cannot implement\n\
the ``Maximum Message Size'' preference.  This option\n\
has been disabled, and messages will be downloaded\n\
regardless of their size.")

#ifdef XP_WIN
ResDef( XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC,   XP_MSG_BASE + 405,
"The mail server responded: \n\
%s\n\
Please enter a new password.")
#else
ResDef( XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC,   XP_MSG_BASE + 405,
 "Could not log in to the mail server.\n\
The server responded:\n\n\
  %s\n\n\
Please enter a new password for user %.100s@%.100s:")
#endif

ResDef( XP_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION, XP_MSG_BASE + 406,
"Connect: Host contacted, sending login information...")

ResDef(MK_MSG_ASSEMBLING_MSG,       XP_MSG_BASE + 407,
 "Assembling message...")

ResDef(MK_MSG_ASSEMB_DONE_MSG,      XP_MSG_BASE + 408,
"Assembling message...Done")

ResDef(MK_MSG_LOAD_ATTACHMNT,       XP_MSG_BASE + 409,
 "Loading attachment...")

ResDef(MK_MSG_LOAD_ATTACHMNTS,      XP_MSG_BASE + 410,
 "Loading attachments...")

ResDef(MK_MSG_DELIVERING_MAIL,      XP_MSG_BASE + 411,
 "Delivering mail...")

ResDef(MK_MSG_DELIV_MAIL,           XP_MSG_BASE + 412,
 "Delivering mail...")

ResDef(MK_MSG_DELIV_MAIL_DONE,      XP_MSG_BASE + 413,
 "Delivering mail...Done")

ResDef(MK_MSG_DELIV_NEWS,           XP_MSG_BASE + 414,
 "Delivering news...")

ResDef(MK_MSG_DELIV_NEWS_DONE,      XP_MSG_BASE + 415,
 "Delivering news...Done")

ResDef(MK_MSG_QUEUEING,             XP_MSG_BASE + 416,
 "Queuing for later delivery...")

ResDef(MK_MSG_WRITING_TO_FCC,       XP_MSG_BASE + 417,
 "Writing to Sent Mail file...")

ResDef(MK_MSG_QUEUED,               XP_MSG_BASE + 418,
 "Queued for later delivery.")

ResDef(MK_MSG_MSG_COMPOSITION,      XP_MSG_BASE + 419,
 "Message Composition")

/* MK_MSG_UNKNOWN_ERROR unused      XP_MSG_BASE + 420, */

ResDef(MK_MSG_LOADING_MESSAGES,     XP_MSG_BASE + 421,
 "Loading summary file...")

ResDef(MK_MSG_LOADED_MESSAGES,          XP_MSG_BASE + 422,
 "loaded %ld of %ld messages")

ResDef(MK_MSG_OPEN_FOLDER,          XP_MSG_BASE + 423,
 "Add Folder")

ResDef(MK_MSG_OPEN_FOLDER2,         XP_MSG_BASE + 424,
 "Add Folder...")

ResDef(MK_MSG_ENTER_FOLDERNAME,     XP_MSG_BASE + 425,
 "Enter the name for your new folder.")

ResDef(MK_MSG_SAVE_MESSAGE_AS,      XP_MSG_BASE + 426,
 "Save Message As")

ResDef(MK_MSG_SAVE_MESSAGES_AS,     XP_MSG_BASE + 427,
 "Save Messages As")

ResDef(MK_MSG_GET_NEW_MAIL,         XP_MSG_BASE + 428,
 "Get New Mail")

ResDef(MK_MSG_DELIV_NEW_MSGS,       XP_MSG_BASE + 429,
 "Send Unsent Messages")

ResDef(MK_MSG_NEW_FOLDER,           XP_MSG_BASE + 430,
 "New Folder...")

ResDef(MK_MSG_COMPRESS_FOLDER,      XP_MSG_BASE + 431,
 "Compress This Folder")

ResDef(MK_MSG_COMPRESS_ALL_FOLDER,  XP_MSG_BASE + 432,
 "Compress All Folders")

ResDef(MK_MSG_OPEN_NEWS_HOST_ETC,       XP_MSG_BASE + 433,
 "Open News Server...")

ResDef(MK_MSG_EMPTY_TRASH_FOLDER,   XP_MSG_BASE + 434,
 "Empty Trash Folder")

ResDef(MK_MSG_PRINT,                XP_MSG_BASE + 435,
 "Print...")

ResDef(MK_MSG_UNDO,                 XP_MSG_BASE + 436,
 "Undo")

ResDef(MK_MSG_REDO,                 XP_MSG_BASE + 437,
 "Redo")

ResDef(MK_MSG_DELETE_SEL_MSGS,      XP_MSG_BASE + 438,
 "Delete Selected Messages")

ResDef(MK_MSG_DELETE_MESSAGE,       XP_MSG_BASE + 439,
 "Delete Message")

ResDef(MK_MSG_DELETE_FOLDER,        XP_MSG_BASE + 440,
 "Delete Folder")

ResDef(MK_MSG_CANCEL_MESSAGE,       XP_MSG_BASE + 441,
 "Cancel Message")

ResDef(MK_MSG_RMV_NEWS_HOST,        XP_MSG_BASE + 442,
 "Remove News Server")

ResDef(MK_MSG_SUBSCRIBE,            XP_MSG_BASE + 443,
 "Subscribe")

ResDef(MK_MSG_UNSUBSCRIBE,          XP_MSG_BASE + 444,
 "Unsubscribe")

ResDef(MK_MSG_SELECT_THREAD,        XP_MSG_BASE + 445,
 "Select Thread")

ResDef(MK_MSG_SELECT_FLAGGED_MSG,   XP_MSG_BASE + 446,
 "Select Flagged Messages")

ResDef(MK_MSG_SELECT_ALL,           XP_MSG_BASE + 447,
 "Select All Messages")

ResDef(MK_MSG_DESELECT_ALL_MSG,     XP_MSG_BASE + 448,
 "Deselect All Messages")

ResDef(MK_MSG_FLAG_MESSAGE,         XP_MSG_BASE + 449,
 "Flag Message")

ResDef(MK_MSG_UNFLAG_MESSAGE,       XP_MSG_BASE + 450,
 "Unflag Message")

ResDef(MK_MSG_AGAIN,                XP_MSG_BASE + 451,
 "Again")

ResDef(MK_MSG_THREAD_MESSAGES,      XP_MSG_BASE + 452,
 "Thread Messages")

ResDef(MK_MSG_BY_DATE,              XP_MSG_BASE + 453,
 "By Date")

ResDef(MK_MSG_BY_SENDER,            XP_MSG_BASE + 454,
 "By Sender")

ResDef(MK_MSG_BY_SUBJECT,           XP_MSG_BASE + 455,
 "By Subject")

ResDef(MK_MSG_BY_MESSAGE_NB,        XP_MSG_BASE + 456,
 "By Message Number")

ResDef(MK_MSG_UNSCRAMBLE,           XP_MSG_BASE + 457,
 "Unscramble (Rot13)")

ResDef(MK_MSG_ADD_FROM_NEW_MSG,     XP_MSG_BASE + 458,
 "Add from Newest Messages")

ResDef(MK_MSG_ADD_FROM_OLD_MSG,     XP_MSG_BASE + 459,
 "Add from Oldest Messages")

ResDef(MK_MSG_GET_MORE_MSGS,        XP_MSG_BASE + 460,
 "Get More Messages")

ResDef(MK_MSG_GET_ALL_MSGS,         XP_MSG_BASE + 461,
 "Get All Messages")

ResDef(MK_MSG_ADDRESS_BOOK,         XP_MSG_BASE + 462,
 "Address Book")

ResDef(MK_MSG_VIEW_ADDR_BK_ENTRY,   XP_MSG_BASE + 463,
 "View Address Book Entry")

ResDef(MK_MSG_ADD_TO_ADDR_BOOK,     XP_MSG_BASE + 464,
 "Add to Address Book")

ResDef(MK_MSG_NEW_NEWS_MESSAGE,     XP_MSG_BASE + 465,
 "New Newsgroup Message")

ResDef(MK_MSG_POST_REPLY,           XP_MSG_BASE + 466,
 "to Newsgroup")

ResDef(MK_MSG_POST_MAIL_REPLY,      XP_MSG_BASE + 467,
 "to Sender and Newsgroup")

ResDef(MK_MSG_NEW_MAIL_MESSAGE,     XP_MSG_BASE + 468,
 "New Message")

ResDef(MK_MSG_REPLY,                XP_MSG_BASE + 469,
 "Reply")

ResDef(MK_MSG_REPLY_TO_ALL,         XP_MSG_BASE + 470,
 "to Sender and All Recipients")

ResDef(MK_MSG_FWD_SEL_MESSAGES,     XP_MSG_BASE + 471,
 "Forward Selected Messages")

ResDef(MK_MSG_FORWARD,              XP_MSG_BASE + 472,
 "Forward As Attachment")

ResDef(MK_MSG_MARK_SEL_AS_READ,     XP_MSG_BASE + 473,
 "Mark Selected as Read")

ResDef(MK_MSG_MARK_AS_READ,         XP_MSG_BASE + 474,
 "Mark as Read")

ResDef(MK_MSG_MARK_SEL_AS_UNREAD,   XP_MSG_BASE + 475,
 "Mark Selected as Unread")

ResDef(MK_MSG_MARK_AS_UNREAD,       XP_MSG_BASE + 476,
 "Mark as Unread")

ResDef(MK_MSG_UNFLAG_ALL_MSGS,      XP_MSG_BASE + 477,
 "Unflag All Messages")

ResDef(MK_MSG_COPY_SEL_MSGS,        XP_MSG_BASE + 478,
 "Copy Selected Messages")

ResDef(MK_MSG_COPY_ONE,             XP_MSG_BASE + 479,
 "Copy")

ResDef(MK_MSG_MOVE_SEL_MSG,         XP_MSG_BASE + 480,
 "Move Selected Messages")

ResDef(MK_MSG_MOVE_ONE,             XP_MSG_BASE + 481,
 "Move")

ResDef(MK_MSG_SAVE_SEL_MSGS,        XP_MSG_BASE + 482,
 "Save Selected Messages As...")

ResDef(MK_MSG_SAVE_AS,              XP_MSG_BASE + 483,
 "Save As...")

ResDef(MK_MSG_MOVE_SEL_MSG_TO,      XP_MSG_BASE + 484,
 "Move Selected Messages To...")

ResDef(MK_MSG_MOVE_MSG_TO,          XP_MSG_BASE + 485,
 "Move This Message To...")

ResDef(MK_MSG_FIRST_MSG,            XP_MSG_BASE + 486,
 "First Message")

ResDef(MK_MSG_NEXT_MSG,             XP_MSG_BASE + 487,
 "Next Message")

ResDef(MK_MSG_PREV_MSG,             XP_MSG_BASE + 488,
 "Previous Message")

ResDef(MK_MSG_LAST_MSG,             XP_MSG_BASE + 489,
 "Last Message")

ResDef(MK_MSG_FIRST_UNREAD,         XP_MSG_BASE + 490,
 "First Unread")

ResDef(MK_MSG_NEXT_UNREAD,          XP_MSG_BASE + 491,
 "Next Unread")

ResDef(MK_MSG_PREV_UNREAD,          XP_MSG_BASE + 492,
 "Previous Unread Message")

ResDef(MK_MSG_LAST_UNREAD,          XP_MSG_BASE + 493,
 "Last Unread")

ResDef(MK_MSG_FIRST_FLAGGED,        XP_MSG_BASE + 494,
 "First Flagged")

ResDef(MK_MSG_NEXT_FLAGGED,         XP_MSG_BASE + 495,
 "Next Flagged")

ResDef(MK_MSG_PREV_FLAGGED,         XP_MSG_BASE + 496,
 "Previous Flagged")

ResDef(MK_MSG_LAST_FLAGGED,         XP_MSG_BASE + 497,
 "Last Flagged")

ResDef(MK_MSG_BACKTRACK,            XP_MSG_BASE + 498,
 "Go back to the last message")

ResDef(MK_MSG_GO_FORWARD,           XP_MSG_BASE + 499,
 "Go Forward from the last message")

ResDef(MK_MSG_MARK_THREAD_READ,     XP_MSG_BASE + 500,
 "Mark Thread Read")

ResDef(MK_MSG_MARK_ALL_READ,    XP_MSG_BASE + 501,
 "Mark All Read")

ResDef(MK_MSG_MARK_SEL_READ,        XP_MSG_BASE + 502,
 "Mark Selected Threads Read")

ResDef(MK_MSG_SHOW_ALL_MSGS,        XP_MSG_BASE + 505,
 "Show All Messages")

ResDef(MK_MSG_SHOW_UNREAD_ONLY,     XP_MSG_BASE + 506,
 "Show Only Unread Messages")

/* 
   MK_MSG_SHOW_MICRO_HEADERS and MK_MSG_SHOW_SOME_HEADERS are way at the end.
 */

ResDef(MK_MSG_SHOW_ALL_HEADERS,     XP_MSG_BASE + 507,
 "All")

ResDef(MK_MSG_QUOTE_MESSAGE,        XP_MSG_BASE + 508,
 "Include Original Text")

ResDef(MK_MSG_FROM,                 XP_MSG_BASE + 509,
 "From")

ResDef(MK_MSG_REPLY_TO,             XP_MSG_BASE + 510,
 "Reply To")

ResDef(MK_MSG_MAIL_TO,              XP_MSG_BASE + 511,
 "Mail To")

ResDef(MK_MSG_MAIL_CC,              XP_MSG_BASE + 512,
 "Mail CC")

ResDef(MK_MSG_MAIL_BCC,             XP_MSG_BASE + 513,
 "Mail BCC")

ResDef(MK_MSG_FILE_CC,              XP_MSG_BASE + 514,
 "File CC")

ResDef(MK_MSG_POST_TO,              XP_MSG_BASE + 515,
 "Newsgroups")

ResDef(MK_MSG_FOLLOWUPS_TO,         XP_MSG_BASE + 516,
 "Followups To")

ResDef(MK_MSG_SUBJECT,              XP_MSG_BASE + 517,
 "Subject")

ResDef(MK_MSG_ATTACHMENT,           XP_MSG_BASE + 518,
 "Attachment")

ResDef(MK_MSG_SEND_FORMATTED_TEXT,  XP_MSG_BASE + 519,
 "Send Formatted Text")

ResDef(MK_MSG_Q4_LATER_DELIVERY,    XP_MSG_BASE + 520,
 "Queue For Later Delivery")

ResDef(MK_MSG_ATTACH_AS_TEXT,       XP_MSG_BASE + 521,
 "Attach As Text")

ResDef(MK_MSG_FLAG_MESSAGES,        XP_MSG_BASE + 522,
 "Flag Messages")

ResDef(MK_MSG_UNFLAG_MESSAGES,      XP_MSG_BASE + 523,
 "Unflag Messages")

ResDef(MK_MSG_SORT_BACKWARD,        XP_MSG_BASE + 524,
 "Ascending")

ResDef(MK_MSG_PARTIAL_MSG_FMT_1,    XP_MSG_BASE + 525,
 "<P><CENTER>\n<TABLE BORDER CELLSPACING=5 CELLPADDING=10 WIDTH=\04280%%\042>\n\
<TR><TD ALIGN=CENTER><FONT SIZE=\042+1\042>Truncated!</FONT><HR>\n")

ResDef(MK_MSG_PARTIAL_MSG_FMT_2,    XP_MSG_BASE + 526,
 "<B>This message exceeded the Maximum Message Size set in Preferences,\n\
so we have only downloaded the first few lines from the mail server.<P>Click <A HREF=\042")

ResDef(MK_MSG_PARTIAL_MSG_FMT_3,    XP_MSG_BASE + 527,
 "\042>here</A> to download the rest of the message.</B></TD></TR></TABLE></CENTER>\n")

ResDef(MK_MSG_NO_HEADERS,           XP_MSG_BASE + 528,
 "(no headers)")

ResDef(MK_MSG_UNSPECIFIED,          XP_MSG_BASE + 529,
 "(unspecified)")

ResDef(MK_MSG_MIME_MAC_FILE,        XP_MSG_BASE + 530,
 "Macintosh File")

ResDef(MK_MSG_DIR_DOESNT_EXIST,     XP_MSG_BASE + 531,
 "The directory %s does not exist.  Mail will not\nwork without it.\n\nCreate it now?")

ResDef(MK_MSG_SAVE_DECODED_AS,      XP_MSG_BASE + 532,
 "Save decoded file as:")

ResDef(MK_MSG_FILE_HAS_CHANGED,     XP_MSG_BASE + 533,
 "The file %s has been changed by some other program!\nOverwrite it?")

ResDef(MK_MSG_OPEN_NEWS_HOST,       XP_MSG_BASE + 534,
 "Open News Server")

ResDef(MK_MSG_ANNOUNCE_NEWSGRP,     XP_MSG_BASE + 535,
 "news.announce.newusers")

ResDef(MK_MSG_QUESTIONS_NEWSGRP,    XP_MSG_BASE + 536,
 "news.newusers.questions")

ResDef(MK_MSG_ANSWERS_NEWSGRP,      XP_MSG_BASE + 537,
 "news.answers")

ResDef(MK_MSG_COMPRESS_FOLDER_ETC,      XP_MSG_BASE + 538,
 "Mail: Compressing folder %s...")

ResDef(MK_MSG_COMPRESS_FOLDER_DONE, XP_MSG_BASE + 539,
 "Mail: Compressing folder %s...Done")

ResDef(MK_MSG_CANT_OPEN,            XP_MSG_BASE + 540,
 "Can't open %s. You may not have permission to write to this file.\nCheck the permissions and try again.")

ResDef(MK_MSG_SAVE_ATTACH_AS,       XP_MSG_BASE + 541,
 "Save attachment as:")


ResDef(XP_THERMO_UH,             XP_MSG_BASE + 542,
 "%lu byte%s")

ResDef(XP_THERMO_SINGULAR_FORMAT,  XP_MSG_BASE + 543,
 "")

ResDef(XP_THERMO_PLURAL_FORMAT,  XP_MSG_BASE + 544,
 "s")

ResDef(XP_THERMO_RATE_REMAINING_FORM,  XP_MSG_BASE + 545,
 "%s of %s (%s, %s)")

ResDef(XP_THERMO_RATE_FORM, XP_MSG_BASE + 546,
 "%s of %s (%s)")

ResDef(XP_THERMO_PERCENT_FORM,  XP_MSG_BASE + 547,
 "%s of %s")

ResDef(XP_THERMO_PERCENT_RATE_FORM,  XP_MSG_BASE + 548,
 "%s read (%s)")

ResDef(XP_THERMO_RAW_COUNT_FORM,  XP_MSG_BASE + 549,
 "%s read")

ResDef( XP_MESSAGE_SENT_WAITING_MAIL_REPLY, XP_MSG_BASE + 550,
"Mail: Message sent; waiting for reply...")

ResDef(XP_GLHIST_INFO_HTML,                     XP_MSG_BASE + 551,
 "<TITLE>Information about the "MOZ_NAME_PRODUCT" global history</TITLE>\n\
<h2>Global history entries</h2>\n\
<HR>")

ResDef(XP_THERMO_PERCENTAGE_FORMAT,   XP_MSG_BASE + 552,
 "%d%%")

ResDef(XP_MSG_IMAGE_PIXELS,         XP_MSG_BASE + 553,  "%s image %dx%d pixels")
ResDef(XP_MSG_IMAGE_NOT_FOUND,      XP_MSG_BASE + 554,  "Couldn't find image of correct URL, size, background, etc.\nin cache:\n%s\n")
ResDef(XP_MSG_XBIT_COLOR,           XP_MSG_BASE + 555,  "%d-bit pseudocolor")
ResDef(XP_MSG_1BIT_MONO,            XP_MSG_BASE + 556,  "1-bit monochrome")
ResDef(XP_MSG_XBIT_GREYSCALE,       XP_MSG_BASE + 557,  "%d-bit greyscale")
ResDef(XP_MSG_XBIT_RGB,             XP_MSG_BASE + 558,  "%d-bit RGB true color.")
ResDef(XP_MSG_DECODED_SIZE,         XP_MSG_BASE + 559,  "Decoded&nbsp;size&nbsp;(bytes):")
ResDef(XP_MSG_WIDTH_HEIGHT,         XP_MSG_BASE + 560,  "%u&nbsp;x&nbsp;%u")
ResDef(XP_MSG_SCALED_FROM,          XP_MSG_BASE + 561,  " (scaled from %u&nbsp;x&nbsp;%u)")
ResDef(XP_MSG_IMAGE_DIM,            XP_MSG_BASE + 562,  "Image&nbsp;dimensions:")
ResDef(XP_MSG_COLOR,                XP_MSG_BASE + 563,  "Color:")
ResDef(XP_MSG_NB_COLORS,            XP_MSG_BASE + 564,  "%d colors")
ResDef(XP_MSG_NONE,                 XP_MSG_BASE + 565,  "(none)")
ResDef(XP_MSG_COLORMAP,             XP_MSG_BASE + 566,  "Colormap:")
ResDef(XP_MSG_BCKDRP_VISIBLE,       XP_MSG_BASE + 567,  "yes, backdrop visible through transparency")
ResDef(XP_MSG_SOLID_BKGND,          XP_MSG_BASE + 568,  "yes, solid color background <tt>#%02x%02x%02x</tt>")
ResDef(XP_MSG_JUST_NO,              XP_MSG_BASE + 569,  "no")
ResDef(XP_MSG_TRANSPARENCY,         XP_MSG_BASE + 570,  "Transparency:")
ResDef(XP_MSG_COMMENT,              XP_MSG_BASE + 571,  "Comment:")
ResDef(XP_MSG_UNKNOWN,              XP_MSG_BASE + 572,  "Unknown")
ResDef(XP_MSG_COMPRESS_REMOVE,      XP_MSG_BASE + 573,  "Compressing image cache:\nremoving %s\n")
ResDef(MK_MSG_ADD_NEWS_GROUP,       XP_MSG_BASE + 574,  "Add Newsgroup...")
ResDef(MK_MSG_FIND_AGAIN,           XP_MSG_BASE + 575,  "Find Again")
ResDef(MK_MSG_SEND,                 XP_MSG_BASE + 576,  "Send")
ResDef(MK_MSG_SEND_LATER,           XP_MSG_BASE + 577,  "Send Later")
ResDef(MK_MSG_ATTACH_ETC,           XP_MSG_BASE + 578,  "Attach...")
ResDef(MK_MSG_ATTACHMENTSINLINE,    XP_MSG_BASE + 579,  "Attachments Inline")
ResDef(MK_MSG_ATTACHMENTSASLINKS,   XP_MSG_BASE + 580,  "Attachments as Links")
ResDef(MK_MSG_FORWARD_QUOTED,       XP_MSG_BASE + 581,  "Forward Quoted")
ResDef(MK_MSG_REMOVE_HOST_CONFIRM,      XP_MSG_BASE + 582,
       "Are you sure you want to remove the news server %s\n\
and all of the newsgroups in it?")
ResDef(MK_MSG_ALL_FIELDS,       XP_MSG_BASE + 583, "All Fields")

#ifdef XP_WIN16
ResDef(MK_MSG_BOGUS_QUEUE_MSG_1,    XP_MSG_BASE + 584,
       "The `Unsent' folder contains a message which is not\n\
scheduled for delivery!")

ResDef(MK_MSG_BOGUS_QUEUE_MSG_N,    XP_MSG_BASE + 585,
       "The `Unsent' folder contains %d messages which are not\n\
scheduled for delivery!")
#else
ResDef(MK_MSG_BOGUS_QUEUE_MSG_1,    XP_MSG_BASE + 584,
       "The `Unsent Messages' folder contains a message which is not\n\
scheduled for delivery!")

ResDef(MK_MSG_BOGUS_QUEUE_MSG_N,    XP_MSG_BASE + 585,
       "The `Unsent Messages' folder contains %d messages which are not\n\
scheduled for delivery!")
#endif

ResDef(MK_MSG_BOGUS_QUEUE_REASON,   XP_MSG_BASE + 586,
       "\n\nThis probably means that some program other than\n\
"MOZ_NAME_PRODUCT" has added messages to this folder.\n")

#ifdef XP_WIN16
ResDef(MK_MSG_WHY_QUEUE_SPECIAL,    XP_MSG_BASE + 587,
       "The `Unsent' folder is special; it is only for storing\n\
messages to be sent later.")
#else
ResDef(MK_MSG_WHY_QUEUE_SPECIAL,    XP_MSG_BASE + 587,
       "The `Unsent Messages' folder is special; it is only for storing\n\
messages to be sent later.")
#endif

ResDef(MK_MSG_NOT_AS_SENT_FOLDER,   XP_MSG_BASE + 588,
       "\nTherefore, you can't use it as your `Sent' folder.\n\
\n\
Please verify that your outgoing messages destination is correct\n\
in your Mail and News preferences.")

ResDef(MK_MSG_QUEUED_DELIVERY_FAILED,   XP_MSG_BASE + 589,
       "An error occurred delivering unsent messages.\n\n\
%s\n\
Continue delivery of any remaining unsent messages?")

ResDef(XP_PASSWORD_FOR_POP3_USER,   XP_MSG_BASE + 590,
       "Password for mail user %.100s@%.100s:")

ResDef(XP_BKMKS_SOMEONE_S_BOOKMARKS,    XP_MSG_BASE + 591,
       "%sBookmarks for %s%s")

ResDef(XP_BKMKS_PERSONAL_BOOKMARKS, XP_MSG_BASE + 592,
       "%sPersonal Bookmarks%s")

ResDef(XP_BKMKS_SOMEONE_S_ADDRESSBOOK,  XP_MSG_BASE + 593,
       "%sAddress book for %s%s")

ResDef(XP_BKMKS_PERSONAL_ADDRESSBOOK,   XP_MSG_BASE + 594,
       "%sPersonal Address book%s")

ResDef(XP_SOCK_CON_SOCK_PROTOCOL,   XP_MSG_BASE + 595,
       "sock: %d   con_sock: %d   protocol: %d\n")

ResDef(XP_URL_NOT_FOUND_IN_CACHE,   XP_MSG_BASE + 596,
       "URL not found in cache: ")

ResDef(XP_PARTIAL_CACHE_ENTRY,  XP_MSG_BASE + 597,
       "Partial cache entry, getting rest from server:\n")

ResDef(XP_CHECKING_SERVER__FORCE_RELOAD,    XP_MSG_BASE + 598,
       "Checking server to verify cache entry\nbecause force_reload is set:\n")

ResDef(XP_OBJECT_HAS_EXPIRED,   XP_MSG_BASE + 599,
       "Object has expired, reloading:\n")

ResDef(XP_CHECKING_SERVER_CACHE_ENTRY,  XP_MSG_BASE + 600,
       "Checking server to verify cache entry:\n")

ResDef(XP_CHECKING_SERVER__LASTMOD_MISS,    XP_MSG_BASE + 601,
       "Checking server to verify cache entry\nbecause last_modified missing:\n")

ResDef(XP_NETSITE_, XP_MSG_BASE + 602,
       "Netsite:")

ResDef(XP_LOCATION_,    XP_MSG_BASE + 603,
       "Location:")

ResDef(XP_FILE_MIME_TYPE_,  XP_MSG_BASE + 604,
       "File&nbsp;MIME&nbsp;Type:")

ResDef(XP_CURRENTLY_UNKNOWN,    XP_MSG_BASE + 605,
       "Currently Unknown")

ResDef(XP_SOURCE_,  XP_MSG_BASE + 606,
       "Source:")

ResDef(XP_CURRENTLY_IN_DISK_CACHE,  XP_MSG_BASE + 607,
       "Currently in disk cache")

ResDef(XP_CURRENTLY_IN_MEM_CACHE,   XP_MSG_BASE + 608,
       "Currently in memory cache" )

ResDef(XP_CURRENTLY_NO_CACHE,   XP_MSG_BASE + 609,
       "Not cached")

ResDef(XP_THE_WINDOW_IS_NOW_INACTIVE,   XP_MSG_BASE + 610,
       "<H1>The window is now inactive</H1>")

ResDef(XP_LOCAL_CACHE_FILE_,    XP_MSG_BASE + 611,
       "Local cache file:")

ResDef(XP_NONE, XP_MSG_BASE + 612,
       "none")

#ifdef XP_UNIX
ResDef(XP_LOCAL_TIME_FMT,   XP_MSG_BASE + 613,
       "%A, %d-%b-%y %H:%M:%S Local time")
#else
ResDef(XP_LOCAL_TIME_FMT,   XP_MSG_BASE + 613,
       "%s Local time")
#endif

ResDef(XP_LAST_MODIFIED,    XP_MSG_BASE + 614,
       "Last Modified:")

#ifdef XP_UNIX
ResDef(XP_GMT_TIME_FMT, XP_MSG_BASE + 615,
       "%A, %d-%b-%y %H:%M:%S GMT")
#else
ResDef(XP_GMT_TIME_FMT, XP_MSG_BASE + 615,
       "%s GMT")
#endif

ResDef(XP_CONTENT_LENGTH_,  XP_MSG_BASE + 616,
       "Content Length:")

ResDef(XP_NO_DATE_GIVEN,    XP_MSG_BASE + 617,
       "No date given")

ResDef(XP_EXPIRES_, XP_MSG_BASE + 618,
       "Expires:")

ResDef(XP_MAC_TYPE_,    XP_MSG_BASE + 619,
       "Mac Type:")

ResDef(XP_MAC_CREATOR_, XP_MSG_BASE + 620,
       "Mac Creator:")

ResDef(XP_CHARSET_, XP_MSG_BASE + 621,
       "Charset:")

ResDef(XP_STATUS_UNKNOWN,   XP_MSG_BASE + 622,
       "Status unknown")

ResDef(XP_SECURITY_,    XP_MSG_BASE + 623,
       "Security:")

ResDef(XP_CERTIFICATE_, XP_MSG_BASE + 624,
      "Certificate:")

ResDef(XP_UNTITLED_DOCUMENT,    XP_MSG_BASE + 625,
       "Untitled document")

ResDef(XP_HAS_THE_FOLLOWING_STRUCT, XP_MSG_BASE + 626,
       "</b></FONT> has the following structure:<p><ul><li>")

ResDef(XP_DOCUMENT_INFO,    XP_MSG_BASE + 627,
       "Page Info")

ResDef(XP_EDIT_NEW_DOC_URL,         XP_MSG_BASE + 628, "about:editfilenew")
ResDef(XP_EDIT_NEW_DOC_NAME,        XP_MSG_BASE + 629, "file:///Untitled")

/* Whoever decided all these numbers should be consecutive should be ashamed. */
ResDef(MK_MSG_SHOW_MICRO_HEADERS,       XP_MSG_BASE + 630,
 "Brief")
ResDef(MK_MSG_SHOW_SOME_HEADERS,        XP_MSG_BASE + 631,
 "Normal")

ResDef(MK_MSG_DELETE_FOLDER_MESSAGES,   XP_MSG_BASE + 632,
 "Deleting '%s' will delete its subfolders and messages.\n\
Are you sure you still want to delete this folder?")

/*
 * Messages to indicate that failover will not be done since
 * the proxies are locked.
 */

ResDef(XP_CONF_LOAD_FAILED_NO_FAILOVER, XP_MSG_BASE + 633,
"The proxy automatic configuration file could not be loaded.\n\n\
Cannot fail over to using no proxies since your autoconfig url\n\
is locked.\n\
See your local system administrator for help." )

ResDef(XP_NO_CONFIG_RECEIVED_NO_FAILOVER, XP_MSG_BASE + 634,
"No proxy automatic configuration file was received.\n\n\
Cannot fail over to using no proxies since your autoconfig url\n\
is locked.\n\
See your local system administrator for help." )

ResDef(XP_BAD_AUTOCONFIG_NO_FAILOVER,   XP_MSG_BASE + 635,
"There was a problem receiving your proxy autoconfig\n\
information.  Since your autoconfig URL has been locally\n\
locked, we cannot failover to allowing no proxies.\n\n\
See your local system administrator for help." )

ResDef(XP_BKMKS_IMPORT_ADDRBOOK, XP_MSG_BASE + 636,
"Import address book file")

ResDef(XP_BKMKS_SAVE_ADDRBOOK, XP_MSG_BASE + 637,
"Export address book file")

ResDef(XP_BKMKS_BOOKMARK,   XP_MSG_BASE + 638,
"this bookmark" )

ResDef(XP_BKMKS_ENTRY,   XP_MSG_BASE + 639,
"this entry" )

ResDef(XP_BKMKS_SECONDS,   XP_MSG_BASE + 640,
"%ld seconds" )

ResDef(XP_BKMKS_MINUTES,   XP_MSG_BASE + 641,
"%ld minutes" )

ResDef(XP_BKMKS_HOURS_MINUTES,   XP_MSG_BASE + 642,
"%ld hours %ld minutes")

ResDef(XP_BKMKS_HEADER,   XP_MSG_BASE + 643,
"Main Bookmarks" )

ResDef(XP_ADDRBOOK_HEADER,   XP_MSG_BASE + 644,
"Address Book" )

ResDef(MK_MSG_WRAP_LONG_LINES, XP_MSG_BASE + 645,
"Wrap long lines" )

ResDef(XP_EDITOR_AUTO_SAVE,     XP_MSG_BASE + 646,
 "Auto Saving %s")

ResDef(XP_EDITOR_NON_HTML,      XP_MSG_BASE + 647,
 "Cannot edit non-HTML documents!")

/* Search attribute names */
/* WARNING -- order must match MSG_SearchAttribute enum */
ResDef(XP_SEARCH_SUBJECT,        XP_MSG_BASE + 648, "subject")
ResDef(XP_SEARCH_SENDER,         XP_MSG_BASE + 649, "sender")
ResDef(XP_SEARCH_BODY,           XP_MSG_BASE + 650, "body")
ResDef(XP_SEARCH_DATE,           XP_MSG_BASE + 651, "date")
ResDef(XP_SEARCH_PRIORITY,       XP_MSG_BASE + 652, "priority")
ResDef(XP_SEARCH_STATUS,         XP_MSG_BASE + 653, "status")
ResDef(XP_SEARCH_TO,             XP_MSG_BASE + 654, "to")
ResDef(XP_SEARCH_CC,             XP_MSG_BASE + 655, "CC")
ResDef(XP_SEARCH_TO_OR_CC,       XP_MSG_BASE + 656, "to or CC")
ResDef(XP_SEARCH_COMMON_NAME,    XP_MSG_BASE + 657, "name")
ResDef(XP_SEARCH_EMAIL_ADDRESS,  XP_MSG_BASE + 658, "e-mail address")
ResDef(XP_SEARCH_PHONE_NUMBER,   XP_MSG_BASE + 659, "phone number")
ResDef(XP_SEARCH_ORG,            XP_MSG_BASE + 660, "organization")
ResDef(XP_SEARCH_ORG_UNIT,       XP_MSG_BASE + 661, "organizational unit")
ResDef(XP_SEARCH_LOCALITY,       XP_MSG_BASE + 662, "city")
ResDef(XP_SEARCH_STREET_ADDRESS, XP_MSG_BASE + 663, "street address")
ResDef(XP_SEARCH_SIZE,           XP_MSG_BASE + 664, "size")
ResDef(XP_SEARCH_ANY_TEXT,       XP_MSG_BASE + 665, "any text")
ResDef(XP_SEARCH_KEYWORDS,       XP_MSG_BASE + 666, "keyword")

/* Search operator names */
/* WARNING -- order must match MSG_SearchOperator enum */
ResDef(XP_SEARCH_CONTAINS,       XP_MSG_BASE + 667, "contains")
ResDef(XP_SEARCH_DOESNT_CONTAIN, XP_MSG_BASE + 668, "doesn't contain")
ResDef(XP_SEARCH_IS,             XP_MSG_BASE + 669, "is")
ResDef(XP_SEARCH_ISNT,           XP_MSG_BASE + 670, "isn't")
ResDef(XP_SEARCH_IS_EMPTY,       XP_MSG_BASE + 671, "is empty")
ResDef(XP_SEARCH_IS_BEFORE,      XP_MSG_BASE + 672, "is before")
ResDef(XP_SEARCH_IS_AFTER,       XP_MSG_BASE + 673, "is after")
ResDef(XP_SEARCH_IS_HIGHER,      XP_MSG_BASE + 674, "is higher than")
ResDef(XP_SEARCH_IS_LOWER,       XP_MSG_BASE + 675, "is lower than")
ResDef(XP_SEARCH_BEGINS_WITH,    XP_MSG_BASE + 676, "begins with")
ResDef(XP_SEARCH_ENDS_WITH,      XP_MSG_BASE + 677, "ends with")
ResDef(XP_SEARCH_SOUNDS_LIKE,    XP_MSG_BASE + 678, "sounds like")
ResDef(XP_SEARCH_RESERVED1,      XP_MSG_BASE + 679, "reserved")
ResDef(XP_FORWARDED_MESSAGE_ATTACHMENT, XP_MSG_BASE + 680, "forward.msg")
ResDef(XP_RETURN_RECEIPT_NOT_SUPPORT, XP_MSG_BASE + 681, 
"Your SMTP server does not support the return receipt function \n\
so your message will be sent without the return receipt request.")
ResDef(XP_SEARCH_AGE,           XP_MSG_BASE + 682,   "age in days")             /* added to support age */
ResDef(XP_SEARCH_IS_GREATER,    XP_MSG_BASE + 683,   "is greater than")
ResDef(XP_SEARCH_IS_LESS,       XP_MSG_BASE + 684,   "is less than")
ResDef(XP_SENDMAIL_BAD_TLS,		XP_MSG_BASE + 685,
"Your SMTP mail server could not start a secure connection.\n\
You have requested to send mail ONLY in secure mode and therefore \
the connection has been aborted. Please check your preferences.")

/* Message status names */
ResDef(XP_STATUS_READ,           XP_MSG_BASE + 686, "read")
ResDef(XP_STATUS_REPLIED,        XP_MSG_BASE + 687, "replied")

ResDef(MK_MSG_RENAME_FOLDER,     XP_MSG_BASE + 688, "Rename selected folder")

ResDef(MK_MSG_SAVE_DRAFT,     XP_MSG_BASE + 689, "Save draft")

ResDef(XP_FILTER_MOVE_TO_FOLDER, XP_MSG_BASE + 690,  "Move to folder")
ResDef(XP_FILTER_CHANGE_PRIORITY,XP_MSG_BASE + 691,  "Change priority")
ResDef(XP_FILTER_DELETE,         XP_MSG_BASE + 692,  "Delete")
ResDef(XP_FILTER_MARK_READ,      XP_MSG_BASE + 693,  "Mark read")
ResDef(XP_FILTER_KILL_THREAD,    XP_MSG_BASE + 694,  "Ignore thread")
ResDef(XP_FILTER_WATCH_THREAD,   XP_MSG_BASE + 695,  "Watch thread")

ResDef(XP_STATUS_FORWARDED,      XP_MSG_BASE + 696,  "forwarded")
ResDef(XP_STATUS_REPLIED_AND_FORWARDED, XP_MSG_BASE + 697, "replied and forwarded")

ResDef (XP_LDAP_SERVER_SIZELIMIT_EXCEEDED, XP_MSG_BASE + 698,
"More items were found than the directory server could return.\n\
Please enter more search criteria and try again.")

ResDef (XP_LDAP_SIZELIMIT_EXCEEDED, XP_MSG_BASE + 699,
"The directory server has found more than %d items.\n\
Please enter more search criteria and try again.")

ResDef (MK_MSG_READ_MORE, XP_MSG_BASE + 700, "Read More" )
ResDef (MK_MSG_NEXTUNREAD_THREAD, XP_MSG_BASE + 701, "Next Unread Thread" )
ResDef (MK_MSG_NEXTUNREAD_CATEGORY, XP_MSG_BASE + 702, "Next Unread Category" )
ResDef (MK_MSG_NEXTUNREAD_GROUP, XP_MSG_BASE + 703, "Next Unread Group" )

ResDef (MK_MSG_BY_PRIORITY,     XP_MSG_BASE + 704, "By Priority" )

ResDef (MK_MSG_CALL,            XP_MSG_BASE + 705, "Call" )
ResDef (MK_MSG_BY_TYPE,         XP_MSG_BASE + 706, "By Type" )
ResDef (MK_MSG_BY_NAME,         XP_MSG_BASE + 707, "By Name" )
ResDef (MK_MSG_BY_NICKNAME,     XP_MSG_BASE + 708, "By Nickname" )
ResDef (MK_MSG_BY_EMAILADDRESS, XP_MSG_BASE + 709, "By Email Address" )
ResDef (MK_MSG_BY_COMPANY,      XP_MSG_BASE + 710, "By Company" )
ResDef (MK_MSG_BY_LOCALITY,     XP_MSG_BASE + 711, "By City" )
ResDef (MK_MSG_SORT_DESCENDING, XP_MSG_BASE + 712, "Descending" )
ResDef (MK_MSG_ADD_NAME,        XP_MSG_BASE + 713, "New Card..." )
ResDef (MK_MSG_ADD_LIST,        XP_MSG_BASE + 714, "New List..." )
ResDef (MK_MSG_PROPERTIES,      XP_MSG_BASE + 715, "Card Properties..." )

ResDef (MK_MSG_SEARCH_STATUS,   XP_MSG_BASE + 716, "Searching %s..." )
ResDef (MK_MSG_NEED_FULLNAME,   XP_MSG_BASE + 717, "You must enter a full name." )
ResDef (MK_MSG_NEED_GIVENNAME,  XP_MSG_BASE + 718, "You must enter a first name." )
ResDef (MK_MSG_REPARSE_FOLDER,  XP_MSG_BASE + 719, "Building summary file for %s...")

ResDef( MK_MSG_ALIVE_THREADS,   XP_MSG_BASE + 720, "All")
ResDef( MK_MSG_KILLED_THREADS,  XP_MSG_BASE + 721, "Ignored Threads")
ResDef( MK_MSG_WATCHED_THREADS, XP_MSG_BASE + 722, "Watched Threads with New")
ResDef( MK_MSG_THREADS_WITH_NEW, XP_MSG_BASE + 723, "Threads with New")

ResDef (XP_LDAP_DIALOG_TITLE,      XP_MSG_BASE + 724, "LDAP Directory Entry")
ResDef (XP_LDAP_OPEN_ENTRY_FAILED, XP_MSG_BASE + 725, "Failed to open entry for %s due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_OPEN_SERVER_FAILED,XP_MSG_BASE + 726, "Failed to open a connection to '%s' due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_BIND_FAILED,       XP_MSG_BASE + 727, "Failed to bind to '%s' due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_SEARCH_FAILED,     XP_MSG_BASE + 728, "Failed to search '%s' due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_MODIFY_FAILED,     XP_MSG_BASE + 729, "Failed to modify entry on '%s' due to LDAP error '%s' (0x%02X)")
ResDef (MK_SEARCH_HITS_COUNTER,    XP_MSG_BASE + 730, "Found %d matches")
ResDef( MK_MSG_UNSUBSCRIBE_GROUP,  XP_MSG_BASE + 731, "Are you sure you want to unsubscribe from %s?")

ResDef (MK_HDR_DOWNLOAD_COUNT,     XP_MSG_BASE + 732, "Received %ld of %ld headers")

ResDef (MK_NO_NEW_DISC_MSGS,       XP_MSG_BASE + 733, "No new messages in newsgroup")
ResDef (MK_MSG_DOWNLOAD_COUNT,     XP_MSG_BASE + 734, "Received %ld of %ld messages")

ResDef(XP_MAIL_SEARCHING,   XP_MSG_BASE + 735,
"Mail: Searching mail folders..." )

ResDef(MK_MSG_IGNORE_THREAD, XP_MSG_BASE + 736,
"Ignore Thread")

ResDef(MK_MSG_IGNORE_THREADS, XP_MSG_BASE + 737,
"Ignore Threads")

ResDef(MK_MSG_WATCH_THREAD, XP_MSG_BASE + 738,
"Watch Thread")

ResDef(MK_MSG_WATCH_THREADS, XP_MSG_BASE + 739,
"Watch Threads")

ResDef(MK_LDAP_COMMON_NAME, XP_MSG_BASE + 740, "Name")
ResDef(MK_LDAP_FAX_NUMBER, XP_MSG_BASE + 741, "Fax")
ResDef(MK_LDAP_GIVEN_NAME, XP_MSG_BASE + 742, "First Name")
ResDef(MK_LDAP_LOCALITY, XP_MSG_BASE + 743, "City")
ResDef(MK_LDAP_PHOTOGRAPH, XP_MSG_BASE + 744, "Photograph")
ResDef(MK_LDAP_EMAIL_ADDRESS, XP_MSG_BASE + 745, "Email")
ResDef(MK_LDAP_MANAGER, XP_MSG_BASE + 746, "Manager")
ResDef(MK_LDAP_ORGANIZATION, XP_MSG_BASE + 747, "Organization")
ResDef(MK_LDAP_OBJECT_CLASS, XP_MSG_BASE + 748, "Object Class")
ResDef(MK_LDAP_ORG_UNIT, XP_MSG_BASE + 749, "Organizational Unit")
ResDef(MK_LDAP_POSTAL_ADDRESS, XP_MSG_BASE + 750, "Mailing Address")
ResDef(MK_LDAP_SECRETARY, XP_MSG_BASE + 751, "Administrative Assistant")
ResDef(MK_LDAP_SURNAME, XP_MSG_BASE + 752, "Last Name")
ResDef(MK_LDAP_STREET, XP_MSG_BASE + 753, "Street")
ResDef(MK_LDAP_PHONE_NUMBER, XP_MSG_BASE + 754, "Phone Number")
ResDef(MK_LDAP_TITLE, XP_MSG_BASE + 755, "Title")
ResDef(MK_LDAP_CAR_LICENSE, XP_MSG_BASE + 756, "Car License Plate")
ResDef(MK_LDAP_BUSINESS_CAT, XP_MSG_BASE + 757, "Business Category")
ResDef(MK_LDAP_DESCRIPTION, XP_MSG_BASE + 758, "Notes")
ResDef(MK_LDAP_DEPT_NUMBER, XP_MSG_BASE + 759, "Department Number")
ResDef(MK_LDAP_EMPLOYEE_TYPE, XP_MSG_BASE + 760, "Employee Type")
ResDef(MK_LDAP_POSTAL_CODE, XP_MSG_BASE + 761, "ZIP Code")

    /* MK_MSG_SECURE_TAG is added to the end of a newshost name to indicate
       a secure newshost. */
ResDef(MK_MSG_SECURE_TAG, XP_MSG_BASE + 762, " (secure)")

ResDef(MK_MSG_NO_CALLPOINT_ADDRESS, XP_MSG_BASE + 763,
"There is no conference address for this person.  Please edit the \n\
entry and try calling them again.")
ResDef(MK_MSG_CANT_DELETE_NEWS_DB, XP_MSG_BASE + 764,
"The local database for %s couldn't be deleted. \n\
Perhaps you are reading the newsgroup in a message list window. \n\
Unsubscribe anyway?")

ResDef(MK_MSG_CALL_NEEDS_IPADDRESS, XP_MSG_BASE + 765,
"Please enter a "MOZ_NAME_PRODUCT" Conference address for this person and try to call again.")

ResDef(MK_MSG_CALL_NEEDS_EMAILADDRESS, XP_MSG_BASE + 766,
"Please enter an email address for this person and try to call again.")

ResDef(MK_MSG_CANT_FIND_CALLPOINT, XP_MSG_BASE + 767,
"Couldn't find "MOZ_NAME_PRODUCT" Conference.  It may need to be installed.")

ResDef(MK_MSG_CANT_CALL_MAILING_LIST, XP_MSG_BASE + 768,
MOZ_NAME_BRAND" Conference is only able to call one person.  Select a single\n\
entry for a person and try again.")

ResDef(MK_MSG_ENTRY_EXISTS_UPDATE, XP_MSG_BASE + 769,
"An entry already exists for %s.  Would you like to replace it?")

ResDef(MK_MSG_ILLEGAL_CHARS_IN_NAME, XP_MSG_BASE + 770,
"That file name contains illegal characters. Please use a different name.")

ResDef(MK_MSG_UNSUBSCRIBE_PROFILE_GROUP,  XP_MSG_BASE + 771,
"%s is a virtual newsgroup. If you\n\
delete it, the server will stop putting messages in the\n\
newsgroup unless you save your search criteria again.\n\n")

ResDef(MK_MSG_ARTICLES_TO_RETRIEVE,  XP_MSG_BASE + 772,
"Found %ld articles to retrieve")

ResDef(MK_MSG_RETRIEVING_ARTICLE_OF, XP_MSG_BASE + 773,
"Retrieving %ld of %ld articles\n\
  in newsgroup %s")

ResDef(MK_MSG_RETRIEVING_ARTICLE, XP_MSG_BASE + 774,
"Retrieving article %ld")

/* Editor object-sizing status message parts */
ResDef(XP_EDT_WIDTH_EQUALS, XP_MSG_BASE + 775,
"Width = %ld")

ResDef(XP_EDT_HEIGHT_EQUALS, XP_MSG_BASE + 776,
"Height = %ld")

ResDef(XP_EDT_PERCENT_ORIGINAL, XP_MSG_BASE + 777,
"(%ld%% of original %s)")

ResDef(XP_EDT_PERCENT_PREVIOUS, XP_MSG_BASE + 778,
"(%ld%% of previous %s)")

ResDef(XP_EDT_WIDTH, XP_MSG_BASE + 779,
"width")

ResDef(XP_EDT_HEIGHT, XP_MSG_BASE + 780,
"height")

ResDef(XP_EDT_AND, XP_MSG_BASE + 781,
" and ")

ResDef(XP_EDT_PIXELS, XP_MSG_BASE + 782,
" pixels  ")

ResDef(XP_EDT_PERCENT_WINDOW, XP_MSG_BASE + 783,
"% of window  ")

ResDef(MK_LDAP_REGION, XP_MSG_BASE + 784, "State")
ResDef(MK_LDAP_DOM_TYPE, XP_MSG_BASE + 785, "Domestic")
ResDef(MK_LDAP_INTL_TYPE, XP_MSG_BASE + 786, "International")
ResDef(MK_LDAP_POSTAL_TYPE, XP_MSG_BASE + 787, "Postal")
ResDef(MK_LDAP_PARCEL_TYPE, XP_MSG_BASE + 788, "Parcel")
ResDef(MK_LDAP_WORK_TYPE, XP_MSG_BASE + 789, "Work")
ResDef(MK_LDAP_HOME_TYPE, XP_MSG_BASE + 790, "Home")
ResDef(MK_LDAP_PREF_TYPE, XP_MSG_BASE + 791, "Preferred")
ResDef(MK_LDAP_VOICE_TYPE, XP_MSG_BASE + 792, "Voice")
ResDef(MK_LDAP_FAX_TYPE, XP_MSG_BASE + 793, "Fax")
ResDef(MK_LDAP_MSG_TYPE, XP_MSG_BASE + 794, "Message")
ResDef(MK_LDAP_CELL_TYPE, XP_MSG_BASE + 795, "Cellular")
ResDef(MK_LDAP_PAGER_TYPE, XP_MSG_BASE + 796, "Pager")
ResDef(MK_LDAP_BBS_TYPE, XP_MSG_BASE + 797, "BBS")
ResDef(MK_LDAP_MODEM_TYPE, XP_MSG_BASE + 798, "Modem")
ResDef(MK_LDAP_CAR_TYPE, XP_MSG_BASE + 799, "Car")
ResDef(MK_LDAP_ISDN_TYPE, XP_MSG_BASE + 800, "ISDN")
ResDef(MK_LDAP_VIDEO_TYPE, XP_MSG_BASE + 801, "Video")
ResDef(MK_LDAP_AOL_TYPE, XP_MSG_BASE + 802, "AOL")
ResDef(MK_LDAP_APPLELINK_TYPE, XP_MSG_BASE + 803, "Applelink")
ResDef(MK_LDAP_ATTMAIL_TYPE, XP_MSG_BASE + 804, "AT&T Mail")
ResDef(MK_LDAP_CSI_TYPE, XP_MSG_BASE + 805, "Compuserve")
ResDef(MK_LDAP_EWORLD_TYPE, XP_MSG_BASE + 806, "eWorld")
ResDef(MK_LDAP_INTERNET_TYPE, XP_MSG_BASE + 807, "Internet")
ResDef(MK_LDAP_IBMMAIL_TYPE, XP_MSG_BASE + 808, "IBM Mail")
ResDef(MK_LDAP_MCIMAIL_TYPE, XP_MSG_BASE + 809, "MCI Mail")
ResDef(MK_LDAP_POWERSHARE_TYPE, XP_MSG_BASE + 810, "Powershare")
ResDef(MK_LDAP_PRODIGY_TYPE, XP_MSG_BASE + 811, "Prodigy")
ResDef(MK_LDAP_TLX_TYPE, XP_MSG_BASE + 812, "Telex")
ResDef(MK_LDAP_MIDDLE_NAME, XP_MSG_BASE + 813, "Additional Name")
ResDef(MK_LDAP_NAME_PREFIX, XP_MSG_BASE + 814, "Prefix")
ResDef(MK_LDAP_NAME_SUFFIX, XP_MSG_BASE + 815, "Suffix")
ResDef(MK_LDAP_TZ, XP_MSG_BASE + 816, "Time Zone")
ResDef(MK_LDAP_GEO, XP_MSG_BASE + 817, "Geographic Position")
ResDef(MK_LDAP_SOUND, XP_MSG_BASE + 818, "Sound")
ResDef(MK_LDAP_REVISION, XP_MSG_BASE + 819, "Revision")
ResDef(MK_LDAP_VERSION, XP_MSG_BASE + 820, "Version")
ResDef(MK_LDAP_KEY, XP_MSG_BASE + 821, "Public Key")
ResDef(MK_LDAP_LOGO, XP_MSG_BASE + 822, "Logo")

ResDef(MK_LDAP_BIRTHDAY, XP_MSG_BASE + 825, "Birthday")
ResDef(MK_LDAP_X400, XP_MSG_BASE + 826, "X400")
ResDef(MK_LDAP_ADDRESS, XP_MSG_BASE + 827, "Address")
ResDef(MK_LDAP_LABEL, XP_MSG_BASE + 828, "Label")
ResDef(MK_LDAP_MAILER, XP_MSG_BASE + 829, "Mailer")
ResDef(MK_LDAP_ROLE, XP_MSG_BASE + 830, "Role")
ResDef(MK_LDAP_UPDATEURL, XP_MSG_BASE + 831, "Update From")
ResDef(MK_LDAP_COOLTALKADDRESS, XP_MSG_BASE + 832,MOZ_NAME_BRAND" Conference Address")
ResDef(MK_LDAP_USEHTML, XP_MSG_BASE + 833, "HTML Mail")
ResDef(MK_ADDR_ADD, XP_MSG_BASE + 834, "Add to Personal Address Book")

ResDef(XP_NSFONT_DEFAULT, XP_MSG_BASE + 835, "Variable Width")
ResDef(XP_NSFONT_FIXED, XP_MSG_BASE + 836, "Fixed Width")
#ifdef XP_WIN
ResDef(XP_NSFONT_ARIAL, XP_MSG_BASE + 837, "Arial")
#else
ResDef(XP_NSFONT_ARIAL, XP_MSG_BASE + 837, "Helvetica")
#endif
ResDef(XP_NSFONTLIST_ARIAL, XP_MSG_BASE + 838, "Arial,Helvetica")

#ifdef XP_WIN
ResDef(XP_NSFONT_TIMES, XP_MSG_BASE + 839, "Times New Roman")
#else
ResDef(XP_NSFONT_TIMES, XP_MSG_BASE + 839, "Times")
#endif
ResDef(XP_NSFONTLIST_TIMES, XP_MSG_BASE + 840, "Times New Roman,Times")

#ifdef XP_WIN
ResDef(XP_NSFONT_COURIER, XP_MSG_BASE + 841, "Courier New")
#else
ResDef(XP_NSFONT_COURIER, XP_MSG_BASE + 841, "Courier")
#endif
ResDef(XP_NSFONTLIST_COURIER, XP_MSG_BASE + 842, "Courier New,Courier")

ResDef(XP_EDT_MSG_TEXT_BUFFER_TOO_LARGE,    (XP_MSG_BASE + 843),
"This document has %ld characters. The Spelling Checker cannot process more than %ld characters.")
ResDef(XP_EDT_MSG_CANNOT_PASTE, (XP_MSG_BASE + 844),
"You cannot paste this much text in a single operation.\nTry pasting the text in several smaller segments.")
ResDef(XP_EDT_CAN_PASTE_AS_TABLE, (XP_MSG_BASE + 845),"Text can be pasted as %d rows and %d columns.\n\n")
ResDef(XP_EDT_REPLACE_CELLS, (XP_MSG_BASE + 846),"Replace existing cells?")
ResDef(XP_EDT_PASTE_AS_TABLE, (XP_MSG_BASE + 847),"Paste text as a new table?")
ResDef(XP_EDT_DELETE_OR_COPY_CAPTION, (XP_MSG_BASE + 848),"Delete/Copy/Cut Selection")
ResDef(XP_EDT_SELECTION_CROSSES_NESTED_TABLE, (XP_MSG_BASE + 849),
"The selection includes a nested table boundary.\nCopying and deleting are not allowed.")
ResDef(XP_EDT_DOCUMENT_BUSY, (XP_MSG_BASE + 850),
"Cannot edit the page at this time.\nPlease try again later.")
ResDef(XP_EDT_TARGET_NAME, (XP_MSG_BASE + 851), "Target Name: ")
ResDef(XP_EDT_LINK_HINT, (XP_MSG_BASE + 852), " [Press Ctrl and click to edit link or jump to target]")
ResDef(XP_EDT_LINK_HINT_MAC, (XP_MSG_BASE + 853), " [Press Cmd and click to edit link or jump to target]")

ResDef(MK_ADDR_DELETE_ALL, XP_MSG_BASE+860, "Delete From All Lists")  /* new entry in address book context menus - delete all from list */
ResDef(MK_ADDR_IMPORT_CARDS, XP_MSG_BASE+861, "Adding cards to %s") /* line text for progress window on importing an address book */
ResDef(MK_ADDR_IMPORT_MAILINGLISTS, XP_MSG_BASE+862, "Updating Mailing Lists in %s") /* line text for progress window on importing an address book */
ResDef(MK_ADDR_IMPORT_TITLE, XP_MSG_BASE+863, "Import") /* title for progress window on importing an address book */
ResDef(MK_ADDR_EXPORT_CARDS, XP_MSG_BASE+864, "Copying cards from %s") /* line text for progress window on exporting cards from an address book */
ResDef(MK_ADDR_EXPORT_TITLE, XP_MSG_BASE+865, "Export") /* title for progress window on exporting an address book */
ResDef(MK_ADDR_DELETE_ENTRIES, XP_MSG_BASE+866, "Deleting entries from %s") /* deleting entries from address book name */
ResDef(MK_ADDR_COPY_ENTRIES,  XP_MSG_BASE+867,"Copying entries to %s") /* copying entries to address book name */
ResDef(MK_ADDR_MOVE_ENTRIES, XP_MSG_BASE+868, "Moving entries to %s") /* moving entries to address book name */


ResDef(XP_EDT_MUST_SAVE_PROMPT, XP_MSG_BASE + 880, "You must save\n%s\nto a local file before editing.\nSave to a file now?")

ResDef (MK_LDAP_ADD_SERVER_TO_PREFS, XP_MSG_BASE + 881,
"Would you like to add %s to your LDAP preferences?")

ResDef(MK_ADDR_BOOK_CARD, XP_MSG_BASE + 882, "Card for %s")

/*  The following are used by mimehtml.cpp to emit header display in HTML */
ResDef(MK_MIMEHTML_DISP_SUBJECT,    XP_MSG_BASE + 883, "Subject")
ResDef(MK_MIMEHTML_DISP_RESENT_COMMENTS,XP_MSG_BASE + 884, "Resent-Comments")
ResDef(MK_MIMEHTML_DISP_RESENT_DATE,    XP_MSG_BASE + 885, "Resent-Date")
ResDef(MK_MIMEHTML_DISP_RESENT_SENDER,  XP_MSG_BASE + 886, "Resent-Sender")
ResDef(MK_MIMEHTML_DISP_RESENT_FROM,    XP_MSG_BASE + 887, "Resent-From")
ResDef(MK_MIMEHTML_DISP_RESENT_TO,  XP_MSG_BASE + 888, "Resent-To")
ResDef(MK_MIMEHTML_DISP_RESENT_CC,  XP_MSG_BASE + 889, "Resent-CC")
ResDef(MK_MIMEHTML_DISP_DATE,       XP_MSG_BASE + 890, "Date")
ResDef(MK_MIMEHTML_DISP_SENDER,     XP_MSG_BASE + 891, "Sender")
ResDef(MK_MIMEHTML_DISP_FROM,       XP_MSG_BASE + 892, "From")
ResDef(MK_MIMEHTML_DISP_REPLY_TO,   XP_MSG_BASE + 893, "Reply-To")
ResDef(MK_MIMEHTML_DISP_ORGANIZATION,   XP_MSG_BASE + 894, "Organization")
ResDef(MK_MIMEHTML_DISP_TO,     XP_MSG_BASE + 895, "To")
ResDef(MK_MIMEHTML_DISP_CC,     XP_MSG_BASE + 896, "CC")
ResDef(MK_MIMEHTML_DISP_NEWSGROUPS, XP_MSG_BASE + 897, "Newsgroups")
ResDef(MK_MIMEHTML_DISP_FOLLOWUP_TO,    XP_MSG_BASE + 898, "Followup-To")
ResDef(MK_MIMEHTML_DISP_REFERENCES, XP_MSG_BASE + 899, "References")
ResDef(MK_MIMEHTML_DISP_NAME,       XP_MSG_BASE + 900, "Name")
ResDef(MK_MIMEHTML_DISP_TYPE,       XP_MSG_BASE + 901, "Type")
ResDef(MK_MIMEHTML_DISP_ENCODING,   XP_MSG_BASE + 902, "Encoding")
ResDef(MK_MIMEHTML_DISP_DESCRIPTION,    XP_MSG_BASE + 903, "Description")

/*  End of emit header stuff */

ResDef(MK_MSG_NEWS_HOST_TABLE_INVALID, XP_MSG_BASE + 904,
"Failed to initialize news servers. Perhaps your Newsgroup Directory preference is invalid.\n\
 You will be able to post to newsgroups, but not read them")

ResDef(MK_MSG_ADD_SENDER_TO_ADDRESS_BOOK, XP_MSG_BASE + 905,
"Sender")

ResDef(MK_MSG_ADD_ALL_TO_ADDRESS_BOOK, XP_MSG_BASE + 906,
"All")

ResDef(MK_MSG_IMAP_CONTAINER_NAME, XP_MSG_BASE + 907, "Messages on %s")

ResDef(MK_MSG_CANT_MOVE_FOLDER_TO_CHILD, XP_MSG_BASE + 908,
"Can't move a folder into a folder it contains")

ResDef(MK_MSG_NEW_GROUPS_DETECTED, XP_MSG_BASE + 909,
"%ld new newsgroups have been created on the %s news server.  To view the list\n\
of new newsgroups, select \042Join Newsgroup\042 and then click on\n\
the \042New Newsgroups\042 tab.")

ResDef (MK_MSG_SEARCH_GROUPNAMES_STATUS, XP_MSG_BASE + 910,
"Searching newsgroup names...")


ResDef(MK_MSG_SEND_ENCRYPTED,       XP_MSG_BASE + 911,
       "Send Encrypted")

ResDef(MK_MSG_SEND_SIGNED,      XP_MSG_BASE + 912,
       "Send Cryptographically Signed")

ResDef(MK_MSG_SECURITY_ADVISOR,     XP_MSG_BASE + 913,
       "Security")


ResDef(MK_MSG_LINK_TO_DOCUMENT,     XP_MSG_BASE + 925,
       "Link to Document")

ResDef(MK_MSG_DOCUMENT_INFO,        XP_MSG_BASE + 926,
       "<B>Document Info:</B>")

ResDef(MK_MSG_IN_MSG_X_USER_WROTE,  XP_MSG_BASE + 927,
       "In message %s %s wrote:<P>")

ResDef(MK_MSG_USER_WROTE,       XP_MSG_BASE + 928,
       "%s wrote:<P>")

ResDef(MK_MSG_UNSPECIFIED_TYPE,     XP_MSG_BASE + 929,
       "unspecified type")

ResDef(MK_MIME_MULTIPART_SIGNED_BLURB,  XP_MSG_BASE + 930,
 "This is a cryptographically signed message in MIME format.")

#ifdef OSF1
ResDef (MK_MSG_MARKREAD_COUNT,           XP_MSG_BASE + 931, "Marked %d messages read")

ResDef (MK_MSG_DONE_MARKREAD_COUNT,      XP_MSG_BASE + 932, "Marked %d messages read...Done")
#else
ResDef (MK_MSG_MARKREAD_COUNT,     XP_MSG_BASE + 931, "Marked %ld messages read")

ResDef (MK_MSG_DONE_MARKREAD_COUNT,    XP_MSG_BASE + 932, "Marked %ld messages read...Done")
#endif

ResDef(MK_MIMEHTML_DISP_MESSAGE_ID,         XP_MSG_BASE + 933, "Message-ID")
ResDef(MK_MIMEHTML_DISP_RESENT_MESSAGE_ID,  XP_MSG_BASE + 934, "Resent-Message-ID")
ResDef(MK_MIMEHTML_DISP_BCC,                XP_MSG_BASE + 935, "BCC")

ResDef (MK_MSG_CANT_DELETE_RESERVED_FOLDER, XP_MSG_BASE + 936,
"Can't delete the reserved folder '%s'.")

ResDef (MK_MSG_CANT_SEARCH_IF_NO_SUMMARY, XP_MSG_BASE + 937,
"Can't search the folder '%s' because it doesn't have a summary file.")

ResDef (XP_STATUS_NEW, XP_MSG_BASE + 938, "New")

ResDef (MK_ADDR_PAB, XP_MSG_BASE + 939,
        "Personal Address Book")

ResDef (MK_MSG_BY_STATUS, XP_MSG_BASE + 940, "By Status")

ResDef (MK_MSG_NEW_MESSAGES_ONLY, XP_MSG_BASE + 941, "New")

ResDef (MK_MSG_ASK_HTML_MAIL_TITLE, XP_MSG_BASE + 942, "HTML Mail Question")

ResDef (MK_MSG_ASK_HTML_MAIL, XP_MSG_BASE + 943, "\
Some of the recipients are not listed as being able to receive HTML\n\
mail.  Would you like to convert the message to plain text or send it\n\
in HTML anyway?\n\
<p>\n\
<table>\n\
<tr><td valign=top>\n\
%-cont-%")

ResDef (MK_MSG_ASK_HTML_MAIL_1, XP_MSG_BASE + 944, "\
<input type=radio name=mail value=1 checked>\n\
</td><td valign=top>\n\
Send in Plain Text and HTML\n\
</td></tr><tr><td valign=top>\n\
<input type=radio name=mail value=2>\n\
</td><td valign=top>\n\
%-cont-%")

ResDef (MK_MSG_ASK_HTML_MAIL_2, XP_MSG_BASE + 945, "\
Send in Plain Text Only\n\
</td></tr><tr><td valign=top>\n\
<input type=radio name=mail value=3>\n\
</td><td valign=top>\n\
Send in HTML Only\n\
</td></tr>\n\
</table>\n\
<center>\n\
<script>\n\
%-cont-%")

ResDef (MK_MSG_ASK_HTML_MAIL_3, XP_MSG_BASE + 946, "\
function Doit(value) {\n\
    document.theform.cmd.value = value;\n\
    document.theform.submit();\n\
}\n\
</script>\n\
<input type=hidden name=cmd value=-1>\n\
%-cont-%")

ResDef (MK_MSG_ASK_HTML_MAIL_4, XP_MSG_BASE + 947, "\
<input type=button value=\042Send\042 onclick=\042Doit(0);\042>\n\
<input type=button value=\042Don't Send\042 onclick=\042Doit(1);\042>\n\
<input type=button value=\042Recipients...\042 onclick=\042Doit(2);\042>\n\
%-cont-%")

ResDef (MK_MSG_ASK_HTML_MAIL_5, XP_MSG_BASE + 948, "\
<input type=button value=\042Help\042 onclick=\042Doit(3);\042>\n\
<input type=hidden name=button value=0>\n\
</center>\n\
")


/* Reserve a bunch extra, just in case. */

ResDef (MK_MSG_ASK_HTML_MAIL_6, XP_MSG_BASE + 949, "")
ResDef (MK_MSG_ASK_HTML_MAIL_7, XP_MSG_BASE + 950, "")
ResDef (MK_MSG_ASK_HTML_MAIL_8, XP_MSG_BASE + 951, "")
ResDef (MK_MSG_ASK_HTML_MAIL_9, XP_MSG_BASE + 952, "")
ResDef (MK_MSG_ASK_HTML_MAIL_10, XP_MSG_BASE + 953, "")

/* String to use to indicate everyone at a given domain.  Used as 
   in "<everyone>@netscape.com". */
ResDef (MK_MSG_EVERYONE, XP_MSG_BASE + 954, "<everyone>")

ResDef (MK_MSG_HTML_RECIPIENTS_TITLE, XP_MSG_BASE + 955, "HTML Recipients")

ResDef (MK_MSG_HTML_RECIPIENTS, XP_MSG_BASE + 956, "\
The recipients and domains below are not listed as being able to read\n\
HTML messages.  If this listing is incorrect, you may change it below.\n\
%-cont-%")

ResDef (MK_MSG_HTML_RECIPIENTS_1, XP_MSG_BASE + 957, "\
<p>\n\
<table>\n\
<tr>\n\
<td>Does not prefer HTML</td>\n\
<td></td>\n\
<td>Prefers HTML</td>\n\
</tr>\n\
<tr>\n\
<td>\n\
<select name=nohtml size=7 multiple\n\
%-cont-%")

ResDef (MK_MSG_HTML_RECIPIENTS_2, XP_MSG_BASE + 958, "\
onChange=\042SelectAllIn(document.theform.html, false);\042>\n\
%1%\n\
</select>\n\
</td>\n\
<td>\n\
<center>\n\
<input type=button name=add\n\
value=\042Add &gt; &gt;\042 onclick=\042DoAdd();\042>\n\
%-cont-%")

ResDef (MK_MSG_HTML_RECIPIENTS_3, XP_MSG_BASE + 959, "\
<p>\n\
<input type=button name=remove\n\
value=\042&lt; &lt; Remove\042 onclick=\042DoRemove();\042>\n\
</center>\n\
</td>\n\
<td>\n\
<select name=html size=7 multiple\n\
%-cont-%")

ResDef (MK_MSG_HTML_RECIPIENTS_4, XP_MSG_BASE + 960, "\
onChange=\042SelectAllIn(document.theform.nohtml, false);\042>\n\
%2%\n\
</select>\n\
</td>\n\
</tr>\n\
</table>\n\
<p>\n\
<center>\n\
%-cont-%")

ResDef (MK_MSG_HTML_RECIPIENTS_5, XP_MSG_BASE + 961, "\
<input type=button value=OK onclick=\042SelectAll(); Doit(0);\042>\n\
<input type=button value=Cancel onclick=\042Doit(1);\042>\n\
<input type=button value=Help onclick=\042Doit(2);\042>\n\
%-cont-%")

ResDef (MK_MSG_HTML_RECIPIENTS_6, XP_MSG_BASE + 962, "\
<input type=hidden name=cmd value=-1>\n\
<input type=hidden name=button value=0>\n\
</center>\n\
<script>\n\
%0%\n\
</script>\n\
")


/* Reserve a bunch extra, just in case. */

ResDef (MK_MSG_HTML_RECIPIENTS_7, XP_MSG_BASE + 963, "")
ResDef (MK_MSG_HTML_RECIPIENTS_8, XP_MSG_BASE + 964, "")
ResDef (MK_MSG_HTML_RECIPIENTS_9, XP_MSG_BASE + 965, "")
ResDef (MK_MSG_HTML_RECIPIENTS_10, XP_MSG_BASE + 966, "")
ResDef (MK_MSG_HTML_RECIPIENTS_11, XP_MSG_BASE + 967, "")

ResDef (MK_ADDR_ENTRY_ALREADY_EXISTS, XP_MSG_BASE + 968, 
    "An Address Book entry with this name and email address already exists.")

ResDef (MK_ADDR_ENTRY_ALREADY_IN_LIST, XP_MSG_BASE + 969, 
    "This Address Book entry is already a member of this list.")

ResDef (MK_LDAP_CUSTOM1, XP_MSG_BASE + 970, "Custom 1")
ResDef (MK_LDAP_CUSTOM2, XP_MSG_BASE + 971, "Custom 2")
ResDef (MK_LDAP_CUSTOM3, XP_MSG_BASE + 972, "Custom 3")
ResDef (MK_LDAP_CUSTOM4, XP_MSG_BASE + 973, "Custom 4")
ResDef (MK_LDAP_CUSTOM5, XP_MSG_BASE + 974, "Custom 5")

ResDef (MK_ADDR_ADD_PERSON_TO_ABOOK, XP_MSG_BASE + 975, 
"Mailing lists can only contain entries from the Personal Address Book.\n\
Would you like to add %s to the address book?")

ResDef (MK_ADDR_ENTRY_IS_LIST, XP_MSG_BASE + 976, 
"A mailing list cannot have itself as a member")
ResDef (MK_ADDR_NEW_CARD, XP_MSG_BASE + 977, 
"New Card")
ResDef (MK_ADDR_NEW_PERCARD, XP_MSG_BASE + 978, 
"New Personal Card")
ResDef (MK_ADDR_PERCARD, XP_MSG_BASE + 979, 
"Personal Card for %s")
ResDef (MK_ADDR_CCNAME, XP_MSG_BASE + 980, "CC: %s")
ResDef (MK_ADDR_BCCNAME, XP_MSG_BASE + 981, "Bcc: %s")
ResDef (MK_ADDR_TONAME, XP_MSG_BASE + 982, "To: %s")

ResDef (XP_EDT_ERR_SAVE_WRITING_ROOT, XP_MSG_BASE + 983,
  "%s can't be saved. Either the disk is full\n\
or the file is locked.\n\n\
Try saving on a different disk or try saving\n\
%s with a different name.")
ResDef (XP_EDT_HEAD_FAILED, XP_MSG_BASE + 984,
        "HEAD call to server failed.\nUpload aborted.")

ResDef (MK_UNABLE_TO_OPEN_ADDR_FILE, XP_MSG_BASE + 985, 
        "Unable to open address book database file.")
ResDef (MK_ADDR_LIST_ALREADY_EXISTS, XP_MSG_BASE + 986,
        "A mailing list with this name already exists.")
#ifndef MOZ_COMMUNICATOR_NAME
ResDef (MK_ADDR_UNABLE_TO_IMPORT, XP_MSG_BASE + 987,
    MOZ_NAME_PRODUCT" is unable to import this file into the address book.")
#else
ResDef (MK_ADDR_UNABLE_TO_IMPORT, XP_MSG_BASE + 987,
    "Communicator is unable to import this file into the address book.")
#endif

ResDef(MK_MSG_PURGING_NEWSGROUP_ETC,        XP_MSG_BASE + 988,
 "Purging newsgroup %s...")

 ResDef(MK_MSG_PURGING_NEWSGROUP_HEADER,    XP_MSG_BASE + 989,
 "Purging newsgroup %s...header %ld")

ResDef(MK_MSG_PURGING_NEWSGROUP_ARTICLE, XP_MSG_BASE + 990,
"Purging newsgroup %s...article %ld")

ResDef(MK_MSG_PURGING_NEWSGROUP_DONE,   XP_MSG_BASE + 991,
 "Purging newsgroup %s...Done")

ResDef (XP_EDT_PUBLISH_ERROR_BODY, XP_MSG_BASE + 992,
  "Make sure you specify the location without the filename:\n\
e.g. http://hostmachine/myfolder/\n\n\
Try to publish to this URL anyway?")

ResDef (XP_EDT_PUBLISH_BAD_URL, XP_MSG_BASE + 993,
  "Publish destination is invalid.")
ResDef (XP_EDT_PUBLISH_BAD_CHAR, XP_MSG_BASE + 994,
  "Publish filename or location contains at least one\n\
of these illegal characters:  % < > \\ or a space.\n\n\
Replace illegal characters with underscore ('_') ?")

ResDef (XP_EDT_PUBLISH_BAD_PROTOCOL, XP_MSG_BASE + 995,
  "Publish destination must begin with ftp:// or http://")
ResDef (XP_EDT_PUBLISH_NO_FILE, XP_MSG_BASE + 996,
  "Publish destination ends in a slash.")
ResDef (XP_EDT_PUBLISH_NO_EXTENSION, XP_MSG_BASE + 997,
  "Publish destination has no file extension.\n\
Allowable extensions are: '.html, .htm, and '.shtml'\n\n\
Append '.html' to your filename?")

ResDef (MK_CVCOLOR_SOURCE_OF, XP_MSG_BASE + 998,
    "Source of: ")
ResDef (MK_ACCESS_COOKIES_THE_SERVER, XP_MSG_BASE + 999,
    "The server ")
ResDef (MK_ACCESS_COOKIES_WISHES, XP_MSG_BASE + 1000,
    "\nwishes to set a cookie that will be sent ")
ResDef (MK_ACCESS_COOKIES_TOANYSERV, XP_MSG_BASE + 1001,
    "\nto any server in the domain ")
ResDef (MK_ACCESS_COOKIES_TOSELF, XP_MSG_BASE + 1002,
    "only back to itself")
ResDef (MK_ACCESS_COOKIES_NAME_AND_VAL, XP_MSG_BASE + 1003,
    "\nThe name and value of the cookie are:\n")
ResDef (MK_ACCESS_COOKIES_COOKIE_WILL_PERSIST, XP_MSG_BASE + 1004,
    "\nThis cookie will persist until ")
ResDef (MK_ACCESS_COOKIES_SET_IT, XP_MSG_BASE + 1005,
    "\nDo you wish to allow the cookie to be set?")

ResDef (MK_CACHE_CONTENT_LENGTH, XP_MSG_BASE + 1006,
    "Content Length:")
ResDef (MK_CACHE_REAL_CONTENT_LENGTH, XP_MSG_BASE + 1007,
    "Real Content Length:")
ResDef (MK_CACHE_CONTENT_TYPE, XP_MSG_BASE + 1008,
    "Content type:")
ResDef (MK_CACHE_LOCAL_FILENAME, XP_MSG_BASE + 1009,
    "Local filename:")
ResDef (MK_CACHE_LAST_MODIFIED, XP_MSG_BASE + 1010,
    "Last Modified:")
ResDef (MK_CACHE_EXPIRES, XP_MSG_BASE + 1011,
    "Expires:")
ResDef (MK_CACHE_LAST_ACCESSED, XP_MSG_BASE + 1012,
    "Last accessed:")
ResDef (MK_CACHE_CHARSET, XP_MSG_BASE + 1013,
    "Character set:")
ResDef (MK_CACHE_SECURE, XP_MSG_BASE + 1014,
    "Secure:")
ResDef (MK_CACHE_USES_RELATIVE_PATH, XP_MSG_BASE + 1015,
    "Uses relative path:")
ResDef (MK_CACHE_FROM_NETSITE_SERVER, XP_MSG_BASE + 1016,
    "From Netsite Server:")

ResDef (XP_EDT_BREAKING_LINKS, XP_MSG_BASE + 1018,
  "The following links could become invalid because\n\
they refer to files on your local hard disk(s).\n\n\
%s\nIf you're sure you set up the links properly, click\n\
OK to continue publishing.")
ResDef (XP_EDT_ERR_SAVE_FILE_WRITE, XP_MSG_BASE + 1019,
  "%s can't be saved because the disk is full or the\n\
file is locked. Try saving on a different disk or try saving\n\
%s with a different name.")
ResDef (XP_EDT_ERR_SAVE_CONTINUE, XP_MSG_BASE + 1020,
  "\n\n\
If you continue saving, %s won't be saved with\n\
this page.")
ResDef (XP_EDT_ERR_SAVE_SRC_NOT_FOUND, XP_MSG_BASE + 1021,
  "The file %s associated with this page can't be\n\
saved.  Make sure the file is in the correct location.")
ResDef (XP_EDT_ERR_SAVE_FILE_READ, XP_MSG_BASE + 1022,
  "The file %s associated with this page can't be\n\
saved because there is a problem with the file.")
ResDef (XP_EDT_ERR_PUBLISH_PREPARING_ROOT, XP_MSG_BASE + 1023,
  "There was a problem preparing %s for\n\
publishing. "MOZ_NAME_PRODUCT" couldn't create a temporary file.")
ResDef (XP_EDT_ERR_CHECK_DISK, XP_MSG_BASE + 1024,
  "\n\n\
Check to see if your hard disk is full.")
ResDef (XP_EDT_ERR_PUBLISH_FILE_WRITE, XP_MSG_BASE + 1025,
  "There was a problem preparing %s for publishing.\n\
"MOZ_NAME_PRODUCT" couldn't create a temporary file.")
ResDef (XP_EDT_ERR_PUBLISH_CONTINUE, XP_MSG_BASE + 1026,
  "\n\n\
If you continue, %s won't be published with\n\
this page.")
ResDef (XP_EDT_ERR_PUBLISH_SRC_NOT_FOUND, XP_MSG_BASE + 1027,
MOZ_NAME_BRAND" couldn't prepare the file %s for\n\
publishing. Make sure the file is in the correct location.")
ResDef (XP_EDT_ERR_PUBLISH_FILE_READ, XP_MSG_BASE + 1028,
  "The file %s associated with this page can't\n\
be published because there is a problem with the file.")
ResDef (XP_EDT_ERR_MAIL_PREPARING_ROOT, XP_MSG_BASE + 1029,
  "There was a problem preparing the message for sending.\n\
"MOZ_NAME_PRODUCT" couldn't create a temporary file.")
ResDef (XP_EDT_ERR_MAIL_FILE_WRITE, XP_MSG_BASE + 1030,
  "There was a problem preparing %s for sending.\n\
"MOZ_NAME_PRODUCT" couldn't create a temporary file.")
ResDef (XP_EDT_ERR_MAIL_SRC_NOT_FOUND, XP_MSG_BASE + 1031,
MOZ_NAME_BRAND" couldn't prepare the file %s for\n\
sending. Make sure the file is in the correct location.")
ResDef (XP_EDT_ERR_MAIL_FILE_READ, XP_MSG_BASE + 1032,
  "The file %s associated with this page can't be\n\
sent because there is a problem with the file.")
ResDef (XP_EDT_ERR_MAIL_CONTINUE, XP_MSG_BASE + 1033,
  "\n\n\
If you continue, %s won't be sent with\n\
this page.")

ResDef (MK_ADDR_VIEW_COMPLETE_VCARD, XP_MSG_BASE + 1034,
  "View Complete Card")
ResDef (MK_ADDR_VIEW_CONDENSED_VCARD, XP_MSG_BASE + 1035,
  "View Condensed Card")

ResDef(MK_MSG_BY_FLAG,      XP_MSG_BASE + 1036,
 "By Flag")

ResDef(MK_MSG_BY_UNREAD,        XP_MSG_BASE + 1037,
 "By Unread")

ResDef(MK_MSG_BY_SIZE,      XP_MSG_BASE + 1038,
 "By Size")

ResDef(XP_ALERT_OFFLINE_MODE_SELECTED,  XP_MSG_BASE + 1039,
MOZ_NAME_BRAND" was unable to connect to the network because\n\
you are in offline mode.\n\
Choose Go Online from the File Menu and try again.")

ResDef(MK_ADDR_FIRST_LAST_SEP,      XP_MSG_BASE + 1040,
 " ")

ResDef(MK_ADDR_LAST_FIRST_SEP,      XP_MSG_BASE + 1041,
 ", ")

ResDef(MK_MSG_CANT_MOVE_FOLDER,     XP_MSG_BASE + 1042,
    "That item can not be moved to the requested location.")

ResDef(XP_SEC_ENTER_EXPORT_PWD,     XP_MSG_BASE + 1043,
    "Enter password to protect data being exported:")

ResDef(MK_SEARCH_SCOPE_ONE,             XP_MSG_BASE + 1044, "in %s")
ResDef(MK_SEARCH_SCOPE_SELECTED,        XP_MSG_BASE + 1045, "in selected items")
ResDef(MK_SEARCH_SCOPE_OFFLINE_MAIL,    XP_MSG_BASE + 1046, "in offline mail folders") 
ResDef(MK_SEARCH_SCOPE_ONLINE_MAIL,     XP_MSG_BASE + 1047, "in online mail folders")
ResDef(MK_SEARCH_SCOPE_SUBSCRIBED_NEWS, XP_MSG_BASE + 1048, "in subscribed newsgroups")
ResDef(MK_SEARCH_SCOPE_ALL_NEWS,        XP_MSG_BASE + 1049, "in searchable newsgroups")

ResDef(MK_ADDR_DEFAULT_DLS,             XP_MSG_BASE + 1050, "Default Directory Server")
ResDef(MK_ADDR_SPECIFIC_DLS,            XP_MSG_BASE + 1051, "Specific Directory Server")
ResDef(MK_ADDR_HOSTNAMEIP,              XP_MSG_BASE + 1052, "Hostname or IP Address")
ResDef(MK_ADDR_CONFINFO,                XP_MSG_BASE + 1053,MOZ_NAME_BRAND" Conference Address")
ResDef(MK_ADDR_ADDINFO,                 XP_MSG_BASE + 1054, "Additional Information:")

ResDef(MK_MSG_HTML_DOMAINS_DIALOG_TITLE, XP_MSG_BASE + 1055, "HTML Domains")

ResDef(MK_MSG_HTML_DOMAINS_DIALOG, XP_MSG_BASE + 1056, "\
<script>\n\
function DeleteSelected() {\n\
  selname = document.theform.selname;\n\
  gone = document.theform.gone;\n\
  var p;\n\
  var i;\n\
  for (i=selname.options.length-1 ; i>=0 ; i--) {\n\
%-cont-%")

ResDef (MK_MSG_HTML_DOMAINS_DIALOG_1, XP_MSG_BASE + 1057, "\
    if (selname.options[i].selected) {\n\
      selname.options[i].selected = 0;\n\
      gone.value = gone.value + \042,\042 + selname.options[i].value;\n\
      for (j=i ; j<selname.options.length ; j++) {\n\
%-cont-%")

ResDef (MK_MSG_HTML_DOMAINS_DIALOG_2, XP_MSG_BASE + 1058, "\
        selname.options[j] = selname.options[j+1];\n\
      }\n\
    }\n\
  }\n\
}\n\
</script>\n\
This is a list of domains that can accept HTML mail.  Anyone whose\n\
%-cont-%")

ResDef (MK_MSG_HTML_DOMAINS_DIALOG_3, XP_MSG_BASE + 1059, "\
e-mail address ends in one of these domains is considered to use a\n\
mail reader which understands HTML, such as "MOZ_NAME_PRODUCT".<p>\n\
You may remove a domain from this list by clicking on it and\n\
%-cont-%")

ResDef (MK_MSG_HTML_DOMAINS_DIALOG_4, XP_MSG_BASE + 1060, "\
choosing Delete.<p>\n\
<select name=selname multiple size=10>\n\
%0%\n\
</select>\n\
<input type=button value=Delete onclick=\042DeleteSelected();\042>\n\
<input type=hidden name=gone value=\042\042>\n\
")

/* Reserve a bunch extra, just in case. */

ResDef (MK_MSG_HTML_DOMAINS_DIALOG_5, XP_MSG_BASE + 1061, "")
ResDef (MK_MSG_HTML_DOMAINS_DIALOG_6, XP_MSG_BASE + 1062, "")
ResDef (MK_MSG_HTML_DOMAINS_DIALOG_7, XP_MSG_BASE + 1063, "")
ResDef (MK_MSG_HTML_DOMAINS_DIALOG_8, XP_MSG_BASE + 1064, "")
ResDef (MK_MSG_HTML_DOMAINS_DIALOG_9, XP_MSG_BASE + 1065, "")


ResDef (MK_MSG_SET_HTML_NEWSGROUP_HEIRARCHY_CONFIRM, XP_MSG_BASE + 1066,
"The newsgroup %s was accepting HTML because all newsgroups whose name\n\
started with \042%s\042 were marked to accept HTML.  This action will\n\
reverse that; newsgroups whose name start with \042%s\042 will no longer\n\
accept HTML.")

ResDef (MK_MSG_NEXT_CATEGORY, XP_MSG_BASE + 1067, "Next Category")

ResDef (MK_MSG_GROUP_NOT_ON_SERVER, XP_MSG_BASE + 1068, 
"The newsgroup %s does not appear to exist on the host %s.\n\
Would you like to unsubscribe from it?")

ResDef(MK_MIMEHTML_SHOW_SECURITY_ADVISOR,   XP_MSG_BASE + 1079,
       "Show Security Information")

ResDef(MK_MIMEHTML_ENC_AND_SIGNED,  XP_MSG_BASE + 1080,
       "Encrypted<BR><NOBR>and Signed</NOBR>")
ResDef(MK_MIMEHTML_SIGNED,      XP_MSG_BASE + 1081, "Signed")
ResDef(MK_MIMEHTML_ENCRYPTED,       XP_MSG_BASE + 1082, "Encrypted")
ResDef(MK_MIMEHTML_CERTIFICATES,    XP_MSG_BASE + 1083, "Certificates")
ResDef(MK_MIMEHTML_ENC_SIGNED_BAD,  XP_MSG_BASE + 1084,
       "Invalid Signature")
ResDef(MK_MIMEHTML_SIGNED_BAD,      XP_MSG_BASE + 1085,
       "Invalid Signature")
ResDef(MK_MIMEHTML_ENCRYPTED_BAD,   XP_MSG_BASE + 1086,
       "Invalid Encryption")
ResDef(MK_MIMEHTML_CERTIFICATES_BAD,    XP_MSG_BASE + 1087,
       "Invalid Certificates")

ResDef (MK_MSG_NEW_NEWSGROUP, XP_MSG_BASE + 1088,
"New Newsgroup")
ResDef (MK_MSG_NEW_CATEGORY, XP_MSG_BASE + 1089,
"Creates a new category in this newsgroup")

ResDef(MK_ADDR_NO_EMAIL_ADDRESS,    XP_MSG_BASE + 1090,
       "There is no email address for %s.")

ResDef (MK_MSG_EXPIRE_COUNT,       XP_MSG_BASE + 1091, "Expired %ld messages")

ResDef (MK_MSG_DONE_EXPIRE_COUNT,      XP_MSG_BASE + 1092, "Expired %ld messages...Done")

/* Localized names of mail folders */
ResDef (MK_MSG_TRASH_L10N_NAME,  XP_MSG_BASE + 1093, "Trash")
ResDef (MK_MSG_INBOX_L10N_NAME,  XP_MSG_BASE + 1094, "Inbox")
#ifdef XP_WIN16
ResDef (MK_MSG_OUTBOX_L10N_NAME, XP_MSG_BASE + 1095, "Unsent")
#else
ResDef (MK_MSG_OUTBOX_L10N_NAME, XP_MSG_BASE + 1095, "Unsent Messages")
#endif
ResDef (MK_MSG_DRAFTS_L10N_NAME, XP_MSG_BASE + 1096, "Drafts")
ResDef (MK_MSG_SENT_L10N_NAME,   XP_MSG_BASE + 1097, "Sent")

/* Caption text for FE_PromptWithCaption */
ResDef (MK_MSG_NEW_FOLDER_CAPTION, XP_MSG_BASE + 1098, "New Folder")
ResDef (MK_MSG_RENAME_FOLDER_CAPTION, XP_MSG_BASE + 1099, "Rename Folder")

ResDef (MK_MSG_MANAGE_MAIL_ACCOUNT, XP_MSG_BASE + 1100, "Manage Mail Account")

ResDef (MK_MSG_UNABLE_MANAGE_MAIL_ACCOUNT, XP_MSG_BASE+1101,
MOZ_NAME_BRAND" is unable to manage your mail account.\n\
Please contact your mail account administrator\n\
or try again later.")

ResDef (MK_MSG_MODERATE_NEWSGROUP, XP_MSG_BASE + 1102,
"Manage Newsgroups")

ResDef(MK_UNABLE_TO_LOCATE_SOCKS_HOST, XP_MSG_BASE + 1103,
MOZ_NAME_BRAND" is unable to locate the socks server:\n\
  %.200s\n\
The server does not have a DNS entry.\n\n\
Check the socks server name in the proxy\n\
configuration and try again.")

ResDef (XP_SEC_PROMPT_NICKNAME_COLLISION,       (XP_MSG_BASE + 1104),
  "An object with that nickname exists.  Please enter a new nickname.")

ResDef(MK_MSG_GET_NEW_DISCUSSION_MSGS, XP_MSG_BASE + 1105, "Get New News Articles")

ResDef(MK_MSG_LOCAL_MAIL, XP_MSG_BASE + 1106, "Local Mail")
ResDef(MK_NEWS_DISCUSSIONS_ON, XP_MSG_BASE + 1107, "%s Newsgroups")

ResDef( XP_RECEIVING_MESSAGE_HEADERS_OF,    XP_MSG_BASE + 1108,
"%s Receiving: message headers %lu of %lu" )

ResDef( XP_RECEIVING_MESSAGE_FLAGS_OF,  XP_MSG_BASE + 1109,
"%s Receiving: message flags %lu of %lu" )

ResDef( XP_IMAP_DELETING_MESSAGES, XP_MSG_BASE + 1110, "Deleting messages...")
ResDef( XP_IMAP_DELETING_MESSAGE, XP_MSG_BASE + 1111, "Deleting message...")
ResDef( XP_IMAP_MOVING_MESSAGES_TO, XP_MSG_BASE + 1112, "Moving messages to %s...")
ResDef( XP_IMAP_MOVING_MESSAGE_TO, XP_MSG_BASE + 1113, "Moving message to %s...")
ResDef( XP_IMAP_COPYING_MESSAGES_TO, XP_MSG_BASE + 1114, "Copying messages %s...")
ResDef( XP_IMAP_COPYING_MESSAGE_TO, XP_MSG_BASE + 1115, "Copying message %s...")
ResDef( XP_IMAP_SELECTING_MAILBOX, XP_MSG_BASE + 1116, "Opening folder %s...")

#ifndef MOZ_COMMUNICATOR_NAME
ResDef(MK_MSG_CONFIRM_MOVE_MAGIC_FOLDER, XP_MSG_BASE + 1117,
"Are you sure you want to move %s away from its default \n\
location? Next time "MOZ_NAME_PRODUCT" runs, a new %s folder \n\
will be created in the default location")
#else
ResDef(MK_MSG_CONFIRM_MOVE_MAGIC_FOLDER, XP_MSG_BASE + 1117,
"Are you sure you want to move %s away from its default \n\
location? Next time Communicator runs, a new %s folder \n\
will be created in the default location")
#endif

ResDef(MK_MSG_UPDATE_MSG_COUNTS, XP_MSG_BASE + 1118, "Update Message Counts")

ResDef(MK_MSG_DELETING_MSGS_STATUS, XP_MSG_BASE + 1119, "Deleting %lu of %lu messages")
ResDef(MK_MSG_COPYING_MSGS_STATUS, XP_MSG_BASE + 1120, "Copying %lu of %lu messages to %s")
ResDef(MK_MSG_MOVING_MSGS_STATUS, XP_MSG_BASE + 1121, "Moving %lu of %lu messages to %s")

ResDef(XP_EDT_PUBLISH_REPORT_ONE_FILE, XP_MSG_BASE + 1122, 
"Your file was uploaded successfully.")
ResDef(XP_EDT_PUBLISH_REPORT_MSG, XP_MSG_BASE + 1123, 
"%d files were uploaded successfully.")

ResDef(MK_MSG_DELIVERING_MESSAGE_TO, XP_MSG_BASE + 1124, "Mail: delivering message %ld to %s...")
ResDef(MK_MSG_DELIVERING_MESSAGE, XP_MSG_BASE + 1125, "Mail: delivering message %ld...")

ResDef(MK_MSG_FORWARDING_ENCRYPTED_WARNING, XP_MSG_BASE + 1126,
       "You are doing an unencrypted Forward of a message which was\n\
encrypted when you received it.  Sending this message unencrypted\n\
may reduce the level of privacy of the attached message.\n\
\n\
Send unencrypted anyway?")

ResDef(XP_EDT_BROWSE_TO_DEFAULT, XP_MSG_BASE + 1127, 
"\n\nBrowse to your default publishing location now?")

ResDef (XP_EDT_BAD_CLIPBOARD_VERSION, XP_MSG_BASE + 1128,
  "You can not copy and paste between different\n\
versions of "MOZ_NAME_PRODUCT" Composer or "MOZ_NAME_PRODUCT" Gold.")
ResDef (XP_EDT_BAD_CLIPBOARD_ENCODING, XP_MSG_BASE + 1129,
  "You can not copy and paste between windows with\n\
different character set encodings.")

ResDef(XP_ALERT_NFS_USEHTTP,            XP_MSG_BASE+1130,   
"NFS URLs not internally supported, use an HTTP proxy server: ")

ResDef(MK_MSG_MARK_CATEGORY_READ,       XP_MSG_BASE+1131, "Mark Category Read")

ResDef(MK_MSG_TRACK_FOLDER_MOVE, XP_MSG_BASE + 1132, 
"Change rule to reflect new folder location?")
ResDef(MK_MSG_TRACK_FOLDER_DEL, XP_MSG_BASE + 1133, 
"Disable filter rule for this folder?")

/* Webfonts strings from modules/libfont */

ResDef(WF_MSG_ABOUT_TITLE, XP_MSG_BASE + 1134,
"Dynamic Fonts")
ResDef(WF_MSG_ABOUT_BEGIN_1, XP_MSG_BASE + 1135,
"<H2>Installed Font Displayers<HR WIDTH=100%></H2>\n\
 <FONT SIZE=+0>\n\
 The font displayers that you have installed and the font formats\n\
 supported by each, are listed below.")
 ResDef(WF_MSG_ABOUT_BEGIN_2, XP_MSG_BASE + 1136,
 "Use the radioboxes to enable or disable a particular format for a\n\
 particular displayer.<P>")
 ResDef(WF_MSG_ABOUT_BEGIN_3, XP_MSG_BASE + 1137,
 "For more information on "MOZ_NAME_PRODUCT" Dynamic Fonts, click\
 <a href=http://home.netscape.com/comprod/products/communicator/version_4.0/dynfonts\
  target=aboutDynamicFonts>here</a><hr>")
ResDef(WF_MSG_ABOUT_END, XP_MSG_BASE + 1138,
"</FONT>")
ResDef(WF_MSG_ABOUT_DISPLAYER_DYNAMIC, XP_MSG_BASE + 1139,
"<TABLE BORDER WIDTH=99%%><TR>\n\
 <TD COLSPAN=3><B>%s</B><BR>%s<BR>Located at: %s<BR><BR></TD>\n\
 </TR>")
ResDef(WF_MSG_ABOUT_DISPLAYER_STATIC, XP_MSG_BASE + 1140,
"<TABLE BORDER WIDTH=99%%><TR>\n\
 <TD COLSPAN=3><B>%s</B><BR>%s<BR><BR></TD>\n\
 </TR>")
ResDef(WF_MSG_ABOUT_DISPLAYER_MIME, XP_MSG_BASE + 1141,
"<TR>\n\
 <TD WIDTH=33%%><INPUT NAME=\042%s\042 TYPE=Radio VALUE=\042%s\042 %s><B>%s</B></TD>\n\
 <TD WIDTH=50%%>%s</TD>\n\
 <TD>%s</TD>\n\
 </TR>")
ResDef(WF_MSG_ABOUT_DISPLAYER_END, XP_MSG_BASE + 1142,
"</TABLE>\n\
 <BR>\n\
 <BR>")
ResDef(WF_MSG_ABOUT_NO_DISPLAYER, XP_MSG_BASE + 1143,
"<B>No Displayers Installed</B>")
ResDef(WF_MSG_NATIVE_DISPLAYER_NAME, XP_MSG_BASE + 1144,
MOZ_NAME_BRAND" Default Font Displayer")
ResDef(WF_MSG_NATIVE_DISPLAYER_DESCRIPTION, XP_MSG_BASE + 1145,
"This font displayer handles fonts that are installed on the system.")

/* More messenger strings */
ResDef (MK_MSG_OUTBOX_L10N_NAME_OLD, XP_MSG_BASE + 1146, "Outbox")
ResDef(MK_MSG_BOGUS_QUEUE_MSG_1_OLD,    XP_MSG_BASE + 1147,
       "The `Outbox' folder contains a message which is not\n\
scheduled for delivery!")
ResDef(MK_MSG_BOGUS_QUEUE_MSG_N_OLD,    XP_MSG_BASE + 1148,
       "The `Outbox' folder contains %d messages which are not\n\
scheduled for delivery!")
ResDef(MK_MSG_BOGUS_QUEUE_REASON_OLD,   XP_MSG_BASE + 1149,
       "\n\nThis probably means that some program other than\n\
"MOZ_NAME_PRODUCT" has added messages to this folder.\n")
ResDef(MK_MSG_WHY_QUEUE_SPECIAL_OLD,    XP_MSG_BASE + 1150,
       "The `Outbox' folder is special; it is only for storing\n\
messages to be sent later.")

ResDef(XP_AUTOADMIN_MISSING,    XP_MSG_BASE + 1151,
 "The local configuration file specified a configuration URL but the AutoAdmin component could not \
 be loaded.  AutoAdmin is required to support remote configuration URLs.  You will be unable to load\
 any remote documents." )

 ResDef(MK_MSG_ADDING_LDAP_TO_AB, XP_MSG_BASE + 1152,
 "Adding to %s: %ld of %ld")

 ResDef(MK_MSG_XSENDER_INTERNAL, XP_MSG_BASE + 1153,
 "<B><FONT COLOR=\042#808080\042>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Internal</FONT></B>")

ResDef(XP_PKCS12_IMPORT_FILE_PROMPT,        (XP_MSG_BASE + 1154),
"File Name to Import")

ResDef(XP_PKCS12_EXPORT_FILE_PROMPT,        (XP_MSG_BASE + 1155),
"File Name to Export")

ResDef(XP_EDT_CP_DOCUMENT_TOO_LARGE_READ, XP_MSG_BASE + 1156,
 "This document is %ld bytes long. That is too large to be read by a Composer Plug-in. \
The maximum allowed size is %ld bytes.")

ResDef(XP_EDT_CP_DOCUMENT_TOO_LARGE_WRITE, XP_MSG_BASE + 1157,
 "The Composer Plug-in tried to create a document that is %ld bytes long. \
The maximum allowed size is %ld bytes.")

ResDef(MK_MIME_SMIME_ENCRYPTED_CONTENT_DESCRIPTION, XP_MSG_BASE + 1158,
 "S/MIME Encrypted Message")

ResDef(MK_MIME_SMIME_SIGNATURE_CONTENT_DESCRIPTION, XP_MSG_BASE + 1159,
 "S/MIME Cryptographic Signature")

/* Netcaster Strings */

ResDef(XP_ALERT_CANT_RUN_NETCASTER, XP_MSG_BASE + 1160,
MOZ_NAME_BRAND" was unable to start Netcaster.\n\
Make sure Netcaster is installed correctly.")


ResDef(XP_EDT_CANT_EDIT_URL, XP_MSG_BASE + 1161,
 "Composer can't open the URL '%s'.\n\n\
You must enter an absolute URL or an absolute pathname.\n\
e.g. 'http://mysystem.com/mydoc.html'")

ResDef(XP_SA_ALG_AND_BITS_FORMAT, XP_MSG_BASE + 1162, "%d-bit %s")

ResDef(MK_MSG_IMAP_MULTIPLE_SELECT_FAILED,  XP_MSG_BASE + 1163,
"Only one operation at a time on this folder is permitted. \
\nPlease wait until the other operation completes and try again.")

ResDef(MK_MSG_CONFIRM_MOVE_FOLDER_TO_TRASH, XP_MSG_BASE + 1164,
"Are you sure you want to move the selected folders into the Trash?")

ResDef(XP_ALERT_NETCASTER_NO_JS, XP_MSG_BASE + 1165,
MOZ_NAME_BRAND" is unable to start Netcaster because Java and/or JavaScript are not enabled. \n\
Please verify that your Advanced Preferences are set correctly and try again.")


ResDef(XP_EDT_ADD_COLUMNS_STATUS, XP_MSG_BASE + 1170,
       "Add %d column(s) to the table")
ResDef(XP_EDT_ADD_ROWS_STATUS, XP_MSG_BASE + 1171,
       "Add %d rows(s) to the table")
ResDef(XP_EDT_PERCENT_CELL, XP_MSG_BASE + 1172,
       "% of parent cell  ")
ResDef(XP_EDT_PERCENT_TABLE, XP_MSG_BASE + 1173,
       "% of table  ")
ResDef(XP_EDT_SEL_TABLE, XP_MSG_BASE + 1174,
       "Click to select table")
ResDef(XP_EDT_SEL_TABLE_EXTRA, XP_MSG_BASE + 1175,
       " (Also press Ctrl to select all cells instead)")
ResDef(XP_EDT_SEL_COL, XP_MSG_BASE + 1176,
       "Click to select all cells in column")
ResDef(XP_EDT_SEL_ROW, XP_MSG_BASE + 1177,
       "Click to select all cells in row")
ResDef(XP_EDT_SEL_CELL, XP_MSG_BASE + 1178,
       "Click to select or deselect cell")
ResDef(XP_EDT_SEL_ALL_CELLS, XP_MSG_BASE + 1179,
       "Click to select all cells in table")
ResDef(XP_EDT_SIZE_TABLE_WIDTH, XP_MSG_BASE + 1180,
       "Drag to change width of table")
ResDef(XP_EDT_SIZE_TABLE_HEIGHT, XP_MSG_BASE + 1181,
       "Drag to change height of table")
ResDef(XP_EDT_SIZE_COL, XP_MSG_BASE + 1182,
       "Drag to change width of column to the left")
ResDef(XP_EDT_SIZE_ROW, XP_MSG_BASE + 1183,
       "Drag to change height of row above")
ResDef(XP_EDT_ADD_ROWS, XP_MSG_BASE + 1184,
       "Drag down to add more rows to the table")
ResDef(XP_EDT_ADD_COLS, XP_MSG_BASE + 1185,
       "Drag right to add more columns to the table")
ResDef(XP_EDT_DRAG_TABLE, XP_MSG_BASE + 1186,
       "Relocate caret or drag selected table or cells")
ResDef(XP_EDT_NOT_ALL_CELLS_PASTED, XP_MSG_BASE + 1187,
       "Not all cells in source were pasted\n")
ResDef(XP_EDT_CLICK_TO_SELECT_IMAGE, XP_MSG_BASE + 1188,
       "Click to select the image before you can drag it")
ResDef(XP_EDT_CLICK_AND_DRAG_IMAGE, XP_MSG_BASE + 1189,
       "Click mouse button down and drag to move or copy image")
ResDef(XP_EDT_SEL_LINE, XP_MSG_BASE + 1190,
       "Click to select the entire line to the right")
ResDef(XP_EDT_SIZE_WIDTH, XP_MSG_BASE + 1191,
       "Drag to change the width of object")
ResDef(XP_EDT_SIZE_HEIGHT, XP_MSG_BASE + 1192,
       "Drag to change the height of object")
ResDef(XP_EDT_SIZE_CORNER, XP_MSG_BASE + 1193,
       "Drag to change width and height of object [Press Ctrl to not constrain aspect ratio]")

ResDef(XP_EDT_CHARSET_LABEL, (XP_MSG_BASE + 1200),
"The character set label of this page is '%s'.")
ResDef(XP_EDT_CHARSET_CANT_EDIT, (XP_MSG_BASE + 1201),
"\nCommunicator does not recognize this and cannot edit it.")
ResDef(XP_EDT_CURRENT_CHARSET, (XP_MSG_BASE + 1202),
"\nYour current character set is '%s'.")
ResDef(XP_EDT_CHARSET_EDIT_REPLACE, (XP_MSG_BASE + 1203),
"\n\nSelect OK to edit the page and change the character set label to '%s'.")
ResDef(XP_EDT_CHARSET_EDIT_CANCEL, (XP_MSG_BASE + 1204),
"\nSelect Cancel to abort editing this page.")
ResDef(XP_EDT_CHARSET_EDIT_NOREPLACE, (XP_MSG_BASE + 1205),
"\nSelect Cancel to edit the page and keep '%s'.")
ResDef(XP_EDT_CHARSET_EDIT_SUGGESTED, (XP_MSG_BASE + 1206),
"\nThe suggested character set to use is '%s'.")
ResDef(XP_EDT_CHARSET_CONVERT_PAGE, (XP_MSG_BASE + 1207),
"This may alter some of the characters in your page.\nYou cannot undo this action.")
ResDef(XP_EDT_CHARSET_SET_METATAG, (XP_MSG_BASE + 1208),
"This only changes the Content-Type information saved with the page.\nIt will not convert any characters in your page.")

/* We would like to reserve the range through 1250 for Editor
*/

ResDef(SU_NOT_A_JAR_FILE, XP_MSG_BASE + 1262,
       "SmartUpdate failed: Downloaded archive is not a JAR file.")

ResDef(SU_SECURITY_CHECK, XP_MSG_BASE + 1263,
       "SmartUpdate failed: JAR archive failed security check. %s.")

ResDef(SU_INSTALL_FILE_HEADER, XP_MSG_BASE + 1264,
       "SmartUpdate failed: JAR archive has no installer file information.")

ResDef(SU_INSTALL_FILE_MISSING, XP_MSG_BASE + 1265,
       "SmartUpdate failed: JAR archive is missing an installer file %s.")

ResDef(XP_GLOBAL_NO_CONFIG_RECEIVED_NO_FAILOVER, XP_MSG_BASE + 1266,
"No automatic configuration file was received.\n\n\
You will be unable to load and documents from the network.\n\
See your local system administrator for help." )

ResDef(XP_GLOBAL_BAD_TYPE_CONFIG_IGNORED,   XP_MSG_BASE + 1267,
 "The automatic configuration file is not of the correct type:\n\n\
        %s\n\n\
Expected the MIME type of application/x-javascript-config." )

ResDef(XP_GLOBAL_CONF_LOAD_FAILED_NO_FAILOVER, XP_MSG_BASE + 1268,
"The automatic configuration file could not be loaded.\n\n\
You will be unable to load any documents from the network.\n\
See your local system administrator for help." )

ResDef(XP_GLOBAL_NO_CONFIG_RECIEVED,    XP_MSG_BASE + 1269,
 "No automatic configuration file was received.\n\n\
Will default based on the last configuration." )

ResDef(XP_GLOBAL_EVEN_SAVED_IS_BAD, XP_MSG_BASE + 1270,
 "The backup automatic configuration file had errors.\n\n\
We will default to the standard configuration." )

ResDef(XP_GLOBAL_BAD_CONFIG_IGNORED,    XP_MSG_BASE + 1271,
 "The automatic configuration file has errors:\n\n        %s\n\n\
We will default to the standard configuration." )

ResDef(XP_GLOBAL_CONFIG_LOAD_ABORTED,   XP_MSG_BASE + 1272,
 "Automatic configuration load was cancelled.\n\n\
We will default to the standard configuration." )

ResDef(XP_PKCS12_SUCCESSFUL_EXPORT,     (XP_MSG_BASE + 1273),
"Your certificates have been successfully exported.")

ResDef(XP_PKCS12_SUCCESSFUL_IMPORT,     (XP_MSG_BASE + 1274),
"Your certificates have been successfully imported.")

ResDef(XP_SEC_ENTER_IMPORT_PWD,         (XP_MSG_BASE + 1275),
"Enter password protecting data to be imported:")

ResDef(MK_MSG_CONTINUE_ADDING,          (XP_MSG_BASE + 1276),
"Would you like to add the remaining addresses to the personal address book?")

ResDef(MK_LDAP_USER_CERTIFICATE, XP_MSG_BASE + 1277, "User Certificate")

ResDef(MK_LDAP_SMIME_USER_CERTIFICATE, XP_MSG_BASE + 1278, "User E-mail Certificate")

ResDef(MK_MSG_UNIQUE_TRASH_RENAME_FAILED, XP_MSG_BASE + 1279,
"The Trash already contains a folder named '%s.'\n\
Please either empty the trash or rename this folder.")

ResDef(MK_MSG_NEEDED_UNIQUE_TRASH_NAME, XP_MSG_BASE + 1280,
"The Trash already contained a folder named '%s.'\n\
The folder which you just deleted can be found in the Trash\n\
under the new name '%s.'")

ResDef(MK_MSG_IMAP_SERVER_SAID, XP_MSG_BASE + 1281,
"The current command did not succeed.  The IMAP server responded:\n%s")

ResDef(SU_INSTALL_ASK_FOR_DIRECTORY, XP_MSG_BASE + 1282,
       "Where would you like to install %s?")

ResDef(EDT_VIEW_SOURCE_WINDOW_TITLE, XP_MSG_BASE + 1283,
"View Document Source")

ResDef(LAY_PAGEINFO_NOINFO, XP_MSG_BASE + 1284,
"<H3>No info while document is loading</H3>\n")
ResDef(LAY_PAGEINFO_FRAME, XP_MSG_BASE + 1285,
"<LI>Frame: ")
ResDef(LAY_PAGEINFO_IMAGE, XP_MSG_BASE + 1286,
"Image:")
ResDef(LAY_PAGEINFO_EMBED, XP_MSG_BASE + 1287,
"Embed:")
ResDef(LAY_PAGEINFO_APPLET, XP_MSG_BASE + 1288,
"Applet:")
ResDef(LAY_PAGEINFO_BACKGROUND_IMAGE, XP_MSG_BASE + 1289,
"Background Image: ")
ResDef(LAY_PAGEINFO_ACTIONURL, XP_MSG_BASE + 1290,
"<LI>Action URL: ")
ResDef(LAY_PAGEINFO_ENCODING, XP_MSG_BASE + 1291,
"<LI>Encoding: ")
ResDef(LAY_PAGEINFO_METHOD, XP_MSG_BASE + 1292,
"<LI>Method: ")
ResDef(LAY_PAGEINFO_LAYER, XP_MSG_BASE + 1293,
"<LI>Layer: ")

ResDef(MK_MSG_MAC_PROMPT_UUENCODE, XP_MSG_BASE + 1294,
"Some of these attachments contain Macintosh specific information. \
Using UUENCODE will cause this information to be lost. \
Continue sending?")

ResDef(XP_SEC_REENTER_TO_CONFIRM_PWD,       XP_MSG_BASE + 1295,
"Re-enter the password to confirm it:")

ResDef(XP_SEC_BAD_CONFIRM_EXPORT_PWD,       XP_MSG_BASE + 1296,
"The passwords entered did not match.  Enter\n\
the password to protect data being exported:")

ResDef(MK_IMAP_STATUS_CREATING_MAILBOX, XP_MSG_BASE + 1297,
"Creating folder...")

ResDef(MK_IMAP_STATUS_SELECTING_MAILBOX, XP_MSG_BASE + 1298,
"Opening folder...")

ResDef(MK_IMAP_STATUS_DELETING_MAILBOX, XP_MSG_BASE + 1299,
"Deleting folder %s...")

ResDef(MK_IMAP_STATUS_RENAMING_MAILBOX, XP_MSG_BASE + 1300,
"Renaming folder %s...")

ResDef(MK_IMAP_STATUS_LOOKING_FOR_MAILBOX, XP_MSG_BASE + 1301,
"Looking for folders...")

ResDef(MK_IMAP_STATUS_SUBSCRIBE_TO_MAILBOX, XP_MSG_BASE + 1302,
"Subscribing to folder %s...")

ResDef(MK_IMAP_STATUS_UNSUBSCRIBE_MAILBOX, XP_MSG_BASE + 1303,
"Unsubscribing from folder %s...")

ResDef(MK_IMAP_STATUS_SEARCH_MAILBOX, XP_MSG_BASE + 1304,
"Searching folder...")

ResDef(MK_IMAP_STATUS_MSG_INFO, XP_MSG_BASE + 1305,
"Getting message info...")

ResDef(MK_IMAP_STATUS_CLOSE_MAILBOX, XP_MSG_BASE + 1306,
"Closing folder...")

ResDef(MK_IMAP_STATUS_EXPUNGING_MAILBOX, XP_MSG_BASE + 1307,
"Compacting folder...")

ResDef(MK_IMAP_STATUS_LOGGING_OUT, XP_MSG_BASE + 1308,
"Logging out...")

ResDef(MK_IMAP_STATUS_CHECK_COMPAT, XP_MSG_BASE + 1309,
"Checking IMAP server capability...")

ResDef(MK_IMAP_STATUS_SENDING_LOGIN, XP_MSG_BASE + 1310,
"Sending login information...")

ResDef(MK_IMAP_STATUS_SENDING_AUTH_LOGIN, XP_MSG_BASE + 1311,
"Sending authenticate login information...")

ResDef(SU_NEED_TO_REBOOT, XP_MSG_BASE + 1312,
    "SmartUpdate is not complete until you reboot Windows")

ResDef(MK_MSG_REPLY_TO_SENDER, XP_MSG_BASE + 1313,
    "to Sender")

ResDef(MK_ADDR_DEFAULT_EXPORT_FILENAME, XP_MSG_BASE + 1314,
    "untitled")

ResDef(MK_MSG_ADVANCE_TO_NEXT_FOLDER, XP_MSG_BASE + 1315,
    "Advance to next unread message in %s?")

ResDef(MK_PORT_ACCESS_NOT_ALLOWED, XP_MSG_BASE + 1316,
    "Sorry, access to the port number given\n\
has been disabled for security reasons")

ResDef(XP_PRIORITY_LOWEST, XP_MSG_BASE + 1317, "Lowest")
ResDef(XP_PRIORITY_LOW, XP_MSG_BASE + 1318, "Low")
ResDef(XP_PRIORITY_NORMAL, XP_MSG_BASE + 1319, "Normal")
ResDef(XP_PRIORITY_HIGH, XP_MSG_BASE + 1320, "High")
ResDef(XP_PRIORITY_HIGHEST, XP_MSG_BASE + 1321, "Highest")
ResDef(XP_PRIORITY_NONE, XP_MSG_BASE + 1322, "NONE")

ResDef(XP_PROGRESS_READ_NEWSGROUP_COUNTS, XP_MSG_BASE + 1323, 
       "Receiving: message totals: %lu of %lu")

ResDef(MK_LDAP_AUTH_PROMPT, XP_MSG_BASE + 1324,
       "Please enter your %s and password for access to %s")

ResDef( XP_FOLDER_RECEIVING_MESSAGE_OF, XP_MSG_BASE + 1325,
"%s - Receiving: message %lu of %lu" )

ResDef(MK_MSG_COLLABRA_DISABLED, XP_MSG_BASE + 1326,
       "Sorry, Collabra has been disabled;  newsgroup functionality \
has been turned off.")

ResDef(MK_IMAP_DOWNLOADING_MESSAGE, XP_MSG_BASE + 1327,
       "Downloading message...")

ResDef(MK_IMAP_CREATE_FOLDER_BUT_NO_SUBSCRIBE, XP_MSG_BASE + 1328,
       "Folder creation succeeded, but "MOZ_NAME_PRODUCT" was unable to subscribe \
the new folder.")

ResDef(MK_IMAP_DELETE_FOLDER_BUT_NO_UNSUBSCRIBE, XP_MSG_BASE + 1329,
       "Folder deletion succeeded, but "MOZ_NAME_PRODUCT" was unable to unsubscribe \
from the folder.")

ResDef(MK_IMAP_RENAME_FOLDER_BUT_NO_SUBSCRIBE, XP_MSG_BASE + 1330,
       "Folder move succeeded, but "MOZ_NAME_PRODUCT" was unable to subscribe \
to the new folder name.")

ResDef(MK_IMAP_RENAME_FOLDER_BUT_NO_UNSUBSCRIBE, XP_MSG_BASE + 1331,
       "Folder move succeeded, but "MOZ_NAME_PRODUCT" was unable to unsubscribe \
from the old folder name.")

ResDef(XP_MSG_JS_CLOSE_WINDOW, XP_MSG_BASE + 1332, "Close Window?")

ResDef(XP_MSG_JS_CLOSE_WINDOW_NAME, XP_MSG_BASE + 1333, "Close Window %s?")

ResDef(MK_ACCESS_YOUR_COOKIES, XP_MSG_BASE + 1334,
       "Your Cookies")

ResDef(MK_ACCESS_NAME, XP_MSG_BASE + 1340,
       "Name:")
ResDef(MK_ACCESS_VALUE, XP_MSG_BASE + 1341,
       "Value:")
ResDef(MK_ACCESS_HOST, XP_MSG_BASE + 1342,
       "Host:")

ResDef(MK_ACCESS_SECURE, XP_MSG_BASE + 1348,
       "Secure:")
ResDef(MK_ACCESS_EXPIRES, XP_MSG_BASE + 1349,
       "Expires:")
ResDef(MK_ACCESS_END_OF_SESSION, XP_MSG_BASE + 1350,
       "at end of session")
ResDef(MK_LDAP_HTML_TITLE, XP_MSG_BASE + 1351,
       "LDAP Search Results")
ResDef(MK_ACCESS_JAVASCRIPT_COOKIE_FILTER, XP_MSG_BASE + 1352,
       "JavaScript Cookie Filter Message:\n")
ResDef(MK_JSFILTERDIALOG_EDIT_TITLE, XP_MSG_BASE + 1353,
       "Edit JavaScript Message Filter")
ResDef(MK_JSFILTERDIALOG_NEW_TITLE, XP_MSG_BASE + 1354,
       "New JavaScript Message Filter")
ResDef(MK_JSFILTERDIALOG_STRING, XP_MSG_BASE + 1355,
"<form name=jsfilterdlg_form action=internal-panel-handler method=post> \
<table width=\042100%%\042> \
<tr><td colspan=2 bgcolor=\042#000000\042> \
    <font color=\042#ffffff\042>JavaScript Message Filter</font> \
%-cont-%")
ResDef(MK_JSFILTERDIALOG_STRING_2, XP_MSG_BASE + 1356,
"<tr><td align=right>Filter Name: \n\
     <td><input name=\042filter_name\042 type=text size=30 value=\042%0%\042> \
%-cont-%")
ResDef(MK_JSFILTERDIALOG_STRING_3, XP_MSG_BASE + 1357,
"<tr><td align=right>JavaScript Function: \n\
     <td><input name=\042filter_script\042 type=text size=30 value=\042%1%\042> \
%-cont-%")
ResDef(MK_JSFILTER_DIALOG_STRING_4, XP_MSG_BASE + 1358,
"<tr><td align=right>Description: \n\
     <td><input name=\042filter_desc\042 type=text size=30 value=\042%2%\042> \
%-cont-%")
ResDef(MK_JSFILTER_DIALOG_STRING_5, XP_MSG_BASE + 1359,
"<tr><td align=right>Filter is \n\
     <td><input type=radio name=enabled value=on %3%>on<input type=radio name=enabled value=off %4%>off \
</table></form>")
ResDef(MK_JSFILTER_DIALOG_STRING_6, XP_MSG_BASE + 1360, "")
ResDef(MK_JSFILTER_DIALOG_STRING_7, XP_MSG_BASE + 1361, "")
ResDef(MK_JSFILTER_DIALOG_STRING_8, XP_MSG_BASE + 1362, "")
ResDef(MK_JSFILTER_DIALOG_STRING_9, XP_MSG_BASE + 1363, "")
ResDef(MK_JSFILTER_DIALOG_STRING_10, XP_MSG_BASE + 1364, "")
ResDef(MK_MSG_RETRIEVE_SELECTED, XP_MSG_BASE + 1365,
       "Retrieve selected messages")
ResDef(MK_MSG_RETRIEVE_FLAGGED, XP_MSG_BASE + 1366,
       "Retrieve flagged messages")

ResDef(MK_IMAP_STATUS_GETTING_NAMESPACE, XP_MSG_BASE + 1367,
       "Checking IMAP Namespace...")

ResDef(MK_LDAP_REPL_CHANGELOG_BOGUS, XP_MSG_BASE + 1368,
       "Can't replicate because the server's change log appears incomplete.")
ResDef(MK_LDAP_REPL_DSE_BOGUS, XP_MSG_BASE + 1369, "Can't replicate because the server's replication information appears incomplete.")
ResDef(MK_LDAP_REPL_CANT_SYNC_REPLICA, XP_MSG_BASE + 1370,"Can't replicate at this time. "MOZ_NAME_BRAND" may be out of memory or busy.")
	   
ResDef(MK_ADD_ADDRESSBOOK, XP_MSG_BASE + 1371,
       "New Address Book...")
ResDef(MK_ADD_LDAPDIRECTORY, XP_MSG_BASE + 1372,
       "New Directory...")
#ifdef XP_WIN16
ResDef (MK_MSG_TEMPLATES_L10N_NAME,  XP_MSG_BASE + 1373, "Template")
#else
ResDef (MK_MSG_TEMPLATES_L10N_NAME,  XP_MSG_BASE + 1373, "Templates")
#endif
ResDef (MK_MSG_SAVING_AS_DRAFT, XP_MSG_BASE + 1374, "Saving as draft ...")
ResDef (MK_MSG_SAVING_AS_TEMPLATE, XP_MSG_BASE + 1375, "Saving as template ...")

ResDef(MK_MSG_ADDBOOK_MOUSEOVER_TEXT, XP_MSG_BASE + 1376,
       "Add %s to your Address Book")
ResDef(MK_MSG_ENTER_NAME_FOR_TEMPLATE, XP_MSG_BASE + 1377,
       "Enter a name for your new message template.")

ResDef(MK_MSG_MDN_DISPLAYED, XP_MSG_BASE + 1378,
MOZ_NAME_BRAND" Messenger has displayed the message. There is no guarantee \
that the content has been read or understood.") 

ResDef(MK_MSG_MDN_DISPATCHED, XP_MSG_BASE + 1379,
"The message has been sent somewhere in some manner (e.g., printed, faxed, forwarded) \
without being displayed to the person you sent it to. They may or may not \
see it later.")

ResDef(MK_MSG_MDN_PROCESSED, XP_MSG_BASE + 1380,
"The message has been processed in some manner (i.e., by some sort of rules or \
server) without being displayed to the person you sent it to. They may or may not see it \
later. There may not even be a human user associated the mailbox.")

ResDef(MK_MSG_MDN_DELETED, XP_MSG_BASE + 1381,
"The message has been deleted. The person you sent it to may or may not have seen it. \
They might \042undelete\042 it at a later time and read it.") 

ResDef(MK_MSG_MDN_DENIED, XP_MSG_BASE + 1382,
"The recipient of the message does not wish to send a return receipt back to \
you.") 

ResDef(MK_MSG_MDN_FAILED, XP_MSG_BASE + 1383,
"A failure occurred. A proper return receipt could not be generated or sent to \
you.")

ResDef(MK_MSG_MDN_WISH_TO_SEND, XP_MSG_BASE + 1384,
"The sender of the message requested a receipt to be returned. \n\
Do you wish to send one?")

ResDef(MK_MSG_DELIV_IMAP,           XP_MSG_BASE + 1385,
 "Delivering message...")
ResDef(MK_MSG_DELIV_IMAP_DONE,          XP_MSG_BASE + 1386,
 "Delivering message... Done")
ResDef(MK_MSG_IMAP_DISCOVERING_MAILBOX, XP_MSG_BASE + 1387, 
"Found folder: %s")

ResDef(MK_MSG_FORWARD_INLINE, XP_MSG_BASE + 1388, "Forward Inline")

/**** 1389 free for grab ***/

ResDef(MK_MSG_IMAP_INBOX_NAME, XP_MSG_BASE + 1390, "Inbox")

ResDef(MK_IMAP_UPGRADE_NO_PERSONAL_NAMESPACE, XP_MSG_BASE + 1391, "The IMAP server indicates that \
you have may not have any personal mail folders.\nPlease verify your subscriptions.")

ResDef(MK_IMAP_UPGRADE_TOO_MANY_FOLDERS, XP_MSG_BASE + 1392, "While trying to automatically subscribe, \
"MOZ_NAME_PRODUCT" has found a large number of IMAP folders.\nPlease select which folders you would like subscribed.")

ResDef(MK_IMAP_UPGRADE_PROMPT_USER, XP_MSG_BASE + 1393,
MOZ_NAME_PRODUCT" has detected that you have upgraded from a previous version of Communicator.\n\
You will need to choose which IMAP folders you want subscribed.")

ResDef(MK_IMAP_UPGRADE_PROMPT_USER_2, XP_MSG_BASE + 1394,
"Any folders which are left unsubscribed will not appear in your folder lists,\n\
but can be subscribed to later by choosing File / Subscribe.")

ResDef(MK_IMAP_UPGRADE_PROMPT_QUESTION, XP_MSG_BASE + 1395,
"Would you like "MOZ_NAME_PRODUCT" to try to automatically subscribe to all your folders?")

ResDef(MK_IMAP_UPGRADE_CUSTOM, XP_MSG_BASE + 1396,
"Please choose which folders you want subscribed in the Subscribe window...")

ResDef(MK_IMAP_UPGRADE_WAIT_WHILE_UPGRADE, XP_MSG_BASE + 1397,
"Please wait while "MOZ_NAME_PRODUCT" upgrades you to use IMAP subscriptions...")

ResDef(MK_IMAP_UPGRADE_SUCCESSFUL, XP_MSG_BASE + 1398,
"The upgrade was successful.")

ResDef(MK_POP3_ONLY_ONE, XP_MSG_BASE + 1399,
"You cannot add another server, because you are using POP.")

ResDef(MK_MSG_REMOVE_MAILHOST_CONFIRM, XP_MSG_BASE + 1400,
"Are you sure you want to delete this server?\n\
All mail downloaded from this server will be erased from your hard drive, and you cannot undo.")

ResDef(MK_IMAP_GETTING_ACL_FOR_FOLDER, XP_MSG_BASE + 1401,
"Getting folder ACL...")

ResDef(MK_MSG_EXPIRE_NEWS_ARTICLES, XP_MSG_BASE + 1402,
"Click here to remove all expired articles")

ResDef(MK_MDN_DISPLAYED_RECEIPT, XP_MSG_BASE + 1403,
"Return Receipt (displayed)")

ResDef(MK_MDN_DISPATCHED_RECEIPT, XP_MSG_BASE + 1404,
"Return Receipt (dispatched)")

ResDef(MK_MDN_PROCESSED_RECEIPT, XP_MSG_BASE + 1405,
"Return Receipt (processed)")

ResDef(MK_MDN_DELETED_RECEIPT, XP_MSG_BASE + 1406,
"Return Receipt (deleted)")

ResDef(MK_MDN_DENIED_RECEIPT, XP_MSG_BASE + 1407,
"Return Receipt (denied)")

ResDef(MK_MDN_FAILED_RECEIPT, XP_MSG_BASE + 1408,
"Return Receipt (failed)")

ResDef(MK_IMAP_GETTING_SERVER_INFO, XP_MSG_BASE + 1409,
"Getting Server Configuration Info...")

ResDef(MK_IMAP_GETTING_MAILBOX_INFO, XP_MSG_BASE + 1410,
"Getting Mailbox Configuration Info...")

ResDef(MK_IMAP_EMPTY_MIME_PART, XP_MSG_BASE + 1411,
"This body part will be downloaded on demand.")

ResDef(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, XP_MSG_BASE + 1412,
"IMAP Error: The message could not be saved due to an error.")

ResDef(MK_IMAP_NO_ONLINE_FOLDER, XP_MSG_BASE + 1413,
"IMAP Error: The online folder information could not be retrieved.")

ResDef(XP_MSG_IMAP_LOGIN_FAILED, XP_MSG_BASE + 1414, "Login failed.")

ResDef(XP_SEARCH_VALUE_REQUIRED, XP_MSG_BASE + 1415, 
       "Please enter some text to search for and try again.")

ResDef(MK_MIMEHTML_SIGNED_UNVERIFIED,       XP_MSG_BASE + 1416,
"Unverified Signature")


/* IMAP ACL Rights descriptions */
ResDef(XP_MSG_IMAP_ACL_FULL_RIGHTS, XP_MSG_BASE + 1417,
       "Full Control")
ResDef(XP_MSG_IMAP_ACL_LOOKUP_RIGHT, XP_MSG_BASE + 1418,
       "Lookup")
ResDef(XP_MSG_IMAP_ACL_READ_RIGHT, XP_MSG_BASE + 1419,
       "Read")
ResDef(XP_MSG_IMAP_ACL_SEEN_RIGHT, XP_MSG_BASE + 1420,
       "Set Read/Unread State")
ResDef(XP_MSG_IMAP_ACL_WRITE_RIGHT, XP_MSG_BASE + 1421,
       "Write")
ResDef(XP_MSG_IMAP_ACL_INSERT_RIGHT, XP_MSG_BASE + 1422,
       "Insert (Copy Into)")
ResDef(XP_MSG_IMAP_ACL_POST_RIGHT, XP_MSG_BASE + 1423,
       "Post")
ResDef(XP_MSG_IMAP_ACL_CREATE_RIGHT, XP_MSG_BASE + 1424,
       "Create Subfolder")
ResDef(XP_MSG_IMAP_ACL_DELETE_RIGHT, XP_MSG_BASE + 1425,
       "Delete Messages")
ResDef(XP_MSG_IMAP_ACL_ADMINISTER_RIGHT, XP_MSG_BASE + 1426,
       "Administer Folder")

ResDef(XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_NAME, XP_MSG_BASE + 1427,
       "Personal Folder")

ResDef(XP_MSG_IMAP_PERSONAL_SHARED_FOLDER_TYPE_NAME, XP_MSG_BASE + 1428,
       "Personal Folder")

ResDef(XP_MSG_IMAP_PUBLIC_FOLDER_TYPE_NAME, XP_MSG_BASE + 1429,
       "Public Folder")

ResDef(XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_NAME, XP_MSG_BASE + 1430,
       "Other User's Folder")

ResDef(XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_DESCRIPTION, XP_MSG_BASE + 1431,
       "This is a personal mail folder.  It is not shared.")

ResDef(XP_MSG_IMAP_PERSONAL_SHARED_FOLDER_TYPE_DESCRIPTION, XP_MSG_BASE + 1432,
       "This is a personal mail folder.  It has been shared.")

ResDef(XP_MSG_IMAP_PUBLIC_FOLDER_TYPE_DESCRIPTION, XP_MSG_BASE + 1433,
       "This is a public folder.")

ResDef(XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_DESCRIPTION, XP_MSG_BASE + 1434,
       "This is a mail folder shared by the user '%s'.")

ResDef(XP_MSG_IMAP_UNKNOWN_USER, XP_MSG_BASE + 1435, "Unknown")

ResDef(XP_MSG_CACHED_PASSWORD_NOT_MATCHED, XP_MSG_BASE + 1436, "Password entered doesn't match last password used with server.")

ResDef(XP_MSG_PASSWORD_FAILED, XP_MSG_BASE + 1437, "You have not entered a password or have exceeded the number of \
password attempts allowed.")

ResDef(MK_MSG_REMOVE_IMAP_HOST_CONFIRM,      XP_MSG_BASE + 1438,
       "Are you sure you want to remove the mail host %s?")

ResDef(MK_MIMEHTML_VERIFY_SIGNATURE,    XP_MSG_BASE + 1439,
       "Verify Signature")

ResDef(MK_MSG_SHOW_ATTACHMENT_PANE, XP_MSG_BASE + 1440,
       "Toggle Attachment Pane")

ResDef(MK_MIMEHTML_DOWNLOAD_STATUS_HEADER, XP_MSG_BASE + 1441,
       "Download Status")

ResDef(MK_MIMEHTML_DOWNLOAD_STATUS_NOT_DOWNLOADED, XP_MSG_BASE + 1442,
       "Not Downloaded Inline")

ResDef(MK_ACCESS_DOMAIN, XP_MSG_BASE + 1443,
           "Domain:")
ResDef(MK_ACCESS_PATH, XP_MSG_BASE + 1444,
           "Path:")
ResDef(MK_ACCESS_YES, XP_MSG_BASE + 1445,
           "Yes")
ResDef(MK_ACCESS_NO, XP_MSG_BASE + 1446,
           "No")

/* -------- Range up tp 1449 reserved for MSGLIB!! ----------------*/

/* the following PRVCY items are all strings used in html */
ResDef(PRVCY_HAS_A_POLICY, XP_MSG_BASE + 1450,
"This site has a <a href=\042javascript:Policy();\042);>privacy policy file</a> \
in which it states what it does with the information that you submit to it.\n")

ResDef(PRVCY_HAS_NO_POLICY, XP_MSG_BASE + 1451,
"We could not find a privacy policy file from this site stating what it does \
with the information you submit to it.\n")

ResDef(PRVCY_CANNOT_SET_COOKIES, XP_MSG_BASE + 1452,
"This site cannot store information (cookies) on your computer.\n")

ResDef(PRVCY_CAN_SET_COOKIES, XP_MSG_BASE + 1453,
"This site has permission to store information (cookies) on your computer.\n")

ResDef(PRVCY_NEEDS_PERMISSION_TO_SET_COOKIES, XP_MSG_BASE + 1454,
"This site needs to ask for permission before storing information (cookies) on your computer.\n")

ResDef(PRVCY_HAS_NOT_SET_COOKIES, XP_MSG_BASE + 1455,
"This site has not stored any information on your computer.\n")

ResDef(PRVCY_HAS_SET_COOKIES, XP_MSG_BASE + 1456,
"This site has stored some <a href=\042javascript:ViewCookies();\042);>information</a> \
on your computer.\n")

ResDef(PRVCY_POLICY_FILE_NAME, XP_MSG_BASE + 1457,
"privacy_policy.html")

/* Related Links XP Strings */

ResDef(XP_RL_ABOUT_RL, XP_MSG_BASE + 1460, "About Related Sites") 

ResDef(XP_RL_ENHANCED_LIST, XP_MSG_BASE + 1461, "Detailed List...") 

ResDef(XP_RL_FETCHING, XP_MSG_BASE + 1462, "Fetching Related Sites...") 

ResDef(XP_RL_UNAVAILABLE, XP_MSG_BASE + 1463, "No Related Sites Available")

ResDef(XP_RL_DISABLED, XP_MSG_BASE + 1464, "Related Sites Disabled")


/* LDAP Replication Strings */

ResDef(MK_LDAP_REPL_PROGRESS_TITLE, XP_MSG_BASE + 1465, "Replicating Directory")
ResDef(MK_LDAP_REPL_CONNECTING, XP_MSG_BASE + 1466, "Connecting to directory server...")
ResDef(MK_LDAP_REPL_CHANGE_ENTRY, XP_MSG_BASE + 1467, "Replicating change entry %d")
ResDef(MK_LDAP_REPL_NEW_ENTRY, XP_MSG_BASE + 1468, "Replicating entry %d")

ResDef(MK_LDAP_AUTHDN_LOOKUP_FAILED, XP_MSG_BASE + 1469, "Mail id invalid or not unique, cannot resolve to directory authorization entry.")


/* Cookies & Signons XP Strings */
ResDef(MK_ACCESS_COOKIES_WISHES_MODIFY, (XP_MSG_BASE + 1476),
"The site %1$s \nwants permission to modify an existing cookie.")

ResDef(MK_ACCESS_SITE_COOKIES_ACCEPTED, (XP_MSG_BASE + 1477),
"Cookies that current site stored on your system")

ResDef(MK_SIGNON_VIEW_SIGNONS, (XP_MSG_BASE + 1478),
"View saved sign-ons")

ResDef(MK_SIGNON_VIEW_REJECTS, (XP_MSG_BASE + 1479),
"View sign-ons that won't be saved")

ResDef(MK_ACCESS_COOKIES_WISHES0, (XP_MSG_BASE + 1480),
"The site %1$s \nwants permission to set a cookie.")

ResDef(MK_ACCESS_COOKIES_WISHES1, (XP_MSG_BASE + 1481),
"The site %1$s \nwants permission to set another cookie.\n\
You currently have a cookie from this site.")

ResDef(MK_ACCESS_COOKIES_WISHESN, (XP_MSG_BASE + 1482),
"The site %1$s \nwants permission to set another cookie.\n\
You currently have %2$d cookies from this site.")

ResDef(MK_ACCESS_COOKIES_REMEMBER, (XP_MSG_BASE + 1483),
"Remember this decision")

ResDef(MK_ACCESS_COOKIES_ACCEPTED, (XP_MSG_BASE + 1484),
"Cookies stored on your system")

ResDef(MK_ACCESS_COOKIES_PERMISSION, (XP_MSG_BASE + 1485),
"Sites that can(+) or cannot(-) store cookies")

ResDef(MK_ACCESS_VIEW_COOKIES, (XP_MSG_BASE + 1486),
"View stored cookies")

ResDef(MK_ACCESS_VIEW_SITES, (XP_MSG_BASE + 1487),
"View sites that can or cannot store cookies")

ResDef(MK_SIGNON_CHANGE, (XP_MSG_BASE + 1488),
"Select the user whose password is being changed.")

ResDef(MK_SIGNON_PASSWORDS_GENERATE, (XP_MSG_BASE + 1490), \
"********")
/* Note: above string used to say "generate" but at Rick Elliott's
 * suggestion it is being changed to be all asterisks.
 */
/*
 * This must be eight-characters long in all translations.  Alternate
 * words that it could translate into would be any word that conveys
 * the idea that the user is asking for a random password to be generated
 * for him, e.g., "random", "select", "chose-one-for-me", etc.  If none
 * are the right length, use a shorter one and include fill characters
 * such as "**pick**".
 */

ResDef(MK_SIGNON_PASSWORDS_REMEMBER, (XP_MSG_BASE + 1491), \
"Remember this as a new password for %1$s at %2$s?")

ResDef(MK_SIGNON_PASSWORDS_FETCH, (XP_MSG_BASE + 1492), \
"Fetch old password for %1$s at %2$s?")

ResDef(MK_SIGNON_YOUR_SIGNONS, (XP_MSG_BASE + 1493),
"Saved sign-ons")

ResDef(MK_SIGNON_YOUR_SIGNON_REJECTS, (XP_MSG_BASE + 1494),
"Sign-ons that won't be saved")

ResDef(MK_SIGNON_NOTIFICATION, (XP_MSG_BASE + 1495),
"For your convenience, the browser can remember your user names \
and passwords so that you won't have to re-type them when you \
return to a site.  ")

ResDef(MK_SIGNON_NOTIFICATION_1, (XP_MSG_BASE + 1496),
"Your passwords will be obscured before being \
saved on your hard drive.  Do you want this feature enabled?")

ResDef(MK_SIGNON_NAG, (XP_MSG_BASE + 1497),
"Do you want to save the user name and password for this form?")

ResDef(MK_SIGNON_REMEMBER, (XP_MSG_BASE + 1498),
"Never save when viewing this page in the future.")

ResDef(MK_SIGNON_SELECTUSER, (XP_MSG_BASE + 1499),
"Select a username to be entered on this form")


/* Location Independence XP Strings */
#define LI_MSG_BASE XP_MSG_BASE + 1500	/* <-- offset = 1500, we don't have much room left above */
ResDef(LI_DOWN_CONFLICT_1, LI_MSG_BASE+1,  "\
Local and server copies of %0% are in conflict. What would you like to do:</P><P>\
%-cont-%")

ResDef(LI_DOWN_CONFLICT_2, LI_MSG_BASE+2,  "\
<script>\
function Doit(value) {\
    document.theform.cmd.value = value;\
    document.theform.submit();\
}\
</script>\
%-cont-%")

ResDef(LI_DOWN_CONFLICT_3, LI_MSG_BASE+3,  "\
<input type=hidden name=cmd value=-1>\
<input type=hidden name=button value=0>\
<INPUT TYPE=button  VALUE=\042Download the file from the server\042 onclick=\042Doit(0);\042><P>\
%-cont-%")

ResDef(LI_DOWN_CONFLICT_4, LI_MSG_BASE+4,  "\
<INPUT TYPE=button  VALUE=\042Keep the local file\042 onclick=\042Doit(1);\042><P>\
%-cont-%")

ResDef(LI_DOWN_CONFLICT_5, LI_MSG_BASE+5,  "\
<INPUT TYPE=\042CHECKBOX\042 NAME=\042checkbox\042 VALUE=\0421\042>\
%-cont-%")

ResDef(LI_DOWN_CONFLICT_6, LI_MSG_BASE+6,  "\
<FONT SIZE=\0422\042>Do this for all the remaining files</FONT> \
")

ResDef(LI_DOWN_TITLE, LI_MSG_BASE+7, "\
Download conflict")

ResDef(LI_UP_CONFLICT_1, LI_MSG_BASE+8,  "\
Local and server copies of %0% are in conflict. What would you like to do:</P><P>\
%-cont-%")

ResDef(LI_UP_CONFLICT_2, LI_MSG_BASE+9,  "\
<script>\
function Doit(value) {\
    document.theform.cmd.value = value;\
    document.theform.submit();\
}\
</script>\
%-cont-%")

ResDef(LI_UP_CONFLICT_3, LI_MSG_BASE+10,  "\
<input type=hidden name=cmd value=-1>\
<input type=hidden name=button value=0>\
<INPUT TYPE=button  VALUE=\042Upload the local file to the server\042 onclick=\042Doit(0);\042><P>\
%-cont-%")

ResDef(LI_UP_CONFLICT_4, LI_MSG_BASE+11,  "\
<INPUT TYPE=button  VALUE=\042Keep the server file\042 onclick=\042Doit(1);\042><P>\
%-cont-%")

ResDef(LI_UP_CONFLICT_5, LI_MSG_BASE+12,  "\
<INPUT TYPE=\042CHECKBOX\042 NAME=\042checkbox\042 VALUE=\0421\042>\
<FONT SIZE=\0422\042>Do this for all the remaining files</FONT> \
")

ResDef(LI_UP_TITLE, LI_MSG_BASE+13, "\
Upload conflict")

ResDef(LI_INIT_LATER, LI_MSG_BASE+14, 
"Your new location independence preference\n\
will take effect the next time\n\
you restart Communicator.")

ResDef(LI_VERIFY_NOACCESS, LI_MSG_BASE+15, 
"An authorization error occurred,\n\
please try retyping your username and password.")

ResDef(LI_VERIFY_DNSFAIL, LI_MSG_BASE+16, 
"The LI server name specified does not exist,\n\
please check the spelling and try again.")

ResDef(LI_VERIFY_NETWORKERROR, LI_MSG_BASE+17,
"An unexpected network error occurred.\n\
Cannot connect to the LI server.")

/* Julian XP Strings */
#define JULIAN_MSG_BASE XP_MSG_BASE + 1550
ResDef(JULIAN_STRING_1, JULIAN_MSG_BASE+1,
"Put your Julian strings here.")

/* Cookie Viewer strings for the trust label processing 
 * NOTE: the purpose strings (_PUR ) consist of a series of
 * english phrases that are glued together based on the value
 * of the purpose setting in the trust label.  There can be
 * one or more of these phrases.  They are glued together in
 * a comma seperated list.
 * For example:
 * "It is used for completion of the current activity, 
 *   site administration, R&D, and personalization."
 */
/* #if defined( TRUST_LABELS ) */
#define TRUST_LABEL_BASE  XP_MSG_BASE + 1570
ResDef(MK_ACCESS_TL_PUR1, (TRUST_LABEL_BASE + 1),
"Furthermore, it may be used for %1$s.")

ResDef(MK_ACCESS_TL_PUR2, (TRUST_LABEL_BASE + 2),
"Furthermore, it may be used for %1$s and %2$s.")

ResDef(MK_ACCESS_TL_PUR3, (TRUST_LABEL_BASE + 3),
"Furthermore, it may be used for %1$s, %2$s and %3$s.")

ResDef(MK_ACCESS_TL_PUR4, (TRUST_LABEL_BASE + 4),
"Furthermore, it may be used for %1$s, %2$s, %3$s and %4$s.")

ResDef(MK_ACCESS_TL_PUR5, (TRUST_LABEL_BASE + 5),
"Furthermore, it may be used for %1$s, %2$s, %3$s, %4$s and %5$s.")

ResDef(MK_ACCESS_TL_PUR6, (TRUST_LABEL_BASE + 6),
"Furthermore, it may be used for %1$s, %2$s, %3$s, %4$s, %5$s and %6$s.")

/* One or more of these next PPHx strings are inserted into the preceding 
   PURx strings.
*/
ResDef(MK_ACCESS_TL_PPH0, (TRUST_LABEL_BASE + 7),
"the completion of your current activity")

ResDef(MK_ACCESS_TL_PPH1, (TRUST_LABEL_BASE + 8),
"site administration")

ResDef(MK_ACCESS_TL_PPH2, (TRUST_LABEL_BASE + 9),
"research and development")

ResDef(MK_ACCESS_TL_PPH3, (TRUST_LABEL_BASE + 10),
"personalization")

ResDef(MK_ACCESS_TL_PPH4, (TRUST_LABEL_BASE + 11),
"direct marketing of products and services")

ResDef(MK_ACCESS_TL_PPH5, (TRUST_LABEL_BASE + 12),
"other uses" )

ResDef(MK_ACCESS_TL_ID1, (TRUST_LABEL_BASE + 13),
"The site had declared that the information contained in this cookie is used to identify you." )

ResDef(MK_ACCESS_TL_ID0, (TRUST_LABEL_BASE + 14),
"The site had declared that the information contained in this cookie is not used to identify you." )

ResDef(MK_ACCESS_TL_BY, (TRUST_LABEL_BASE + 15),
"Contact %1$s to confirm these privacy practices.")   

ResDef(MK_ACCESS_TL_RECP1, (TRUST_LABEL_BASE + 16),
"It is %1$s." )

ResDef(MK_ACCESS_TL_RECP2, (TRUST_LABEL_BASE + 17),
"It is %1$s and %2$s." )

ResDef(MK_ACCESS_TL_RECP3, (TRUST_LABEL_BASE + 18),
"It is %1$s, %2$s and %3$s." )

/* One or more of these next 4 strings are inserted into the preceding 
   RECPx strings.
*/
ResDef(MK_ACCESS_TL_RPH0, (TRUST_LABEL_BASE + 19),
"used only by this site" )

ResDef(MK_ACCESS_TL_RPH1, (TRUST_LABEL_BASE + 20),
"shared with this site's partners who have the same privacy practices" )

ResDef(MK_ACCESS_TL_RPH2, (TRUST_LABEL_BASE + 21),
"shared with this site's partners who have different privacy practices" )

ResDef(MK_ACCESS_TL_RPH3, (TRUST_LABEL_BASE + 22),
"available to anybody" )

/* #endif */

/* RDF XP Strings */

#define RDF_MSG_BASE XP_MSG_BASE + 1700

ResDef(RDF_HTML_STR, RDF_MSG_BASE+1,  "\
<CENTER><TABLE ALIGN=center WIDTH=470 BORDER=0>%0%</TABLE></CENTER><P>%1%")

ResDef(RDF_HTML_STR_1, RDF_MSG_BASE+2,  "\
<TR><TD ALIGN=RIGHT><B>%s:</B></TD><TD><INPUT TYPE=TEXT NAME='%s' WIDTH=36 SIZE=36 VALUE='%s'></TD></TR>\n")

ResDef(RDF_HTML_STR_2, RDF_MSG_BASE+3,  "\
<TR><TD ALIGN=RIGHT><B>%s:</B></TD><TD><INPUT TYPE=PASSWORD NAME='%s' WIDTH=40 SIZE=40 VALUE='%s'></TD></TR>\n")

ResDef(RDF_HTML_STR_3, RDF_MSG_BASE+4,  "\
<TR><TD ALIGN=RIGHT><B>%s:</B></TD><TD>%s</TD></TR>\n")

ResDef(RDF_HTML_STR_4, RDF_MSG_BASE+5,  "\
<TR><TD><INPUT TYPE=CHECKBOX NAME='%s' VALUE='%s' %s> <B>%s</B></TD></TR><BR>\n")

ResDef(RDF_HTML_STR_5, RDF_MSG_BASE+6,  "\
<TR><TD ALIGN=RIGHT><B>%s:</B></TD><TD><TEXTAREA ROWS=3 COLS=24 NAME='%s'>%s</TEXTAREA></TD></TR>\n")

ResDef(RDF_HTML_WINDATE, RDF_MSG_BASE+7,  "%#m/%#d/%Y %#I:%M %p")
ResDef(RDF_HTML_MACDATE, RDF_MSG_BASE+8,  "%m/%d/%Y %I:%M %p")

ResDef(RDF_DATA_1, RDF_MSG_BASE+9,  "Untitled URL")
ResDef(RDF_DATA_2, RDF_MSG_BASE+10,  "Untitled Folder")
ResDef(RDF_DELETEFILE, RDF_MSG_BASE+11,  "Delete file '%s' ?")
ResDef(RDF_UNABLETODELETEFILE, RDF_MSG_BASE+12,  "File: '%s' could not be deleted.")
ResDef(RDF_DELETEFOLDER, RDF_MSG_BASE+13,  "Delete folder '%s' and its contents?")
ResDef(RDF_UNABLETODELETEFOLDER, RDF_MSG_BASE+14,  "Folder: '%s' could not be deleted.")
ResDef(RDF_NEWPASSWORD, RDF_MSG_BASE+15,  "New Password:")
ResDef(RDF_CONFIRMPASSWORD, RDF_MSG_BASE+16,  "Confirm New Password:")
ResDef(RDF_MISMATCHPASSWORD, RDF_MSG_BASE+17,  "Passwords did not match.")
ResDef(RDF_ENTERPASSWORD, RDF_MSG_BASE+18,  "Enter Password for '%s' ?")
ResDef(RDF_SITEMAPNAME, RDF_MSG_BASE+19,  "Sitemap")
ResDef(RDF_RELATEDLINKSNAME, RDF_MSG_BASE+20,  "Related Links")
ResDef(RDF_DEFAULTCOLUMNNAME, RDF_MSG_BASE+21,  "Name")
ResDef(RDF_NEWWORKSPACEPROMPT, RDF_MSG_BASE+22,  "New workspace name:")
ResDef(RDF_DELETEWORKSPACE, RDF_MSG_BASE+23,  "Delete workspace '%s' and its contents?")

ResDef(RDF_ADDITIONS_ALLOWED, RDF_MSG_BASE+24, "Prevent Additions")
ResDef(RDF_DELETION_ALLOWED, RDF_MSG_BASE+25, "Prevent Deletion")
ResDef(RDF_ICON_URL_LOCKED, RDF_MSG_BASE+26, "Lock Icon URL")
ResDef(RDF_NAME_LOCKED, RDF_MSG_BASE+27, "Lock Name")
ResDef(RDF_COPY_ALLOWED, RDF_MSG_BASE+28, "Prevent Copies")
ResDef(RDF_MOVE_ALLOWED, RDF_MSG_BASE+29, "Prevent Moves")
ResDef(RDF_WORKSPACE_POS_LOCKED, RDF_MSG_BASE+30, "Lock Workspace Position")

ResDef(RDF_WEEKOF, RDF_MSG_BASE+31, "Week of %d/%d/%d")
ResDef(RDF_WITHINLASTHOUR, RDF_MSG_BASE+32, "Within the last hour")
ResDef(RDF_TODAY, RDF_MSG_BASE+33, "Today")

#define RDF_DAY_BASE RDF_MSG_BASE + 34
ResDef(RDF_DAY_0, RDF_DAY_BASE+0,  "Sunday")
ResDef(RDF_DAY_1, RDF_DAY_BASE+1,  "Monday")
ResDef(RDF_DAY_2, RDF_DAY_BASE+2,  "Tuesday")
ResDef(RDF_DAY_3, RDF_DAY_BASE+3,  "Wednesday")
ResDef(RDF_DAY_4, RDF_DAY_BASE+4,  "Thursday")
ResDef(RDF_DAY_5, RDF_DAY_BASE+5,  "Friday")
ResDef(RDF_DAY_6, RDF_DAY_BASE+6,  "Saturday")

#define RDF_CMD_BASE RDF_MSG_BASE + 41
ResDef(RDF_CMD_0, RDF_CMD_BASE+0,  " ")
ResDef(RDF_CMD_1, RDF_CMD_BASE+1,  "Open or Close")
ResDef(RDF_CMD_2, RDF_CMD_BASE+2,  "Open File")
ResDef(RDF_CMD_3, RDF_CMD_BASE+3,  "Print")
ResDef(RDF_CMD_4, RDF_CMD_BASE+4,  "Open URL In New Window")
ResDef(RDF_CMD_5, RDF_CMD_BASE+5,  "Open URL in Composer")
ResDef(RDF_CMD_6, RDF_CMD_BASE+6,  "Open As Workspace")
ResDef(RDF_CMD_7, RDF_CMD_BASE+7,  "New Bookmark...")
ResDef(RDF_CMD_8, RDF_CMD_BASE+8,  "New Folder")
ResDef(RDF_CMD_9, RDF_CMD_BASE+9,  "New Separator")
ResDef(RDF_CMD_10, RDF_CMD_BASE+10,  "Make Alias")
ResDef(RDF_CMD_11, RDF_CMD_BASE+11,  "Add To Bookmarks")
ResDef(RDF_CMD_12, RDF_CMD_BASE+12,  "Save As...")
ResDef(RDF_CMD_13, RDF_CMD_BASE+13,  "Create Shortcut")
ResDef(RDF_CMD_14, RDF_CMD_BASE+14,  "Set Personal Toolbar Folder")
ResDef(RDF_CMD_15, RDF_CMD_BASE+15,  "Set Bookmark Menu")
ResDef(RDF_CMD_16, RDF_CMD_BASE+16,  "Set New Bookmark Folder")
ResDef(RDF_CMD_17, RDF_CMD_BASE+17,  "Cut")
ResDef(RDF_CMD_18, RDF_CMD_BASE+18,  "Copy")
ResDef(RDF_CMD_19, RDF_CMD_BASE+19,  "Paste")
ResDef(RDF_CMD_20, RDF_CMD_BASE+20,  "Delete File...")
ResDef(RDF_CMD_21, RDF_CMD_BASE+21,  "Delete Folder...")
ResDef(RDF_CMD_22, RDF_CMD_BASE+22,  "Reveal in Finder")
ResDef(RDF_CMD_23, RDF_CMD_BASE+23,  "Properties...")
ResDef(RDF_CMD_24, RDF_CMD_BASE+24,  "Rename Workspace")
ResDef(RDF_CMD_25, RDF_CMD_BASE+25,  "Delete Workspace...")
ResDef(RDF_CMD_26, RDF_CMD_BASE+26,  "Move Workspace Up")
ResDef(RDF_CMD_27, RDF_CMD_BASE+27,  "Move Workspace Down")
ResDef(RDF_CMD_28, RDF_CMD_BASE+28,  "Refresh")
ResDef(RDF_CMD_29, RDF_CMD_BASE+29,  "Export...")
ResDef(RDF_CMD_30, RDF_CMD_BASE+30,  "Remove as Bookmark Menu")
ResDef(RDF_CMD_31, RDF_CMD_BASE+31,  "Remove as New Bookmark Folder")
ResDef(RDF_CMD_32, RDF_CMD_BASE+32,  "Set Password...")
ResDef(RDF_CMD_33, RDF_CMD_BASE+33,  "Remove Password...")
ResDef(RDF_CMD_34, RDF_CMD_BASE+34,  "Export All...")
ResDef(RDF_CMD_35, RDF_CMD_BASE+35,  "Undo")
ResDef(RDF_CMD_36, RDF_CMD_BASE+36,  "New Workspace...")
ResDef(RDF_CMD_37, RDF_CMD_BASE+37,  "Rename")
ResDef(RDF_CMD_38, RDF_CMD_BASE+38,  "Find...")

ResDef(RDF_MAIN_TITLE, RDF_MSG_BASE+100,  "Information")
ResDef(RDF_COLOR_TITLE, RDF_MSG_BASE+101,  "Color Information")
ResDef(RDF_MISSION_CONTROL_TITLE, RDF_MSG_BASE+102,  "Mission Control Settings")
ResDef(RDF_TREE_COLORS_TITLE, RDF_MSG_BASE+103,  "Tree Colors")
ResDef(RDF_SELECTION_COLORS_TITLE, RDF_MSG_BASE+104,  "Selection Colors")
ResDef(RDF_COLUMN_COLORS_TITLE, RDF_MSG_BASE+105,  "Column Colors")
ResDef(RDF_TITLEBAR_COLORS_TITLE, RDF_MSG_BASE+106,  "Title Bar Colors")
ResDef(RDF_APPLETALK_TOP_NAME, RDF_MSG_BASE+107,  "Appletalk Zones and File Servers")
ResDef(RDF_PERSONAL_TOOLBAR_NAME, RDF_MSG_BASE+108,  "Personal Toolbar")
ResDef(RDF_FIND_TITLE, RDF_MSG_BASE+109,  "Find")

ResDef(RDF_HTML_STR_NUMBER, RDF_MSG_BASE+110,  "\
<TR><TD ALIGN=RIGHT><B>%s:</B></TD><TD><INPUT TYPE=TEXT NAME='%s' WIDTH=6 SIZE=6 VALUE='%s'></TD></TR>\n")
ResDef(RDF_HTML_INFOHEADER_STR, RDF_MSG_BASE+111,  "<TR><TD ALIGN=LEFT BGCOLOR='#7F7F7F' COLSPAN=2>\
<FONT SIZE=+1 COLOR='#FFFFFF'><B>%s</B></FONT></TD></TR>\n")
ResDef(RDF_HTML_MAININFOHEADER_STR, RDF_MSG_BASE+112,  "<TR><TD ALIGN=LEFT BGCOLOR='#7F7F7F' COLSPAN=2>\
<FONT SIZE=+1 COLOR='#FFFFFF'><B>'%s' Information</B></FONT></TD></TR>\n")
ResDef(RDF_HTML_EMPTYHEADER_STR, RDF_MSG_BASE+113,  "<TR><TD> <P></TD></TR>\n")
ResDef(RDF_HTML_COLOR_STR, RDF_MSG_BASE+114,  "\
<TR><TD ALIGN=RIGHT><B>%s:</B></TD><TD>\
<INPUT TYPE=TEXT NAME='%s' WIDTH=36 SIZE=36 VALUE='%s' onChange='return setColor(\042%scube\042,this.value)'>\
&nbsp;<IMG SRC='' SUPPRESS=true NAME='%simage' WIDTH=1 HEIGHT=1 BORDER=0 ALIGN=top VALIGN=top><BR></TD></TR>\n")

ResDef(RDF_FOREGROUND_COLOR_STR, RDF_MSG_BASE+120,  "Foreground Color")
ResDef(RDF_BACKGROUND_COLOR_STR, RDF_MSG_BASE+121,  "Background Color")
ResDef(RDF_BACKGROUND_IMAGE_STR, RDF_MSG_BASE+122,  "Background Image URL")
ResDef(RDF_SHOW_TREE_CONNECTIONS_STR, RDF_MSG_BASE+123,  "Show Tree Connections")
ResDef(RDF_CONNECTION_FG_COLOR_STR, RDF_MSG_BASE+124,  "Connection Foreground Color")
ResDef(RDF_OPEN_TRIGGER_IMAGE_STR, RDF_MSG_BASE+125,  "Open Trigger Image URL")
ResDef(RDF_CLOSED_TRIGGER_IMAGE_STR, RDF_MSG_BASE+126,  "Closed Trigger Image URL")
ResDef(RDF_SHOW_HEADERS_STR, RDF_MSG_BASE+127,  "Show Headers")
ResDef(RDF_SHOW_HEADER_DIVIDERS_STR, RDF_MSG_BASE+128,  "Show Header Dividers")
ResDef(RDF_SORT_COLUMN_FG_COLOR_STR, RDF_MSG_BASE+129,  "Sort Column Foreground Color")
ResDef(RDF_SORT_COLUMN_BG_COLOR_STR, RDF_MSG_BASE+130,  "Sort Column Background Color")
ResDef(RDF_DIVIDER_COLOR_STR, RDF_MSG_BASE+131,  "Divider Color")
ResDef(RDF_SHOW_COLUMN_DIVIDERS_STR, RDF_MSG_BASE+132,  "Show Column Dividers")
ResDef(RDF_SELECTED_HEADER_FG_COLOR_STR, RDF_MSG_BASE+133,  "Selected Header Foreground Color")
ResDef(RDF_SELECTED_HEADER_BG_COLOR_STR, RDF_MSG_BASE+134,  "Selected Header Background Color")
ResDef(RDF_SHOW_COLUMN_HILITING_STR, RDF_MSG_BASE+135,  "Show Column Hiliting")
ResDef(RDF_TRIGGER_PLACEMENT_STR, RDF_MSG_BASE+136,  "Trigger Placement")

ResDef(RDF_URL_STR, RDF_MSG_BASE+140,  "URL")
ResDef(RDF_DESCRIPTION_STR, RDF_MSG_BASE+141,  "Description")
ResDef(RDF_FIRST_VISIT_STR, RDF_MSG_BASE+142,  "First visited on")
ResDef(RDF_LAST_VISIT_STR, RDF_MSG_BASE+143,  "Last visited on")
ResDef(RDF_NUM_ACCESSES_STR, RDF_MSG_BASE+144,  "# of Accesses")
ResDef(RDF_CREATED_ON_STR, RDF_MSG_BASE+145,  "Created on")
ResDef(RDF_LAST_MOD_STR, RDF_MSG_BASE+146,  "Last modified on")
ResDef(RDF_SIZE_STR, RDF_MSG_BASE+147,  "Size")
ResDef(RDF_ADDED_ON_STR, RDF_MSG_BASE+148,  "Added on")
ResDef(RDF_ICON_URL_STR, RDF_MSG_BASE+149,  "Icon URL")
ResDef(RDF_LARGE_ICON_URL_STR, RDF_MSG_BASE+150,  "Large Icon URL")
ResDef(RDF_HTML_URL_STR, RDF_MSG_BASE+151,  "HTML URL")
ResDef(RDF_HTML_HEIGHT_STR, RDF_MSG_BASE+152,  "HTML Height")
ResDef(RDF_NAME_STR, RDF_MSG_BASE+153,  "Name")
ResDef(RDF_SHORTCUT_STR, RDF_MSG_BASE+154,  "Shortcut")

ResDef(RDF_SETCOLOR_JS, RDF_MSG_BASE+160,  "\
<SCRIPT>\
function setColor(cubeName,rgb) {\
if(rgb=='')rgb=document.bgColor;\
for(x=0;x<document.layers.length;x++){\
l=document.layers[x];\
if(l.name==cubeName){\
l.document.bgColor=rgb;\
l.document.fgColor=rgb;\
break;\
}\
}\
return(true);\
}\
</SCRIPT>")

ResDef(RDF_DEFAULTCOLOR_JS, RDF_MSG_BASE+161,  "\
<SCRIPT>\
document.layers['%scube'].x = document.images['%simage'].x;\
document.layers['%scube'].y = document.images['%simage'].y;\
setColor('%scube', document.forms[0].%s.value);\
document.layers['%scube'].visibility='SHOW';\
</SCRIPT>")

ResDef(RDF_COLOR_LAYER, RDF_MSG_BASE+163,  "<LAYER NAME='%scube' CLIP='0,0,18,18' \
WIDTH=18 HEIGHT=18 TOP=0 LEFT=0 VISIBILITY=HIDE><BODY></BODY></LAYER>")

ResDef(RDF_HTMLCOLOR_STR, RDF_MSG_BASE+164,  "\
%0%<CENTER><TABLE ALIGN=center WIDTH=470 BORDER=0>%1%</TABLE></CENTER><P>%2%")


ResDef(RDF_SELECT_START, RDF_MSG_BASE+170, "<SELECT NAME='%s'>\n")
ResDef(RDF_SELECT_END, RDF_MSG_BASE+171, "</SELECT>\n")
ResDef(RDF_SELECT_OPTION, RDF_MSG_BASE+172, "<OPTION VALUE='%s'>%s\n")

ResDef(RDF_FIND_STR1, RDF_MSG_BASE+180, "Find items in ")
ResDef(RDF_FIND_STR2, RDF_MSG_BASE+181, " whose<P>")
ResDef(RDF_FIND_INPUT_STR, RDF_MSG_BASE+182, "<INPUT TYPE='text' NAME='%s' VALUE='%s' WIDTH=20>")

ResDef(RDF_LOCAL_LOCATION_STR, RDF_MSG_BASE+190, "local workspaces")
ResDef(RDF_REMOTE_LOCATION_STR, RDF_MSG_BASE+191, "remote workspaces")
ResDef(RDF_ALL_LOCATION_STR, RDF_MSG_BASE+192, "all workspaces")

ResDef(RDF_CONTAINS_STR, RDF_MSG_BASE+193, "contains")
ResDef(RDF_IS_STR, RDF_MSG_BASE+194, "is")
ResDef(RDF_IS_NOT_STR, RDF_MSG_BASE+195, "is not")
ResDef(RDF_STARTS_WITH_STR, RDF_MSG_BASE+196, "starts with")
ResDef(RDF_ENDS_WITH_STR, RDF_MSG_BASE+197, "ends with")
ResDef(RDF_FIND_FULLNAME_STR, RDF_MSG_BASE+198, "Find: %s %s '%s'")
ResDef(RDF_SHORTCUT_CONFLICT_STR, RDF_MSG_BASE+199, "'%s' is assigned as a shortcut for '%s'. Reassign it?")

ResDef(RDF_AFP_CLIENT_37_STR, RDF_MSG_BASE+200, "Please install AppleShare Client version 3.7 or later.")
ResDef(RDF_AFP_AUTH_FAILED_STR, RDF_MSG_BASE+201, "User authentication failed.")
ResDef(RDF_AFP_PW_EXPIRED_STR, RDF_MSG_BASE+202, "Password expired.")
ResDef(RDF_AFP_ALREADY_MOUNTED_STR, RDF_MSG_BASE+203, "Volume already mounted.")
ResDef(RDF_AFP_MAX_SERVERS_STR, RDF_MSG_BASE+204, "Maximum number of volumes has been mounted.")
ResDef(RDF_AFP_NOT_RESPONDING_STR, RDF_MSG_BASE+205, "Server is not responding.")
ResDef(RDF_AFP_SAME_NODE_STR, RDF_MSG_BASE+206, "Failed to log on to a server running on this machine.")
ResDef(RDF_AFP_ERROR_NUM_STR, RDF_MSG_BASE+207, "\rError %d")

ResDef(RDF_VOLUME_DESC_STR, RDF_MSG_BASE+210, "Volume")
ResDef(RDF_DIRECTORY_DESC_STR, RDF_MSG_BASE+211, "Directory")
ResDef(RDF_FILE_DESC_STR, RDF_MSG_BASE+212, "File")
ResDef(RDF_FTP_NAME_STR, RDF_MSG_BASE+213, "FTP Locations")


/* The following messages are for capabilities based Signed Applets/JS */
#define CAPS_MSG_BASE XP_MSG_BASE + 3500

ResDef(CAPS_TARGET_RISK_STR_LOW, (CAPS_MSG_BASE + 1), "\
low")

ResDef(CAPS_TARGET_RISK_STR_MEDIUM, (CAPS_MSG_BASE + 2), "\
medium")

ResDef(CAPS_TARGET_RISK_STR_HIGH, (CAPS_MSG_BASE + 3), "\
high")

ResDef(CAPS_TARGET_RISK_COLOR_LOW, (CAPS_MSG_BASE + 4), "\
#aaffaa")

ResDef(CAPS_TARGET_RISK_COLOR_MEDIUM, (CAPS_MSG_BASE + 5), "\
#ffffaa")

ResDef(CAPS_TARGET_RISK_COLOR_HIGH, (CAPS_MSG_BASE + 6), "\
#ffaaaa")

ResDef(CAPS_TARGET_HELP_URL, (CAPS_MSG_BASE + 7), "\
http://home.netscape.com/eng/mozilla/4.0/handbook/")

ResDef(CAPS_TARGET_DESC_FILE_READ, (CAPS_MSG_BASE + 8), "\
Reading files stored in your computer")

ResDef(CAPS_TARGET_DETAIL_DESC_FILE_READ, (CAPS_MSG_BASE + 9), "\
Reading any files stored on hard disks or other storage media connected to \
your computer.")

ResDef(CAPS_TARGET_URL_FILE_READ, (CAPS_MSG_BASE + 10), "\
#FileRead")

ResDef(CAPS_TARGET_DESC_FILE_WRITE, (CAPS_MSG_BASE + 11), "\
Modifying files stored in your computer")

ResDef(CAPS_TARGET_DETAIL_DESC_FILE_WRITE, (CAPS_MSG_BASE + 12), "\
Modifying any files stored on hard disks or other storage media connected to \
you computer.")

ResDef(CAPS_TARGET_URL_FILE_WRITE, (CAPS_MSG_BASE + 13), "\
#FileWrite")

ResDef(CAPS_TARGET_DESC_FILE_DELETE, (CAPS_MSG_BASE + 14), "\
Deleting files stored in your computer")

ResDef(CAPS_TARGET_DETAIL_DESC_FILE_DELETE, (CAPS_MSG_BASE + 15), "\
Deletion of any files stored on hard disks or other storage media connected \
to your computer.")

ResDef(CAPS_TARGET_URL_FILE_DELETE, (CAPS_MSG_BASE + 16), "\
#FileDelete")

ResDef(CAPS_TARGET_DESC_IMPERSONATOR, (CAPS_MSG_BASE + 17), "\
Access to impersonate as another application")

ResDef(CAPS_TARGET_DETAIL_DESC_IMPERSONATOR, (CAPS_MSG_BASE + 18), "\
Access to impersonate as another application")

ResDef(CAPS_TARGET_URL_IMPERSONATOR, (CAPS_MSG_BASE + 19), "\
#Impersonator")

ResDef(CAPS_TARGET_DESC_BROWSER_READ, (CAPS_MSG_BASE + 20), "\
Access to browser data")

ResDef(CAPS_TARGET_DETAIL_DESC_BROWSER_READ, (CAPS_MSG_BASE + 21), "\
Access to browser data that may be considered private, such as a list \
of web sites visited or the contents of web page forms you may have filled in.")

ResDef(CAPS_TARGET_URL_BROWSER_READ, (CAPS_MSG_BASE + 22), "\
#BrowserRead")

ResDef(CAPS_TARGET_DESC_BROWSER_WRITE, (CAPS_MSG_BASE + 23), "\
Modifying the browser")

ResDef(CAPS_TARGET_DETAIL_DESC_BROWSER_WRITE, (CAPS_MSG_BASE + 24), "\
Modifying the browser in a potentially dangerous way, such as creating \
windows that may look like they belong to another program or positioning \
windows anywhere on the screen.")

ResDef(CAPS_TARGET_URL_BROWSER_WRITE, (CAPS_MSG_BASE + 25), "\
#BrowserWrite")

ResDef(CAPS_TARGET_DESC_BROWSER_ACCESS, (CAPS_MSG_BASE + 26), "\
Reading or modifying browser data")

ResDef(CAPS_TARGET_DETAIL_DESC_BROWSER_ACCESS, (CAPS_MSG_BASE + 27), "\
Reading or modifying browser data that may be considered private, such as a \
list of web sites visited or the contents of web forms you may have filled in. \
Modifications may also include creating windows that look like they belong to \
another program or positioning windowsanywhere on the screen.")

ResDef(CAPS_TARGET_URL_BROWSER_ACCESS, (CAPS_MSG_BASE + 28), "\
#BrowserAccess")

ResDef(CAPS_TARGET_DESC_PREFS_READ, (CAPS_MSG_BASE + 29), "\
Reading preferences settings")

ResDef(CAPS_TARGET_DETAIL_DESC_PREFS_READ, (CAPS_MSG_BASE + 30), "\
Access to read the current settings of your preferences.")

ResDef(CAPS_TARGET_URL_PREFS_READ, (CAPS_MSG_BASE + 31), "\
#PrefsRead")

ResDef(CAPS_TARGET_DESC_PREFS_WRITE, (CAPS_MSG_BASE + 32), "\
Modifying preferences settings")

ResDef(CAPS_TARGET_DETAIL_DESC_PREFS_WRITE, (CAPS_MSG_BASE + 33), "\
Modifying the current settings of your preferences.")

ResDef(CAPS_TARGET_URL_PREFS_WRITE, (CAPS_MSG_BASE + 34), "\
#PrefsWrite")

ResDef(CAPS_TARGET_DESC_SEND_MAIL, (CAPS_MSG_BASE + 35), "\
Sending email messages on your behalf")

ResDef(CAPS_TARGET_DETAIL_DESC_SEND_MAIL, (CAPS_MSG_BASE + 36), "\
Sending email messages on your behalf")

ResDef(CAPS_TARGET_URL_SEND_MAIL, (CAPS_MSG_BASE + 37), "\
#SendMail")

ResDef(CAPS_TARGET_DESC_REG_PRIVATE, (CAPS_MSG_BASE + 38), "\
Access to the vendor's portion of your computer's registry of installed \
software")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_PRIVATE, (CAPS_MSG_BASE + 39), "\
Most computers store information about installed software, such as version \
numbers, in a registry file. When you install new software, the installation \
program sometimes needs to read or change entries in the portion of the \
%-cont-%")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_PRIVATE_1, (CAPS_MSG_BASE + 40), "\
registry that describes the software vendor's products. You should grant \
this form of access only if you are installing new software from a reliable \
vendor. The entity that signs the software can access only that entity's \
portion of the registry.")

ResDef(CAPS_TARGET_URL_REG_PRIVATE, (CAPS_MSG_BASE + 41), "\
#RegPrivate")

ResDef(CAPS_TARGET_DESC_REG_STANDARD, (CAPS_MSG_BASE + 42), "\
Access to shared information in the computer's registry of installed software")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_STANDARD, (CAPS_MSG_BASE + 43), "\
Most computers store information about installed software, such as version \
numbers, in a registry file. This file also includes information shared by \
%-cont-%")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_STANDARD_1, (CAPS_MSG_BASE + 44), "\
all programs installed on your computer, including information about the user \
or the system. Programs that have access to shared registry information can \
obtain information about other programs that have the same access. This allows \
%-cont-%")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_STANDARD_2, (CAPS_MSG_BASE + 45), "\
programs that work closely together to get information about each other. \
You should grant this form of access only if you know that the program \
requesting it is designed to work with other programs on your hard disk.")

ResDef(CAPS_TARGET_URL_REG_STANDARD, (CAPS_MSG_BASE + 46), "\
#RegStandard")

ResDef(CAPS_TARGET_DESC_REG_ADMIN, (CAPS_MSG_BASE + 47), "\
Access to any part of your computer's registry of installed software")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_ADMIN, (CAPS_MSG_BASE + 48), "\
Most computers store information about installed software, such as version \
numbers, in a registry file. System administrators sometimes need to change \
%-cont-%")

ResDef(CAPS_TARGET_DETAIL_DESC_REG_ADMIN_1, (CAPS_MSG_BASE + 49), "\
entries in the registry for software from a variety of vendors. You should \
grant this form of access only if you are running software provided by your \
system administrator.")

ResDef(CAPS_TARGET_URL_REG_ADMIN, (CAPS_MSG_BASE + 50), "\
#RegAdmin")

ResDef(CAPS_TARGET_DESC_ACCOUNT_SETUP, (CAPS_MSG_BASE + 51), "\
Access required to setup and configure your browser")

ResDef(CAPS_TARGET_DETAIL_DESC_ACCOUNT_SETUP, (CAPS_MSG_BASE + 52), "\
Access to, and modification of, browser data, preferences, files, networking \
and modem configuration. This access is commonly granted to the main setup \
program for your browser.")

ResDef(CAPS_TARGET_URL_ACCOUNT_SETUP, (CAPS_MSG_BASE + 53), "\
#AccountSetup")

ResDef(CAPS_TARGET_DESC_SAR, (CAPS_MSG_BASE + 54), "\
Access to the site archive file")

ResDef(CAPS_TARGET_DETAIL_DESC_SAR, (CAPS_MSG_BASE + 55), "\
Access required to add, modify, or delete site archive files and make \
arbitrary network connections in the process. This form of access is required \
%-cont-%")

ResDef(CAPS_TARGET_DETAIL_DESC_SAR_1, (CAPS_MSG_BASE + 56), "\
only by netcasting applications such as Netscape Netcaster, which request it \
in combination with several other kinds of access. Applications should not \
normally request this access by itself, and you should not normally grant it.")

ResDef(CAPS_TARGET_URL_SAR, (CAPS_MSG_BASE + 57), "\
#SiteArchive")

ResDef(CAPS_TARGET_DESC_CANVAS_ACCESS, (CAPS_MSG_BASE + 58), "\
Displaying text or graphics anywhere on the screen")

ResDef(CAPS_TARGET_DETAIL_DESC_CANVAS_ACCESS, (CAPS_MSG_BASE + 59), "\
Displaying HTML text or graphics on any part of the screen, without window \
borders, toolbars, or menus. Typically granted to invoke canvas mode, screen \
savers, and so on.")

ResDef(CAPS_TARGET_URL_CANVAS_ACCESS, (CAPS_MSG_BASE + 60), "\
#CanvasAccess")

ResDef(CAPS_TARGET_DESC_FILE_ACCESS, (CAPS_MSG_BASE + 61), "\
Reading, modification, or deletion of any of your files")

ResDef(CAPS_TARGET_DETAIL_DESC_FILE_ACCESS, (CAPS_MSG_BASE + 62), "\
This form of access is typically required by a program such as a word \
processor or a debugger that needs to create, read, modify, or delete files \
on hard disks or other storage media connected to your computer.")

ResDef(CAPS_TARGET_URL_FILE_ACCESS, (CAPS_MSG_BASE + 63), "\
#FileAccess")

ResDef(CAPS_TARGET_DESC_UNINSTALL,  (CAPS_MSG_BASE + 64), "\
Uninstall software")

ResDef(CAPS_TARGET_DETAIL_DESC_UNINSTALL, (CAPS_MSG_BASE + 65), "\
Access required for automatic removal of previously installed software.")

ResDef(CAPS_TARGET_URL_UNINSTALL, (CAPS_MSG_BASE + 66), "\
#Uninstall")

ResDef(CAPS_TARGET_DESC_SOFTWAREINSTALL, (CAPS_MSG_BASE + 67), "\
Installing and running software on your computer")

ResDef(CAPS_TARGET_DETAIL_DESC_SOFTWAREINSTALL, (CAPS_MSG_BASE + 68), "\
Installing software on your computer's hard disk. An installation \
program can also execute or delete any software on your computer. \
You should not grant this form of access unless you are installing or \
updating software from a reliable source.")

ResDef(CAPS_TARGET_URL_SOFTWAREINSTALL, (CAPS_MSG_BASE + 69), "\
#SoftwareInstall")

ResDef(CAPS_TARGET_DESC_SILENTINSTALL, (CAPS_MSG_BASE + 70), "\
Installing and running software without warning you")

ResDef(CAPS_TARGET_DETAIL_DESC_SILENTINSTALL, (CAPS_MSG_BASE + 71), "\
Installing software on your computer's main hard disk without giving you any \
warning, potentially deleting other files on the hard disk. Any software on the \
hard disk may be executed in the process. This is an extremely dangerous form \
of access. It should be granted by system administrators only.")

ResDef(CAPS_TARGET_URL_SILENTINSTALL, (CAPS_MSG_BASE + 72), "\
#SilentInstall")

ResDef(CAPS_TARGET_DESC_ALL_JAVA_PERMISSION, (CAPS_MSG_BASE + 73), "\
Complete access to your computer for java programs")

ResDef(CAPS_TARGET_DETAIL_DESC_ALL_JAVA_PERMISSION, (CAPS_MSG_BASE + 74), "\
Complete access required by java programs to your computer, such as Java \
Virtual machine reading, writing, deleting information from your disk, \
and to send receive and send information to any computer on the Internet.")

ResDef(CAPS_TARGET_URL_ALL_JAVA_PERMISSION, (CAPS_MSG_BASE + 75), "\
#AllJavaPermission")

ResDef(CAPS_TARGET_DESC_ALL_JS_PERMISSION, (CAPS_MSG_BASE + 76), "\
Access to all Privileged JavaScript operations")

ResDef(CAPS_TARGET_DETAIL_DESC_ALL_JS_PERMISSION, (CAPS_MSG_BASE + 77), "\
Access to all Privileged JavaScript operations.")

ResDef(CAPS_TARGET_URL_ALL_JS_PERMISSION, (CAPS_MSG_BASE + 78), "\
#AllJavaScriptPermission")


/* The following messages are for Software Update */
#define SU_MSG_BASE XP_MSG_BASE + 4000

ResDef(SU_DETAILS_DELETE_COMPONENT, (SU_MSG_BASE + 1), "\
Delete component ")

ResDef(SU_DETAILS_DELETE_FILE, (SU_MSG_BASE + 2), "\
Delete ")

ResDef(SU_DETAILS_EXECUTE_PROGRESS, (SU_MSG_BASE + 3), "\
Execute {0}")

ResDef(SU_DETAILS_EXECUTE_PROGRESS2, (SU_MSG_BASE + 4), "\
Execute {0} ({1})")

ResDef(SU_DETAILS_INSTALL_FILE_MSG_ID, (SU_MSG_BASE + 5), "\
Install ")

ResDef(SU_DETAILS_PATCH, (SU_MSG_BASE + 6), "\
Patch ")

ResDef(SU_DETAILS_REPLACE_FILE_MSG_ID, (SU_MSG_BASE + 7), "\
Replace ")

ResDef(SU_DETAILS_UNINSTALL, (SU_MSG_BASE + 8), "\
Uninstall ")

ResDef(SU_ERROR_BAD_JS_ARGUMENT, (SU_MSG_BASE + 9), "\
Verification failed. 'this' must be passed to SoftwareUpdate constructor. ")

ResDef(SU_ERROR_BAD_PACKAGE_NAME, (SU_MSG_BASE + 10), "\
Package name is null or empty in StartInstall.")

ResDef(SU_ERROR_BAD_PACKAGE_NAME_AS, (SU_MSG_BASE + 11), "\
Bad package name in AddSubcomponent. Did you call StartInstall()?")

ResDef(SU_ERROR_EXTRACT_FAILED, (SU_MSG_BASE + 12), "\
Extraction of JAR file failed.")

ResDef(SU_ERROR_FILE_IS_DIRECTORY, (SU_MSG_BASE + 13), "\
Failure while deleting file. File is a directory. ")

ResDef(SU_ERROR_FILE_READ_ONLY, (SU_MSG_BASE + 14), "\
Failure while deleting file. File is read only. ")

ResDef(SU_ERROR_INSTALL_FILE_UNEXPECTED, (SU_MSG_BASE + 15), "\
Unexpected error when installing a file ")

ResDef(SU_ERROR_MISSING_INSTALLER, (SU_MSG_BASE + 16), "\
Could not get installer name out of the global MIME headers inside the JAR file.")

ResDef(SU_ERROR_NOT_IN_REGISTRY, (SU_MSG_BASE + 17), "\
Failure while deleting component. Component not found in registry. ")

ResDef(SU_ERROR_NO_CERTIFICATE, (SU_MSG_BASE + 18), "\
Installer script does not have a certificate.")

ResDef(SU_ERROR_NO_SUCH_COMPONENT, (SU_MSG_BASE + 19), "\
No such component ")

ResDef(SU_ERROR_OUT_OF_MEMORY, (SU_MSG_BASE + 20), "\
Out of memory")

ResDef(SU_ERROR_SMART_UPDATE_DISABLED, (SU_MSG_BASE + 21), "\
SmartUpdate disabled")

ResDef(SU_ERROR_TOO_MANY_CERTIFICATES, (SU_MSG_BASE + 22), "\
Installer script had more than one certificate")

ResDef(SU_ERROR_UNEXPECTED, (SU_MSG_BASE + 23), "\
Unexpected error in ")

ResDef(SU_ERROR_VERIFICATION_FAILED, (SU_MSG_BASE + 24), "\
Security integrity check failed.")

ResDef(SU_ERROR_WIN_PROFILE_MUST_CALL_START, (SU_MSG_BASE + 25), "\
Must call StartInstall() to initialize SoftwareUpdate.")

ResDef(SU_INSTALLWIN_INSTALLING, (SU_MSG_BASE + 26), "\
Installing... ")

ResDef(SU_INSTALLWIN_TITLE, (SU_MSG_BASE + 27), "\
SmartUpdate: %s")

ResDef(SU_INSTALLWIN_UNPACKING, (SU_MSG_BASE + 28), "\
Unpacking files for installation ")

ResDef(SU_PROGRESS_DOWNLOAD_LINE1, (SU_MSG_BASE + 29), "\
Location: %s")

ResDef(SU_PROGRESS_DOWNLOAD_TITLE, (SU_MSG_BASE + 30), "\
SmartUpdate: Downloading Install")

ResDef(SU_SMARTUPDATE, (SU_MSG_BASE + 31), "\
SmartUpdate\nConfigure software installation")

ResDef(SU_UNINSTALL, (SU_MSG_BASE + 32), "\
Uninstall ")

ResDef(SU_CONTINUE_UNINSTALL, (SU_MSG_BASE + 33), "\
Are you sure you want to uninstall %s? ")

ResDef(SU_ERROR_UNINSTALL, (SU_MSG_BASE + 34), "\
Error in uninstall")

ResDef(REGPACK_PROGRESS_TITLE, (SU_MSG_BASE + 35), "\
Updating Netscape Client Registry")

ResDef(REGPACK_PROGRESS_LINE1, (SU_MSG_BASE + 36), "\
Communicator is updating your Netscape registry.")

ResDef(REGPACK_PROGRESS_LINE2, (SU_MSG_BASE + 37), "\
%d bytes of %d bytes")


END_STR(mcom_include_xp_msg_i_strings)

/* WARNING: DO NOT TAKE ERROR CODE -666, it is used internally
   by the message lib */


#endif /* _ALLXPSTR_H_ */

