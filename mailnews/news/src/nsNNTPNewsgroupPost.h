/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __nsNNTPNewsgroupPost_h
#define __nsNNTPNewsgroupPost_h

#include "msgCore.h"
#include "nsINNTPNewsgroupPost.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "prmem.h"
#include "nsISupportsObsolete.h"

#define IDX_HEADER_FROM             0
#define IDX_HEADER_NEWSGROUPS       1
#define IDX_HEADER_SUBJECT          2

// set this to the last required header
#define IDX_HEADER_LAST_REQUIRED    IDX_HEADER_SUBJECT

#define IDX_HEADER_PATH             3
#define IDX_HEADER_DATE             4

#define IDX_HEADER_REPLYTO          5
#define IDX_HEADER_SENDER           6
#define IDX_HEADER_FOLLOWUPTO       7
#define IDX_HEADER_DATERECEIVED     8
#define IDX_HEADER_EXPIRES          9
#define IDX_HEADER_CONTROL          10
#define IDX_HEADER_DISTRIBUTION     11
#define IDX_HEADER_ORGANIZATION     12
#define IDX_HEADER_REFERENCES       13

// stuff that's required to be in the message,
// but probably generated on the server
#define IDX_HEADER_RELAYVERSION     14
#define IDX_HEADER_POSTINGVERSION   15
#define IDX_HEADER_MESSAGEID        16

// keep this in sync with the above
#define HEADER_LAST                 IDX_HEADER_MESSAGEID

class nsNNTPNewsgroupPost : public nsINNTPNewsgroupPost {
    
public:
    nsNNTPNewsgroupPost();
    virtual ~nsNNTPNewsgroupPost();
    
    NS_DECL_ISUPPORTS
    
    // Required headers
    NS_IMPL_CLASS_GETSET_STR(RelayVersion, m_header[IDX_HEADER_RELAYVERSION]);
    NS_IMPL_CLASS_GETSET_STR(PostingVersion, m_header[IDX_HEADER_POSTINGVERSION]);
    NS_IMPL_CLASS_GETSET_STR(From, m_header[IDX_HEADER_FROM]);
    NS_IMPL_CLASS_GETSET_STR(Date, m_header[IDX_HEADER_DATE]);
    NS_IMPL_CLASS_GETSET_STR(Subject, m_header[IDX_HEADER_SUBJECT]);

    NS_IMPL_CLASS_GETTER_STR(GetNewsgroups, m_header[IDX_HEADER_NEWSGROUPS]); 
    NS_IMPL_CLASS_GETSET_STR(Path, m_header[IDX_HEADER_PATH]);

    // Optional Headers
    NS_IMPL_CLASS_GETSET_STR(ReplyTo, m_header[IDX_HEADER_REPLYTO]);
    NS_IMPL_CLASS_GETSET_STR(Sender, m_header[IDX_HEADER_SENDER]);
    NS_IMPL_CLASS_GETSET_STR(FollowupTo, m_header[IDX_HEADER_FOLLOWUPTO]);
    NS_IMPL_CLASS_GETSET_STR(DateRecieved, m_header[IDX_HEADER_DATERECEIVED]);
    NS_IMPL_CLASS_GETSET_STR(Expires, m_header[IDX_HEADER_EXPIRES]);
    NS_IMPL_CLASS_GETSET_STR(Control, m_header[IDX_HEADER_CONTROL]);
    NS_IMPL_CLASS_GETSET_STR(Distribution, m_header[IDX_HEADER_DISTRIBUTION]);
    NS_IMPL_CLASS_GETSET_STR(Organization, m_header[IDX_HEADER_ORGANIZATION]);
    NS_IMPL_CLASS_GETSET_STR(Body, m_body);    
    NS_IMPL_CLASS_GETTER_STR(GetReferences, m_header[IDX_HEADER_REFERENCES]);

    NS_IMPL_CLASS_GETTER(GetIsControl, PRBool, m_isControl);

    // the message can be stored in a file....allow accessors for getting and setting
    // the file name to post...
    NS_IMETHOD SetPostMessageFile(nsIFileSpec * aFileName);
    NS_IMETHOD GetPostMessageFile(nsIFileSpec ** aFileName);

    NS_IMETHOD AddNewsgroup(const char *newsgroupName);
    
private:
    nsIFileSpec *m_postMessageFile;
    char *m_header[HEADER_LAST+1];
    char *m_body;
    char *m_messageBuffer;
    PRBool m_isControl;
};

#endif /* __nsNNTPNewsgroupPost_h */
