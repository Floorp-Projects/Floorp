/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsString.h"
#include "plstr.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsIParser.h"
#include "nsIDocument.h"
#include "nsDetectionAdaptor.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//--------------------------------------------------------------
NS_IMETHODIMP nsMyObserver::Notify(
    const char* aCharset, nsDetectionConfident aConf)
{
    nsresult rv = NS_OK;
    if(!mCharset.EqualsWithConversion(aCharset)) {
      if(mNotifyByReload) {
        rv = mWebShellSvc->SetRendering( PR_FALSE);
        rv = mWebShellSvc->StopDocumentLoad();
        rv = mWebShellSvc->ReloadDocument(aCharset, kCharsetFromAutoDetection);
      } else {
        nsAutoString newcharset; newcharset.AssignWithConversion(aCharset);
        if(mWeakRefDocument) {
             mWeakRefDocument->SetDocumentCharacterSet(newcharset);
        }
        if(mWeakRefParser) {
             mWeakRefParser->SetDocumentCharset(newcharset, kCharsetFromAutoDetection);
        }
      }
    }
    return NS_OK;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsMyObserver::Init( nsIWebShellServices* aWebShellSvc, 
                   nsIDocument* aDocument,
                   nsIParser* aParser,
                   const PRUnichar* aCharset,
                   const char* aCommand)
{
    if(aCommand) {
        mCommand = aCommand;
    }
    if(aCharset) {
        mCharset = aCharset;
    }
    if(aDocument) {
        mWeakRefDocument = aDocument;
    }
    if(aParser) {
        mWeakRefParser = aParser;
    }
    if(nsnull != aWebShellSvc)
    {
        mWebShellSvc = aWebShellSvc;
        return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
}
//--------------------------------------------------------------
NS_IMPL_ISUPPORTS1 ( nsMyObserver ,nsICharsetDetectionObserver);

//--------------------------------------------------------------
nsDetectionAdaptor::nsDetectionAdaptor( void ) 
{
     NS_INIT_REFCNT();
     mDontFeedToDetector = PR_TRUE;
}
//--------------------------------------------------------------
nsDetectionAdaptor::~nsDetectionAdaptor()
{
}

//--------------------------------------------------------------
NS_IMPL_ISUPPORTS2 (nsDetectionAdaptor, nsIParserFilter, nsICharsetDetectionAdaptor);

//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::Init(
    nsIWebShellServices* aWebShellSvc, nsICharsetDetector *aDetector,
    nsIDocument* aDocument, nsIParser* aParser, const PRUnichar* aCharset,
    const char* aCommand)
{
  if((nsnull != aWebShellSvc) && (nsnull != aDetector) && (nsnull != aCharset))
  {
    nsresult rv = NS_OK;
    mObserver = new nsMyObserver();
    if(!mObserver)
       return NS_ERROR_OUT_OF_MEMORY;

    rv = mObserver->Init(aWebShellSvc, aDocument, aParser, aCharset, aCommand);
    if(NS_SUCCEEDED(rv)) {
      rv = aDetector->Init(mObserver.get());
      if(NS_SUCCEEDED(rv)) {
        mDetector = aDetector;
        mDontFeedToDetector = PR_FALSE;
        return NS_OK;
      }
    }
  }
  return NS_ERROR_ILLEGAL_VALUE;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::RawBuffer
    (const char * buffer, PRUint32 * buffer_length) 
{
    if((mDontFeedToDetector) || (!mDetector))
       return NS_OK;
    nsresult rv = NS_OK;
    rv = mDetector->DoIt((const char*)buffer, *buffer_length, &mDontFeedToDetector);
    if(mObserver) 
       mObserver->SetNotifyByReload(PR_TRUE);

    return NS_OK;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::Finish()
{
    if((mDontFeedToDetector) || (!mDetector))
       return NS_OK;
    nsresult rv = NS_OK;
    rv = mDetector->Done();

    return NS_OK;
}

