/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTITLEElement_h_
#define mozilla_dom_HTMLTITLEElement_h_

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsStubMutationObserver.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class HTMLTitleElement final : public nsGenericHTMLElement,
                               public nsStubMutationObserver
{
public:
  using Element::GetText;
  using Element::SetText;

  explicit HTMLTitleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  //HTMLTitleElement
  void GetText(DOMString& aText, ErrorResult& aError);
  void SetText(const nsAString& aText, ErrorResult& aError);

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) override;

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual void DoneAddingChildren(bool aHaveNotified) override;

protected:
  virtual ~HTMLTitleElement();

  virtual JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
    override final;

private:
  void SendTitleChangeEvent(bool aBound);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLTitleElement_h_
