/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLDocument_h
#define mozilla_dom_XMLDocument_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIScriptContext.h"

class nsIURI;
class nsIChannel;

namespace mozilla {
namespace dom {

class XMLDocument : public nsDocument,
                    public nsIDOMXMLDocument
{
public:
  explicit XMLDocument(const char* aContentType = "application/xml");

  NS_DECL_ISUPPORTS_INHERITED

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) override;
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal) override;

  virtual void SetSuppressParserErrorElement(bool aSuppress) override;
  virtual bool SuppressParserErrorElement() override;

  virtual void SetSuppressParserErrorConsoleMessages(bool aSuppress) override;
  virtual bool SuppressParserErrorConsoleMessages() override;

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* channel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr) override;

  virtual void EndLoad() override;

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  virtual nsresult Init() override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.


  // WebIDL API
  bool Load(const nsAString& aUrl, CallerType aCallerType, ErrorResult& aRv);
  bool Async() const
  {
    return mAsync;
  }
  void SetAsync(bool aAsync)
  {
    mAsync = aAsync;
  }

  // .location is [Unforgeable], so we have to make it clear that the
  // nsIDocument version applies to us (it's shadowed by the XPCOM thing on
  // nsDocument).
  using nsIDocument::GetLocation;
  // But then we need to also pull in the nsDocument XPCOM version
  // because nsXULDocument tries to forward to it.
  using nsDocument::GetLocation;

protected:
  virtual ~XMLDocument();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

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

  // If true, do not output <parsererror> elements. Per spec, XMLHttpRequest
  // shouldn't output them, whereas DOMParser/others should (see bug 918703).
  bool mSuppressParserErrorElement;

  // If true, do not log parsing errors to the web console (see bug 884693).
  bool mSuppressParserErrorConsoleMessages;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XMLDocument_h
