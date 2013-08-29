/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_FixedArityList_h
#define jit_FixedArityList_h

namespace js {
namespace jit {

template <typename T, size_t Arity>
class FixedArityList
{
    T list_[Arity];

  public:
    FixedArityList()
      : list_()
    { }

    T &operator [](size_t index) {
        JS_ASSERT(index < Arity);
        return list_[index];
    }
    const T &operator [](size_t index) const {
        JS_ASSERT(index < Arity);
        return list_[index];
    }
};

template <typename T>
class FixedArityList<T, 0>
{
  public:
    T &operator [](size_t index) {
        JS_NOT_REACHED("no items");
        static T *operand = NULL;
        return *operand;
    }
    const T &operator [](size_t index) const {
        JS_NOT_REACHED("no items");
        static T *operand = NULL;
        return *operand;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_FixedArityList_h */
