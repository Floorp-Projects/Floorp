/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAMap.h"
#include "CachedTableAccessible.h"
#include "RemoteAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/gfx/Matrix.h"
#include "nsAccessibilityService.h"
#include "mozilla/Unused.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"
#include "Pivot.h"
#include "Relation.h"
#include "mozilla/a11y/RelationType.h"
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

void RemoteAccessible::Shutdown() {
  MOZ_DIAGNOSTIC_ASSERT(!IsDoc());
  xpcAccessibleDocument* xpcDoc =
      GetAccService()->GetCachedXPCDocument(Document());
  if (xpcDoc) {
    xpcDoc->NotifyOfShutdown(static_cast<RemoteAccessible*>(this));
  }

  if (IsTable() || IsTableCell()) {
    CachedTableAccessible::Invalidate(this);
  }

  // Remove this acc's relation map from the doc's map of
  // reverse relations. Prune forward relations associated with this
  // acc's reverse relations. This also removes the acc's map of reverse
  // rels from the mDoc's mReverseRelations.
  PruneRelationsOnShutdown();

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
  ProxyDestroyed(static_cast<RemoteAccessible*>(this));
  mDoc->RemoveAccessible(static_cast<RemoteAccessible*>(this));
}

void RemoteAccessible::SetChildDoc(DocAccessibleParent* aChildDoc) {
  MOZ_ASSERT(aChildDoc);
  MOZ_ASSERT(mChildren.Length() == 0);
  mChildren.AppendElement(aChildDoc);
}

void RemoteAccessible::ClearChildDoc(DocAccessibleParent* aChildDoc) {
  MOZ_ASSERT(aChildDoc);
  // This is possible if we're replacing one document with another: Doc 1
  // has not had a chance to remove itself, but was already replaced by Doc 2
  // in SetChildDoc(). This could result in two subsequent calls to
  // ClearChildDoc() even though mChildren.Length() == 1.
  MOZ_ASSERT(mChildren.Length() <= 1);
  mChildren.RemoveElement(aChildDoc);
}

uint32_t RemoteAccessible::EmbeddedChildCount() {
  size_t count = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (mChildren[i]->IsEmbeddedObject()) {
      count++;
    }
  }

  return count;
}

