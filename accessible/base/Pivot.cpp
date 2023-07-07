/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Pivot.h"

#include "AccIterator.h"
#include "LocalAccessible.h"
#include "RemoteAccessible.h"
#include "nsAccUtils.h"
#include "nsIAccessiblePivot.h"

#include "mozilla/a11y/Accessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Pivot
////////////////////////////////////////////////////////////////////////////////

Pivot::Pivot(Accessible* aRoot) : mRoot(aRoot) { MOZ_COUNT_CTOR(Pivot); }

Pivot::~Pivot() { MOZ_COUNT_DTOR(Pivot); }

Accessible* Pivot::AdjustStartPosition(Accessible* aAnchor, PivotRule& aRule,
                                       uint16_t* aFilterResult) {
  Accessible* matched = aAnchor;
  *aFilterResult = aRule.Match(aAnchor);

  if (aAnchor && aAnchor != mRoot) {
    for (Accessible* temp = aAnchor->Parent(); temp && temp != mRoot;
         temp = temp->Parent()) {
      uint16_t filtered = aRule.Match(temp);
      if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
        *aFilterResult = filtered;
        matched = temp;
      }
    }
  }

  return matched;
}

Accessible* Pivot::SearchBackward(Accessible* aAnchor, PivotRule& aRule,
                                  bool aSearchCurrent) {
  // Initial position could be unset, in that case return null.
  if (!aAnchor) {
    return nullptr;
  }

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;

  Accessible* acc = AdjustStartPosition(aAnchor, aRule, &filtered);

  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return acc;
  }

  while (acc && acc != mRoot) {
    Accessible* parent = acc->Parent();
#if defined(ANDROID)
    MOZ_ASSERT(
        acc->IsLocal() || (acc->IsRemote() && parent->IsRemote()),
        "Pivot::SearchBackward climbed out of remote subtree in Android!");
#endif
    int32_t idxInParent = acc->IndexInParent();
    while (idxInParent > 0 && parent) {
      acc = parent->ChildAt(--idxInParent);
      if (!acc) {
        continue;
      }

      filtered = aRule.Match(acc);

      Accessible* lastChild = acc->LastChild();
      while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
             lastChild) {
        parent = acc;
        acc = lastChild;
        idxInParent = acc->IndexInParent();
        filtered = aRule.Match(acc);
        lastChild = acc->LastChild();
      }

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return acc;
      }
    }

    acc = parent;
    if (!acc) {
      break;
    }

    filtered = aRule.Match(acc);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return acc;
    }
  }

  return nullptr;
}

Accessible* Pivot::SearchForward(Accessible* aAnchor, PivotRule& aRule,
                                 bool aSearchCurrent) {
  // Initial position could be not set, in that case begin search from root.
  Accessible* acc = aAnchor ? aAnchor : mRoot;

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  acc = AdjustStartPosition(acc, aRule, &filtered);
  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return acc;
  }

  while (acc) {
    Accessible* firstChild = acc->FirstChild();
    while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
           firstChild) {
      acc = firstChild;
      filtered = aRule.Match(acc);

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return acc;
      }
      firstChild = acc->FirstChild();
    }

    Accessible* sibling = nullptr;
    Accessible* temp = acc;
    do {
      if (temp == mRoot) {
        break;
      }

      sibling = temp->NextSibling();

      if (sibling) {
        break;
      }
      temp = temp->Parent();
#if defined(ANDROID)
      MOZ_ASSERT(
          acc->IsLocal() || (acc->IsRemote() && temp->IsRemote()),
          "Pivot::SearchForward climbed out of remote subtree in Android!");
#endif

    } while (temp);

    if (!sibling) {
      break;
    }

    acc = sibling;
    filtered = aRule.Match(acc);
    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return acc;
    }
  }

  return nullptr;
}

Accessible* Pivot::Next(Accessible* aAnchor, PivotRule& aRule,
                        bool aIncludeStart) {
  return SearchForward(aAnchor, aRule, aIncludeStart);
}

Accessible* Pivot::Prev(Accessible* aAnchor, PivotRule& aRule,
                        bool aIncludeStart) {
  return SearchBackward(aAnchor, aRule, aIncludeStart);
}

