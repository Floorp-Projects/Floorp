// Copyright (C) 2011 The Android Open Source Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#ifndef __GABIXX_CXXABI_H__
#define __GABIXX_CXXABI_H__

#include <typeinfo>

namespace __cxxabiv1
{
  extern "C" void __cxa_pure_virtual();

  // Derived types of type_info below are based on 2.9.5 of C++ ABI.

  // Typeinfo for fundamental types.
  class __fundamental_type_info : public std::type_info
  {
  public:
    ~__fundamental_type_info();
  };

  // Typeinfo for array types.
  class __array_type_info : public std::type_info
  {
  public:
    ~__array_type_info();
  };

  // Typeinfo for function types.
  class __function_type_info : public std::type_info
  {
  public:
    ~__function_type_info();
  };

  // Typeinfo for enum types.
  class __enum_type_info : public std::type_info
  {
  public:
    ~__enum_type_info();
  };

  // Typeinfo for classes with no bases.
  class __class_type_info : public std::type_info
  {
  public:
    ~__class_type_info();

    enum class_type_info_code
      {
        CLASS_TYPE_INFO_CODE,
        SI_CLASS_TYPE_INFO_CODE,
        VMI_CLASS_TYPE_INFO_CODE
      };

    virtual class_type_info_code
    code() const { return CLASS_TYPE_INFO_CODE; }
  };

  // Typeinfo for classes containing only a single, public, non-virtual base at
  // offset zero.
  class __si_class_type_info : public __class_type_info
  {
  public:
    ~__si_class_type_info();
    const __class_type_info *__base_type;

    virtual __class_type_info::class_type_info_code
    code() const { return SI_CLASS_TYPE_INFO_CODE; }
  };

  struct __base_class_type_info
  {
  public:
    const __class_type_info *__base_type;

    // All but the lower __offset_shift bits of __offset_flags are a signed
    // offset. For a non-virtual base, this is the offset in the object of the
    // base subobject. For a virtual base, this is the offset in the virtual
    // table of the virtual base offset for the virtual base referenced
    // (negative).
    long __offset_flags;

    enum __offset_flags_masks
      {
        __virtual_mask = 0x1,
        __public_mask = 0x2,
        __offset_shift = 8
      };

    bool inline
    is_virtual() const { return (__offset_flags & __virtual_mask) != 0; }

    bool inline
    is_public() const { return (__offset_flags & __public_mask) != 0; }

    // FIXME: Right-shift of signed integer is implementation dependent.
    long inline
    offset() const { return __offset_flags >> __offset_shift; }

    long inline
    flags() const { return __offset_flags & ((1L << __offset_shift) - 1); }
  };

  // Typeinfo for classes with bases that do not satisfy the
  // __si_class_type_info constraints.
  class __vmi_class_type_info : public __class_type_info
  {
  public:
    ~__vmi_class_type_info();
    unsigned int __flags;
    unsigned int __base_count;
    __base_class_type_info __base_info[1];

    enum __flags_masks
      {
        __non_diamond_repeat_mask = 0x1,
        __diamond_shaped_mask = 0x2
      };

    virtual __class_type_info::class_type_info_code
    code() const { return VMI_CLASS_TYPE_INFO_CODE; }
  };

  class __pbase_type_info : public std::type_info
  {
  public:
    ~__pbase_type_info();
    unsigned int __flags;
    const std::type_info *__pointee;

    enum __masks
      {
        __const_mask = 0x1,
        __volatile_mask = 0x2,
        __restrict_mask = 0x4,
        __incomplete_mask = 0x8,
        __incomplete_class_mask = 0x10
      };
  };

  class __pointer_type_info : public __pbase_type_info
  {
  public:
    ~__pointer_type_info();
  };

  class __pointer_to_member_type_info : public __pbase_type_info
  {
  public:
    ~__pointer_to_member_type_info();
  };
}

namespace abi = __cxxabiv1;

#endif /* defined(__GABIXX_CXXABI_H__) */