int32_t RemoteAccessible::IndexOfEmbeddedChild(Accessible* aChild) {
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

Accessible* RemoteAccessible::EmbeddedChildAt(uint32_t aChildIdx) {
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

LocalAccessible* RemoteAccessible::OuterDocOfRemoteBrowser() const {
  auto tab = static_cast<dom::BrowserParent*>(mDoc->Manager());
  dom::Element* frame = tab->GetOwnerElement();
  NS_ASSERTION(frame, "why isn't the tab in a frame!");
  if (!frame) return nullptr;

  DocAccessible* chromeDoc = GetExistingDocAccessible(frame->OwnerDoc());

  return chromeDoc ? chromeDoc->GetAccessible(frame) : nullptr;
}

void RemoteAccessible::SetParent(RemoteAccessible* aParent) {
  if (!aParent) {
    mParent = kNoParent;
  } else {
    MOZ_ASSERT(!IsDoc() || !aParent->IsDoc());
    mParent = aParent->ID();
  }
}

RemoteAccessible* RemoteAccessible::RemoteParent() const {
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

void RemoteAccessible::ApplyCache(CacheUpdateType aUpdateType,
                                  AccAttributes* aFields) {
  if (!aFields) {
    MOZ_ASSERT_UNREACHABLE("ApplyCache called with aFields == null");
    return;
  }

  const nsTArray<bool> relUpdatesNeeded = PreProcessRelations(aFields);
  if (auto maybeViewportCache =
          aFields->GetAttribute<nsTArray<uint64_t>>(CacheKey::Viewport)) {
    // Updating the viewport cache means the offscreen state of this
    // document's accessibles has changed. Update the HashSet we use for
    // checking offscreen state here.
    MOZ_ASSERT(IsDoc(),
               "Fetched the viewport cache from a non-doc accessible?");
    AsDoc()->mOnScreenAccessibles.Clear();
    for (auto id : *maybeViewportCache) {
      AsDoc()->mOnScreenAccessibles.Insert(id);
    }
  }

  if (aUpdateType == CacheUpdateType::Initial) {
    mCachedFields = aFields;
  } else {
    if (!mCachedFields) {
      // The fields cache can be uninitialized if there were no cache-worthy
      // fields in the initial cache push.
      // We don't do a simple assign because we don't want to store the
      // DeleteEntry entries.
      mCachedFields = new AccAttributes();
    }
    mCachedFields->Update(aFields);
  }

  if (IsTextLeaf()) {
    RemoteAccessible* parent = RemoteParent();
    if (parent && parent->IsHyperText()) {
      parent->InvalidateCachedHyperTextOffsets();
    }
  }

  PostProcessRelations(relUpdatesNeeded);
}

ENameValueFlag RemoteAccessible::Name(nsString& aName) const {
  ENameValueFlag nameFlag = eNameOK;
  if (mCachedFields) {
    if (IsText()) {
      mCachedFields->GetAttribute(CacheKey::Text, aName);
      return eNameOK;
    }
    auto cachedNameFlag =
        mCachedFields->GetAttribute<int32_t>(CacheKey::NameValueFlag);
    if (cachedNameFlag) {
      nameFlag = static_cast<ENameValueFlag>(*cachedNameFlag);
    }
    if (mCachedFields->GetAttribute(CacheKey::Name, aName)) {
      VERIFY_CACHE(CacheDomain::NameAndDescription);
      return nameFlag;
    }
  }

  MOZ_ASSERT(aName.IsEmpty());
  aName.SetIsVoid(true);
  return nameFlag;
}

void RemoteAccessible::Description(nsString& aDescription) const {
  if (mCachedFields) {
    mCachedFields->GetAttribute(CacheKey::Description, aDescription);
    VERIFY_CACHE(CacheDomain::NameAndDescription);
  }
}

void RemoteAccessible::Value(nsString& aValue) const {
  if (mCachedFields) {
    if (mCachedFields->HasAttribute(CacheKey::TextValue)) {
      mCachedFields->GetAttribute(CacheKey::TextValue, aValue);
      VERIFY_CACHE(CacheDomain::Value);
      return;
    }

    if (HasNumericValue()) {
      double checkValue = CurValue();
      if (!std::isnan(checkValue)) {
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
          const_cast<RemoteAccessible*>(this)->GetSelectedItem(0);
      if (option) {
        option->Name(aValue);
      } else {
        // If no selected item, determine the value from descendant elements.
        nsTextEquivUtils::GetTextEquivFromSubtree(this, aValue);
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

double RemoteAccessible::CurValue() const {
  if (mCachedFields) {
    if (auto value =
            mCachedFields->GetAttribute<double>(CacheKey::NumericValue)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *value;
    }
  }

  return UnspecifiedNaN<double>();
}

double RemoteAccessible::MinValue() const {
  if (mCachedFields) {
    if (auto min = mCachedFields->GetAttribute<double>(CacheKey::MinValue)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *min;
    }
  }

  return UnspecifiedNaN<double>();
}

double RemoteAccessible::MaxValue() const {
  if (mCachedFields) {
    if (auto max = mCachedFields->GetAttribute<double>(CacheKey::MaxValue)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *max;
    }
  }

  return UnspecifiedNaN<double>();
}

double RemoteAccessible::Step() const {
  if (mCachedFields) {
    if (auto step = mCachedFields->GetAttribute<double>(CacheKey::Step)) {
      VERIFY_CACHE(CacheDomain::Value);
      return *step;
    }
  }

  return UnspecifiedNaN<double>();
}

bool RemoteAccessible::SetCurValue(double aValue) {
  if (!HasNumericValue() || IsProgress()) {
    return false;
  }

  const uint32_t kValueCannotChange = states::READONLY | states::UNAVAILABLE;
  if (State() & kValueCannotChange) {
    return false;
  }

  double checkValue = MinValue();
  if (!std::isnan(checkValue) && aValue < checkValue) {
    return false;
  }

  checkValue = MaxValue();
  if (!std::isnan(checkValue) && aValue > checkValue) {
    return false;
  }

  Unused << mDoc->SendSetCurValue(mID, aValue);
  return true;
}

bool RemoteAccessible::ContainsPoint(int32_t aX, int32_t aY) {
  if (!BoundsWithOffset(Nothing(), true).Contains(aX, aY)) {
    return false;
  }
  if (!IsTextLeaf()) {
    if (IsImage() || IsImageMap() || !HasChildren() ||
        RefPtr{DisplayStyle()} != nsGkAtoms::inlinevalue) {
      // This isn't an inline element that might contain text, so we don't need
      // to walk lines. It's enough that our rect contains the point.
      return true;
    }
    // Non-image inline elements with children can wrap across lines just like
    // text leaves; see below.
    // Walk the children, which will walk the lines of text in any text leaves.
    uint32_t count = ChildCount();
    for (uint32_t c = 0; c < count; ++c) {
      RemoteAccessible* child = RemoteChildAt(c);
      if (child->Role() == roles::TEXT_CONTAINER && child->IsClipped()) {
        // There is a clipped child. This is a candidate for fuzzy hit testing.
        // See RemoteAccessible::DoFuzzyHittesting.
        return true;
      }
      if (child->ContainsPoint(aX, aY)) {
        return true;
      }
    }
    // None of our descendants contain the point, so nor do we.
    return false;
  }
  // This is a text leaf. The text might wrap across lines, which means our
  // rect might cover a wider area than the actual text. For example, if the
  // text begins in the middle of the first line and wraps on to the second,
  // the rect will cover the start of the first line and the end of the second.
  auto lines = GetCachedTextLines();
  if (!lines) {
    // This means the text is empty or occupies a single line (but does not
    // begin the line). In that case, the Bounds check above is sufficient,
    // since there's only one rect.
    return true;
  }
  uint32_t length = lines->Length();
  MOZ_ASSERT(length > 0,
             "Line starts shouldn't be in cache if there aren't any");
  if (length == 0 || (length == 1 && (*lines)[0] == 0)) {
    // This means the text begins and occupies a single line. Again, the Bounds
    // check above is sufficient.
    return true;
  }
  // Walk the lines of the text. Even if this text doesn't start at the
  // beginning of a line (i.e. lines[0] > 0), we always want to consider its
  // first line.
  int32_t lineStart = 0;
  for (uint32_t index = 0; index <= length; ++index) {
    int32_t lineEnd;
    if (index < length) {
      int32_t nextLineStart = (*lines)[index];
      if (nextLineStart == 0) {
        // This Accessible starts at the beginning of a line. Here, we always
        // treat 0 as the first line start anyway.
        MOZ_ASSERT(index == 0);
        continue;
      }
      lineEnd = nextLineStart - 1;
    } else {
      // This is the last line.
      lineEnd = static_cast<int32_t>(nsAccUtils::TextLength(this)) - 1;
    }
    MOZ_ASSERT(lineEnd >= lineStart);
    nsRect lineRect = GetCachedCharRect(lineStart);
    if (lineEnd > lineStart) {
      lineRect.UnionRect(lineRect, GetCachedCharRect(lineEnd));
    }
    if (BoundsWithOffset(Some(lineRect), true).Contains(aX, aY)) {
      return true;
    }
    lineStart = lineEnd + 1;
  }
  return false;
}

RemoteAccessible* RemoteAccessible::DoFuzzyHittesting() {
  uint32_t childCount = ChildCount();
  if (!childCount) {
    return nullptr;
  }
  // Check if this match has a clipped child.
  // This usually indicates invisible text, and we're
  // interested in returning the inner text content
  // even if it doesn't contain the point we're hittesting.
  RemoteAccessible* clippedContainer = nullptr;
  for (uint32_t i = 0; i < childCount; i++) {
    RemoteAccessible* child = RemoteChildAt(i);
    if (child->Role() == roles::TEXT_CONTAINER) {
      if (child->IsClipped()) {
        clippedContainer = child;
        break;
      }
    }
  }
  // If we found a clipped container, descend it in search of
  // meaningful text leaves. Ignore non-text-leaf/text-container
  // siblings.
  RemoteAccessible* container = clippedContainer;
  while (container) {
    RemoteAccessible* textLeaf = nullptr;
    bool continueSearch = false;
    childCount = container->ChildCount();
    for (uint32_t i = 0; i < childCount; i++) {
      RemoteAccessible* child = container->RemoteChildAt(i);
      if (child->Role() == roles::TEXT_CONTAINER) {
        container = child;
        continueSearch = true;
        break;
      }
      if (child->IsTextLeaf()) {
        textLeaf = child;
        // Don't break here -- it's possible a text container
        // exists as another sibling, and we should descend as
        // deep as possible.
      }
    }
    if (textLeaf) {
      return textLeaf;
    }
    if (!continueSearch) {
      // We didn't find anything useful in this set of siblings.
      // Don't keep searching
      break;
    }
  }
  return nullptr;
}

Accessible* RemoteAccessible::ChildAtPoint(
    int32_t aX, int32_t aY, LocalAccessible::EWhichChildAtPoint aWhichChild) {
  // Elements that are partially on-screen should have their bounds masked by
  // their containing scroll area so hittesting yields results that are
  // consistent with the content's visual representation. Pass this value to
  // bounds calculation functions to indicate that we're hittesting.
  const bool hitTesting = true;

  if (IsOuterDoc() && aWhichChild == EWhichChildAtPoint::DirectChild) {
    // This is an iframe, which is as deep as the viewport cache goes. The
    // caller wants a direct child, which can only be the embedded document.
    if (BoundsWithOffset(Nothing(), hitTesting).Contains(aX, aY)) {
      return RemoteFirstChild();
    }
    return nullptr;
  }

  RemoteAccessible* lastMatch = nullptr;
  // If `this` is a document, use its viewport cache instead of
  // the cache of its parent document.
  if (DocAccessibleParent* doc = IsDoc() ? AsDoc() : mDoc) {
    if (!doc->mCachedFields) {
      // A client call might arrive after we've constructed doc but before we
      // get a cache push for it.
      return nullptr;
    }
    if (auto maybeViewportCache =
            doc->mCachedFields->GetAttribute<nsTArray<uint64_t>>(
                CacheKey::Viewport)) {
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
            acc->BoundsWithOffset(Nothing(), hitTesting).Contains(aX, aY)) {
          // acc is an iframe, which is as deep as the viewport cache goes. This
          // iframe contains the requested point.
          RemoteAccessible* innerDoc = acc->RemoteFirstChild();
          if (innerDoc) {
            MOZ_ASSERT(innerDoc->IsDoc());
            // Search the embedded document's viewport cache so we return the
            // deepest descendant in that embedded document.
            Accessible* deepestAcc = innerDoc->ChildAtPoint(
                aX, aY, EWhichChildAtPoint::DeepestChild);
            MOZ_ASSERT(!deepestAcc || deepestAcc->IsRemote());
            lastMatch = deepestAcc ? deepestAcc->AsRemote() : nullptr;
            break;
          }
          // If there is no embedded document, the iframe itself is the deepest
          // descendant.
          lastMatch = acc;
          break;
        }

        if (acc == this) {
          MOZ_ASSERT(!acc->IsOuterDoc());
          // Even though we're searching from the doc's cache
          // this call shouldn't pass the boundary defined by
          // the acc this call originated on. If we hit `this`,
          // return our most recent match.
          if (!lastMatch &&
              BoundsWithOffset(Nothing(), hitTesting).Contains(aX, aY)) {
            // If we haven't found a match, but `this` contains the point we're
            // looking for, set it as our temp last match so we can
            // (potentially) do fuzzy hittesting on it below.
            lastMatch = acc;
          }
          break;
        }

        if (acc->ContainsPoint(aX, aY)) {
          // Because our rects are in hittesting order, the
          // first match we encounter is guaranteed to be the
          // deepest match.
          lastMatch = acc;
          break;
        }
      }
      if (lastMatch) {
        RemoteAccessible* fuzzyMatch = lastMatch->DoFuzzyHittesting();
        lastMatch = fuzzyMatch ? fuzzyMatch : lastMatch;
      }
    }
  }

  if (aWhichChild == EWhichChildAtPoint::DirectChild && lastMatch) {
    // lastMatch is the deepest match. Walk up to the direct child of this.
    RemoteAccessible* parent = lastMatch->RemoteParent();
    for (;;) {
      if (parent == this) {
        break;
      }
      if (!parent || parent->IsDoc()) {
        // `this` is not an ancestor of lastMatch. Ignore lastMatch.
        lastMatch = nullptr;
        break;
      }
      lastMatch = parent;
      parent = parent->RemoteParent();
    }
  } else if (aWhichChild == EWhichChildAtPoint::DeepestChild && lastMatch &&
             !IsDoc() && !IsAncestorOf(lastMatch)) {
    // If we end up with a match that is not in the ancestor chain
    // of the accessible this call originated on, we should ignore it.
    // This can happen when the aX, aY given are outside `this`.
    lastMatch = nullptr;
  }

  if (!lastMatch && BoundsWithOffset(Nothing(), hitTesting).Contains(aX, aY)) {
    // Even though the hit target isn't inside `this`, the point is still
    // within our bounds, so fall back to `this`.
    return this;
  }

  return lastMatch;
}

Maybe<nsRect> RemoteAccessible::RetrieveCachedBounds() const {
  if (!mCachedFields) {
    return Nothing();
  }

  Maybe<const nsTArray<int32_t>&> maybeArray =
      mCachedFields->GetAttribute<nsTArray<int32_t>>(
          CacheKey::ParentRelativeBounds);
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

void RemoteAccessible::ApplyCrossDocOffset(nsRect& aBounds) const {
  if (!IsDoc()) {
    // We should only apply cross-doc offsets to documents. If we're anything
    // else, return early here.
    return;
  }

  RemoteAccessible* parentAcc = RemoteParent();
  if (!parentAcc || !parentAcc->IsOuterDoc()) {
    return;
  }

  Maybe<const nsTArray<int32_t>&> maybeOffset =
      parentAcc->mCachedFields->GetAttribute<nsTArray<int32_t>>(
          CacheKey::CrossDocOffset);
  if (!maybeOffset) {
    return;
  }

  MOZ_ASSERT(maybeOffset->Length() == 2);
  const nsTArray<int32_t>& offset = *maybeOffset;
  // Our retrieved value is in app units, so we don't need to do any
  // unit conversion here.
  aBounds.MoveBy(offset[0], offset[1]);
}

bool RemoteAccessible::ApplyTransform(nsRect& aCumulativeBounds) const {
  // First, attempt to retrieve the transform from the cache.
  Maybe<const UniquePtr<gfx::Matrix4x4>&> maybeTransform =
      mCachedFields->GetAttribute<UniquePtr<gfx::Matrix4x4>>(
          CacheKey::TransformMatrix);
  if (!maybeTransform) {
    return false;
  }

  auto mtxInPixels = gfx::Matrix4x4Typed<CSSPixel, CSSPixel>::FromUnknownMatrix(
      *(*maybeTransform));

  // Our matrix is in CSS Pixels, so we need our rect to be in CSS
  // Pixels too. Convert before applying.
  auto boundsInPixels = CSSRect::FromAppUnits(aCumulativeBounds);
  boundsInPixels = mtxInPixels.TransformBounds(boundsInPixels);
  aCumulativeBounds = CSSRect::ToAppUnits(boundsInPixels);

  return true;
}

bool RemoteAccessible::ApplyScrollOffset(nsRect& aBounds) const {
  Maybe<const nsTArray<int32_t>&> maybeScrollPosition =
      mCachedFields->GetAttribute<nsTArray<int32_t>>(CacheKey::ScrollPosition);

  if (!maybeScrollPosition || maybeScrollPosition->Length() != 2) {
    return false;
  }
  // Our retrieved value is in app units, so we don't need to do any
  // unit conversion here.
  const nsTArray<int32_t>& scrollPosition = *maybeScrollPosition;

  // Scroll position is an inverse representation of scroll offset (since the
  // further the scroll bar moves down the page, the further the page content
  // moves up/closer to the origin).
  nsPoint scrollOffset(-scrollPosition[0], -scrollPosition[1]);

  aBounds.MoveBy(scrollOffset.x, scrollOffset.y);

  // Return true here even if the scroll offset was 0,0 because the RV is used
  // as a scroll container indicator. Non-scroll containers won't have cached
  // scroll position.
  return true;
}

nsRect RemoteAccessible::BoundsInAppUnits() const {
  if (dom::CanonicalBrowsingContext* cbc = mDoc->GetBrowsingContext()->Top()) {
    if (dom::BrowserParent* bp = cbc->GetBrowserParent()) {
      DocAccessibleParent* topDoc = bp->GetTopLevelDocAccessible();
      if (topDoc && topDoc->mCachedFields) {
        auto appUnitsPerDevPixel = topDoc->mCachedFields->GetAttribute<int32_t>(
            CacheKey::AppUnitsPerDevPixel);
        MOZ_ASSERT(appUnitsPerDevPixel);
        return LayoutDeviceIntRect::ToAppUnits(Bounds(), *appUnitsPerDevPixel);
      }
    }
  }
  return LayoutDeviceIntRect::ToAppUnits(Bounds(), AppUnitsPerCSSPixel());
}

bool RemoteAccessible::IsFixedPos() const {
  MOZ_ASSERT(mCachedFields);
  if (auto maybePosition =
          mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::CssPosition)) {
    return *maybePosition == nsGkAtoms::fixed;
  }

  return false;
}

bool RemoteAccessible::IsOverflowHidden() const {
  MOZ_ASSERT(mCachedFields);
  if (auto maybeOverflow =
          mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::CSSOverflow)) {
    return *maybeOverflow == nsGkAtoms::hidden;
  }

  return false;
}

bool RemoteAccessible::IsClipped() const {
  MOZ_ASSERT(mCachedFields);
  if (mCachedFields->GetAttribute<bool>(CacheKey::IsClipped)) {
    return true;
  }

  return false;
}

LayoutDeviceIntRect RemoteAccessible::BoundsWithOffset(
    Maybe<nsRect> aOffset, bool aBoundsAreForHittesting) const {
  Maybe<nsRect> maybeBounds = RetrieveCachedBounds();
  if (maybeBounds) {
    nsRect bounds = *maybeBounds;
    // maybeBounds is parent-relative. However, the transform matrix we cache
    // (if any) is meant to operate on self-relative rects. Therefore, make
    // bounds self-relative until after we transform.
    bounds.MoveTo(0, 0);
    const DocAccessibleParent* topDoc = IsDoc() ? AsDoc() : nullptr;

    if (aOffset.isSome()) {
      // The rect we've passed in is in app units, so no conversion needed.
      nsRect internalRect = *aOffset;
      bounds.SetRectX(bounds.x + internalRect.x, internalRect.width);
      bounds.SetRectY(bounds.y + internalRect.y, internalRect.height);
    }

    Unused << ApplyTransform(bounds);
    // Now apply the parent-relative offset.
    bounds.MoveBy(maybeBounds->TopLeft());

    ApplyCrossDocOffset(bounds);

    LayoutDeviceIntRect devPxBounds;
    const Accessible* acc = Parent();
    bool encounteredFixedContainer = IsFixedPos();
    while (acc && acc->IsRemote()) {
      // Return early if we're hit testing and our cumulative bounds are empty,
      // since walking the ancestor chain won't produce any hits.
      if (aBoundsAreForHittesting && bounds.IsEmpty()) {
        return LayoutDeviceIntRect{};
      }

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
                  CacheKey::Resolution);
          MOZ_ASSERT(res, "No cached document resolution found.");
          bounds.ScaleRoundOut(res.valueOr(1.0f));

          topDoc = remoteAcc->AsDoc();
        }

        // We don't account for the document offset of iframes when
        // computing parent-relative bounds. Instead, we store this value
        // separately on all iframes and apply it here. See the comments in
        // LocalAccessible::BundleFieldsForCache where we set the
        // nsGkAtoms::crossorigin attribute.
        remoteAcc->ApplyCrossDocOffset(remoteBounds);
        if (!encounteredFixedContainer) {
          // Apply scroll offset, if applicable. Only the contents of an
          // element are affected by its scroll offset, which is why this call
          // happens in this loop instead of both inside and outside of
          // the loop (like ApplyTransform).
          // Never apply scroll offsets past a fixed container.
          const bool hasScrollArea = remoteAcc->ApplyScrollOffset(bounds);

          // If we are hit testing and the Accessible has a scroll area, ensure
          // that the bounds we've calculated so far are constrained to the
          // bounds of the scroll area. Without this, we'll "hit" the off-screen
          // portions of accs that are are partially (but not fully) within the
          // scroll area. This is also a problem for accs with overflow:hidden;
          if (aBoundsAreForHittesting &&
              (hasScrollArea || remoteAcc->IsOverflowHidden())) {
            nsRect selfRelativeVisibleBounds(0, 0, remoteBounds.width,
                                             remoteBounds.height);
            bounds = bounds.SafeIntersect(selfRelativeVisibleBounds);
          }
        }
        if (remoteAcc->IsDoc()) {
          // Fixed elements are document relative, so if we've hit a
          // document we're now subject to that document's styling
          // (including scroll offsets that operate on it).
          // This ordering is important, we don't want to apply scroll
          // offsets on this doc's content.
          encounteredFixedContainer = false;
        }
        if (!encounteredFixedContainer) {
          // The transform matrix we cache (if any) is meant to operate on
          // self-relative rects. Therefore, we must apply the transform before
          // we make bounds parent-relative.
          Unused << remoteAcc->ApplyTransform(bounds);
          // Regardless of whether this is a doc, we should offset `bounds`
          // by the bounds retrieved here. This is how we build screen
          // coordinates from relative coordinates.
          bounds.MoveBy(remoteBounds.X(), remoteBounds.Y());
        }

        if (remoteAcc->IsFixedPos()) {
          encounteredFixedContainer = true;
        }
        // we can't just break here if we're scroll suppressed because we still
        // need to find the top doc.
      }
      acc = acc->Parent();
    }

    MOZ_ASSERT(topDoc);
    if (topDoc) {
      // We use the top documents app-units-per-dev-pixel even though
      // theoretically nested docs can have different values. Practically,
      // that isn't likely since we only offer zoom controls for the top
      // document and all subdocuments inherit from it.
      auto appUnitsPerDevPixel = topDoc->mCachedFields->GetAttribute<int32_t>(
          CacheKey::AppUnitsPerDevPixel);
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

LayoutDeviceIntRect RemoteAccessible::Bounds() const {
  return BoundsWithOffset(Nothing());
}

Relation RemoteAccessible::RelationByType(RelationType aType) const {
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

  if (aType == RelationType::LINKS_TO && Role() == roles::LINK) {
    Pivot p = Pivot(mDoc);
    nsString href;
    Value(href);
    int32_t i = href.FindChar('#');
    int32_t len = static_cast<int32_t>(href.Length());
    if (i != -1 && i < (len - 1)) {
      nsDependentSubstring anchorName = Substring(href, i + 1, len);
      MustPruneSameDocRule rule;
      Accessible* nameMatch = nullptr;
      for (Accessible* match = p.Next(mDoc, rule); match;
           match = p.Next(match, rule)) {
        nsString currID;
        match->DOMNodeID(currID);
        MOZ_ASSERT(match->IsRemote());
        if (anchorName.Equals(currID)) {
          return Relation(match->AsRemote());
        }
        if (!nameMatch) {
          nsString currName = match->AsRemote()->GetCachedHTMLNameAttribute();
          if (match->TagName() == nsGkAtoms::a && anchorName.Equals(currName)) {
            // If we find an element with a matching ID, we should return
            // that, but if we don't we should return the first anchor with
            // a matching name. To avoid doing two traversals, store the first
            // name match here.
            nameMatch = match;
          }
        }
      }
      return nameMatch ? Relation(nameMatch->AsRemote()) : Relation();
    }

    return Relation();
  }

  // Handle ARIA tree, treegrid parent/child relations. Each of these cases
  // relies on cached group info. To find the parent of an accessible, use the
  // unified conceptual parent.
  if (aType == RelationType::NODE_CHILD_OF) {
    const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
    if (roleMapEntry && (roleMapEntry->role == roles::OUTLINEITEM ||
                         roleMapEntry->role == roles::LISTITEM ||
                         roleMapEntry->role == roles::ROW)) {
      if (const AccGroupInfo* groupInfo =
              const_cast<RemoteAccessible*>(this)->GetOrCreateGroupInfo()) {
        return Relation(groupInfo->ConceptualParent());
      }
    }
    return Relation();
  }

  // To find the children of a parent, provide an iterator through its items.
  if (aType == RelationType::NODE_PARENT_OF) {
    const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
    if (roleMapEntry && (roleMapEntry->role == roles::OUTLINEITEM ||
                         roleMapEntry->role == roles::LISTITEM ||
                         roleMapEntry->role == roles::ROW ||
                         roleMapEntry->role == roles::OUTLINE ||
                         roleMapEntry->role == roles::LIST ||
                         roleMapEntry->role == roles::TREE_TABLE)) {
      return Relation(new ItemIterator(this));
    }
    return Relation();
  }

  if (aType == RelationType::MEMBER_OF) {
    Relation rel = Relation();
    // HTML radio buttons with cached names should be grouped.
    if (IsHTMLRadioButton()) {
      nsString name = GetCachedHTMLNameAttribute();
      if (name.IsEmpty()) {
        return rel;
      }

      RemoteAccessible* ancestor = RemoteParent();
      while (ancestor && ancestor->Role() != roles::FORM && ancestor != mDoc) {
        ancestor = ancestor->RemoteParent();
      }
      if (ancestor) {
        // Sometimes we end up with an unparented acc here, potentially
        // because the acc is being moved. See bug 1807639.
        // Pivot expects to be created with a non-null mRoot.
        Pivot p = Pivot(ancestor);
        PivotRadioNameRule rule(name);
        Accessible* match = p.Next(ancestor, rule);
        while (match) {
          rel.AppendTarget(match->AsRemote());
          match = p.Next(match, rule);
        }
      }
      return rel;
    }

    if (IsARIARole(nsGkAtoms::radio)) {
      // ARIA radio buttons should be grouped by their radio group
      // parent, if one exists.
      RemoteAccessible* currParent = RemoteParent();
      while (currParent && currParent->Role() != roles::RADIO_GROUP) {
        currParent = currParent->RemoteParent();
      }

      if (currParent && currParent->Role() == roles::RADIO_GROUP) {
        // If we found a radiogroup parent, search for all
        // roles::RADIOBUTTON children and add them to our relation.
        // This search will include the radio button this method
        // was called from, which is expected.
        Pivot p = Pivot(currParent);
        PivotRoleRule rule(roles::RADIOBUTTON);
        Accessible* match = p.Next(currParent, rule);
        while (match) {
          MOZ_ASSERT(match->IsRemote(),
                     "We should only be traversing the remote tree.");
          rel.AppendTarget(match->AsRemote());
          match = p.Next(match, rule);
        }
      }
    }
    // By webkit's standard, aria radio buttons do not get grouped
    // if they lack a group parent, so we return an empty
    // relation here if the above check fails.
    return rel;
  }

  Relation rel;
  if (!mCachedFields) {
    return rel;
  }

  for (const auto& data : kRelationTypeAtoms) {
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
    if (auto reverseIdsEntry = accRelMapEntry.Data().Lookup(aType)) {
      rel.AppendIter(new RemoteAccIterator(reverseIdsEntry.Data(), Document()));
    }
  }

  // We handle these relations here rather than before cached relations because
  // the cached relations need to take precedence. For example, a <figure> with
  // both aria-labelledby and a <figcaption> must return two LABELLED_BY
  // targets: the aria-labelledby and then the <figcaption>.
  if (aType == RelationType::LABELLED_BY && TagName() == nsGkAtoms::figure) {
    uint32_t count = ChildCount();
    for (uint32_t c = 0; c < count; ++c) {
      RemoteAccessible* child = RemoteChildAt(c);
      MOZ_ASSERT(child);
      if (child->TagName() == nsGkAtoms::figcaption) {
        rel.AppendTarget(child);
      }
    }
  } else if (aType == RelationType::LABEL_FOR &&
             TagName() == nsGkAtoms::figcaption) {
    if (RemoteAccessible* parent = RemoteParent()) {
      if (parent->TagName() == nsGkAtoms::figure) {
        rel.AppendTarget(parent);
      }
    }
  }

  return rel;
}

void RemoteAccessible::AppendTextTo(nsAString& aText, uint32_t aStartOffset,
                                    uint32_t aLength) {
  if (IsText()) {
    if (mCachedFields) {
      if (auto text = mCachedFields->GetAttribute<nsString>(CacheKey::Text)) {
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

nsTArray<bool> RemoteAccessible::PreProcessRelations(AccAttributes* aFields) {
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
                aFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::TagName)) {
          tag = *maybeTag;
        }
      }
      MOZ_ASSERT(
          tag || IsTextLeaf() || IsDoc(),
          "Could not fetch tag via TagName() or from initial cache push!");
      if (tag != data.mValidTag) {
        // If this rel doesn't apply to us, do no pre-processing. Also,
        // note in our updateTracker that we should do no post-processing.
        updateTracker.AppendElement(false);
        continue;
      }
    }

    nsStaticAtom* const relAtom = data.mAtom;
    auto newRelationTargets =
        aFields->GetAttribute<nsTArray<uint64_t>>(relAtom);
    bool shouldAddNewImplicitRels =
        newRelationTargets && newRelationTargets->Length();

    // Remove existing implicit relations if we need to perform an update, or
    // if we've received a DeleteEntry(). Only do this if mCachedFields is
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
          // We need to call `Lookup` here instead of `LookupOrInsert` because
          // it's possible the ID we're querying is from an acc that has since
          // been Shutdown(), and so has intentionally removed its reverse rels
          // from the doc's reverse rel cache.
          if (auto reverseRels = Document()->mReverseRelations.Lookup(id)) {
            // Then fetch its reverse relation's ID list. This should be safe
            // to do via LookupOrInsert because by the time we've gotten here,
            // we know the acc and `this` are still alive in the doc. If we hit
            // the following assert, we don't have parity on implicit/explicit
            // rels and something is wrong.
            nsTArray<uint64_t>& reverseRelIDs =
                reverseRels->LookupOrInsert(data.mReverseType);
            //  There might be other reverse relations stored for this acc, so
            //  remove our ID instead of deleting the array entirely.
            DebugOnly<bool> removed = reverseRelIDs.RemoveElement(ID());
            MOZ_ASSERT(removed, "Can't find old reverse relation");
          }
        }
      }
    }

    updateTracker.AppendElement(shouldAddNewImplicitRels);
  }

  return updateTracker;
}

void RemoteAccessible::PostProcessRelations(const nsTArray<bool>& aToUpdate) {
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
        nsTHashMap<RelationType, nsTArray<uint64_t>>& relations =
            Document()->mReverseRelations.LookupOrInsert(id);
        nsTArray<uint64_t>& ids = relations.LookupOrInsert(data.mReverseType);
        ids.AppendElement(ID());
      }
    }
  }
}

