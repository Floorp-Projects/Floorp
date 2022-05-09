/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of some generic iterators over ancestor nodes.
 *
 * Note that these keep raw pointers to the nodes they iterate from, and as
 * such the DOM should not be mutated while they're in use. There are debug
 * assertions (via nsMutationGuard) that check this in debug builds.
 */

#ifndef mozilla_dom_AncestorIterator_h
#define mozilla_dom_AncestorIterator_h

#include "nsINode.h"
#include "nsIContentInlines.h"
#include "FilteredNodeIterator.h"

namespace mozilla::dom {

#ifdef DEBUG
#  define MUTATION_GUARD(class_name_) \
    nsMutationGuard mMutationGuard;   \
    ~class_name_() { MOZ_ASSERT(!mMutationGuard.Mutated(0)); }
#else
#  define MUTATION_GUARD(class_name_)
#endif

#define DEFINE_ANCESTOR_ITERATOR(name_, method_)                         \
  class Inclusive##name_ {                                               \
    using Self = Inclusive##name_;                                       \
                                                                         \
   public:                                                               \
    explicit Inclusive##name_(const nsINode& aNode)                      \
        : mCurrent(const_cast<nsINode*>(&aNode)) {}                      \
    Self& begin() { return *this; }                                      \
    std::nullptr_t end() const { return nullptr; }                       \
    bool operator!=(std::nullptr_t) const { return !!mCurrent; }         \
    void operator++() { mCurrent = mCurrent->method_(); }                \
    nsINode* operator*() { return mCurrent; }                            \
                                                                         \
    MUTATION_GUARD(Inclusive##name_)                                     \
                                                                         \
   protected:                                                            \
    explicit Inclusive##name_(nsINode* aCurrent) : mCurrent(aCurrent) {} \
    nsINode* mCurrent;                                                   \
  };                                                                     \
  class name_ : public Inclusive##name_ {                                \
   public:                                                               \
    using Super = Inclusive##name_;                                      \
    explicit name_(const nsINode& aNode)                                 \
        : Inclusive##name_(aNode.method_()) {}                           \
  };                                                                     \
  template <typename T>                                                  \
  class name_##OfTypeIterator : public FilteredNodeIterator<T, name_> {  \
   public:                                                               \
    explicit name_##OfTypeIterator(const nsINode& aNode)                 \
        : FilteredNodeIterator<T, name_>(aNode) {}                       \
  };                                                                     \
  template <typename T>                                                  \
  class Inclusive##name_##OfTypeIterator                                 \
      : public FilteredNodeIterator<T, Inclusive##name_> {               \
   public:                                                               \
    explicit Inclusive##name_##OfTypeIterator(const nsINode& aNode)      \
        : FilteredNodeIterator<T, Inclusive##name_>(aNode) {}            \
  };

DEFINE_ANCESTOR_ITERATOR(Ancestors, GetParentNode)
DEFINE_ANCESTOR_ITERATOR(FlatTreeAncestors, GetFlattenedTreeParentNode)

#undef MUTATION_GUARD

}  // namespace mozilla::dom

template <typename T>
inline mozilla::dom::AncestorsOfTypeIterator<T> nsINode::AncestorsOfType()
    const {
  return mozilla::dom::AncestorsOfTypeIterator<T>(*this);
}

template <typename T>
inline mozilla::dom::InclusiveAncestorsOfTypeIterator<T>
nsINode::InclusiveAncestorsOfType() const {
  return mozilla::dom::InclusiveAncestorsOfTypeIterator<T>(*this);
}

template <typename T>
inline mozilla::dom::FlatTreeAncestorsOfTypeIterator<T>
nsINode::FlatTreeAncestorsOfType() const {
  return mozilla::dom::FlatTreeAncestorsOfTypeIterator<T>(*this);
}

template <typename T>
inline mozilla::dom::InclusiveFlatTreeAncestorsOfTypeIterator<T>
nsINode::InclusiveFlatTreeAncestorsOfType() const {
  return mozilla::dom::InclusiveFlatTreeAncestorsOfTypeIterator<T>(*this);
}

template <typename T>
inline T* nsINode::FirstAncestorOfType() const {
  return *(AncestorsOfType<T>());
}

#endif  // mozilla_dom_AncestorIterator.h
