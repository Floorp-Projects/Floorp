/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsImapUrl_h___
#define nsImapUrl_h___

#include "nsIImapUrl.h"
#include "nsIImapMockChannel.h"
#include "nsCOMPtr.h"
#include "nsMsgMailNewsUrl.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapServerSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"

#include "nsWeakPtr.h"
#include "nsXPIDLString.h"
#include "nsIFileSpec.h"

class nsImapUrl : public nsIImapUrl, public nsMsgMailNewsUrl, public nsIMsgMessageUrl, public nsIMsgI18NUrl
{
public:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIURI override
  NS_IMETHOD SetSpec(const nsACString &aSpec);
  NS_IMETHOD SetQuery(const nsACString &aQuery);

  ////////////////////////////////////////////////////////////////////////////// 
  // we support the nsIImapUrl interface
  //////////////////////////////////////////////////////////////////////////////
  NS_DECL_NSIIMAPURL

  // nsIMsgMailNewsUrl overrides
  NS_IMETHOD IsUrlType(PRUint32 type, PRBool *isType);
  NS_IMETHOD SetMsgWindow(nsIMsgWindow *aMsgWindow);
  NS_IMETHOD GetFolder(nsIMsgFolder **aFolder);
  NS_IMETHOD SetFolder(nsIMsgFolder *aFolder);
  // nsIMsgMessageUrl
  NS_DECL_NSIMSGMESSAGEURL
  NS_DECL_NSIMSGI18NURL
  
  // nsImapUrl
  nsImapUrl();
  virtual ~nsImapUrl();
  
  static nsresult ConvertToCanonicalFormat(const char *folderName, char onlineDelimiter, char **resultingCanonicalPath);
  static nsresult EscapeSlashes(const char *sourcePath, char **resultPath);
  static nsresult UnescapeSlashes(char *path);
  static char *	ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar);
  
protected:
  virtual nsresult ParseUrl();
  virtual const char * GetUserName() { return m_userName;}
  
  char		*m_listOfMessageIds;
  
  // handle the imap specific parsing
  void		ParseImapPart(char *imapPartOfUrl);
  
  void		ParseFolderPath(char **resultingCanonicalPath);
  void		ParseSearchCriteriaString();
  void		ParseChildDiscoveryDepth();
  void		ParseUidChoice();
  void		ParseMsgFlags();
  void		ParseListOfMessageIds();
  void          ParseCustomMsgFetchAttribute();

  nsresult GetMsgFolder(nsIMsgFolder **msgFolder);
  
  char        *m_sourceCanonicalFolderPathSubString;
  char        *m_destinationCanonicalFolderPathSubString;
  char		*m_tokenPlaceHolder;
  char		*m_urlidSubString;
  char		m_onlineSubDirSeparator;
  char		*m_searchCriteriaString;	// should we use m_search, or is this special?
  nsCString     m_command;       // for custom commands
  nsCString     m_msgFetchAttribute; // for fetching custom msg attributes
  nsCString     m_customAttributeResult; // for fetching custom msg attributes
  nsCString     m_customCommandResult; // custom command response
  nsCString     m_customAddFlags;       // these two are for setting and clearing custom flags
  nsCString     m_customSubtractFlags;
  PRPackedBool m_validUrl;
  PRPackedBool m_runningUrl;
  PRPackedBool m_idsAreUids;
  PRPackedBool m_mimePartSelectorDetected;
  PRPackedBool m_allowContentChange;	// if PR_FALSE, we can't use Mime parts on demand
  PRPackedBool m_fetchPartsOnDemand; // if PR_TRUE, we should fetch leave parts on server.
  PRPackedBool m_msgLoadingFromCache; // if PR_TRUE, we might need to mark read on server
  PRPackedBool m_externalLinkUrl; // if PR_TRUE, we're running this url because the user
  PRPackedBool m_shouldStoreMsgOffline;
  PRPackedBool m_rerunningUrl; // first attempt running this failed with connection error; retrying
  nsImapContentModifiedType	m_contentModified;
  PRInt32 m_discoveryDepth;

  nsXPIDLCString  m_userName;
  nsXPIDLCString  m_serverKey;
  // event sinks
  imapMessageFlagsType	m_flags;
  nsImapAction	m_imapAction;

  nsWeakPtr m_imapFolder;
  nsWeakPtr m_imapMailFolderSink;
  nsWeakPtr m_imapMessageSink;
  nsWeakPtr m_imapExtensionSink;
  nsWeakPtr m_imapMiscellaneousSink;

  nsWeakPtr m_imapServerSink;

  // online message copy support; i don't have a better solution yet
  nsCOMPtr <nsISupports> m_copyState;   // now, refcounted.
  nsIFileSpec* m_fileSpec;
  nsCOMPtr<nsIImapMockChannel> m_mockChannel;

    // used by save message to disk
  nsCOMPtr<nsIFileSpec> m_messageFileSpec;
  PRBool                m_addDummyEnvelope;
  PRBool                m_canonicalLineEnding; // CRLF
  
  nsCString mURI; // the RDF URI associated with this url.
  nsCString mCharsetOverride; // used by nsIMsgI18NUrl...
};

#endif /* nsImapUrl_h___ */