void RemoteAccessible::PruneRelationsOnShutdown() {
  auto reverseRels = mDoc->mReverseRelations.Lookup(ID());
  if (!reverseRels) {
    return;
  }
  for (auto const& data : kRelationTypeAtoms) {
    // Fetch the list of targets for this reverse relation
    auto reverseTargetList = reverseRels->Lookup(data.mReverseType);
    if (!reverseTargetList) {
      continue;
    }
    for (uint64_t id : *reverseTargetList) {
      // For each target, retrieve its corresponding forward relation target
      // list
      RemoteAccessible* affectedAcc = mDoc->GetAccessible(id);
      if (!affectedAcc) {
        // It's possible the affect acc also shut down, in which case
        // we don't have anything to update.
        continue;
      }
      if (auto forwardTargetList =
              affectedAcc->mCachedFields
                  ->GetMutableAttribute<nsTArray<uint64_t>>(data.mAtom)) {
        forwardTargetList->RemoveElement(ID());
        if (!forwardTargetList->Length()) {
          // The ID we removed was the only thing in the list, so remove the
          // entry from the cache entirely -- don't leave an empty array.
          affectedAcc->mCachedFields->Remove(data.mAtom);
        }
      }
    }
  }
  // Remove this ID from the document's map of reverse relations.
  reverseRels.Remove();
}

