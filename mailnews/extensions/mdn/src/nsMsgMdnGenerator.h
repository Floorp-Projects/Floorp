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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Tsai <jt95070@netscape.net>
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

#ifndef _nsMsgMdnGenerator_H_
#define _nsMsgMdnGenerator_H_

#include "nsIMsgMdnGenerator.h"
#include "nsCOMPtr.h"
#include "nsIMimeConverter.h"
#include "nsIUrlListener.h"
#include "nsXPIDLString.h"
#include "nsIMsgIncomingServer.h"
#include "nsFileStream.h"
#include "nsIOutputStream.h"
#include "nsIFileSpec.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgWindow.h"
#include "nsIMimeHeaders.h"

#define eNeverSendOp ((PRInt32) 0)
#define eAutoSendOp ((PRInt32) 1)
#define eAskMeOp ((PRInt32) 2)
#define eDeniedOp ((PRInt32) 3)

class nsMsgMdnGenerator : public nsIMsgMdnGenerator, public nsIUrlListener
{
public:
    NS_DECL_ISUPPORTS 
    NS_DECL_NSIMSGMDNGENERATOR
    NS_DECL_NSIURLLISTENER
    
    nsMsgMdnGenerator();
    virtual ~nsMsgMdnGenerator();
    
private:
    // Sanity Check methods
    PRBool ProcessSendMode(); // must called prior ValidateReturnPath
    PRBool ValidateReturnPath();
    PRBool NotInToOrCc();
    PRBool MailAddrMatch(const char *addr1, const char *addr2);
    
    nsresult StoreMDNSentFlag(nsIMsgFolder *folder, nsMsgKey key);
    
    nsresult CreateMdnMsg();
    nsresult CreateFirstPart(); 
    nsresult CreateSecondPart();
    nsresult CreateThirdPart();
    nsresult SendMdnMsg();

    // string bundle helper methods
    nsresult GetStringFromName(const PRUnichar *aName, PRUnichar **aResultString);
    nsresult FormatStringFromName(const PRUnichar *aName, 
                                                 const PRUnichar *aString, 
                                                 PRUnichar **aResultString);
    
    // other helper methods
    nsresult InitAndProcess();
    nsresult OutputAllHeaders();
    nsresult WriteString(const char *str);
  
private:
    EDisposeType m_disposeType;
    nsCOMPtr<nsIMsgWindow> m_window;
    nsCOMPtr<nsIOutputStream> m_outputStream;
    nsCOMPtr<nsIFileSpec> m_fileSpec;
    nsCOMPtr<nsIMsgIdentity> m_identity;
    nsXPIDLCString m_charset;
    nsXPIDLCString m_email;
    nsXPIDLCString m_mimeSeparator;
    nsXPIDLCString m_messageId;
    nsCOMPtr<nsIMsgFolder> m_folder;
    nsCOMPtr<nsIMsgIncomingServer> m_server;
    nsCOMPtr<nsIMimeHeaders> m_headers;
    nsXPIDLCString m_dntRrt;
    PRInt32 m_notInToCcOp;
    PRInt32 m_outsideDomainOp;
    PRInt32 m_otherOp;
    PRPackedBool m_reallySendMdn;
    PRPackedBool m_autoSend;
    PRPackedBool m_autoAction;
    PRPackedBool m_mdnEnabled;
};

#endif // _nsMsgMdnGenerator_H_

