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
#ifndef nsDetectionAdaptor_h__
#define nsDetectionAdaptor_h__

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIWebShellServices.h"

#include "nsIParserFilter.h"
static NS_DEFINE_IID(kIParserFilterIID, NS_IPARSERFILTER_IID);

class CToken;

//--------------------------------------------------------------
class nsMyObserver : public nsICharsetDetectionObserver
{
 public:
   NS_DECL_ISUPPORTS

 public:
   nsMyObserver( void )
   {
     NS_INIT_REFCNT();
     mWebShellSvc = nsnull;
     mNotifyByReload = PR_FALSE;
     mWeakRefDocument = nsnull;
     mWeakRefParser = nsnull;
   }
   virtual  ~nsMyObserver( void )
   {
     // do not release nor delete mWeakRefDocument
     // do not release nor delete mWeakRefParser
   }


   // Methods to support nsICharsetDetectionAdaptor
   NS_IMETHOD Init(nsIWebShellServices* aWebShellSvc, 
                   nsIDocument* aDocument,
                   nsIParser* aParser,
                   const PRUnichar* aCharset,
                   const char* aCommand);

   // Methods to support nsICharsetDetectionObserver
   NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf);
   void SetNotifyByReload(PRBool aByReload) { mNotifyByReload = aByReload; };
 private:
     nsCOMPtr<nsIWebShellServices> mWebShellSvc;
     PRBool mNotifyByReload;

     //The adaptor is owned by parser as filter, and adaptor owns detector, 
     //which in turn owns observer. Parser also live within the lifespan of 
     //document. The ownership tree is like:
     //  document->parser->adaptor->detector->observer
     //We do not want to own Document & Parser to avoid ownership loop. That 
     //will cause memory leakage. 
     //If in future this chain got changed, ie. parser outlives document, or 
     //detector outlives parser, we might want to change weak reference here. 
     nsIDocument* mWeakRefDocument;
     nsIParser* mWeakRefParser;
     nsAutoString mCharset;
     nsCAutoString mCommand;
};

class nsDetectionAdaptor : 
                           public nsIParserFilter,
                           public nsICharsetDetectionAdaptor
{
 public:
   NS_DECL_ISUPPORTS

 public:
   nsDetectionAdaptor( void );
   virtual  ~nsDetectionAdaptor( void );

   // Methods to support nsICharsetDetectionAdaptor
   NS_IMETHOD Init(nsIWebShellServices* aWebShellSvc, nsICharsetDetector *aDetector, 
                   nsIDocument* aDocument,
                   nsIParser* aParser,
                   const PRUnichar* aCharset,
                   const char* aCommand=nsnull);
  
   // Methode to suppor nsIParserFilter
   NS_IMETHOD RawBuffer(const char * buffer, PRUint32 * buffer_length) ;
   NS_IMETHOD Finish();

   // really don't care the following two, only because they are defined
   // in nsIParserFilter.h
   NS_IMETHOD WillAddToken(CToken & token) { return NS_OK; };
   NS_IMETHOD ProcessTokens( void ) {return NS_OK;};

  private:
     nsCOMPtr<nsICharsetDetector> mDetector;
     PRBool mDontFeedToDetector;
     nsCOMPtr<nsMyObserver> mObserver; 
};

#endif /* nsDetectionAdaptor_h__ */
