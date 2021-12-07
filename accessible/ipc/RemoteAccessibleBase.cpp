/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/a11y/RemoteAccessibleBase.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/Unused.h"
#include "nsAccUtils.h"
#include "RelationType.h"
#include "xpcAccessibleDocument.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#  define VERIFY_CACHE(domain)                                     \
    if (logging::IsEnabled(logging::eCache)) {                     \
      Unused << mDoc->SendVerifyCache(mID, domain, mCachedFields); \
    }
#else
#  define VERIFY_CACHE(domain) \
    do {                       \
    } while (0)

#endif

namespace mozilla {
namespace a11y {

template <class Derived>
void RemoteAccessibleBase<Derived>::Shutdown() {
  MOZ_DIAGNOSTIC_ASSERT(!IsDoc());
  xpcAccessibleDocument* xpcDoc =
      GetAccService()->GetCachedXPCDocument(Document());
  if (xpcDoc) {
    xpcDoc->NotifyOfShutdown(static_cast<Derived*>(this));
  }

  // XXX Ideally  this wouldn't be necessary, but it seems OuterDoc accessibles
  // can be destroyed before the doc they own.
  uint32_t childCount = mChildren.Length();
  if (!IsOuterDoc()) {
    for (uint32_t idx = 0; idx < childCount; idx++) mChildren[idx]->Shutdown();
  } else {
    if (childCount > 1) {
      MOZ_CRASH("outer doc has too many documents!");
    } else if (childCount == 1) {
      mChildren[0]->AsDoc()->Unbind();
    }
  }

  mChildren.Clear();
  ProxyDestroyed(static_cast<Derived*>(this));
  mDoc->RemoveAccessible(static_cast<Derived*>(this));
}

template <class Derived>
void RemoteAccessibleBase<Derived>::SetChildDoc(
    DocAccessibleParent* aChildDoc) {
  MOZ_ASSERT(aChildDoc);
  MOZ_ASSERT(mChildren.Length() == 0);
  mChildren.AppendElement(aChildDoc);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::ClearChildDoc(
    DocAccessibleParent* aChildDoc) {
  MOZ_ASSERT(aChildDoc);
  // This is possible if we're replacing one document with another: Doc 1
  // has not had a chance to remove itself, but was already replaced by Doc 2
  // in SetChildDoc(). This could result in two subsequent calls to
  // ClearChildDoc() even though mChildren.Length() == 1.
  MOZ_ASSERT(mChildren.Length() <= 1);
  mChildren.RemoveElement(aChildDoc);
}

template <class Derived>
uint32_t RemoteAccessibleBase<Derived>::EmbeddedChildCount() const {
  size_t count = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (mChildren[i]->IsEmbeddedObject()) {
      count++;
    }
  }

  return count;
}

template <class Derived>
int32_t RemoteAccessibleBase<Derived>::IndexOfEmbeddedChild(
    Accessible* aChild) {
  size_t index = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (mChildren[i]->IsEmbeddedObject()) {
      if (mChildren[i] == aChild) {
        return index;
      }

      index++;
    }
  }

  return -1;
}

template <class Derived>
Accessible* RemoteAccessibleBase<Derived>::EmbeddedChildAt(uint32_t aChildIdx) {
  size_t index = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (!mChildren[i]->IsEmbeddedObject()) {
      continue;
    }

    if (index == aChildIdx) {
      return mChildren[i];
    }

    index++;
  }

  return nullptr;
}

