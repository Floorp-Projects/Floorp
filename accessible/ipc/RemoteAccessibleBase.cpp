/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAMap.h"
#include "CachedTableAccessible.h"
#include "DocAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/a11y/RemoteAccessibleBase.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/Unused.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"
#include "Pivot.h"
#include "Relation.h"
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

  if (IsTable() || IsTableCell()) {
    CachedTableAccessible::Invalidate(this);
  }

  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Remove this acc's relation map from the doc's map of
    // reverse relations. We don't need to do additional processing
    // of the corresponding forward relations, because this shutdown
    // should trigger a cache update from the content process.
    // Similarly, we don't need to remove the reverse rels created
    // by this acc's forward rels because they'll be cleared during
    // the next update's call to PreProcessRelations().
    // In short, accs are responsible for managing their own
    // reverse relation map, both in PreProcessRelations() and in
    // Shutdown().
    Unused << mDoc->mReverseRelations.Remove(ID());
  }

  // XXX Ideally  this wouldn't be necessary, but it seems OuterDoc
  // accessibles can be destroyed before the doc they own.
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
uint32_t RemoteAccessibleBase<Derived>::EmbeddedChildCount() {
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
  if (!aParent) {
    mParent = kNoParent;
  } else {
    MOZ_ASSERT(!IsDoc() || !aParent->IsDoc());
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
  ENameValueFlag nameFlag = eNameOK;
  if (mCachedFields) {
    if (IsText()) {
      mCachedFields->GetAttribute(nsGkAtoms::text, aName);
      return eNameOK;
    }
    auto cachedNameFlag =
        mCachedFields->GetAttribute<int32_t>(nsGkAtoms::explicit_name);
    if (cachedNameFlag) {
      nameFlag = static_cast<ENameValueFlag>(*cachedNameFlag);
    }
    if (mCachedFields->GetAttribute(nsGkAtoms::name, aName)) {
      VERIFY_CACHE(CacheDomain::NameAndDescription);
      return nameFlag;
    }
  }

  MOZ_ASSERT(aName.IsEmpty());
  if (nameFlag != eNoNameOnPurpose) {
    aName.SetIsVoid(true);
  }
  return nameFlag;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::Description(nsString& aDescription) const {
  if (mCachedFields) {
    mCachedFields->GetAttribute(nsGkAtoms::description, aDescription);
    VERIFY_CACHE(CacheDomain::NameAndDescription);
  }
}

template <class Derived>
void RemoteAccessibleBase<Derived>::Value(nsString& aValue) const {
  if (mCachedFields) {
    if (mCachedFields->HasAttribute(nsGkAtoms::aria_valuetext)) {
      mCachedFields->GetAttribute(nsGkAtoms::aria_valuetext, aValue);
      VERIFY_CACHE(CacheDomain::Value);
      return;
    }

    if (HasNumericValue()) {
      double checkValue = CurValue();
      if (!IsNaN(checkValue)) {
        aValue.AppendFloat(checkValue);
      }
      return;
    }

    const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
    // Value of textbox is a textified subtree.
    if (roleMapEntry && roleMapEntry->Is(nsGkAtoms::textbox)) {
      nsTextEquivUtils::GetTextEquivFromSubtree(this, aValue);
      return;
    }

    if (IsCombobox()) {
      // For combo boxes, rely on selection state to determine the value.
      const Accessible* option =
          const_cast<RemoteAccessibleBase<Derived>*>(this)->GetSelectedItem(0);
      if (option) {
        option->Name(aValue);
      }
      return;
    }

    if (IsTextLeaf() || IsImage()) {
      if (const Accessible* actionAcc = ActionAncestor()) {
        if (const_cast<Accessible*>(actionAcc)->State() & states::LINKED) {
          // Text and image descendants of links expose the link URL as the
          // value.
          return actionAcc->Value(aValue);
        }
      }
    }
  }
}

template <class Derived>
double RemoteAccessibleBase<Derived>::CurValue() const {
  if (mCachedFields) {
    if (auto value = mCachedFields->GetAttribute<double>(nsGkAtoms::value)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *value;
    }
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
double RemoteAccessibleBase<Derived>::MinValue() const {
  if (mCachedFields) {
    if (auto min = mCachedFields->GetAttribute<double>(nsGkAtoms::min)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *min;
    }
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
double RemoteAccessibleBase<Derived>::MaxValue() const {
  if (mCachedFields) {
    if (auto max = mCachedFields->GetAttribute<double>(nsGkAtoms::max)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *max;
    }
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
double RemoteAccessibleBase<Derived>::Step() const {
  if (mCachedFields) {
    if (auto step = mCachedFields->GetAttribute<double>(nsGkAtoms::step)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *step;
    }
  }

  return UnspecifiedNaN<double>();
}

template <class Derived>
Accessible* RemoteAccessibleBase<Derived>::ChildAtPoint(
    int32_t aX, int32_t aY, LocalAccessible::EWhichChildAtPoint aWhichChild) {
  if (IsOuterDoc() && aWhichChild == EWhichChildAtPoint::DirectChild) {
    // This is an iframe, which is as deep as the viewport cache goes. The
    // caller wants a direct child, which can only be the embedded document.
    if (Bounds().Contains(aX, aY)) {
      return RemoteFirstChild();
    }
    return nullptr;
  }

  RemoteAccessible* lastMatch = nullptr;
  // If `this` is a document, use its viewport cache instead of
  // the cache of its parent document.
  if (DocAccessibleParent* doc = IsDoc() ? AsDoc() : mDoc) {
    if (auto maybeViewportCache =
            doc->mCachedFields->GetAttribute<nsTArray<uint64_t>>(
                nsGkAtoms::viewport)) {
      // The retrieved viewport cache contains acc IDs in hittesting order.
      // That is, items earlier in the list have z-indexes that are larger than
      // those later in the list. If you were to build a tree by z-index, where
      // chilren have larger z indices than their parents, iterating this list
      // is essentially a postorder tree traversal.
      const nsTArray<uint64_t>& viewportCache = *maybeViewportCache;

      for (auto id : viewportCache) {
        RemoteAccessible* acc = doc->GetAccessible(id);
        if (!acc) {
          // This can happen if the acc died in between
          // pushing the viewport cache and doing this hittest
          continue;
        }

        if (acc->IsOuterDoc() &&
            aWhichChild == EWhichChildAtPoint::DeepestChild &&
            acc->Bounds().Contains(aX, aY)) {
          // acc is an iframe, which is as deep as the viewport cache goes. This
          // iframe contains the requested point.
          RemoteAccessible* innerDoc = acc->RemoteFirstChild();
          if (innerDoc) {
            MOZ_ASSERT(innerDoc->IsDoc());
            // Search the embedded document's viewport cache so we return the
            // deepest descendant in that embedded document.
            return innerDoc->ChildAtPoint(aX, aY,
                                          EWhichChildAtPoint::DeepestChild);
          }
          // If there is no embedded document, the iframe itself is the deepest
          // descendant.
          return acc;
        }

        if (acc == this) {
          MOZ_ASSERT(!acc->IsOuterDoc());
          // Even though we're searching from the doc's cache
          // this call shouldn't pass the boundary defined by
          // the acc this call originated on. If we hit `this`,
          // return our most recent match.
          break;
        }

        if (acc->Bounds().Contains(aX, aY)) {
          if (aWhichChild == EWhichChildAtPoint::DeepestChild) {
            // Because our rects are in hittesting order, the
            // first match we encounter is guaranteed to be the
            // deepest match.
            lastMatch = acc;
            break;
          }

          // We're looking for a DirectChild match. Update our
          // `lastMatch` marker as we ascend towards `this`.
          lastMatch = acc;
        }
      }
    }
  }

  if (!lastMatch && Bounds().Contains(aX, aY)) {
    return this;
  }

  return lastMatch;
}

template <class Derived>
Maybe<nsRect> RemoteAccessibleBase<Derived>::RetrieveCachedBounds() const {
  if (!mCachedFields) {
    return Nothing();
  }

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
void RemoteAccessibleBase<Derived>::ApplyCrossProcOffset(
    nsRect& aBounds) const {
  Maybe<const nsTArray<int32_t>&> maybeOffset =
      mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::crossorigin);
  if (!maybeOffset) {
    return;
  }

  MOZ_ASSERT(maybeOffset->Length() == 2);
  const nsTArray<int32_t>& offset = *maybeOffset;
  // Our retrieved value is in app units, so we don't need to do any
  // unit conversion here.
  aBounds.MoveBy(offset[0], offset[1]);
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::ApplyTransform(nsRect& aBounds) const {
  // First, attempt to retrieve the transform from the cache.
  Maybe<const UniquePtr<gfx::Matrix4x4>&> maybeTransform =
      mCachedFields->GetAttribute<UniquePtr<gfx::Matrix4x4>>(
          nsGkAtoms::transform);
  if (!maybeTransform) {
    return false;
  }
  // The transform matrix we cache is meant to operate on rects
  // within the coordinate space of the frame to which the
  // transform is applied (self-relative rects). We cache bounds
  // relative to some ancestor. Remove the relative offset before
  // transforming. The transform matrix will add it back in.
  aBounds.MoveTo(0, 0);
  auto mtxInPixels = gfx::Matrix4x4Typed<CSSPixel, CSSPixel>::FromUnknownMatrix(
      *(*maybeTransform));

  // Our matrix is in CSS Pixels, so we need our rect to be in CSS
  // Pixels too. Convert before applying.
  auto boundsInPixels = CSSRect::FromAppUnits(aBounds);
  boundsInPixels = mtxInPixels.TransformBounds(boundsInPixels);
  aBounds = CSSRect::ToAppUnits(boundsInPixels);

  return true;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::ApplyScrollOffset(nsRect& aBounds) const {
  Maybe<const nsTArray<int32_t>&> maybeScrollPosition =
      mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::scrollPosition);

  if (!maybeScrollPosition || maybeScrollPosition->Length() != 2) {
    return;
  }
  // Our retrieved value is in app units, so we don't need to do any
  // unit conversion here.
  const nsTArray<int32_t>& scrollPosition = *maybeScrollPosition;

  // Scroll position is an inverse representation of scroll offset (since the
  // further the scroll bar moves down the page, the further the page content
  // moves up/closer to the origin).
  nsPoint scrollOffset(-scrollPosition[0], -scrollPosition[1]);

  aBounds.MoveBy(scrollOffset.x, scrollOffset.y);
}

template <class Derived>
nsRect RemoteAccessibleBase<Derived>::BoundsInAppUnits() const {
  if (dom::CanonicalBrowsingContext* cbc = mDoc->GetBrowsingContext()->Top()) {
    if (dom::BrowserParent* bp = cbc->GetBrowserParent()) {
      DocAccessibleParent* topDoc = bp->GetTopLevelDocAccessible();
      if (topDoc && topDoc->mCachedFields) {
        auto appUnitsPerDevPixel = topDoc->mCachedFields->GetAttribute<int32_t>(
            nsGkAtoms::_moz_device_pixel_ratio);
        MOZ_ASSERT(appUnitsPerDevPixel);
        return LayoutDeviceIntRect::ToAppUnits(Bounds(), *appUnitsPerDevPixel);
      }
    }
  }
  return LayoutDeviceIntRect::ToAppUnits(Bounds(), AppUnitsPerCSSPixel());
}

template <class Derived>
LayoutDeviceIntRect RemoteAccessibleBase<Derived>::BoundsWithOffset(
    Maybe<nsRect> aOffset) const {
  Maybe<nsRect> maybeBounds = RetrieveCachedBounds();
  if (maybeBounds) {
    nsRect bounds = *maybeBounds;
    const DocAccessibleParent* topDoc = IsDoc() ? AsDoc() : nullptr;

    if (aOffset.isSome()) {
      // The rect we've passed in is in app units, so no conversion needed.
      nsRect internalRect = *aOffset;
      bounds.SetRectX(bounds.x + internalRect.x, internalRect.width);
      bounds.SetRectY(bounds.y + internalRect.y, internalRect.height);
    }

    Unused << ApplyTransform(bounds);

    LayoutDeviceIntRect devPxBounds;
    const Accessible* acc = Parent();
    const RemoteAccessibleBase<Derived>* recentAcc = this;
    while (acc && acc->IsRemote()) {
      RemoteAccessible* remoteAcc = const_cast<Accessible*>(acc)->AsRemote();

      if (Maybe<nsRect> maybeRemoteBounds = remoteAcc->RetrieveCachedBounds()) {
        nsRect remoteBounds = *maybeRemoteBounds;
        // We need to take into account a non-1 resolution set on the
        // presshell. This happens with async pinch zooming, among other
        // things. We can't reliably query this value in the parent process,
        // so we retrieve it from the document's cache.
        if (remoteAcc->IsDoc()) {
          // Apply the document's resolution to the bounds we've gathered
          // thus far. We do this before applying the document's offset
          // because document accs should not have their bounds scaled by
          // their own resolution. They should be scaled by the resolution
          // of their containing document (if any).
          Maybe<float> res =
              remoteAcc->AsDoc()->mCachedFields->GetAttribute<float>(
                  nsGkAtoms::resolution);
          MOZ_ASSERT(res, "No cached document resolution found.");
          bounds.ScaleRoundOut(res.valueOr(1.0f));

          topDoc = remoteAcc->AsDoc();
        }

        if (remoteAcc->IsOuterDoc()) {
          if (recentAcc && recentAcc->IsDoc() &&
              !recentAcc->AsDoc()->IsTopLevel() &&
              recentAcc->AsDoc()->IsTopLevelInContentProcess()) {
            // We're unable to account for the document offset of remote,
            // cross process iframes when computing parent-relative bounds.
            // Instead we store this value separately and apply it here.
            remoteAcc->ApplyCrossProcOffset(remoteBounds);
          }
        }

        // Apply scroll offset, if applicable. Only the contents of an
        // element are affected by its scroll offset, which is why this call
        // happens in this loop instead of both inside and outside of
        // the loop (like ApplyTransform).
        remoteAcc->ApplyScrollOffset(remoteBounds);

        // Regardless of whether this is a doc, we should offset `bounds`
        // by the bounds retrieved here. This is how we build screen
        // coordinates from relative coordinates.
        bounds.MoveBy(remoteBounds.X(), remoteBounds.Y());
        Unused << remoteAcc->ApplyTransform(bounds);
      }
      recentAcc = remoteAcc;
      acc = acc->Parent();
    }

    MOZ_ASSERT(topDoc);
    if (topDoc) {
      // We use the top documents app-units-per-dev-pixel even though
      // theoretically nested docs can have different values. Practically,
      // that isn't likely since we only offer zoom controls for the top
      // document and all subdocuments inherit from it.
      auto appUnitsPerDevPixel = topDoc->mCachedFields->GetAttribute<int32_t>(
          nsGkAtoms::_moz_device_pixel_ratio);
      MOZ_ASSERT(appUnitsPerDevPixel);
      if (appUnitsPerDevPixel) {
        // Convert our existing `bounds` rect from app units to dev pixels
        devPxBounds = LayoutDeviceIntRect::FromAppUnitsToNearest(
            bounds, *appUnitsPerDevPixel);
      }
    }

#if !defined(ANDROID)
    // This block is not thread safe because it queries a LocalAccessible.
    // It is also not needed in Android since the only local accessible is
    // the outer doc browser that has an offset of 0.
    // acc could be null if the OuterDocAccessible died before the top level
    // DocAccessibleParent.
    if (LocalAccessible* localAcc =
            acc ? const_cast<Accessible*>(acc)->AsLocal() : nullptr) {
      // LocalAccessible::Bounds returns screen-relative bounds in
      // dev pixels.
      LayoutDeviceIntRect localBounds = localAcc->Bounds();

      // The root document will always have an APZ resolution of 1,
      // so we don't factor in its scale here. We also don't scale
      // by GetFullZoom because LocalAccessible::Bounds already does
      // that.
      devPxBounds.MoveBy(localBounds.X(), localBounds.Y());
    }
#endif

    return devPxBounds;
  }

  return LayoutDeviceIntRect();
}

template <class Derived>
LayoutDeviceIntRect RemoteAccessibleBase<Derived>::Bounds() const {
  return BoundsWithOffset(Nothing());
}

template <class Derived>
Relation RemoteAccessibleBase<Derived>::RelationByType(
    RelationType aType) const {
  // We are able to handle some relations completely in the
  // parent process, without the help of the cache. Those
  // relations are enumerated here. Other relations, whose
  // types are stored in kRelationTypeAtoms, are processed
  // below using the cache.
  if (aType == RelationType::CONTAINING_TAB_PANE) {
    if (dom::CanonicalBrowsingContext* cbc = mDoc->GetBrowsingContext()) {
      if (dom::CanonicalBrowsingContext* topCbc = cbc->Top()) {
        if (dom::BrowserParent* bp = topCbc->GetBrowserParent()) {
          return Relation(bp->GetTopLevelDocAccessible());
        }
      }
    }
    return Relation();
  }

  Relation rel;
  if (!mCachedFields) {
    return rel;
  }

  for (auto data : kRelationTypeAtoms) {
    if (data.mType != aType ||
        (data.mValidTag && TagName() != data.mValidTag)) {
      continue;
    }

    if (auto maybeIds =
            mCachedFields->GetAttribute<nsTArray<uint64_t>>(data.mAtom)) {
      rel.AppendIter(new RemoteAccIterator(*maybeIds, Document()));
    }
    // Each relation type has only one relevant cached attribute,
    // so break after we've handled the attr for this type,
    // even if we didn't find any targets.
    break;
  }

  if (auto accRelMapEntry = mDoc->mReverseRelations.Lookup(ID())) {
    if (auto reverseIdsEntry =
            accRelMapEntry.Data().Lookup(static_cast<uint64_t>(aType))) {
      rel.AppendIter(new RemoteAccIterator(reverseIdsEntry.Data(), Document()));
    }
  }

  return rel;
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
nsTArray<bool> RemoteAccessibleBase<Derived>::PreProcessRelations(
    AccAttributes* aFields) {
  nsTArray<bool> updateTracker(ArrayLength(kRelationTypeAtoms));
  for (auto const& data : kRelationTypeAtoms) {
    if (data.mValidTag) {
      // The relation we're currently processing only applies to particular
      // elements. Check to see if we're one of them.
      nsAtom* tag = TagName();
      if (!tag) {
        // TagName() returns null on an initial cache push -- check aFields
        // for a tag name instead.
        if (auto maybeTag =
                aFields->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::tag)) {
          tag = *maybeTag;
        }
      }
      MOZ_ASSERT(
          tag || IsTextLeaf(),
          "Could not fetch tag via TagName() or from initial cache push!");
      if (tag != data.mValidTag) {
        // If this rel doesn't apply to us, do no pre-processing. Also,
        // note in our updateTracker that we should do no post-processing.
        updateTracker.AppendElement(false);
        continue;
      }
    }

    if (!data.mReverseType) {
      updateTracker.AppendElement(false);
      continue;
    }

    nsStaticAtom* const relAtom = data.mAtom;
    auto newRelationTargets =
        aFields->GetAttribute<nsTArray<uint64_t>>(relAtom);
    bool shouldAddNewImplicitRels =
        newRelationTargets && newRelationTargets->Length();

    // Remove existing implicit relations if we need to perform an update, or
    // if we've recieved a DeleteEntry(). Only do this if mCachedFields is
    // initialized. If mCachedFields is not initialized, we still need to
    // construct the update array so we correctly handle reverse rels in
    // PostProcessRelations.
    if ((shouldAddNewImplicitRels ||
         aFields->GetAttribute<DeleteEntry>(relAtom)) &&
        mCachedFields) {
      if (auto maybeOldIDs =
              mCachedFields->GetAttribute<nsTArray<uint64_t>>(relAtom)) {
        for (uint64_t id : *maybeOldIDs) {
          // For each target, fetch its reverse relation map
          nsTHashMap<nsUint64HashKey, nsTArray<uint64_t>>& reverseRels =
              Document()->mReverseRelations.LookupOrInsert(id);
          // Then fetch its reverse relation's ID list
          nsTArray<uint64_t>& reverseRelIDs = reverseRels.LookupOrInsert(
              static_cast<uint64_t>(*data.mReverseType));
          //  There might be other reverse relations stored for this acc, so
          //  remove our ID instead of deleting the array entirely.
          DebugOnly<bool> removed = reverseRelIDs.RemoveElement(ID());
          MOZ_ASSERT(removed, "Can't find old reverse relation");
        }
      }
    }

    updateTracker.AppendElement(shouldAddNewImplicitRels);
  }

  return updateTracker;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::PostProcessRelations(
    const nsTArray<bool>& aToUpdate) {
  size_t updateCount = aToUpdate.Length();
  MOZ_ASSERT(updateCount == ArrayLength(kRelationTypeAtoms),
             "Did not note update status for every relation type!");
  for (size_t i = 0; i < updateCount; i++) {
    if (aToUpdate.ElementAt(i)) {
      // Since kRelationTypeAtoms was used to generate aToUpdate, we
      // know the ith entry of aToUpdate corresponds to the relation type in
      // the ith entry of kRelationTypeAtoms. Fetch the related data here.
      auto const& data = kRelationTypeAtoms[i];

      const nsTArray<uint64_t>& newIDs =
          *mCachedFields->GetAttribute<nsTArray<uint64_t>>(data.mAtom);
      for (uint64_t id : newIDs) {
        nsTHashMap<nsUint64HashKey, nsTArray<uint64_t>>& relations =
            Document()->mReverseRelations.LookupOrInsert(id);
        MOZ_ASSERT(data.mReverseType,
                   "Updating implicit rels, but no implicit rel exists?");
        nsTArray<uint64_t>& ids =
            relations.LookupOrInsert(static_cast<uint64_t>(*data.mReverseType));
        ids.AppendElement(ID());
      }
    }
  }
}

template <class Derived>
uint32_t RemoteAccessibleBase<Derived>::GetCachedTextLength() {
  MOZ_ASSERT(!HasChildren());
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
  MOZ_ASSERT(!HasChildren());
  if (!mCachedFields) {
    return Nothing();
  }
  VERIFY_CACHE(CacheDomain::Text);
  return mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::line);
}

template <class Derived>
Maybe<nsTArray<nsRect>> RemoteAccessibleBase<Derived>::GetCachedCharData() {
  MOZ_ASSERT(IsText());
  if (!mCachedFields) {
    return Nothing();
  }

  if (Maybe<const nsTArray<int32_t>&> maybeCharData =
          mCachedFields->GetAttribute<nsTArray<int32_t>>(
              nsGkAtoms::characterData)) {
    const nsTArray<int32_t>& charData = *maybeCharData;
    nsTArray<nsRect> rects;
    for (int i = 0; i < static_cast<int32_t>(charData.Length()); i += 4) {
      nsRect r(charData[i], charData[i + 1], charData[i + 2], charData[i + 3]);
      rects.AppendElement(r);
    }
    return Some(std::move(rects));
  }

  return Nothing();
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
RefPtr<const AccAttributes>
RemoteAccessibleBase<Derived>::GetCachedARIAAttributes() const {
  if (mCachedFields) {
    auto attrs =
        mCachedFields->GetAttributeRefPtr<AccAttributes>(nsGkAtoms::aria);
    VERIFY_CACHE(CacheDomain::ARIA);
    return attrs;
  }
  return nullptr;
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

    // Fetch our current opacity value from the cache.
    auto opacity = Opacity();
    if (opacity && *opacity == 1.0f) {
      state |= states::OPAQUE1;
    } else {
      // If we can't retrieve an opacity value, or if the value we retrieve
      // is less than one, ensure the OPAQUE1 bit is cleared.
      // It's possible this bit was set in the cached `rawState` vector, but
      // we've since been notified of a style change invalidating that state.
      state &= ~states::OPAQUE1;
    }

    auto* cbc = mDoc->GetBrowsingContext();
    if (cbc && !cbc->IsActive()) {
      // If our browsing context is _not_ active, we're in a background tab
      // and inherently offscreen.
      state |= states::OFFSCREEN;
    } else if (this == mDoc) {
      // mDoc is not stored in its own viewport cache, so we handle it
      // separately here. See bug 1774197.
      // We assume top level tab docs are on screen if their BC is active, so
      // we don't need additional handling for them here. We still need to trap
      // the case where `this == mDoc && AsDoc()->IsTopLevel()`, otherwise we'll
      // fail to find mDoc in the following `else if` case, and incorrectly
      // return the top-level tab document as offscreen.
      if (!AsDoc()->IsTopLevel()) {
        // Otherwise, if we're dealing with an iframe (OOP or in process) in the
        // active browsing context, we use its Outer Doc's position in the
        // embedding document's viewport to determine if the iframe has been
        // scrolled offscreen.
        Accessible* parent = Parent();
        MOZ_ASSERT(parent && parent->IsRemote(),
                   "Could not find remote parent for embedded content doc?");
        RemoteAccessible* outerDoc = parent->AsRemote();
        DocAccessibleParent* embeddingDocument = outerDoc->Document();
        if (embeddingDocument &&
            !embeddingDocument->mOnScreenAccessibles.Contains(outerDoc->ID())) {
          state |= states::OFFSCREEN;
        }
      }
    } else if (!mDoc->mOnScreenAccessibles.Contains(ID())) {
      state |= states::OFFSCREEN;
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
already_AddRefed<AccAttributes> RemoteAccessibleBase<Derived>::Attributes() {
  RefPtr<AccAttributes> attributes = new AccAttributes();
  nsAccessibilityService* accService = GetAccService();
  if (!accService) {
    // The service can be shut down before RemoteAccessibles. If it is shut
    // down, we can't calculate some attributes. We're about to die anyway.
    return attributes.forget();
  }

  if (mCachedFields) {
    // We use GetAttribute instead of GetAttributeRefPtr because we need
    // nsAtom, not const nsAtom.
    if (auto tag =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::tag)) {
      attributes->SetAttribute(nsGkAtoms::tag, *tag);
    }

    GroupPos groupPos = GroupPosition();
    nsAccUtils::SetAccGroupAttrs(attributes, groupPos.level, groupPos.setSize,
                                 groupPos.posInSet);

    bool hierarchical = false;
    uint32_t itemCount = AccGroupInfo::TotalItemCount(this, &hierarchical);
    if (itemCount) {
      attributes->SetAttribute(nsGkAtoms::child_item_count,
                               static_cast<int32_t>(itemCount));
    }

    if (hierarchical) {
      attributes->SetAttribute(nsGkAtoms::tree, true);
    }

    if (auto inputType = mCachedFields->GetAttribute<RefPtr<nsAtom>>(
            nsGkAtoms::textInputType)) {
      attributes->SetAttribute(nsGkAtoms::textInputType, *inputType);
    }

    if (RefPtr<nsAtom> display = DisplayStyle()) {
      attributes->SetAttribute(nsGkAtoms::display, display);
    }

    if (TableCellAccessibleBase* cell = AsTableCellBase()) {
      TableAccessibleBase* table = cell->Table();
      uint32_t row = cell->RowIdx();
      uint32_t col = cell->ColIdx();
      int32_t cellIdx = table->CellIndexAt(row, col);
      if (cellIdx != -1) {
        attributes->SetAttribute(nsGkAtoms::tableCellIndex, cellIdx);
      }
    }

    if (bool layoutGuess = TableIsProbablyForLayout()) {
      attributes->SetAttribute(nsGkAtoms::layout_guess, layoutGuess);
    }

    accService->MarkupAttributes(this, attributes);

    const nsRoleMapEntry* roleMap = ARIARoleMap();
    nsAutoString role;
    mCachedFields->GetAttribute(nsGkAtoms::role, role);
    if (role.IsEmpty()) {
      if (roleMap && roleMap->roleAtom != nsGkAtoms::_empty) {
        // Single, known role.
        attributes->SetAttribute(nsGkAtoms::xmlroles, roleMap->roleAtom);
      } else if (nsAtom* landmark = LandmarkRole()) {
        // Landmark role from markup; e.g. HTML <main>.
        attributes->SetAttribute(nsGkAtoms::xmlroles, landmark);
      }
    } else {
      // Unknown role or multiple roles.
      attributes->SetAttribute(nsGkAtoms::xmlroles, std::move(role));
    }

    if (roleMap) {
      nsAutoString live;
      if (nsAccUtils::GetLiveAttrValue(roleMap->liveAttRule, live)) {
        attributes->SetAttribute(nsGkAtoms::aria_live, std::move(live));
      }
    }

    if (auto ariaAttrs = GetCachedARIAAttributes()) {
      ariaAttrs->CopyTo(attributes);
    }

    nsAccUtils::SetLiveContainerAttributes(attributes, this);
  }

  nsAutoString name;
  if (Name(name) != eNameFromSubtree && !name.IsVoid()) {
    attributes->SetAttribute(nsGkAtoms::explicit_name, true);
  }

  return attributes.forget();
}

template <class Derived>
nsAtom* RemoteAccessibleBase<Derived>::TagName() const {
  if (mCachedFields) {
    if (auto tag =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::tag)) {
      return *tag;
    }
  }

  return nullptr;
}

template <class Derived>
already_AddRefed<nsAtom> RemoteAccessibleBase<Derived>::DisplayStyle() const {
  if (mCachedFields) {
    if (auto display =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::display)) {
      RefPtr<nsAtom> result = *display;
      return result.forget();
    }
  }
  return nullptr;
}

template <class Derived>
Maybe<float> RemoteAccessibleBase<Derived>::Opacity() const {
  if (mCachedFields) {
    // GetAttribute already returns a Maybe<float>, so we don't
    // need to do any additional manipulation.
    return mCachedFields->GetAttribute<float>(nsGkAtoms::opacity);
  }

  return Nothing();
}

template <class Derived>
void RemoteAccessibleBase<Derived>::LiveRegionAttributes(
    nsAString* aLive, nsAString* aRelevant, Maybe<bool>* aAtomic,
    nsAString* aBusy) const {
  if (!mCachedFields) {
    return;
  }
  RefPtr<const AccAttributes> attrs = GetCachedARIAAttributes();
  if (!attrs) {
    return;
  }
  if (aLive) {
    attrs->GetAttribute(nsGkAtoms::aria_live, *aLive);
  }
  if (aRelevant) {
    attrs->GetAttribute(nsGkAtoms::aria_relevant, *aRelevant);
  }
  if (aAtomic) {
    if (auto value =
            attrs->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::aria_atomic)) {
      *aAtomic = Some(*value == nsGkAtoms::_true);
    }
  }
  if (aBusy) {
    attrs->GetAttribute(nsGkAtoms::aria_busy, *aBusy);
  }
}

template <class Derived>
nsAtom* RemoteAccessibleBase<Derived>::GetPrimaryAction() const {
  if (mCachedFields) {
    if (auto action =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::action)) {
      return *action;
    }
  }

  return nullptr;
}

template <class Derived>
uint8_t RemoteAccessibleBase<Derived>::ActionCount() const {
  uint8_t actionCount = 0;
  if (mCachedFields) {
    if (HasPrimaryAction() || ActionAncestor()) {
      actionCount++;
    }

    if (mCachedFields->HasAttribute(nsGkAtoms::longdesc)) {
      actionCount++;
    }
    VERIFY_CACHE(CacheDomain::Actions);
  }

  return actionCount;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::ActionNameAt(uint8_t aIndex,
                                                 nsAString& aName) {
  if (mCachedFields) {
    aName.Truncate();
    nsAtom* action = GetPrimaryAction();
    bool hasActionAncestor = !action && ActionAncestor();

    switch (aIndex) {
      case 0:
        if (action) {
          action->ToString(aName);
        } else if (hasActionAncestor) {
          aName.AssignLiteral("click ancestor");
        } else if (mCachedFields->HasAttribute(nsGkAtoms::longdesc)) {
          aName.AssignLiteral("showlongdesc");
        }
        break;
      case 1:
        if ((action || hasActionAncestor) &&
            mCachedFields->HasAttribute(nsGkAtoms::longdesc)) {
          aName.AssignLiteral("showlongdesc");
        }
        break;
      default:
        break;
    }
  }
  VERIFY_CACHE(CacheDomain::Actions);
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::DoAction(uint8_t aIndex) const {
  if (ActionCount() < aIndex + 1) {
    return false;
  }

  Unused << mDoc->SendDoActionAsync(mID, aIndex);
  return true;
}

template <class Derived>
KeyBinding RemoteAccessibleBase<Derived>::AccessKey() const {
  if (mCachedFields) {
    if (auto value =
            mCachedFields->GetAttribute<uint64_t>(nsGkAtoms::accesskey)) {
      return KeyBinding(*value);
    }
  }
  return KeyBinding();
}

template <class Derived>
void RemoteAccessibleBase<Derived>::SelectionRanges(
    nsTArray<TextRange>* aRanges) const {
  Document()->SelectionRanges(aRanges);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::ARIAGroupPosition(
    int32_t* aLevel, int32_t* aSetSize, int32_t* aPosInSet) const {
  if (!mCachedFields) {
    return;
  }

  if (aLevel) {
    if (auto level =
            mCachedFields->GetAttribute<int32_t>(nsGkAtoms::aria_level)) {
      *aLevel = *level;
    }
  }
  if (aSetSize) {
    if (auto setsize =
            mCachedFields->GetAttribute<int32_t>(nsGkAtoms::aria_setsize)) {
      *aSetSize = *setsize;
    }
  }
  if (aPosInSet) {
    if (auto posinset =
            mCachedFields->GetAttribute<int32_t>(nsGkAtoms::aria_posinset)) {
      *aPosInSet = *posinset;
    }
  }
}

template <class Derived>
AccGroupInfo* RemoteAccessibleBase<Derived>::GetGroupInfo() const {
  if (!mCachedFields) {
    return nullptr;
  }

  if (auto groupInfo = mCachedFields->GetAttribute<UniquePtr<AccGroupInfo>>(
          nsGkAtoms::group)) {
    return groupInfo->get();
  }

  return nullptr;
}

template <class Derived>
AccGroupInfo* RemoteAccessibleBase<Derived>::GetOrCreateGroupInfo() {
  AccGroupInfo* groupInfo = GetGroupInfo();
  if (groupInfo) {
    return groupInfo;
  }

  groupInfo = AccGroupInfo::CreateGroupInfo(this);
  if (groupInfo) {
    if (!mCachedFields) {
      mCachedFields = new AccAttributes();
    }

    mCachedFields->SetAttribute(nsGkAtoms::group, groupInfo);
  }

  return groupInfo;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::InvalidateGroupInfo() {
  if (mCachedFields) {
    mCachedFields->Remove(nsGkAtoms::group);
  }
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::HasPrimaryAction() const {
  return mCachedFields && mCachedFields->HasAttribute(nsGkAtoms::action);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::TakeFocus() const {
  Unused << mDoc->SendTakeFocus(mID);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::ScrollTo(uint32_t aHow) const {
  Unused << mDoc->SendScrollTo(mID, aHow);
}

////////////////////////////////////////////////////////////////////////////////
// SelectAccessible

template <class Derived>
void RemoteAccessibleBase<Derived>::SelectedItems(
    nsTArray<Accessible*>* aItems) {
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTED);
  for (Accessible* selected = p.First(rule); selected;
       selected = p.Next(selected, rule)) {
    aItems->AppendElement(selected);
  }
}

template <class Derived>
uint32_t RemoteAccessibleBase<Derived>::SelectedItemCount() {
  uint32_t count = 0;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTED);
  for (Accessible* selected = p.First(rule); selected;
       selected = p.Next(selected, rule)) {
    count++;
  }

  return count;
}

template <class Derived>
Accessible* RemoteAccessibleBase<Derived>::GetSelectedItem(uint32_t aIndex) {
  uint32_t index = 0;
  Accessible* selected = nullptr;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTED);
  for (selected = p.First(rule); selected && index < aIndex;
       selected = p.Next(selected, rule)) {
    index++;
  }

  return selected;
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::IsItemSelected(uint32_t aIndex) {
  uint32_t index = 0;
  Accessible* selectable = nullptr;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTABLE);
  for (selectable = p.First(rule); selectable && index < aIndex;
       selectable = p.Next(selectable, rule)) {
    index++;
  }

  return selectable && selectable->State() & states::SELECTED;
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::AddItemToSelection(uint32_t aIndex) {
  uint32_t index = 0;
  Accessible* selectable = nullptr;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTABLE);
  for (selectable = p.First(rule); selectable && index < aIndex;
       selectable = p.Next(selectable, rule)) {
    index++;
  }

  if (selectable) selectable->SetSelected(true);

  return static_cast<bool>(selectable);
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::RemoveItemFromSelection(uint32_t aIndex) {
  uint32_t index = 0;
  Accessible* selectable = nullptr;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTABLE);
  for (selectable = p.First(rule); selectable && index < aIndex;
       selectable = p.Next(selectable, rule)) {
    index++;
  }

  if (selectable) selectable->SetSelected(false);

  return static_cast<bool>(selectable);
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::SelectAll() {
  if ((State() & states::MULTISELECTABLE) == 0) {
    return false;
  }

  bool success = false;
  Accessible* selectable = nullptr;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTABLE);
  for (selectable = p.First(rule); selectable;
       selectable = p.Next(selectable, rule)) {
    success = true;
    selectable->SetSelected(true);
  }
  return success;
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::UnselectAll() {
  if ((State() & states::MULTISELECTABLE) == 0) {
    return false;
  }

  bool success = false;
  Accessible* selectable = nullptr;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTABLE);
  for (selectable = p.First(rule); selectable;
       selectable = p.Next(selectable, rule)) {
    success = true;
    selectable->SetSelected(false);
  }
  return success;
}

template <class Derived>
void RemoteAccessibleBase<Derived>::TakeSelection() {
  Unused << mDoc->SendTakeSelection(mID);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::SetSelected(bool aSelect) {
  Unused << mDoc->SendSetSelected(mID, aSelect);
}

template <class Derived>
TableAccessibleBase* RemoteAccessibleBase<Derived>::AsTableBase() {
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup());
  if (IsTable()) {
    return CachedTableAccessible::GetFrom(this);
  }
  return nullptr;
}

template <class Derived>
TableCellAccessibleBase* RemoteAccessibleBase<Derived>::AsTableCellBase() {
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup());
  if (IsTableCell()) {
    return CachedTableCellAccessible::GetFrom(this);
  }
  return nullptr;
}

template <class Derived>
bool RemoteAccessibleBase<Derived>::TableIsProbablyForLayout() {
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup());
  if (mCachedFields) {
    if (auto layoutGuess =
            mCachedFields->GetAttribute<bool>(nsGkAtoms::layout_guess)) {
      return *layoutGuess;
    }
  }
  return false;
}

template <class Derived>
const nsTArray<int32_t>&
RemoteAccessibleBase<Derived>::GetCachedHyperTextOffsets() const {
  if (mCachedFields) {
    if (auto offsets =
            mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::offset)) {
      return *offsets;
    }
  }
  nsTArray<int32_t> newOffsets;
  BuildCachedHyperTextOffsets(newOffsets);
  if (!mCachedFields) {
    const_cast<RemoteAccessibleBase<Derived>*>(this)->mCachedFields =
        new AccAttributes();
  }
  mCachedFields->SetAttribute(nsGkAtoms::offset, std::move(newOffsets));
  return *mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::offset);
}

template <class Derived>
void RemoteAccessibleBase<Derived>::SetCaretOffset(int32_t aOffset) {
  Unused << mDoc->SendSetCaretOffset(mID, aOffset);
}

template <class Derived>
Maybe<int32_t> RemoteAccessibleBase<Derived>::GetIntARIAAttr(
    nsAtom* aAttrName) const {
  if (RefPtr<const AccAttributes> attrs = GetCachedARIAAttributes()) {
    if (auto val = attrs->GetAttribute<int32_t>(aAttrName)) {
      return val;
    }
  }
  return Nothing();
}

template class RemoteAccessibleBase<RemoteAccessible>;

}  // namespace a11y
}  // namespace mozilla
