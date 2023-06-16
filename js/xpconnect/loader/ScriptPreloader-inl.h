/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptPreloader_inl_h
#define ScriptPreloader_inl_h

#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Range.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsString.h"
#include "nsTArray.h"

#include <prio.h>

namespace mozilla {

namespace loader {

using mozilla::dom::AutoJSAPI;

static inline Result<Ok, nsresult> Write(PRFileDesc* fd, const void* data,
                                         int32_t len) {
  if (PR_Write(fd, data, len) != len) {
    return Err(NS_ERROR_FAILURE);
  }
  return Ok();
}

static inline Result<Ok, nsresult> WritePadding(PRFileDesc* fd,
                                                uint8_t padding) {
  static const char paddingBytes[8] = "PADBYTE";
  MOZ_DIAGNOSTIC_ASSERT(padding <= sizeof(paddingBytes));

  if (padding == 0) {
    return Ok();
  }

  if (PR_Write(fd, static_cast<const void*>(paddingBytes), padding) !=
      padding) {
    return Err(NS_ERROR_FAILURE);
  }
  return Ok();
}

struct MOZ_RAII AutoSafeJSAPI : public AutoJSAPI {
  AutoSafeJSAPI() { Init(); }
};

template <typename T>
struct Matcher;

// Wraps the iterator for a nsTHashTable so that it may be used as a range
// iterator. Each iterator result acts as a smart pointer to the hash element,
// and has a Remove() method which will remove the element from the hash.
//
// It also accepts an optional Matcher instance against which to filter the
// elements which should be iterated over.
//
// Example:
//
//    for (auto& elem : HashElemIter<HashType>(hash)) {
//        if (elem->IsDead()) {
//            elem.Remove();
//        }
//    }
template <typename T>
class HashElemIter {
  using Iterator = typename T::Iterator;
  using ElemType = typename T::UserDataType;

  T& hash_;
  Matcher<ElemType>* matcher_;
  Iterator iter_;

 public:
  explicit HashElemIter(T& hash, Matcher<ElemType>* matcher = nullptr)
      : hash_(hash), matcher_(matcher), iter_(hash.Iter()) {}

  class Elem {
    friend class HashElemIter<T>;

    HashElemIter<T>& iter_;
    bool done_;

    Elem(HashElemIter& iter, bool done) : iter_(iter), done_(done) {
      skipNonMatching();
    }

    Iterator& iter() { return iter_.iter_; }

    void skipNonMatching() {
      if (iter_.matcher_) {
        while (!done_ && !iter_.matcher_->Matches(get())) {
          iter().Next();
          done_ = iter().Done();
        }
      }
    }

   public:
    Elem& operator*() { return *this; }

    ElemType get() {
      if (done_) {
        return nullptr;
      }
      return iter().UserData();
    }

    const ElemType get() const { return const_cast<Elem*>(this)->get(); }

    ElemType operator->() { return get(); }

    const ElemType operator->() const { return get(); }

    operator ElemType() { return get(); }

    void Remove() { iter().Remove(); }

    Elem& operator++() {
      MOZ_ASSERT(!done_);

      iter().Next();
      done_ = iter().Done();

      skipNonMatching();
      return *this;
    }

    bool operator!=(Elem& other) const {
      return done_ != other.done_ || this->get() != other.get();
    }
  };

  Elem begin() { return Elem(*this, iter_.Done()); }

  Elem end() { return Elem(*this, true); }
};

template <typename T>
HashElemIter<T> IterHash(T& hash,
                         Matcher<typename T::UserDataType>* matcher = nullptr) {
  return HashElemIter<T>(hash, matcher);
}

template <typename T, typename F>
bool Find(T&& iter, F&& match) {
  for (auto& elem : iter) {
    if (match(elem)) {
      return true;
    }
  }
  return false;
}

};  // namespace loader
};  // namespace mozilla

#endif  // ScriptPreloader_inl_h
