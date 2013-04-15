/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLDocument_h
#define mozilla_dom_XMLDocument_h

#include "nsDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIScriptContext.h"

class nsIParser;
class nsIDOMNode;
class nsIURI;
class nsIChannel;

namespace mozilla {
namespace dom {

class XMLDocument : public nsDocument
{
public:
  XMLDocument(const char* aContentType = "application/xml");
  virtual ~XMLDocument();

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

  virtual void DocSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const;
  // DocSizeOfIncludingThis is inherited from nsIDocument.


  // WebIDL API
  bool Load(const nsAString& aUrl, mozilla::ErrorResult& aRv);
  bool Async() const
  {
    return mAsync;
  }
  // The XPCOM SetAsync is ok for us

  // .location is [Unforgeable], so we have to make it clear that the
  // nsIDocument version applies to us (it's shadowed by the XPCOM thing on
  // nsDocument).
  using nsIDocument::GetLocation;
  // But then we need to also pull in the nsDocument XPCOM version
  // because nsXULDocument tries to forward to it.
  using nsDocument::GetLocation;

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope) MOZ_OVERRIDE;

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

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XMLDocument_h
