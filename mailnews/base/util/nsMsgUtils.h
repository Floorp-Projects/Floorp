/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NSMSGUTILS_H
#define _NSMSGUTILS_H

#include "nsIURL.h"
#include "nsIMsgMessageService.h"
#include "nsString.h"
#include "nsIEnumerator.h"
#include "nsIMsgFolder.h"
#include "msgCore.h"
#include "nsCOMPtr.h"

class nsILocalFile;

//These are utility functions that can used throughout the mailnews code

NS_MSG_BASE nsresult GetMessageServiceContractIDForURI(const char *uri, nsCString &contractID);

NS_MSG_BASE nsresult GetMessageServiceFromURI(const char *uri, nsIMsgMessageService **aMessageService);

NS_MSG_BASE nsresult GetMsgDBHdrFromURI(const char *uri, nsIMsgDBHdr **msgHdr);

NS_MSG_BASE nsresult CreateStartupUrl(const char *uri, nsIURI** aUrl);

NS_MSG_BASE nsresult NS_MsgGetPriorityFromString(const char *priority, nsMsgPriorityValue *outPriority);

NS_MSG_BASE nsresult NS_MsgGetUntranslatedPriorityName (nsMsgPriorityValue p, nsString *outName);

NS_MSG_BASE nsresult NS_MsgHashIfNecessary(nsAutoString &name);
NS_MSG_BASE nsresult NS_MsgHashIfNecessary(nsCAutoString &name);

NS_MSG_BASE nsresult NS_MsgCreatePathStringFromFolderURI(const char *folderURI, nsCString& pathString);

NS_MSG_BASE PRBool NS_MsgStripRE(const char **stringP, PRUint32 *lengthP, char **modifiedSubject=nsnull);

NS_MSG_BASE char * NS_MsgSACopy(char **destination, const char *source);

NS_MSG_BASE char * NS_MsgSACat(char **destination, const char *source);

NS_MSG_BASE nsresult NS_MsgEscapeEncodeURLPath(const nsAString& str, nsAFlatCString& result);

NS_MSG_BASE nsresult NS_MsgDecodeUnescapeURLPath(const nsASingleFragmentCString& path, nsAString& result);

NS_MSG_BASE PRBool WeAreOffline();

// Check if a folder with aFolderUri exists
NS_MSG_BASE nsresult GetExistingFolder(const char *aFolderURI, nsIMsgFolder **aFolder);

// Escape lines starting with "From ", ">From ", etc. in a buffer.
NS_MSG_BASE nsresult EscapeFromSpaceLine(nsIFileSpec *pDst, char *start, const char *end);
NS_MSG_BASE PRBool IsAFromSpaceLine(char *start, const char *end);

NS_MSG_BASE nsresult NS_GetPersistentFile(const char *relPrefName,
                                          const char *absPrefName,
                                          const char *dirServiceProp, // Can be NULL
                                          PRBool& gotRelPref,
                                          nsILocalFile **aFile);

NS_MSG_BASE nsresult NS_SetPersistentFile(const char *relPrefName,
                                          const char *absPrefName,
                                          nsILocalFile *aFile);

NS_MSG_BASE nsresult CreateServicesForPasswordManager();

NS_MSG_BASE nsresult IsRFC822HeaderFieldName(const char *aHdr, PRBool *aResult);

  /**
   * this needs a listener, because we might have to create the folder
   * on the server, and that is asynchronous
   */
NS_MSG_BASE nsresult GetOrCreateFolder(const nsACString & aURI, nsIUrlListener *aListener);

// digest needs to be a pointer to a 16 byte buffer
#define DIGEST_LENGTH 16

NS_MSG_BASE nsresult MSGCramMD5(const char *text, PRInt32 text_len, const char *key, PRInt32 key_len, unsigned char *digest);
NS_MSG_BASE nsresult MSGApopMD5(const char *text, PRInt32 text_len, const char *password, PRInt32 password_len, unsigned char *digest);
#endif

