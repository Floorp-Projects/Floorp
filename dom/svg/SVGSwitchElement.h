/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGSwitchElement_h
#define mozilla_dom_SVGSwitchElement_h

#include "mozilla/dom/SVGGraphicsElement.h"
#include "nsCOMPtr.h"

class nsSVGSwitchFrame;

nsresult NS_NewSVGSwitchElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGGraphicsElement SVGSwitchElementBase;

class SVGSwitchElement final : public SVGSwitchElementBase {
  friend class ::nsSVGSwitchFrame;

 protected:
  friend nsresult(::NS_NewSVGSwitchElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGSwitchElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGSwitchElement() = default;
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  nsIContent* GetActiveChild() const { return mActiveChild; }
  void MaybeInvalidate();

  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGSwitchElement,
                                           SVGSwitchElementBase)
  // nsINode
  virtual nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                     bool aNotify) override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 private:
  void UpdateActiveChild() { mActiveChild = FindActiveChild(); }
  nsIContent* FindActiveChild() const;

  // only this child will be displayed
  nsCOMPtr<nsIContent> mActiveChild;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGSwitchElement_h
