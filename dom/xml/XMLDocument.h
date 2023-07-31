/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLDocument_h
#define mozilla_dom_XMLDocument_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Document.h"
#include "nsIScriptContext.h"

class nsIURI;
class nsIChannel;

namespace mozilla::dom {

class XMLDocument : public Document {
 public:
  explicit XMLDocument(const char* aContentType = "application/xml");

  NS_INLINE_DECL_REFCOUNTING_INHERITED(XMLDocument, Document)

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) override;
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal,
                          nsIPrincipal* aPartitionedPrincipal) override;

  virtual void SetSuppressParserErrorElement(bool aSuppress) override;
  virtual bool SuppressParserErrorElement() override;

  virtual void SetSuppressParserErrorConsoleMessages(bool aSuppress) override;
  virtual bool SuppressParserErrorConsoleMessages() override;

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* channel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset = true) override;

  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void EndLoad() override;

  virtual nsresult Init(nsIPrincipal* aPrincipal,
                        nsIPrincipal* aPartitionedPrincipal) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual void DocAddSizeOfExcludingThis(
      nsWindowSizes& aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from Document.

  // .location is [Unforgeable], so we have to make it clear that the Document
  // version applies to us (it's shadowed by the XPCOM thing on Document).
  using Document::GetLocation;

 protected:
  virtual ~XMLDocument() = default;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  friend nsresult(::NS_NewXMLDocument)(Document**, nsIPrincipal*, nsIPrincipal*,
                                       bool, bool);

  // mChannelIsPending indicates whether we're currently asynchronously loading
  // data from mChannel.  It's set to true when we first find out about the
  // channel (StartDocumentLoad) and set to false in EndLoad or if ResetToURI()
  // is called.  In the latter case our mChannel is also cancelled.  Note that
  // if this member is true, mChannel cannot be null.
  bool mChannelIsPending;

  // If true. we're really a Document, not an XMLDocument
  bool mIsPlainDocument;

  // If true, do not output <parsererror> elements. Per spec, XMLHttpRequest
  // shouldn't output them, whereas DOMParser/others should (see bug 918703).
  bool mSuppressParserErrorElement;

  // If true, do not log parsing errors to the web console (see bug 884693).
  bool mSuppressParserErrorConsoleMessages;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XMLDocument_h