uint32_t RemoteAccessible::GetCachedTextLength() {
  MOZ_ASSERT(!HasChildren());
  if (!mCachedFields) {
    return 0;
  }
  VERIFY_CACHE(CacheDomain::Text);
  auto text = mCachedFields->GetAttribute<nsString>(CacheKey::Text);
  if (!text) {
    return 0;
  }
  return text->Length();
}

Maybe<const nsTArray<int32_t>&> RemoteAccessible::GetCachedTextLines() {
  MOZ_ASSERT(!HasChildren());
  if (!mCachedFields) {
    return Nothing();
  }
  VERIFY_CACHE(CacheDomain::Text);
  return mCachedFields->GetAttribute<nsTArray<int32_t>>(
      CacheKey::TextLineStarts);
}

nsRect RemoteAccessible::GetCachedCharRect(int32_t aOffset) {
  MOZ_ASSERT(IsText());
  if (!mCachedFields) {
    return nsRect();
  }

  if (Maybe<const nsTArray<int32_t>&> maybeCharData =
          mCachedFields->GetAttribute<nsTArray<int32_t>>(
              CacheKey::TextBounds)) {
    const nsTArray<int32_t>& charData = *maybeCharData;
    const int32_t index = aOffset * kNumbersInRect;
    if (index < static_cast<int32_t>(charData.Length())) {
      return nsRect(charData[index], charData[index + 1], charData[index + 2],
                    charData[index + 3]);
    }
    // It is valid for a client to call this with an offset 1 after the last
    // character because of the insertion point at the end of text boxes.
    MOZ_ASSERT(index == static_cast<int32_t>(charData.Length()));
  }

  return nsRect();
}

