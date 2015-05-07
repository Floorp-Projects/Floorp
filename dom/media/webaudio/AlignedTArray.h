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
  */
template <typename E, int N, typename Alloc>
class AlignedTArray_Impl : public nsTArray_Impl<E, Alloc>
{
  static_assert((N & (N-1)) == 0, "N must be power of 2");
  typedef nsTArray_Impl<E, Alloc>                    base_type;
public:
  typedef E                                          elem_type;
  typedef typename base_type::size_type              size_type;
  typedef typename base_type::index_type             index_type;

  AlignedTArray_Impl() {}
  explicit AlignedTArray_Impl(size_type capacity) : base_type(capacity+sExtra) {}
  elem_type* Elements() { return getAligned(base_type::Elements()); }
  const elem_type* Elements() const { return getAligned(base_type::Elements()); }
  elem_type& operator[](index_type i) { return Elements()[i];}
  const elem_type& operator[](index_type i) const { return Elements()[i]; }

  typename Alloc::ResultType SetLength(size_type newLen) {
    return base_type::SetLength(newLen + sExtra);
  }
  size_type Length() const {
    return base_type::Length() <= sExtra ? 0 : base_type::Length() - sExtra;
  }

private:
  AlignedTArray_Impl(const AlignedTArray_Impl& other) = delete;
  void operator=(const AlignedTArray_Impl& other) = delete;

  static const size_type sPadding = N <= MOZ_ALIGNOF(E) ? 0 : N - MOZ_ALIGNOF(E);
  static const size_type sExtra = (sPadding + sizeof(E) - 1) / sizeof(E);

  template <typename U>
  static U* getAligned(U* p)
  {
    return reinterpret_cast<U*>(((uintptr_t)p + N - 1) & ~(N-1));
  }
};

template <typename E, int N=32>
class AlignedTArray : public AlignedTArray_Impl<E, N, nsTArrayInfallibleAllocator>
{
public:
  typedef AlignedTArray_Impl<E, N, nsTArrayInfallibleAllocator> base_type;
  typedef AlignedTArray<E, N>                                   self_type;
  typedef typename base_type::size_type                         size_type;

  AlignedTArray() {}
  explicit AlignedTArray(size_type capacity) : base_type(capacity) {}
private:
  AlignedTArray(const AlignedTArray& other) = delete;
  void operator=(const AlignedTArray& other) = delete;
};

template <typename E, int N=32>
class AlignedFallibleTArray : public AlignedTArray_Impl<E, N, nsTArrayFallibleAllocator>
{
public:
  typedef AlignedTArray_Impl<E, N, nsTArrayFallibleAllocator> base_type;
  typedef AlignedFallibleTArray<E, N>                         self_type;
  typedef typename base_type::size_type                       size_type;

  AlignedFallibleTArray() {}
  explicit AlignedFallibleTArray(size_type capacity) : base_type(capacity) {}
private:
  AlignedFallibleTArray(const AlignedFallibleTArray& other) = delete;
  void operator=(const AlignedFallibleTArray& other) = delete;
};

#endif // AlignedTArray_h__
