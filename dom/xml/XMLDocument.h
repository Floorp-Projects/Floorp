/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLDocument_h
#define mozilla_dom_XMLDocument_h

#include "mozilla/Attributes.h"
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
  explicit XMLDocument(const char* aContentType = "application/xml");

  NS_DECL_ISUPPORTS_INHERITED

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) MOZ_OVERRIDE;
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal) MOZ_OVERRIDE;

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* channel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr) MOZ_OVERRIDE;

  virtual void EndLoad() MOZ_OVERRIDE;

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  virtual nsresult Init() MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const MOZ_OVERRIDE;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.


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
  virtual ~XMLDocument();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  friend nsresult (::NS_NewXMLDocument)(nsIDocument**, bool, bool);


  // mChannelIsPending indicates whether we're currently asynchronously loading
  // data from mChannel (via document.load() or normal load).  It's set to true
  // when we first find out about the channel (StartDocumentLoad) and set to
  // false in EndLoad or if ResetToURI() is called.  In the latter case our
  // mChannel is also cancelled.  Note that if this member is true, mChannel
  // cannot be null.
  bool mChannelIsPending;
  bool mAsync;
  bool mLoopingForSyncLoad;

  // If true. we're really a Document, not an XMLDocument
  bool mIsPlainDocument;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XMLDocument_h