void RemoteAccessible::DOMNodeID(nsString& aID) const {
  if (mCachedFields) {
    mCachedFields->GetAttribute(CacheKey::DOMNodeID, aID);
    VERIFY_CACHE(CacheDomain::DOMNodeIDAndClass);
  }
}

void RemoteAccessible::ScrollToPoint(uint32_t aScrollType, int32_t aX,
                                     int32_t aY) {
  Unused << mDoc->SendScrollToPoint(mID, aScrollType, aX, aY);
}

#if !defined(XP_WIN)
void RemoteAccessible::Announce(const nsString& aAnnouncement,
                                uint16_t aPriority) {
  Unused << mDoc->SendAnnounce(mID, aAnnouncement, aPriority);
}
#endif  // !defined(XP_WIN)

void RemoteAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                              int32_t aEndOffset,
                                              uint32_t aCoordinateType,
                                              int32_t aX, int32_t aY) {
  Unused << mDoc->SendScrollSubstringToPoint(mID, aStartOffset, aEndOffset,
                                             aCoordinateType, aX, aY);
}

RefPtr<const AccAttributes> RemoteAccessible::GetCachedTextAttributes() {
  MOZ_ASSERT(IsText() || IsHyperText());
  if (mCachedFields) {
    auto attrs = mCachedFields->GetAttributeRefPtr<AccAttributes>(
        CacheKey::TextAttributes);
    VERIFY_CACHE(CacheDomain::Text);
    return attrs;
  }
  return nullptr;
}

