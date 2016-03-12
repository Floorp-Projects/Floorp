/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLDetailsElement_h
#define mozilla_dom_HTMLDetailsElement_h

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

// HTMLDetailsElement implements the <details> tag, which is used as a
// disclosure widget from which the user can obtain additional information or
// controls. Please see the spec for more information.
// https://html.spec.whatwg.org/multipage/forms.html#the-details-element
//
class HTMLDetailsElement final : public nsGenericHTMLElement
{
public:
  using NodeInfo = mozilla::dom::NodeInfo;

  static bool IsDetailsEnabled();

  explicit HTMLDetailsElement(already_AddRefed<NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLDetailsElement, details)

  nsIContent* GetFirstSummary() const;

  nsresult Clone(NodeInfo* aNodeInfo, nsINode** aResult) const override;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const override;

  nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                         nsAttrValueOrString* aValue, bool aNotify) override;

  // HTMLDetailsElement WebIDL
  bool Open() const { return GetBoolAttr(nsGkAtoms::open); }

  void SetOpen(bool aOpen, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::open, aOpen, aError);
  }

  void ToggleOpen()
  {
    ErrorResult rv;
    SetOpen(!Open(), rv);
    rv.SuppressException();
  }

protected:
  virtual ~HTMLDetailsElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

  class ToggleEventDispatcher final : public AsyncEventDispatcher
  {
  public:
    // According to the html spec, a 'toggle' event is a simple event which does
    // not bubble.
    explicit ToggleEventDispatcher(nsINode* aTarget)
      : AsyncEventDispatcher(aTarget, NS_LITERAL_STRING("toggle"), false)
    {
    }

    NS_IMETHOD Run() override
    {
      auto* details = static_cast<HTMLDetailsElement*>(mTarget.get());
      details->mToggleEventDispatcher = nullptr;
      return AsyncEventDispatcher::Run();
    }
  };

  RefPtr<ToggleEventDispatcher> mToggleEventDispatcher;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLDetailsElement_h */
