/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_FixedArityList_h
#define ion_FixedArityList_h

namespace js {
namespace ion {

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
        MOZ_ASSUME_NOT_REACHED("no items");
    }
    const T &operator [](size_t index) const {
        MOZ_ASSUME_NOT_REACHED("no items");
    }
};

} // namespace ion
} // namespace js

#endif /* ion_FixedArityList_h */