already_AddRefed<AccAttributes> RemoteAccessible::DefaultTextAttributes() {
  RefPtr<const AccAttributes> attrs = GetCachedTextAttributes();
  RefPtr<AccAttributes> result = new AccAttributes();
  if (attrs) {
    attrs->CopyTo(result);
  }
  return result.forget();
}

RefPtr<const AccAttributes> RemoteAccessible::GetCachedARIAAttributes() const {
  if (mCachedFields) {
    auto attrs = mCachedFields->GetAttributeRefPtr<AccAttributes>(
        CacheKey::ARIAAttributes);
    VERIFY_CACHE(CacheDomain::ARIA);
    return attrs;
  }
  return nullptr;
}

nsString RemoteAccessible::GetCachedHTMLNameAttribute() const {
  if (mCachedFields) {
    if (auto maybeName =
            mCachedFields->GetAttribute<nsString>(CacheKey::DOMName)) {
      return *maybeName;
    }
  }
  return nsString();
}

uint64_t RemoteAccessible::State() {
  uint64_t state = 0;
  if (mCachedFields) {
    if (auto rawState =
            mCachedFields->GetAttribute<uint64_t>(CacheKey::State)) {
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

    ApplyImplicitState(state);

    auto* cbc = mDoc->GetBrowsingContext();
    if (cbc && !cbc->IsActive()) {
      // If our browsing context is _not_ active, we're in a background tab
      // and inherently offscreen.
      state |= states::OFFSCREEN;
    } else {
      // If we're in an active browsing context, there are a few scenarios we
      // need to address:
      // - We are an iframe document in the visual viewport
      // - We are an iframe document out of the visual viewport
      // - We are non-iframe content in the visual viewport
      // - We are non-iframe content out of the visual viewport
      // We assume top level tab docs are on screen if their BC is active, so
      // we don't need additional handling for them here.
      if (!mDoc->IsTopLevel()) {
        // Here we handle iframes and iframe content.
        // We use an iframe's outer doc's position in the embedding document's
        // viewport to determine if the iframe has been scrolled offscreen.
        Accessible* docParent = mDoc->Parent();
        // In rare cases, we might not have an outer doc yet. Return if that's
        // the case.
        if (NS_WARN_IF(!docParent || !docParent->IsRemote())) {
          return state;
        }

        RemoteAccessible* outerDoc = docParent->AsRemote();
        DocAccessibleParent* embeddingDocument = outerDoc->Document();
        if (embeddingDocument &&
            !embeddingDocument->mOnScreenAccessibles.Contains(outerDoc->ID())) {
          // Our embedding document's viewport cache doesn't contain the ID of
          // our outer doc, so this iframe (and any of its content) is
          // offscreen.
          state |= states::OFFSCREEN;
        } else if (this != mDoc && !mDoc->mOnScreenAccessibles.Contains(ID())) {
          // Our embedding document's viewport cache contains the ID of our
          // outer doc, but the iframe's viewport cache doesn't contain our ID.
          // We are offscreen.
          state |= states::OFFSCREEN;
        }
      } else if (this != mDoc && !mDoc->mOnScreenAccessibles.Contains(ID())) {
        // We are top level tab content (but not a top level tab doc).
        // If our tab doc's viewport cache doesn't contain our ID, we're
        // offscreen.
        state |= states::OFFSCREEN;
      }
    }
  }

  return state;
}

already_AddRefed<AccAttributes> RemoteAccessible::Attributes() {
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
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::TagName)) {
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

    if (auto inputType =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::InputType)) {
      attributes->SetAttribute(nsGkAtoms::textInputType, *inputType);
    }

    if (RefPtr<nsAtom> display = DisplayStyle()) {
      attributes->SetAttribute(nsGkAtoms::display, display);
    }

    if (TableCellAccessible* cell = AsTableCell()) {
      TableAccessible* table = cell->Table();
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
    mCachedFields->GetAttribute(CacheKey::ARIARole, role);
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

    nsString id;
    DOMNodeID(id);
    if (!id.IsEmpty()) {
      attributes->SetAttribute(nsGkAtoms::id, std::move(id));
    }

    nsString className;
    mCachedFields->GetAttribute(CacheKey::DOMNodeClass, className);
    if (!className.IsEmpty()) {
      attributes->SetAttribute(nsGkAtoms::_class, std::move(className));
    }

    if (IsImage()) {
      nsString src;
      mCachedFields->GetAttribute(CacheKey::SrcURL, src);
      if (!src.IsEmpty()) {
        attributes->SetAttribute(nsGkAtoms::src, std::move(src));
      }
    }

    if (IsTextField()) {
      nsString placeholder;
      mCachedFields->GetAttribute(CacheKey::HTMLPlaceholder, placeholder);
      if (!placeholder.IsEmpty()) {
        attributes->SetAttribute(nsGkAtoms::placeholder,
                                 std::move(placeholder));
        attributes->Remove(nsGkAtoms::aria_placeholder);
      }
    }
  }

  nsAutoString name;
  if (Name(name) != eNameFromSubtree && !name.IsVoid()) {
    attributes->SetAttribute(nsGkAtoms::explicit_name, true);
  }

  // Expose the string value via the valuetext attribute. We test for the value
  // interface because we don't want to expose traditional Value() information
  // such as URLs on links and documents, or text in an input.
  // XXX This is only needed for ATK, since other APIs have native ways to
  // retrieve value text. We should probably move this into ATK specific code.
  // For now, we do this because LocalAccessible does it.
  if (HasNumericValue()) {
    nsString valuetext;
    Value(valuetext);
    attributes->SetAttribute(nsGkAtoms::aria_valuetext, std::move(valuetext));
  }

  return attributes.forget();
}

