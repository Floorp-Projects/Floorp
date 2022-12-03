/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTITLEELEMENT_H_
#define DOM_SVG_SVGTITLEELEMENT_H_

#include "mozilla/Attributes.h"
#include "SVGElement.h"
#include "nsStubMutationObserver.h"

nsresult NS_NewSVGTitleElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
namespace mozilla::dom {

using SVGTitleElementBase = SVGElement;

class SVGTitleElement final : public SVGTitleElementBase,
                              public nsStubMutationObserver {
 protected:
  friend nsresult(::NS_NewSVGTitleElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGTitleElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGTitleElement() = default;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult BindToTree(BindContext&, nsINode& aParent) override;

  void UnbindFromTree(bool aNullParent = true) override;

  void DoneAddingChildren(bool aHaveNotified) override;

 private:
  void SendTitleChangeEvent(bool aBound);
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGTITLEELEMENT_H_
