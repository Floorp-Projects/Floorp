/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Include files we are going to want available to all files....these files include
   NSPR, memory, and string header files among others */

#ifndef msgCore_h__
#define msgCore_h__

#include "nscore.h"

#include "nsCRT.h"
#include "nsString.h"
#include "nsVoidArray.h"

class nsIMsgDBHdr;
class nsIMsgFolder;

// include common interfaces such as the service manager and the repository....
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

/* NS_ERROR_MODULE_MAILNEWS is defined in mozilla/xpcom/public/nsError.h */

/*
 * NS_ERROR macros - use these macros to generate error constants
 * to be used by XPCOM interfaces and possibly other useful things
 * do not use these macros in your code - declare error macros for
 * each specific error you need.
 *
 * for example:
 * #define NS_MSG_ERROR_NO_SUCH_FOLDER NS_MSG_GENERATE_FAILURE(4)
 * 
 */

/* use these routines to generate error values */
#define NS_MSG_GENERATE_RESULT(severity, value) \
NS_ERROR_GENERATE(severity, NS_ERROR_MODULE_MAILNEWS, value)

#define NS_MSG_GENERATE_SUCCESS(value) \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_MAILNEWS, value)

#define NS_MSG_GENERATE_FAILURE(value) \
NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_MAILNEWS, value)

/* these are shortcuts to generate simple errors with a zero value */
#define NS_MSG_SUCCESS NS_MSG_GENERATE_SUCCESS(0)
#define NS_MSG_FAILURE NS_MSG_GENERATE_FAILURE(0)

// mscott I should look into moving this into raptor base so everyone can use it...
#define IS_SPACE(VAL)                \
    (((((intn)(VAL)) & 0x7f) == ((intn)(VAL))) && isspace((intn)(VAL)) )

#define IS_DIGIT(i) ((((unsigned int) (i)) > 0x7f) ? (int) 0 : isdigit(i))

#if defined(XP_WIN) || defined(XP_OS2)
#define IS_ALPHA(VAL)  (isascii((int)(VAL)) && isalpha((int)(VAL)))
#else
#define IS_ALPHA(VAL) ((((unsigned int) (VAL)) > 0x7f) ? FALSE : isalpha((int)(VAL)))
#endif


/* for retrieving information out of messenger nsresults */

#define NS_IS_MSG_ERROR(err) \
 (NS_ERROR_GET_MODULE(err) == NS_ERROR_MODULE_MAILNEWS)

#define NS_MSG_SUCCEEDED(err) \
 (NS_IS_MSG_ERROR(err) && NS_SUCCEEDED(err))

#define NS_MSG_FAILED(err) \
 (NS_IS_MSG_ERROR(err) && NS_FAILED(err))

/* is this where we define our errors? Obviously, there has to be a central
	place so we don't use the same error codes.
*/
#define NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE NS_MSG_GENERATE_FAILURE(5)
#define NS_MSG_ERROR_FOLDER_SUMMARY_MISSING NS_MSG_GENERATE_FAILURE(6)
#define NS_MSG_ERROR_FOLDER_MISSING NS_MSG_GENERATE_FAILURE(7)

#define NS_MSG_MESSAGE_NOT_FOUND NS_MSG_GENERATE_FAILURE(8)
#define NS_MSG_NOT_A_MAIL_FOLDER NS_MSG_GENERATE_FAILURE(9)

#define NS_MSG_FOLDER_BUSY NS_MSG_GENERATE_FAILURE(10)

#define NS_MSG_COULD_NOT_CREATE_DIRECTORY NS_MSG_GENERATE_FAILURE(11)
#define NS_MSG_CANT_CREATE_FOLDER NS_MSG_GENERATE_FAILURE(12)

#define NS_MSG_FILTER_PARSE_ERROR NS_MSG_GENERATE_FAILURE(13)

#define NS_MSG_FOLDER_UNREADABLE NS_MSG_GENERATE_FAILURE(14)

#define NS_MSG_ERROR_WRITING_MAIL_FOLDER NS_MSG_GENERATE_FAILURE(15)

#define NS_MSG_ERROR_NO_SEARCH_VALUES NS_MSG_GENERATE_FAILURE(16)

#define NS_MSG_ERROR_INVALID_SEARCH_SCOPE NS_MSG_GENERATE_FAILURE(17)

#define NS_MSG_ERROR_INVALID_SEARCH_TERM NS_MSG_GENERATE_FAILURE(18)

#define NS_MSG_FOLDER_EXISTS NS_MSG_GENERATE_FAILURE(19)

#define NS_MSG_ERROR_OFFLINE NS_MSG_GENERATE_FAILURE(20)

#define NS_MSG_POP_FILTER_TARGET_ERROR NS_MSG_GENERATE_FAILURE(21)

#define NS_MSG_INVALID_OR_MISSING_SERVER NS_MSG_GENERATE_FAILURE(22)

#define NS_MSG_SERVER_USERNAME_MISSING NS_MSG_GENERATE_FAILURE(23)

#define NS_MSG_INVALID_DBVIEW_INDEX NS_MSG_GENERATE_FAILURE(24)

#define NS_MSG_NEWS_ARTICLE_NOT_FOUND NS_MSG_GENERATE_FAILURE(25)

#define NS_MSG_ERROR_COPY_FOLDER_ABORTED NS_MSG_GENERATE_FAILURE(26)

/* ducarroz: error codes for message compose are defined into compose\src\nsMsgComposeStringBundle.h.
             Message compose use the same error code space than other mailnews modules. To avoid any
             conflict, I reserve values between 12500 and 12999 for it.
*/
#define NS_MSGCOMP_ERROR_BEGIN	12500
/* NS_ERROR_NNTP_NO_CROSS_POSTING lives here, and not in nsMsgComposeStringBundle.h, because it is used in news and compose. */
#define NS_ERROR_NNTP_NO_CROSS_POSTING NS_MSG_GENERATE_FAILURE(12554)
#define NS_MSGCOMP_ERROR_END	12999

#define MSG_LINEBREAK NS_LINEBREAK
#define MSG_LINEBREAK_LEN NS_LINEBREAK_LEN

/* mscott - i'm only turning  NS_MSG_BASE on for windows so
   define it as empty for the other platforms. */

#ifdef XP_WIN

#ifdef _IMPL_NS_MSG_BASE
#define NS_MSG_BASE NS_EXPORT
#else
#define NS_MSG_BASE NS_IMPORT
#endif

#else
#define NS_MSG_BASE
#endif

////////////////////////////////////////////////////////////////////////////////
// Utilities 

// mscott: one wouldn't normally have to add the NS_MSG_BASE prefix here 
// except this function is implemented in base\util.
nsresult NS_MSG_BASE
nsGetMailFolderSeparator(nsString& result);

////////////////////////////////////////////////////////////////////////////////

#endif