nsAtom* RemoteAccessible::TagName() const {
  if (mCachedFields) {
    if (auto tag =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::TagName)) {
      return *tag;
    }
  }

  return nullptr;
}

already_AddRefed<nsAtom> RemoteAccessible::InputType() const {
  if (mCachedFields) {
    if (auto inputType =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::InputType)) {
      RefPtr<nsAtom> result = *inputType;
      return result.forget();
    }
  }

  return nullptr;
}

already_AddRefed<nsAtom> RemoteAccessible::DisplayStyle() const {
  if (mCachedFields) {
    if (auto display =
            mCachedFields->GetAttribute<RefPtr<nsAtom>>(CacheKey::CSSDisplay)) {
      RefPtr<nsAtom> result = *display;
      return result.forget();
    }
  }
  return nullptr;
}

float RemoteAccessible::Opacity() const {
  if (mCachedFields) {
    if (auto opacity = mCachedFields->GetAttribute<float>(CacheKey::Opacity)) {
      return *opacity;
    }
  }

  return 1.0f;
}

void RemoteAccessible::LiveRegionAttributes(nsAString* aLive,
                                            nsAString* aRelevant,
                                            Maybe<bool>* aAtomic,
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

Maybe<bool> RemoteAccessible::ARIASelected() const {
  if (mCachedFields) {
    return mCachedFields->GetAttribute<bool>(CacheKey::ARIASelected);
  }
  return Nothing();
}

nsAtom* RemoteAccessible::GetPrimaryAction() const {
  if (mCachedFields) {
    if (auto action = mCachedFields->GetAttribute<RefPtr<nsAtom>>(
            CacheKey::PrimaryAction)) {
      return *action;
    }
  }

  return nullptr;
}

uint8_t RemoteAccessible::ActionCount() const {
  uint8_t actionCount = 0;
  if (mCachedFields) {
    if (HasPrimaryAction() || ActionAncestor()) {
      actionCount++;
    }

    if (mCachedFields->HasAttribute(CacheKey::HasLongdesc)) {
      actionCount++;
    }
    VERIFY_CACHE(CacheDomain::Actions);
  }

  return actionCount;
}

void RemoteAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
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
        } else if (mCachedFields->HasAttribute(CacheKey::HasLongdesc)) {
          aName.AssignLiteral("showlongdesc");
        }
        break;
      case 1:
        if ((action || hasActionAncestor) &&
            mCachedFields->HasAttribute(CacheKey::HasLongdesc)) {
          aName.AssignLiteral("showlongdesc");
        }
        break;
      default:
        break;
    }
  }
  VERIFY_CACHE(CacheDomain::Actions);
}

bool RemoteAccessible::DoAction(uint8_t aIndex) const {
  if (ActionCount() < aIndex + 1) {
    return false;
  }

  Unused << mDoc->SendDoActionAsync(mID, aIndex);
  return true;
}

KeyBinding RemoteAccessible::AccessKey() const {
  if (mCachedFields) {
    if (auto value =
            mCachedFields->GetAttribute<uint64_t>(CacheKey::AccessKey)) {
      return KeyBinding(*value);
    }
  }
  return KeyBinding();
}

void RemoteAccessible::SelectionRanges(nsTArray<TextRange>* aRanges) const {
  Document()->SelectionRanges(aRanges);
}

