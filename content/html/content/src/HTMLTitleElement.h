/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTITLEElement_h_
#define mozilla_dom_HTMLTITLEElement_h_

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLTitleElement.h"
#include "nsGenericHTMLElement.h"
#include "nsStubMutationObserver.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class HTMLTitleElement MOZ_FINAL : public nsGenericHTMLElement,
                                   public nsIDOMHTMLTitleElement,
                                   public nsStubMutationObserver
{
public:
  using Element::GetText;
  using Element::SetText;

  HTMLTitleElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~HTMLTitleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLTitleElement
  NS_DECL_NSIDOMHTMLTITLEELEMENT

  //HTMLTitleElement
  //The xpcom GetTextContent() never fails so we just use that.
  void SetText(const nsAString& aText, ErrorResult& aError)
  {
    aError = SetText(aText);
  }

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  virtual void DoneAddingChildren(bool aHaveNotified) MOZ_OVERRIDE;

protected:

  virtual JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> scope)
    MOZ_OVERRIDE MOZ_FINAL;

private:
  void SendTitleChangeEvent(bool aBound);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLTitleElement_h_
