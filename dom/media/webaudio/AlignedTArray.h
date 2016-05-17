/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AlignedTArray_h__
#define AlignedTArray_h__

#include "mozilla/Alignment.h"
#include "nsTArray.h"

/**
 * E: element type, must be a POD type.
 * N: N bytes alignment for the first element, defaults to 32
 * S: S bytes of inline storage
  */
template <typename E, int S, int N = 32>
class AlignedAutoTArray : private AutoTArray<E, S + N>
{
  static_assert((N & (N-1)) == 0, "N must be power of 2");
  typedef AutoTArray<E, S + N> base_type;
public:
  typedef E                                          elem_type;
  typedef typename base_type::size_type              size_type;
  typedef typename base_type::index_type             index_type;

  AlignedAutoTArray() {}
  explicit AlignedAutoTArray(size_type capacity) : base_type(capacity + sExtra) {}
  elem_type* Elements() { return getAligned(base_type::Elements()); }
  const elem_type* Elements() const { return getAligned(base_type::Elements()); }
  elem_type& operator[](index_type i) { return Elements()[i];}
  const elem_type& operator[](index_type i) const { return Elements()[i]; }

  void SetLength(size_type newLen)
  {
    base_type::SetLength(newLen + sExtra);
  }

  MOZ_MUST_USE
  bool SetLength(size_type newLen, const mozilla::fallible_t&)
  {
    return base_type::SetLength(newLen + sExtra, mozilla::fallible);
  }

  size_type Length() const {
    return base_type::Length() <= sExtra ? 0 : base_type::Length() - sExtra;
  }

  using base_type::ShallowSizeOfExcludingThis;
  using base_type::ShallowSizeOfIncludingThis;

private:
  AlignedAutoTArray(const AlignedAutoTArray& other) = delete;
  void operator=(const AlignedAutoTArray& other) = delete;

  static const size_type sPadding = N <= MOZ_ALIGNOF(E) ? 0 : N - MOZ_ALIGNOF(E);
  static const size_type sExtra = (sPadding + sizeof(E) - 1) / sizeof(E);

  template <typename U>
  static U* getAligned(U* p)
  {
    return reinterpret_cast<U*>(((uintptr_t)p + N - 1) & ~(N-1));
  }
};

/**
 * E: element type, must be a POD type.
 * N: N bytes alignment for the first element, defaults to 32
  */
template <typename E, int N = 32>
class AlignedTArray : private nsTArray_Impl<E, nsTArrayInfallibleAllocator>
{
  static_assert((N & (N-1)) == 0, "N must be power of 2");
  typedef nsTArray_Impl<E, nsTArrayInfallibleAllocator> base_type;
public:
  typedef E                                          elem_type;
  typedef typename base_type::size_type              size_type;
  typedef typename base_type::index_type             index_type;

  AlignedTArray() {}
  explicit AlignedTArray(size_type capacity) : base_type(capacity + sExtra) {}
  elem_type* Elements() { return getAligned(base_type::Elements()); }
  const elem_type* Elements() const { return getAligned(base_type::Elements()); }
  elem_type& operator[](index_type i) { return Elements()[i];}
  const elem_type& operator[](index_type i) const { return Elements()[i]; }

  void SetLength(size_type newLen)
  {
    base_type::SetLength(newLen + sExtra);
  }

  MOZ_MUST_USE
  bool SetLength(size_type newLen, const mozilla::fallible_t&)
  {
    return base_type::SetLength(newLen + sExtra, mozilla::fallible);
  }

  size_type Length() const {
    return base_type::Length() <= sExtra ? 0 : base_type::Length() - sExtra;
  }

  using base_type::ShallowSizeOfExcludingThis;
  using base_type::ShallowSizeOfIncludingThis;

private:
  AlignedTArray(const AlignedTArray& other) = delete;
  void operator=(const AlignedTArray& other) = delete;

  static const size_type sPadding = N <= MOZ_ALIGNOF(E) ? 0 : N - MOZ_ALIGNOF(E);
  static const size_type sExtra = (sPadding + sizeof(E) - 1) / sizeof(E);

  template <typename U>
  static U* getAligned(U* p)
  {
    return reinterpret_cast<U*>(((uintptr_t)p + N - 1) & ~(N-1));
  }
};


#endif // AlignedTArray_h__
