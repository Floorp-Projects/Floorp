/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _nsMimeXULEmitter_h_
#define _nsMimeXULEmitter_h_

#include "prtypes.h"
#include "prio.h"
#include "nsMimeBaseEmitter.h"
#include "nsIMimeEmitter.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIChannel.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMsgHeaderParser.h"
#include "nsCOMPtr.h"


typedef struct {
  nsString            contractID;
  nsIMimeMiscStatus   *obj;
} miscStatusType;

class nsMimeXULEmitter : public nsMimeBaseEmitter {
public: 
    nsMimeXULEmitter ();
    virtual       ~nsMimeXULEmitter (void);

    // Header handling output routines.
    NS_IMETHOD    AddHeaderField(const char *field, const char *value);
    NS_IMETHOD    EndHeader();

    // Body handling routines
    NS_IMETHOD    StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset);
    NS_IMETHOD    WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten);
    NS_IMETHOD    EndBody();

    // Complete() the emitter operation.
    NS_IMETHOD    Complete();

    //
    // XUL Specific Output methods
    //
    NS_IMETHOD    WriteXULHeader();
    NS_IMETHOD    WriteXULTag(const char *tagName, const char *value);
    NS_IMETHOD    WriteMiscXULTag(const char *tagName, const char *value);
    NS_IMETHOD    WriteEmailAddrXULTag(const char *tagName, const char *value);
    nsresult      OutputEmailAddresses(const char *aHeader, const char *aEmailAddrs);
    nsresult      ProcessSingleEmailEntry(const char *curHeader, char *curName, char *curAddress);
    nsresult      WriteXULTagPrefix(const char *tagName, const char *value);
    nsresult      WriteXULTagPostfix(const char *tagName, const char *value);

    // For Interesting XUL output!
    nsresult      DumpAttachmentMenu();
    nsresult      DumpAddBookIcon(char *fromLine);
    nsresult      DumpSubjectFromDate();
    nsresult      DumpToCC();
    nsresult      DumpRestOfHeaders();
    nsresult      DumpBody();
    nsresult      OutputGenericHeader(const char *aHeaderVal);

    // For storing recipient info in the history database...
    nsresult      DoSpecialSenderProcessing(const char *field, const char *value);

    // For per-email address status processing...
    nsresult      DoGlobalStatusProcessing();
    nsresult      DoWindowStatusProcessing();
    nsresult      BuildListOfStatusProviders();
    nsIMimeMiscStatus   *GetStatusObjForContractID(nsCString aContractID);

protected:
    PRInt32             mCutoffValue;

    // For fancy per-email status!
    nsVoidArray         *mMiscStatusArray;
};


/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeXULEmitter(const nsIID& iid, void **result);

#endif /* _nsMimeXULEmitter_h_ */
