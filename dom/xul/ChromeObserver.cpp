/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ChromeObserver.h"

#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIFrame.h"

#include "nsContentUtils.h"
#include "nsView.h"
#include "nsPresContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsXULElement.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(ChromeObserver, nsIMutationObserver)

ChromeObserver::ChromeObserver(Document* aDocument)
    : nsStubMutationObserver(), mDocument(aDocument) {}

ChromeObserver::~ChromeObserver() = default;

void ChromeObserver::Init() {
  mDocument->AddMutationObserver(this);
  Element* rootElement = mDocument->GetRootElement();
  if (!rootElement) {
    return;
  }
  nsAutoScriptBlocker scriptBlocker;
  uint32_t attributeCount = rootElement->GetAttrCount();
  for (uint32_t i = 0; i < attributeCount; i++) {
    BorrowedAttrInfo info = rootElement->GetAttrInfoAt(i);
    const nsAttrName* name = info.mName;
    if (name->LocalName() == nsGkAtoms::chromemargin) {
      // Some linux windows managers have an issue when the chrome margin is
      // applied while the browser is loading (bug 1598848). For now, skip
      // applying this attribute when initializing.
      continue;
    }
    AttributeChanged(rootElement, name->NamespaceID(), name->LocalName(),
                     MutationEvent_Binding::ADDITION, nullptr);
  }
}

nsIWidget* ChromeObserver::GetWindowWidget() {
  // only top level chrome documents can set the titlebar color
  if (mDocument && mDocument->IsRootDisplayDocument()) {
    nsCOMPtr<nsISupports> container = mDocument->GetContainer();
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
    if (baseWindow) {
      nsCOMPtr<nsIWidget> mainWidget;
      baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
      return mainWidget;
    }
  }
  return nullptr;
}

void ChromeObserver::SetDrawsTitle(bool aState) {
  nsIWidget* mainWidget = GetWindowWidget();
  if (mainWidget) {
    // We can do this synchronously because SetDrawsTitle doesn't have any
    // synchronous effects apart from a harmless invalidation.
    mainWidget->SetDrawsTitle(aState);
  }
}

class MarginSetter : public Runnable {
 public:
  explicit MarginSetter(nsIWidget* aWidget)
      : mozilla::Runnable("MarginSetter"),
        mWidget(aWidget),
        mMargin(-1, -1, -1, -1) {}
  MarginSetter(nsIWidget* aWidget, const LayoutDeviceIntMargin& aMargin)
      : mozilla::Runnable("MarginSetter"), mWidget(aWidget), mMargin(aMargin) {}

  NS_IMETHOD Run() override {
    // SetNonClientMargins can dispatch native events, hence doing
    // it off a script runner.
    mWidget->SetNonClientMargins(mMargin);
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIWidget> mWidget;
  LayoutDeviceIntMargin mMargin;
};

void ChromeObserver::SetChromeMargins(const nsAttrValue* aValue) {
  if (!aValue) return;

  nsIWidget* mainWidget = GetWindowWidget();
  if (!mainWidget) return;

  // top, right, bottom, left - see nsAttrValue
  nsAutoString tmp;
  aValue->ToString(tmp);
  nsIntMargin margins;
  if (nsContentUtils::ParseIntMarginValue(tmp, margins)) {
    nsContentUtils::AddScriptRunner(new MarginSetter(
        mainWidget, LayoutDeviceIntMargin::FromUnknownMargin(margins)));
  }
}

void ChromeObserver::AttributeChanged(dom::Element* aElement,
                                      int32_t aNamespaceID, nsAtom* aName,
                                      int32_t aModType,
                                      const nsAttrValue* aOldValue) {
  // We only care about changes to the root element.
  if (!mDocument || aElement != mDocument->GetRootElement()) {
    return;
  }

  const nsAttrValue* value = aElement->GetParsedAttr(aName, aNamespaceID);
  if (value) {
    // Hide chrome if needed
    if (aName == nsGkAtoms::hidechrome) {
      HideWindowChrome(value->Equals(u"true"_ns, eCaseMatters));
    } else if (aName == nsGkAtoms::chromemargin) {
      SetChromeMargins(value);
    }
    // title and drawintitlebar are settable on
    // any root node (windows, dialogs, etc)
    else if (aName == nsGkAtoms::title) {
      mDocument->NotifyPossibleTitleChange(false);
    } else if (aName == nsGkAtoms::drawtitle) {
      SetDrawsTitle(value->Equals(u"true"_ns, eCaseMatters));
    } else if (aName == nsGkAtoms::localedir) {
      // if the localedir changed on the root element, reset the document
      // direction
      mDocument->ResetDocumentDirection();
    } else if (aName == nsGkAtoms::lwtheme) {
      // if the lwtheme changed, make sure to reset the document lwtheme
      // cache
      mDocument->ResetDocumentLWTheme();
    }
  } else {
    if (aName == nsGkAtoms::hidechrome) {
      HideWindowChrome(false);
    } else if (aName == nsGkAtoms::chromemargin) {
      ResetChromeMargins();
    } else if (aName == nsGkAtoms::localedir) {
      // if the localedir changed on the root element, reset the document
      // direction
      mDocument->ResetDocumentDirection();
    } else if (aName == nsGkAtoms::lwtheme) {
      // if the lwtheme changed, make sure to restyle appropriately
      mDocument->ResetDocumentLWTheme();
    } else if (aName == nsGkAtoms::drawtitle) {
      SetDrawsTitle(false);
    }
  }
}

void ChromeObserver::NodeWillBeDestroyed(nsINode* aNode) {
  mDocument = nullptr;
}

void ChromeObserver::ResetChromeMargins() {
  nsIWidget* mainWidget = GetWindowWidget();
  if (!mainWidget) return;
  // See nsIWidget
  nsContentUtils::AddScriptRunner(new MarginSetter(mainWidget));
}

nsresult ChromeObserver::HideWindowChrome(bool aShouldHide) {
  // only top level chrome documents can hide the window chrome
  if (!mDocument->IsRootDisplayDocument()) return NS_OK;

  nsPresContext* presContext = mDocument->GetPresContext();

  if (presContext && presContext->IsChrome()) {
    nsIFrame* frame = mDocument->GetDocumentElement()->GetPrimaryFrame();

    if (frame) {
      nsView* view = frame->GetClosestView();

      if (view) {
        nsIWidget* w = view->GetWidget();
        NS_ENSURE_STATE(w);
        w->HideWindowChrome(aShouldHide);
      }
    }
  }

  return NS_OK;
}

}  // namespace mozilla::dom
