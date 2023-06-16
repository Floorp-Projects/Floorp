/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ManualNAC_h
#define mozilla_ManualNAC_h

#include "mozilla/dom/Element.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

// 16 seems to be the maximum number of manual Native Anonymous Content (NAC)
// nodes that editor creates for a given element.
//
// These need to be manually removed by the machinery that sets the NAC,
// otherwise we'll leak.
using ManualNACArray = AutoTArray<RefPtr<dom::Element>, 16>;

/**
 * Smart pointer class to own "manual" Native Anonymous Content, and perform
 * the necessary registration and deregistration on the parent element.
 */
class ManualNACPtr final {
 public:
  ManualNACPtr() = default;
  MOZ_IMPLICIT ManualNACPtr(decltype(nullptr)) {}
  explicit ManualNACPtr(already_AddRefed<dom::Element> aNewNAC)
      : mPtr(aNewNAC) {
    if (!mPtr) {
      return;
    }

    // Record the NAC on the element, so that AllChildrenIterator can find it.
    nsIContent* parentContent = mPtr->GetParent();
    auto nac = static_cast<ManualNACArray*>(
        parentContent->GetProperty(nsGkAtoms::manualNACProperty));
    if (!nac) {
      nac = new ManualNACArray();
      parentContent->SetProperty(nsGkAtoms::manualNACProperty, nac,
                                 nsINode::DeleteProperty<ManualNACArray>);
    }
    nac->AppendElement(mPtr);
  }

  // We use move semantics, and delete the copy-constructor and operator=.
  ManualNACPtr(ManualNACPtr&& aOther) : mPtr(std::move(aOther.mPtr)) {}
  ManualNACPtr(ManualNACPtr& aOther) = delete;
  ManualNACPtr& operator=(ManualNACPtr&& aOther) {
    mPtr = std::move(aOther.mPtr);
    return *this;
  }
  ManualNACPtr& operator=(ManualNACPtr& aOther) = delete;

  ~ManualNACPtr() { Reset(); }

  void Reset() {
    if (!mPtr) {
      return;
    }

    RefPtr<dom::Element> ptr = std::move(mPtr);
    RemoveContentFromNACArray(ptr);
  }

  static bool IsManualNAC(nsIContent* aAnonContent) {
    MOZ_ASSERT(aAnonContent->IsRootOfNativeAnonymousSubtree());
    MOZ_ASSERT(aAnonContent->IsInComposedDoc());

    auto* nac = static_cast<ManualNACArray*>(
        aAnonContent->GetParent()->GetProperty(nsGkAtoms::manualNACProperty));
    return nac && nac->Contains(aAnonContent);
  }

  static void RemoveContentFromNACArray(nsIContent* aAnonymousContent) {
    nsIContent* parentContent = aAnonymousContent->GetParent();
    if (!parentContent) {
      NS_WARNING("Potentially leaking manual NAC");
      return;
    }

    // Remove reference from the parent element.
    auto* nac = static_cast<ManualNACArray*>(
        parentContent->GetProperty(nsGkAtoms::manualNACProperty));
    // Document::AdoptNode might remove all properties before destroying editor.
    // So we have to consider that NAC could be already removed.
    if (nac) {
      nac->RemoveElement(aAnonymousContent);
      if (nac->IsEmpty()) {
        parentContent->RemoveProperty(nsGkAtoms::manualNACProperty);
      }
    }

    aAnonymousContent->UnbindFromTree();
  }

  dom::Element* get() const { return mPtr.get(); }
  dom::Element* operator->() const { return get(); }
  operator dom::Element*() const& { return get(); }

 private:
  RefPtr<dom::Element> mPtr;
};

}  // namespace mozilla

inline void ImplCycleCollectionUnlink(mozilla::ManualNACPtr& field) {
  field.Reset();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& callback,
    const mozilla::ManualNACPtr& field, const char* name, uint32_t flags = 0) {
  CycleCollectionNoteChild(callback, field.get(), name, flags);
}

#endif  // #ifndef mozilla_ManualNAC_h
