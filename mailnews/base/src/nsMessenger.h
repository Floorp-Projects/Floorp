/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __nsMsgAppCore_h
#define __nsMsgAppCore_h

#include "nsCom.h"
#include "nscore.h"
#include "nsIMessenger.h"
#include "nsCOMPtr.h"
#include "nsITransactionManager.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsFileSpec.h"
#include "nsIDocShell.h"
#include "nsIStringBundle.h"
#include "nsIFindComponent.h"

class nsMessenger : public nsIMessenger
{
  
public:
  nsMessenger();
  virtual ~nsMessenger();

	NS_DECL_ISUPPORTS
  
	NS_DECL_NSIMESSENGER
    
  nsresult Alert(const char * stringName);
    nsresult SaveAttachment(nsIFileSpec *fileSpec, const char* unescapedUrl,
                            const char* messageUri, void *closure);
  nsresult PromptIfFileExists(nsFileSpec &fileSpec);

protected:
	nsresult DoDelete(nsIRDFCompositeDataSource* db, nsISupportsArray *srcArray, nsISupportsArray *deletedArray);
	nsresult DoCommand(nsIRDFCompositeDataSource *db, char * command, nsISupportsArray *srcArray, 
					   nsISupportsArray *arguments);
  PRUnichar *GetString(const PRUnichar *aStringName);
  nsresult InitStringBundle();

private:
  nsresult InitializeSearch(nsIFindComponent *finder);
  nsString mId;
  void *mScriptObject;
  nsCOMPtr<nsITransactionManager> mTxnMgr;

  /* rhp - need this to drive message display */
  nsIDOMWindowInternal              *mWindow;
  nsCOMPtr<nsIMsgWindow>    mMsgWindow;
  nsCOMPtr<nsIDocShell>     mDocShell;

  // String bundles...
  nsCOMPtr<nsIStringBundle>   mStringBundle;

  nsCOMPtr <nsIDocumentLoaderObserver> m_docLoaderObserver;

  PRBool      mCharsetInitialized;
  void        InitializeDisplayCharset();
  nsCOMPtr<nsISupports>  mSearchContext;
  nsCString   mLastDisplayURI; // this used when the user attempts to force a charset reload of a message...we need to get the last displayed
                               // uri so we can re-display it..
};

#define NS_MESSENGER_CID \
{ /* 3f181950-c14d-11d2-b7f2-00805f05ffa5 */      \
  0x3f181950, 0xc14d, 0x11d2,											\
    {0xb7, 0xf2, 0x0, 0x80, 0x5f, 0x05, 0xff, 0xa5}}


#endif
