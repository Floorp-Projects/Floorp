/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_FLATTEN_H_
#define DOM_QUOTA_FLATTEN_H_

#include <iterator>
#include <type_traits>
#include <utility>

// XXX This should be moved to MFBT.

namespace mozilla::dom::quota {

namespace detail {

using std::begin;
using std::end;

template <typename T, typename NestedRange>
auto Flatten(NestedRange&& aRange) -> std::enable_if_t<
    std::is_same_v<T, std::decay_t<typename decltype(begin(
                          std::declval<const NestedRange&>()))::value_type>>,
    std::conditional_t<std::is_rvalue_reference_v<NestedRange>,
                       std::decay_t<NestedRange>, NestedRange>> {
  return std::forward<NestedRange>(aRange);
}

template <typename T, typename NestedRange>
struct FlatIter {
  using OuterIterator =
      decltype(begin(std::declval<const std::decay_t<NestedRange>&>()));
  using InnerIterator =
      decltype(begin(*begin(std::declval<const std::decay_t<NestedRange>&>())));

  explicit FlatIter(const NestedRange& aRange, OuterIterator aIter)
      : mOuterIter{std::move(aIter)}, mOuterEnd{end(aRange)} {
    InitInner();
  }

  const T& operator*() const { return *mInnerIter; }

  FlatIter& operator++() {
    ++mInnerIter;
    if (mInnerIter == mInnerEnd) {
      ++mOuterIter;
      InitInner();
    }
    return *this;
  }

  bool operator!=(const FlatIter& aOther) const {
    return mOuterIter != aOther.mOuterIter ||
           (mOuterIter != mOuterEnd && mInnerIter != aOther.mInnerIter);
  }

 private:
  void InitInner() {
    while (mOuterIter != mOuterEnd) {
      const typename OuterIterator::value_type& innerRange = *mOuterIter;

      mInnerIter = begin(innerRange);
      mInnerEnd = end(innerRange);

      if (mInnerIter != mInnerEnd) {
        break;
      }

      ++mOuterIter;
    }
  }

  OuterIterator mOuterIter;
  const OuterIterator mOuterEnd;

  InnerIterator mInnerIter;
  InnerIterator mInnerEnd;
};

template <typename T, typename NestedRange>
struct FlatRange {
  explicit FlatRange(NestedRange aRange) : mRange{std::move(aRange)} {}

  auto begin() const {
    using std::begin;
    return FlatIter<T, NestedRange>{mRange, begin(mRange)};
  }
  auto end() const {
    using std::end;
    return FlatIter<T, NestedRange>{mRange, end(mRange)};
  }

 private:
  NestedRange mRange;
};

template <typename T, typename NestedRange>
auto Flatten(NestedRange&& aRange) -> std::enable_if_t<
    !std::is_same_v<
        T, std::decay_t<typename decltype(begin(
               std::declval<const std::decay_t<NestedRange>&>()))::value_type>>,
    FlatRange<T, NestedRange>> {
  return FlatRange<T, NestedRange>{std::forward<NestedRange>(aRange)};
}

}  // namespace detail

template <typename T, typename NestedRange>
auto Flatten(NestedRange&& aRange) -> decltype(auto) {
  return detail::Flatten<T>(std::forward<NestedRange>(aRange));
}

}  // namespace mozilla::dom::quota

#endif
