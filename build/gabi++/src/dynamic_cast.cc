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
// dynamic_cast.cc: RTTI support.
//
// References:
// Itanium C++ ABI at http://www.codesourcery.com/public/cxx-abi/abi.html
// IHI0041A C++ Application Binary Interface for the ARM architecture.
//

#include <cxxabi.h>

#include <cstddef>
#include <cassert>

namespace
{
  // Adjust a pointer by an offset.

  const void*
  adjust_pointer(const void* p, std::ptrdiff_t off)
  {
    // FIXME: should we align pointer after adjustment?
    const char *cp = reinterpret_cast<const char*>(p) + off;
    return reinterpret_cast<const void*>(cp);
  }

  // Return the vtable pointer of a polymorphic object pointed by p.

  inline const void*
  get_vtable(const void* p)
  {
    return *reinterpret_cast<void*const*>(p);
  }

  // Return a pointer to a __class_type_info in a vtable.

  inline const abi::__class_type_info*
  get_class_type_info(const void* vtable)
  {
    const void* type_info_ptr = adjust_pointer(vtable, -sizeof(void*));
    return *reinterpret_cast<abi::__class_type_info*const*>(type_info_ptr);
  }

  // Return offset to object in a vtable.

  inline std::ptrdiff_t
  get_offset_to_top(const void* vtable)
  {
    const void* type_info_ptr_address = adjust_pointer(vtable, -sizeof(void*));
    const void* offset_to_top_address =
      adjust_pointer(type_info_ptr_address, -sizeof(std::ptrdiff_t));
    return *reinterpret_cast<const std::ptrdiff_t*>(offset_to_top_address);
  }

  // Return the virtual pointer to the most derived object of referred by a
  // pointer p.

  const void*
  get_most_derived_object(const void* p)
  {
    const void* vtable = get_vtable(p);
    std::ptrdiff_t offset_to_top = get_offset_to_top(vtable);
    return adjust_pointer(p, offset_to_top);
  }

  // We assume that -1 cannot be a valid pointer to object.
  const void * const ambiguous_object =
    reinterpret_cast<const void*>(-1);

  // Return a pointer to the subobject described by base_info.

  const void*
  get_subobject(const void* object,
                const void* vtable,
                const abi::__base_class_type_info* base_info)
  {
    long offset = base_info->offset();
    if (base_info->is_virtual())
      {
        const std::ptrdiff_t* virtual_base_offset_address =
          static_cast<const std::ptrdiff_t*> (adjust_pointer(vtable, offset));
        offset = *virtual_base_offset_address;
      }
    return adjust_pointer(object, offset);
  }

  // Helper of __dyanmic_cast to walk the type tree of an object.

  const void *
  walk_object(const void *object,
              const abi::__class_type_info *type,
              const void *match_object,
              const abi::__class_type_info *match_type)
  {
    if (*type == *match_type)
      return (match_object == NULL || object == match_object) ? object : NULL;

    switch(type->code())
      {
      case abi::__class_type_info::CLASS_TYPE_INFO_CODE:
        // This isn't not the class you're looking for.
        return NULL;

      case abi::__class_type_info::SI_CLASS_TYPE_INFO_CODE:
        // derived type has a single public base at offset 0.
        {
          const abi::__si_class_type_info* ti =
            static_cast<const abi::__si_class_type_info*>(type);
          return walk_object(object, ti->__base_type, match_object,
                             match_type);
        }

      case abi::__class_type_info::VMI_CLASS_TYPE_INFO_CODE:
        {
          const void* vtable = get_vtable(object);
          const abi::__vmi_class_type_info* ti =
            static_cast<const abi::__vmi_class_type_info*>(type);

          // Look at all direct bases.
          const void* result = NULL;
          for (unsigned i = 0; i < ti->__base_count; ++i)
            {
              if (!ti->__base_info[i].is_public())
                continue;

              const void *subobject =
                get_subobject(object, vtable, &ti->__base_info[i]);
              const void* walk_subobject_result =
                walk_object(subobject, ti->__base_info[i].__base_type,
                            match_object, match_type);

              if (walk_subobject_result == ambiguous_object)
                return ambiguous_object;
              else if (walk_subobject_result != NULL)
                {
                  if (result == NULL)
                    {
                      result = walk_subobject_result;
                    }
                  else if (result != walk_subobject_result)
                    return ambiguous_object;
                }
            }
          return result;
        }

      default:
        assert(0);
      }
    return NULL;
  }

  // Bookkeeping structure for derived-to-base cast in the general case.
  struct cast_context
  {
  public:
    const void* object;
    const abi::__class_type_info *src_type;
    const abi::__class_type_info *dst_type;
    std::ptrdiff_t src2dst_offset;

    const void* dst_object;
    const void* result;

    cast_context(const void* obj, const abi::__class_type_info *src,
                 const abi::__class_type_info *dst, std::ptrdiff_t offset)
      : object(obj), src_type(src), dst_type(dst), src2dst_offset(offset),
        dst_object(NULL), result(NULL)
    { }
  };

  // based-to-derive cast in the general case.

