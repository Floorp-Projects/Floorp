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
#ifndef __nsMsgQuote_h__
#define __nsMsgQuote_h__

#include "nsIMsgQuote.h" 
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsIMsgMessageService.h"
#include "nsIStreamListener.h"
#include "nsIMimeStreamConverter.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

class nsMsgQuote;

class nsMsgQuoteListener: public nsIMsgQuoteListener
{
public:
	nsMsgQuoteListener();
	virtual     ~nsMsgQuoteListener();

	NS_DECL_ISUPPORTS

	// nsIMimeStreamConverterListener support
	NS_DECL_NSIMIMESTREAMCONVERTERLISTENER
  NS_DECL_NSIMSGQUOTELISTENER

private:
  nsWeakPtr mMsgQuote;
};

class nsMsgQuote: public nsIMsgQuote, public nsSupportsWeakReference {
public: 
  nsMsgQuote();
  virtual ~nsMsgQuote();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGQUOTE

  // 
  // Implementation data...
  //
  nsCOMPtr<nsIStreamListener> mStreamListener;
  PRBool			mQuoteHeaders;
  nsCOMPtr<nsIMsgQuoteListener> mQuoteListener;
  nsCOMPtr<nsIChannel> mQuoteChannel;
};

#endif /* __nsMsgQuote_h__ */
