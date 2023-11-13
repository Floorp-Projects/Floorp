/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDetailsElement.h"

#include "mozilla/dom/HTMLDetailsElementBinding.h"
#include "mozilla/dom/HTMLSummaryElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ScopeExit.h"
#include "nsContentUtils.h"
#include "nsTextNode.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Details)

namespace mozilla::dom {

HTMLDetailsElement::~HTMLDetailsElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLDetailsElement)

HTMLDetailsElement::HTMLDetailsElement(already_AddRefed<NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {
  SetupShadowTree();
}

HTMLSummaryElement* HTMLDetailsElement::GetFirstSummary() const {
  // XXX: Bug 1245032: Might want to cache the first summary element.
  for (nsIContent* child = nsINode::GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (auto* summary = HTMLSummaryElement::FromNode(child)) {
      return summary;
    }
  }
  return nullptr;
}

void HTMLDetailsElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                      const nsAttrValue* aValue,
                                      const nsAttrValue* aOldValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::open) {
    bool wasOpen = !!aOldValue;
    bool isOpen = !!aValue;
    if (wasOpen != isOpen) {
      auto stringForState = [](bool aOpen) {
        return aOpen ? u"open"_ns : u"closed"_ns;
      };
      nsAutoString oldState;
      if (mToggleEventDispatcher) {
        oldState.Truncate();
        static_cast<ToggleEvent*>(mToggleEventDispatcher->mEvent.get())
            ->GetOldState(oldState);
        mToggleEventDispatcher->Cancel();
      } else {
        oldState.Assign(stringForState(wasOpen));
      }
      RefPtr<ToggleEvent> toggleEvent = CreateToggleEvent(
          u"toggle"_ns, oldState, stringForState(isOpen), Cancelable::eNo);
      mToggleEventDispatcher =
          new AsyncEventDispatcher(this, toggleEvent.forget());
      mToggleEventDispatcher->PostDOMEvent();
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

void HTMLDetailsElement::SetupShadowTree() {
  const bool kNotify = false;
  AttachAndSetUAShadowRoot(NotifyUAWidgetSetup::No);
  RefPtr<ShadowRoot> sr = GetShadowRoot();
  if (NS_WARN_IF(!sr)) {
    return;
  }

  nsNodeInfoManager* nim = OwnerDoc()->NodeInfoManager();
  RefPtr<NodeInfo> slotNodeInfo = nim->GetNodeInfo(
      nsGkAtoms::slot, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
  {
    RefPtr<NodeInfo> linkNodeInfo = nim->GetNodeInfo(
        nsGkAtoms::link, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
    RefPtr<nsGenericHTMLElement> link =
        NS_NewHTMLLinkElement(linkNodeInfo.forget());
    if (NS_WARN_IF(!link)) {
      return;
    }
    link->SetAttr(nsGkAtoms::rel, u"stylesheet"_ns, IgnoreErrors());
    link->SetAttr(nsGkAtoms::href,
                  u"resource://content-accessible/details.css"_ns,
                  IgnoreErrors());
    sr->AppendChildTo(link, kNotify, IgnoreErrors());
  }
  {
    RefPtr<nsGenericHTMLElement> slot =
        NS_NewHTMLSlotElement(do_AddRef(slotNodeInfo));
    if (NS_WARN_IF(!slot)) {
      return;
    }
    slot->SetAttr(kNameSpaceID_None, nsGkAtoms::name,
                  u"internal-main-summary"_ns, kNotify);
    sr->AppendChildTo(slot, kNotify, IgnoreErrors());

    RefPtr<NodeInfo> summaryNodeInfo = nim->GetNodeInfo(
        nsGkAtoms::summary, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
    RefPtr<nsGenericHTMLElement> summary =
        NS_NewHTMLSummaryElement(summaryNodeInfo.forget());
    if (NS_WARN_IF(!summary)) {
      return;
    }

    nsAutoString defaultSummaryText;
    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "DefaultSummary", defaultSummaryText);
    RefPtr<nsTextNode> description = new (nim) nsTextNode(nim);
    description->SetText(defaultSummaryText, kNotify);
    summary->AppendChildTo(description, kNotify, IgnoreErrors());

    slot->AppendChildTo(summary, kNotify, IgnoreErrors());
  }
  {
    RefPtr<nsGenericHTMLElement> slot =
        NS_NewHTMLSlotElement(slotNodeInfo.forget());
    if (NS_WARN_IF(!slot)) {
      return;
    }
    sr->AppendChildTo(slot, kNotify, IgnoreErrors());
  }
}

void HTMLDetailsElement::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  if (mToggleEventDispatcher == aEvent) {
    mToggleEventDispatcher = nullptr;
  }
}

JSObject* HTMLDetailsElement::WrapNode(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return HTMLDetailsElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLDetailsElement::HandleInvokeInternal(nsAtom* aAction,
                                              ErrorResult& aRv) {
  if (nsContentUtils::EqualsIgnoreASCIICase(aAction, nsGkAtoms::_auto) ||
      nsContentUtils::EqualsIgnoreASCIICase(aAction, nsGkAtoms::toggle)) {
    ToggleOpen();
  } else if (nsContentUtils::EqualsIgnoreASCIICase(aAction, nsGkAtoms::close)) {
    if (Open()) {
      SetOpen(false, IgnoreErrors());
    }
  } else if (nsContentUtils::EqualsIgnoreASCIICase(aAction, nsGkAtoms::open)) {
    if (!Open()) {
      SetOpen(true, IgnoreErrors());
    }
  }
}

}  // namespace mozilla::dom