  void
  base_to_derived_cast(const void *object,
                       const abi::__class_type_info *type,
                       cast_context* context)
  {
    const void* saved_dst_object = context->dst_object;
    bool is_dst_type = *type == *context->dst_type;
    if (is_dst_type)
      context->dst_object = object;

    if (object == context->object
        && context->dst_object != NULL
        && *type == *context->src_type)
      {
        if (context->result == NULL)
          context->result = context->dst_object;
        else if (context->result != context->dst_object)
          context->result = ambiguous_object;
        context->dst_object = saved_dst_object;
        return;
      }

    switch(type->code())
      {
      case abi::__class_type_info::CLASS_TYPE_INFO_CODE:
        // This isn't not the class you're looking for.
        break;

      case abi::__class_type_info::SI_CLASS_TYPE_INFO_CODE:
        // derived type has a single public base at offset 0.
        {
          const abi::__si_class_type_info* ti =
            static_cast<const abi::__si_class_type_info*>(type);
          base_to_derived_cast(object, ti->__base_type, context);
          break;
        }

      case abi::__class_type_info::VMI_CLASS_TYPE_INFO_CODE:
        {
          const void* vtable = get_vtable(object);
          const abi::__vmi_class_type_info* ti =
            static_cast<const abi::__vmi_class_type_info*>(type);

          // Look at all direct bases.
          for (unsigned i = 0; i < ti->__base_count; ++i)
            {
              if (!ti->__base_info[i].is_public())
                continue;

              const void *subobject =
                get_subobject(object, vtable, &ti->__base_info[i]);
              base_to_derived_cast(subobject, ti->__base_info[i].__base_type,
                                   context);

              // FIXME: Use flags in base_info to optimize search.
              if (context->result == ambiguous_object)
                break;
            }
          break;
        }

      default:
        assert(0);
      }
     context->dst_object = saved_dst_object;
  }
} // namespace

namespace __cxxabiv1
{
#define DYNAMIC_CAST_NO_HINT -1
#define DYNAMIC_CAST_NOT_PUBLIC_BASE -2
#define DYNAMIC_CAST_MULTIPLE_PUBLIC_NONVIRTUAL_BASE -3

  /* v: source address to be adjusted; nonnull, and since the
   *    source object is polymorphic, *(void**)v is a virtual pointer.
   * src: static type of the source object.
   * dst: destination type (the "T" in "dynamic_cast<T>(v)").
   * src2dst_offset: a static hint about the location of the
   *    source subobject with respect to the complete object;
   *    special negative values are:
   *       -1: no hint
   *       -2: src is not a public base of dst
   *       -3: src is a multiple public base type but never a
   *           virtual base type
   *    otherwise, the src type is a unique public nonvirtual
   *    base type of dst at offset src2dst_offset from the
   *    origin of dst.
   */
  extern "C" void*
  __dynamic_cast (const void *v,
                  const abi::__class_type_info *src,
                  const abi::__class_type_info *dst,
                  std::ptrdiff_t src2dst_offset)
  {
    const void* most_derived_object = get_most_derived_object(v);
    const void* vtable = get_vtable(most_derived_object);
    const abi::__class_type_info* most_derived_class_type_info =
      get_class_type_info(vtable);

    // If T is not a public base type of the most derived class referred
    // by v, the cast always fails.
    void* t_object =
      const_cast<void*>(walk_object(most_derived_object,
                                    most_derived_class_type_info, NULL, dst));
    if (t_object == NULL)
      return NULL;

    // C++ ABI 2.9.7 The dynamic_cast Algorithm:
    //
    // If, in the most derived object pointed (referred) to by v, v points
    // (refers) to a public base class subobject of a T object [note: this can
    // be checked at compile time], and if only one object of type T is derived
    // from the subobject pointed (referred) to by v, the result is a pointer
    // (an lvalue referring) to that T object.

    // We knew that src is not a public base, so base-to-derived cast
    // is not possible.  This works even if there are multiple subobjects
    // of type T in the most derived object.
    if (src2dst_offset != DYNAMIC_CAST_NOT_PUBLIC_BASE)
      {
        // If it is known that v points to a public base class subobject
        // of a T object, simply adjust the pointer by the offset.
        if (t_object != ambiguous_object && src2dst_offset >= 0)
          return const_cast<void*>(adjust_pointer(v, -src2dst_offset));

        // If there is only one T type subobject, we only need to look at
        // there.  Otherwise, look for the subobject referred by v in the
        // most derived object.
        cast_context context(v, src, dst, src2dst_offset);
        if (t_object != ambiguous_object)
          base_to_derived_cast(t_object, dst, &context);
        else
          base_to_derived_cast(most_derived_object,
                               most_derived_class_type_info, &context);

        if (context.result != NULL && context.result != ambiguous_object)
          return const_cast<void*>(context.result);
      }

    // C++ ABI 2.9.7 The dynamic_cast Algorithm:
    //
    // Otherwise, if v points (refers) to a public base class subobject of the
    // most derived object, and the type of the most derived object has an
    // unambiguous public base class of type T, the result is a pointer (an
    // lvalue referring) to the T subobject of the most derived object.
    // Otherwise, the run-time check fails.

    // Check to see if T is a unambiguous public base class.
    if (t_object == ambiguous_object)
      return NULL;

    // See if v refers to a public base class subobject.
    const void* v_object =
      walk_object(most_derived_object, most_derived_class_type_info, v, src);
    return v_object == v ? t_object : NULL;
  }
} // namespace __cxxabiv1
