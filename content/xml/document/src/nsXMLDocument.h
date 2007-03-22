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
 * The Original Code is Mozilla Communicator client code.
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

#ifndef nsXMLDocument_h___
#define nsXMLDocument_h___

#include "nsDocument.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIChannelEventSink.h"
#include "nsIDOMXMLDocument.h"
#include "nsIScriptContext.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"

class nsIParser;
class nsIDOMNode;
class nsIURI;
class nsIChannel;

class nsXMLDocument : public nsDocument,
                      public nsIInterfaceRequestor,
                      public nsIChannelEventSink
{
public:
  nsXMLDocument(const char* aContentType = "application/xml");
  virtual ~nsXMLDocument();

  NS_DECL_ISUPPORTS_INHERITED

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal);

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* channel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     PRBool aReset = PR_TRUE,
                                     nsIContentSink* aSink = nsnull);

  virtual void EndLoad();

  virtual PRBool IsLoadedAsData();

  // nsIDOMNode interface
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // nsIDOMDocument interface
  NS_IMETHOD GetElementById(const nsAString& aElementId,
                            nsIDOMElement** aReturn);

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsIHTTPEventSink
  NS_DECL_NSICHANNELEVENTSINK

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  virtual nsresult Init();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsXMLDocument, nsDocument)

protected:
  virtual nsresult GetLoadGroup(nsILoadGroup **aLoadGroup);

  nsCOMPtr<nsIScriptContext> mScriptContext;

  // mChannelIsPending indicates whether we're currently asynchronously loading
  // data from mChannel (via document.load() or normal load).  It's set to true
  // when we first find out about the channel (StartDocumentLoad) and set to
  // false in EndLoad or if ResetToURI() is called.  In the latter case our
  // mChannel is also cancelled.  Note that if this member is true, mChannel
  // cannot be null.
  PRPackedBool mChannelIsPending;
  PRPackedBool mCrossSiteAccessEnabled;
  PRPackedBool mLoadedAsData;
  PRPackedBool mLoadedAsInteractiveData;
  PRPackedBool mAsync;
  PRPackedBool mLoopingForSyncLoad;
};


#endif // nsXMLDocument_h___