template <class Derived>
LocalAccessible* RemoteAccessibleBase<Derived>::OuterDocOfRemoteBrowser()
    const {
  auto tab = static_cast<dom::BrowserParent*>(mDoc->Manager());
  dom::Element* frame = tab->GetOwnerElement();
  NS_ASSERTION(frame, "why isn't the tab in a frame!");
  if (!frame) return nullptr;

  DocAccessible* chromeDoc = GetExistingDocAccessible(frame->OwnerDoc());

  return chromeDoc ? chromeDoc->GetAccessible(frame) : nullptr;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::SetParent(Derived* aParent) {
  MOZ_ASSERT(IsDoc(), "we should only reparent documents");
  if (!aParent) {
    mParent = kNoParent;
  } else {
    MOZ_ASSERT(!aParent->IsDoc());
    mParent = aParent->ID();
  }
}

template <class Derived>
Derived* RemoteAccessibleBase<Derived>::RemoteParent() const {
  if (mParent == kNoParent) {
    return nullptr;
  }

  // if we are not a document then are parent is another proxy in the same
  // document.  That means we can just ask our document for the proxy with our
  // parent id.
  if (!IsDoc()) {
    return Document()->GetAccessible(mParent);
  }

  // If we are a top level document then our parent is not a proxy.
  if (AsDoc()->IsTopLevel()) {
    return nullptr;
  }

  // Finally if we are a non top level document then our parent id is for a
  // proxy in our parent document so get the proxy from there.
  DocAccessibleParent* parentDoc = AsDoc()->ParentDoc();
  MOZ_ASSERT(parentDoc);
  MOZ_ASSERT(mParent);
  return parentDoc->GetAccessible(mParent);
}

template <class Derived>
ENameValueFlag RemoteAccessibleBase<Derived>::Name(nsString& aName) const {
  if (mCachedFields) {
    if (IsText()) {
      mCachedFields->GetAttribute(nsGkAtoms::text, aName);
      return eNameOK;
    }
    if (mCachedFields->GetAttribute(nsGkAtoms::name, aName)) {
      auto nameFlag =
          mCachedFields->GetAttribute<int32_t>(nsGkAtoms::explicit_name);
      VERIFY_CACHE(CacheDomain::NameAndDescription);
      return nameFlag ? static_cast<ENameValueFlag>(*nameFlag) : eNameOK;
    }
  }

  return eNameOK;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::Description(nsString& aDescription) const {
  if (mCachedFields) {
    mCachedFields->GetAttribute(nsGkAtoms::description, aDescription);
    VERIFY_CACHE(CacheDomain::NameAndDescription);
  }
}

template <class Derived>
double RemoteAccessibleBase<Derived>::CurValue() const {
  if (auto value = mCachedFields->GetAttribute<double>(nsGkAtoms::value)) {
    VERIFY_CACHE(CacheDomain::Value);
    return *value;
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
double RemoteAccessibleBase<Derived>::MinValue() const {
  if (auto min = mCachedFields->GetAttribute<double>(nsGkAtoms::min)) {
    VERIFY_CACHE(CacheDomain::Value);
    return *min;
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
double RemoteAccessibleBase<Derived>::MaxValue() const {
  if (auto max = mCachedFields->GetAttribute<double>(nsGkAtoms::max)) {
    VERIFY_CACHE(CacheDomain::Value);
    return *max;
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
double RemoteAccessibleBase<Derived>::Step() const {
  if (auto step = mCachedFields->GetAttribute<double>(nsGkAtoms::step)) {
    VERIFY_CACHE(CacheDomain::Value);
    return *step;
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
Maybe<nsRect> RemoteAccessibleBase<Derived>::RetrieveCachedBounds() const {
  Maybe<const nsTArray<int32_t>&> maybeArray =
      mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::relativeBounds);
  if (maybeArray) {
    const nsTArray<int32_t>& relativeBoundsArr = *maybeArray;
    MOZ_ASSERT(relativeBoundsArr.Length() == 4,
               "Incorrectly sized bounds array");
    nsRect relativeBoundsRect(relativeBoundsArr[0], relativeBoundsArr[1],
                              relativeBoundsArr[2], relativeBoundsArr[3]);
    return Some(relativeBoundsRect);
  }

  return Nothing();
}

template <class Derived>
nsIntRect RemoteAccessibleBase<Derived>::Bounds() const {
  if (mCachedFields) {
    Maybe<nsRect> maybeBounds = RetrieveCachedBounds();
    if (maybeBounds) {
      nsRect bounds = *maybeBounds;
      nsIntRect devPxBounds;
      dom::CanonicalBrowsingContext* cbc =
          static_cast<dom::BrowserParent*>(mDoc->Manager())
              ->GetBrowsingContext()
              ->Top();
      dom::BrowserParent* bp = cbc->GetBrowserParent();
      nsPresContext* presContext =
          bp->GetOwnerElement()->OwnerDoc()->GetPresContext();

      const Accessible* acc = this;
      while (acc) {
        if (LocalAccessible* localAcc =
                const_cast<Accessible*>(acc)->AsLocal()) {
          // LocalAccessible::Bounds returns screen-relative bounds in
          // dev pixels.
          nsIntRect localBounds = localAcc->Bounds();

          // Convert our existing `bounds` rect from app units to dev pixels
          devPxBounds =
              bounds.ToNearestPixels(presContext->AppUnitsPerDevPixel());

          // We factor in our zoom level before offsetting by
          // `localBounds`, which has already taken zoom into account.
          devPxBounds.ScaleRoundOut(cbc->GetFullZoom());

          // The root document will always have an APZ resolution of 1,
          // so we don't factor in its scale here. We also don't scale
          // by GetFullZoom because LocalAccessible::Bounds already does
          // that.
          devPxBounds.MoveBy(localBounds.X(), localBounds.Y());

          break;
        }

        RemoteAccessible* remoteAcc = const_cast<Accessible*>(acc)->AsRemote();
        // Verify that remoteAcc is not `this`, since `bounds` was
        // initialised to include this->RetrieveCachedBounds()
        Maybe<nsRect> maybeRemoteBounds =
            (remoteAcc == this) ? Nothing() : remoteAcc->RetrieveCachedBounds();

        if (maybeRemoteBounds) {
          // We need to take into account a non-1 resolution set on the
          // presshell. This happens with async pinch zooming, among other
          // things. We can't reliably query this value in the parent process,
          // so we retrieve it from the document's cache.
          Maybe<float> res;
          if (remoteAcc->IsDoc()) {
            // Apply the document's resolution to the bounds we've gathered
            // thus far. We do this before applying the document's offset
            // because document accs should not have their bounds scaled by
            // their own resolution. They should be scaled by the resolution
            // of their containing document (if any). We also skip this in the
            // case that remoteAcc == this, since that implies `bounds` should
            // be scaled relative to its parent doc.
            res = remoteAcc->AsDoc()->mCachedFields->GetAttribute<float>(
                nsGkAtoms::resolution);
            bounds.ScaleRoundOut(res.valueOr(1.0f));
          }

          // Regardless of whether this is a doc, we should offset `bounds`
          // by the bounds retrieved here. This is how we build screen
          // coordinates from relative coordinates.
          nsRect remoteBounds = *maybeRemoteBounds;
          bounds.MoveBy(remoteBounds.X(), remoteBounds.Y());
        }

        acc = acc->Parent();
      }

      PresShell* presShell = presContext->PresShell();

      // Our relative bounds are pulled from the coordinate space of the layout
      // viewport, but we need them to be in the coordinate space of the visual
      // viewport. We calculate the difference and translate our bounds here.
      nsPoint viewportOffset = presShell->GetVisualViewportOffset() -
                               presShell->GetLayoutViewportOffset();
      devPxBounds.MoveBy(-(
          viewportOffset.ToNearestPixels(presContext->AppUnitsPerDevPixel())));

      return devPxBounds;
    }
  }

  return nsIntRect();
}

template <class Derived>
void RemoteAccessibleBase<Derived>::AppendTextTo(nsAString& aText,
                                                 uint32_t aStartOffset,
                                                 uint32_t aLength) {
  if (IsText()) {
    if (mCachedFields) {
      if (auto text = mCachedFields->GetAttribute<nsString>(nsGkAtoms::text)) {
        aText.Append(Substring(*text, aStartOffset, aLength));
      }
      VERIFY_CACHE(CacheDomain::Text);
    }
    return;
  }

  if (aStartOffset != 0 || aLength == 0) {
    return;
  }

  if (IsHTMLBr()) {
    aText += kForcedNewLineChar;
  } else if (RemoteParent() && nsAccUtils::MustPrune(RemoteParent())) {
    // Expose the embedded object accessible as imaginary embedded object
    // character if its parent hypertext accessible doesn't expose children to
    // AT.
    aText += kImaginaryEmbeddedObjectChar;
  } else {
    aText += kEmbeddedObjectChar;
  }
}

template <class Derived>
uint32_t RemoteAccessibleBase<Derived>::GetCachedTextLength() {
  MOZ_ASSERT(IsText());
  if (!mCachedFields) {
    return 0;
  }
  VERIFY_CACHE(CacheDomain::Text);
  auto text = mCachedFields->GetAttribute<nsString>(nsGkAtoms::text);
  if (!text) {
    return 0;
  }
  return text->Length();
}

template <class Derived>
Maybe<const nsTArray<int32_t>&>
RemoteAccessibleBase<Derived>::GetCachedTextLines() {
  MOZ_ASSERT(IsText());
  if (!mCachedFields) {
    return Nothing();
  }
  VERIFY_CACHE(CacheDomain::Text);
  return mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::line);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::DOMNodeID(nsString& aID) const {
  if (mCachedFields) {
    mCachedFields->GetAttribute(nsGkAtoms::id, aID);
    VERIFY_CACHE(CacheDomain::DOMNodeID);
  }
}

template <class Derived>
RefPtr<const AccAttributes>
RemoteAccessibleBase<Derived>::GetCachedTextAttributes() {
  MOZ_ASSERT(IsText() || IsHyperText());
  if (mCachedFields) {
    auto attrs =
        mCachedFields->GetAttributeRefPtr<AccAttributes>(nsGkAtoms::style);
    VERIFY_CACHE(CacheDomain::Text);
    return attrs;
  }
  return nullptr;
}

template <class Derived>
already_AddRefed<AccAttributes>
RemoteAccessibleBase<Derived>::DefaultTextAttributes() {
  RefPtr<const AccAttributes> attrs = GetCachedTextAttributes();
  RefPtr<AccAttributes> result = new AccAttributes();
  if (attrs) {
    attrs->CopyTo(result);
  }
  return result.forget();
}

template <class Derived>
uint64_t RemoteAccessibleBase<Derived>::State() {
  uint64_t state = 0;
  if (mCachedFields) {
    if (auto rawState =
            mCachedFields->GetAttribute<uint64_t>(nsGkAtoms::state)) {
      VERIFY_CACHE(CacheDomain::State);
      state = *rawState;
      // Handle states that are derived from other states.
      if (!(state & states::UNAVAILABLE)) {
        state |= states::ENABLED | states::SENSITIVE;
      }
      if (state & states::EXPANDABLE && !(state & states::EXPANDED)) {
        state |= states::COLLAPSED;
      }
    }
  }
  auto* browser = static_cast<dom::BrowserParent*>(Document()->Manager());
  if (browser == dom::BrowserParent::GetFocused()) {
    if (this == Document()->GetFocusedAcc()) {
      state |= states::FOCUSED;
    }
  }
  return state;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::TakeFocus() const {
  Unused << mDoc->SendTakeFocus(mID);
}

template class RemoteAccessibleBase<RemoteAccessible>;

}  // namespace a11y
}  // namespace mozilla