bool RemoteAccessible::RemoveFromSelection(int32_t aSelectionNum) {
  MOZ_ASSERT(IsHyperText());
  if (SelectionCount() <= aSelectionNum) {
    return false;
  }

  Unused << mDoc->SendRemoveTextSelection(mID, aSelectionNum);

  return true;
}

void RemoteAccessible::ARIAGroupPosition(int32_t* aLevel, int32_t* aSetSize,
                                         int32_t* aPosInSet) const {
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

AccGroupInfo* RemoteAccessible::GetGroupInfo() const {
  if (!mCachedFields) {
    return nullptr;
  }

  if (auto groupInfo = mCachedFields->GetAttribute<UniquePtr<AccGroupInfo>>(
          CacheKey::GroupInfo)) {
    return groupInfo->get();
  }

  return nullptr;
}

AccGroupInfo* RemoteAccessible::GetOrCreateGroupInfo() {
  AccGroupInfo* groupInfo = GetGroupInfo();
  if (groupInfo) {
    return groupInfo;
  }

  groupInfo = AccGroupInfo::CreateGroupInfo(this);
  if (groupInfo) {
    if (!mCachedFields) {
      mCachedFields = new AccAttributes();
    }

    mCachedFields->SetAttribute(CacheKey::GroupInfo, groupInfo);
  }

  return groupInfo;
}

void RemoteAccessible::InvalidateGroupInfo() {
  if (mCachedFields) {
    mCachedFields->Remove(CacheKey::GroupInfo);
  }
}

void RemoteAccessible::GetPositionAndSetSize(int32_t* aPosInSet,
                                             int32_t* aSetSize) {
  if (IsHTMLRadioButton()) {
    *aSetSize = 0;
    Relation rel = RelationByType(RelationType::MEMBER_OF);
    while (Accessible* radio = rel.Next()) {
      ++*aSetSize;
      if (radio == this) {
        *aPosInSet = *aSetSize;
      }
    }
    return;
  }

  Accessible::GetPositionAndSetSize(aPosInSet, aSetSize);
}

bool RemoteAccessible::HasPrimaryAction() const {
  return mCachedFields && mCachedFields->HasAttribute(CacheKey::PrimaryAction);
}

void RemoteAccessible::TakeFocus() const { Unused << mDoc->SendTakeFocus(mID); }

void RemoteAccessible::ScrollTo(uint32_t aHow) const {
  Unused << mDoc->SendScrollTo(mID, aHow);
}

////////////////////////////////////////////////////////////////////////////////
// SelectAccessible

void RemoteAccessible::SelectedItems(nsTArray<Accessible*>* aItems) {
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTED);
  for (Accessible* selected = p.First(rule); selected;
       selected = p.Next(selected, rule)) {
    aItems->AppendElement(selected);
  }
}

uint32_t RemoteAccessible::SelectedItemCount() {
  uint32_t count = 0;
  Pivot p = Pivot(this);
  PivotStateRule rule(states::SELECTED);
  for (Accessible* selected = p.First(rule); selected;
       selected = p.Next(selected, rule)) {
    count++;
  }

  return count;
}

Accessible* RemoteAccessible::GetSelectedItem(uint32_t aIndex) {
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

bool RemoteAccessible::IsItemSelected(uint32_t aIndex) {
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

bool RemoteAccessible::AddItemToSelection(uint32_t aIndex) {
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

bool RemoteAccessible::RemoveItemFromSelection(uint32_t aIndex) {
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

bool RemoteAccessible::SelectAll() {
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

bool RemoteAccessible::UnselectAll() {
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

void RemoteAccessible::TakeSelection() {
  Unused << mDoc->SendTakeSelection(mID);
}

void RemoteAccessible::SetSelected(bool aSelect) {
  Unused << mDoc->SendSetSelected(mID, aSelect);
}

TableAccessible* RemoteAccessible::AsTable() {
  if (IsTable()) {
    return CachedTableAccessible::GetFrom(this);
  }
  return nullptr;
}

TableCellAccessible* RemoteAccessible::AsTableCell() {
  if (IsTableCell()) {
    return CachedTableCellAccessible::GetFrom(this);
  }
  return nullptr;
}

bool RemoteAccessible::TableIsProbablyForLayout() {
  if (mCachedFields) {
    if (auto layoutGuess =
            mCachedFields->GetAttribute<bool>(CacheKey::TableLayoutGuess)) {
      return *layoutGuess;
    }
  }
  return false;
}

nsTArray<int32_t>& RemoteAccessible::GetCachedHyperTextOffsets() {
  if (mCachedFields) {
    if (auto offsets = mCachedFields->GetMutableAttribute<nsTArray<int32_t>>(
            CacheKey::HyperTextOffsets)) {
      return *offsets;
    }
  }
  nsTArray<int32_t> newOffsets;
  if (!mCachedFields) {
    mCachedFields = new AccAttributes();
  }
  mCachedFields->SetAttribute(CacheKey::HyperTextOffsets,
                              std::move(newOffsets));
  return *mCachedFields->GetMutableAttribute<nsTArray<int32_t>>(
      CacheKey::HyperTextOffsets);
}

void RemoteAccessible::SetCaretOffset(int32_t aOffset) {
  Unused << mDoc->SendSetCaretOffset(mID, aOffset);
}

Maybe<int32_t> RemoteAccessible::GetIntARIAAttr(nsAtom* aAttrName) const {
  if (RefPtr<const AccAttributes> attrs = GetCachedARIAAttributes()) {
    if (auto val = attrs->GetAttribute<int32_t>(aAttrName)) {
      return val;
    }
  }
  return Nothing();
}

void RemoteAccessible::Language(nsAString& aLocale) {
  if (!IsHyperText()) {
    return;
  }
  if (auto attrs = GetCachedTextAttributes()) {
    attrs->GetAttribute(nsGkAtoms::language, aLocale);
  }
}

void RemoteAccessible::ReplaceText(const nsAString& aText) {
  Unused << mDoc->SendReplaceText(mID, aText);
}

void RemoteAccessible::InsertText(const nsAString& aText, int32_t aPosition) {
  Unused << mDoc->SendInsertText(mID, aText, aPosition);
}

void RemoteAccessible::CopyText(int32_t aStartPos, int32_t aEndPos) {
  Unused << mDoc->SendCopyText(mID, aStartPos, aEndPos);
}

void RemoteAccessible::CutText(int32_t aStartPos, int32_t aEndPos) {
  Unused << mDoc->SendCutText(mID, aStartPos, aEndPos);
}

void RemoteAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos) {
  Unused << mDoc->SendDeleteText(mID, aStartPos, aEndPos);
}

void RemoteAccessible::PasteText(int32_t aPosition) {
  Unused << mDoc->SendPasteText(mID, aPosition);
}

size_t RemoteAccessible::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

size_t RemoteAccessible::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) {
  size_t size = 0;

  // Count attributes.
  if (mCachedFields) {
    size += mCachedFields->SizeOfIncludingThis(aMallocSizeOf);
  }

  // We don't recurse into mChildren because they're already counted in their
  // document's mAccessibles.
  size += mChildren.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return size;
}

}  // namespace a11y
}  // namespace mozilla