Accessible* Pivot::First(PivotRule& aRule) {
  return SearchForward(mRoot, aRule, true);
}

Accessible* Pivot::Last(PivotRule& aRule) {
  Accessible* lastAcc = mRoot;

  // First go to the last accessible in pre-order
  while (lastAcc && lastAcc->HasChildren()) {
    lastAcc = lastAcc->LastChild();
  }

  // Search backwards from last accessible and find the last occurrence in the
  // doc
  return SearchBackward(lastAcc, aRule, true);
}

Accessible* Pivot::AtPoint(int32_t aX, int32_t aY, PivotRule& aRule) {
  Accessible* match = nullptr;
  Accessible* child =
      mRoot ? mRoot->ChildAtPoint(aX, aY,
                                  Accessible::EWhichChildAtPoint::DeepestChild)
            : nullptr;
  while (child && (mRoot != child)) {
    uint16_t filtered = aRule.Match(child);

    // Ignore any matching nodes that were below this one
    if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
      match = nullptr;
    }

    // Match if no node below this is a match
    if ((filtered & nsIAccessibleTraversalRule::FILTER_MATCH) && !match) {
      LayoutDeviceIntRect childRect = child->IsLocal()
                                          ? child->AsLocal()->Bounds()
                                          : child->AsRemote()->Bounds();
      // Double-check child's bounds since the deepest child may have been out
      // of bounds. This assures we don't return a false positive.
      if (childRect.Contains(aX, aY)) {
        match = child;
      }
    }

    child = child->Parent();
  }

  return match;
}

// Role Rule

PivotRoleRule::PivotRoleRule(mozilla::a11y::role aRole)
    : mRole(aRole), mDirectDescendantsFrom(nullptr) {}

PivotRoleRule::PivotRoleRule(mozilla::a11y::role aRole,
                             Accessible* aDirectDescendantsFrom)
    : mRole(aRole), mDirectDescendantsFrom(aDirectDescendantsFrom) {}

uint16_t PivotRoleRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAcc)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (mDirectDescendantsFrom && (aAcc != mDirectDescendantsFrom)) {
    // If we've specified mDirectDescendantsFrom, we should ignore
    // non-direct descendants of from the specified AoP. Because
    // pivot performs a preorder traversal, the first aAcc
    // object(s) that don't equal mDirectDescendantsFrom will be
    // mDirectDescendantsFrom's children. We'll process them, but ignore
    // their subtrees thereby processing direct descendants of
    // mDirectDescendantsFrom only.
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (aAcc && aAcc->Role() == mRole) {
    result |= nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return result;
}

// State Rule

PivotStateRule::PivotStateRule(uint64_t aState) : mState(aState) {}

uint16_t PivotStateRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAcc)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (aAcc && (aAcc->State() & mState)) {
    result = nsIAccessibleTraversalRule::FILTER_MATCH |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  return result;
}

// LocalAccInSameDocRule

uint16_t LocalAccInSameDocRule::Match(Accessible* aAcc) {
  LocalAccessible* acc = aAcc ? aAcc->AsLocal() : nullptr;
  if (!acc) {
    return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
  if (acc->IsOuterDoc()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
  return nsIAccessibleTraversalRule::FILTER_MATCH;
}

// Radio Button Name Rule

PivotRadioNameRule::PivotRadioNameRule(const nsString& aName) : mName(aName) {}

uint16_t PivotRadioNameRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;
  RemoteAccessible* remote = aAcc->AsRemote();
  if (!remote) {
    // We need the cache to be able to fetch the name attribute below.
    return result;
  }

  if (nsAccUtils::MustPrune(aAcc) || aAcc->IsOuterDoc()) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (remote->IsHTMLRadioButton()) {
    nsString currName = remote->GetCachedHTMLNameAttribute();
    if (!currName.IsEmpty() && mName.Equals(currName)) {
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// MustPruneSameDocRule

uint16_t MustPruneSameDocRule::Match(Accessible* aAcc) {
  if (!aAcc) {
    return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (nsAccUtils::MustPrune(aAcc) || aAcc->IsOuterDoc()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  return nsIAccessibleTraversalRule::FILTER_MATCH;
}
