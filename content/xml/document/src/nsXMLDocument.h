/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLDocument_h___
#define nsXMLDocument_h___

#include "nsDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIScriptContext.h"

class nsIParser;
class nsIDOMNode;
class nsIURI;
class nsIChannel;

class nsXMLDocument : public nsDocument
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
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr);

  virtual void EndLoad();

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  virtual nsresult Init();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual void DocSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const;
  // DocSizeOfIncludingThis is inherited from nsIDocument.

protected:
  // mChannelIsPending indicates whether we're currently asynchronously loading
  // data from mChannel (via document.load() or normal load).  It's set to true
  // when we first find out about the channel (StartDocumentLoad) and set to
  // false in EndLoad or if ResetToURI() is called.  In the latter case our
  // mChannel is also cancelled.  Note that if this member is true, mChannel
  // cannot be null.
  bool mChannelIsPending;
  bool mAsync;
  bool mLoopingForSyncLoad;
};


#endif // nsXMLDocument_h___
