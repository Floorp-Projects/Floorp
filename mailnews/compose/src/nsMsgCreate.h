/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgCreate_H_
#define _nsMsgCreate_H_

#include "nsIMsgDraft.h" 
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsIMsgMessageService.h"
#include "nsIMimeStreamConverter.h"

class nsMsgDraft: public nsIMsgDraft {
public: 
	nsMsgDraft();
	virtual ~nsMsgDraft();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

  /* long QuoteMessage (in wstring msgURI, in nsIOutputStream outStream, nsIMessage **aMsgToReplace); */
  NS_IMETHOD  OpenDraftMsg(const PRUnichar *msgURI, nsIMessage **aMsgToReplace);

  /* long QuoteMessage (in wstring msgURI, in nsIOutputStream outStream, nsIMessage **aMsgToReplace); */
  NS_IMETHOD  OpenEditorTemplate(const PRUnichar *msgURI, nsIMessage **aMsgToReplace);

  nsresult    ProcessDraftOrTemplateOperation(const PRUnichar *msgURI, nsMimeOutputType aOutType,
                                              nsIMessage **aMsgToReplace);

  // 
  // Implementation data...
  //
  nsFileSpec            *mTmpFileSpec;
  nsIFileSpec           *mTmpIFileSpec;
  char                  *mURI;
  nsIMsgMessageService  *mMessageService;
  nsMimeOutputType      mOutType;
};

// Will be used by factory to generate a nsMsgQuote class...
nsresult      NS_NewMsgDraft(const nsIID &aIID, void ** aInstancePtrResult);

nsIMessage * GetIMessageFromURI(const PRUnichar *msgURI);


#endif /* _nsMsgCreate_H_ */
