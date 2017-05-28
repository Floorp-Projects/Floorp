/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FRAMEPROPERTIES_H_
#define FRAMEPROPERTIES_H_

#include "mozilla/MemoryReporting.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"

class nsIFrame;

namespace mozilla {

struct FramePropertyDescriptorUntyped
{
  /**
   * mDestructor will be called if it's non-null.
   */
  typedef void UntypedDestructor(void* aPropertyValue);
  UntypedDestructor* mDestructor;
  /**
   * mDestructorWithFrame will be called if it's non-null and mDestructor
   * is null. WARNING: The frame passed to mDestructorWithFrame may
   * be a dangling frame pointer, if this is being called during
   * presshell teardown. Do not use it except to compare against
   * other frame pointers. No frame will have been allocated with
   * the same address yet.
   */
  typedef void UntypedDestructorWithFrame(const nsIFrame* aFrame,
                                          void* aPropertyValue);
  UntypedDestructorWithFrame* mDestructorWithFrame;
  /**
   * mDestructor and mDestructorWithFrame may both be null, in which case
   * no value destruction is a no-op.
   */

protected:
  /**
   * At most one destructor should be passed in. In general, you should
   * just use the static function FramePropertyDescriptor::New* below
   * instead of using this constructor directly.
   */
  constexpr FramePropertyDescriptorUntyped(
    UntypedDestructor* aDtor, UntypedDestructorWithFrame* aDtorWithFrame)
    : mDestructor(aDtor)
    , mDestructorWithFrame(aDtorWithFrame)
  {}
};

/**
 * A pointer to a FramePropertyDescriptor serves as a unique property ID.
 * The FramePropertyDescriptor stores metadata about the property.
 * Currently the only metadata is a destructor function. The destructor
 * function is called on property values when they are overwritten or
 * deleted.
 *
 * To use this class, declare a global (i.e., file, class or function-scope
 * static member) FramePropertyDescriptor and pass its address as
 * aProperty in the FramePropertyTable methods.
 */
template<typename T>
struct FramePropertyDescriptor : public FramePropertyDescriptorUntyped
{
  typedef void Destructor(T* aPropertyValue);
  typedef void DestructorWithFrame(const nsIFrame* aaFrame,
                                   T* aPropertyValue);

  template<Destructor Dtor>
  static constexpr const FramePropertyDescriptor<T> NewWithDestructor()
  {
    return { Destruct<Dtor>, nullptr };
  }

  template<DestructorWithFrame Dtor>
  static constexpr
  const FramePropertyDescriptor<T> NewWithDestructorWithFrame()
  {
    return { nullptr, DestructWithFrame<Dtor> };
  }

  static constexpr const FramePropertyDescriptor<T> NewWithoutDestructor()
  {
    return { nullptr, nullptr };
  }

private:
  constexpr FramePropertyDescriptor(
    UntypedDestructor* aDtor, UntypedDestructorWithFrame* aDtorWithFrame)
    : FramePropertyDescriptorUntyped(aDtor, aDtorWithFrame)
  {}

  template<Destructor Dtor>
  static void Destruct(void* aPropertyValue)
  {
    Dtor(static_cast<T*>(aPropertyValue));
  }

  template<DestructorWithFrame Dtor>
  static void DestructWithFrame(const nsIFrame* aFrame, void* aPropertyValue)
  {
    Dtor(aFrame, static_cast<T*>(aPropertyValue));
  }
};

// SmallValueHolder<T> is a placeholder intended to be used as template
// argument of FramePropertyDescriptor for types which can fit into the
// size of a pointer directly. This class should never be defined, so
// that we won't use it for unexpected purpose by mistake.
template<typename T>
class SmallValueHolder;

namespace detail {

template<typename T>
struct FramePropertyTypeHelper
{
  typedef T* Type;
};
template<typename T>
struct FramePropertyTypeHelper<SmallValueHolder<T>>
{
  typedef T Type;
};

}

/**
 * The FrameProperties class is optimized for storing 0 or 1 properties on
 * a given frame. Storing very large numbers of properties on a single
 * frame will not be efficient.
 * 
 * Property values are passed as void* but do not actually have to be
 * valid pointers. You can use NS_INT32_TO_PTR/NS_PTR_TO_INT32 to
 * store int32_t values. Null/zero values can be stored and retrieved.
 * Of course, the destructor function (if any) must handle such values
 * correctly.
 */
class FrameProperties
{
public:
  template<typename T>
  using Descriptor = const FramePropertyDescriptor<T>*;
  using UntypedDescriptor = const FramePropertyDescriptorUntyped*;

  template<typename T>
  using PropertyType = typename detail::FramePropertyTypeHelper<T>::Type;

  explicit FrameProperties()
#ifdef DEBUG
    : mMaxLength(0)
#endif
  {
  }

  ~FrameProperties()
  {
    MOZ_ASSERT(mMaxLength > 0, "redundant FrameProperties!");
    MOZ_ASSERT(mProperties.Length() == 0, "forgot to delete properties");
  }

  /**
   * Set a property value. This requires a linear search through
   * the properties of the frame. Any existing value for the property
   * is destroyed.
   */
  template<typename T>
  void Set(Descriptor<T> aProperty, PropertyType<T> aValue,
           const nsIFrame* aFrame)
  {
    void* ptr = ReinterpretHelper<T>::ToPointer(aValue);
    SetInternal(aProperty, ptr, aFrame);
  }

  /**
   * Add a property value; the descriptor MUST NOT already be present.
   */
  template<typename T>
  void Add(Descriptor<T> aProperty, PropertyType<T> aValue)
  {
    MOZ_ASSERT(!Has(aProperty), "duplicate frame property");
    void* ptr = ReinterpretHelper<T>::ToPointer(aValue);
    AddInternal(aProperty, ptr);
  }

  /**
   * @return true if @aProperty is set. This requires a linear search through the
   * properties of the frame.
   *
   * In most cases, this shouldn't be used outside of assertions, because if
   * you're doing a lookup anyway it would be far more efficient to call Get()
   * or Remove() and check the aFoundResult outparam to find out whether the
   * property is set. Legitimate non-assertion uses include:
   *
   *   - Checking if a frame property is set in cases where that's all we want
   *     to know (i.e., we don't intend to read the actual value or remove the
   *     property).
   *
   *   - Calling Has() before Set() in cases where we don't want to overwrite
   *     an existing value for the frame property.
   *
   * The HasSkippingBitCheck variant doesn't test NS_FRAME_HAS_PROPERTIES
   * on aFrame, so it is safe to call after aFrame has been destroyed as
   * long as, since that destruction happened, it isn't possible for a
   * new frame to have been created and the same property added.
   */
  template<typename T>
  bool Has(Descriptor<T> aProperty) const
  {
    return mProperties.IndexOf(aProperty, 0, PropertyComparator())
           != nsTArray<PropertyValue>::NoIndex;
  }

  /**
   * Get a property value. This requires a linear search through
   * the properties of the frame. If the frame has no such property,
   * returns zero-filled result, which means null for pointers and
   * zero for integers and floating point types.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame has a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  template<typename T>
  PropertyType<T> Get(Descriptor<T> aProperty,
                      bool* aFoundResult = nullptr) const
  {
    void* ptr = GetInternal(aProperty, aFoundResult);
    return ReinterpretHelper<T>::FromPointer(ptr);
  }

  /**
   * Remove a property value. This requires a linear search through
   * the properties of the frame. The old property value is returned
   * (and not destroyed). If the frame has no such property,
   * returns zero-filled result, which means null for pointers and
   * zero for integers and floating point types.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame had a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  template<typename T>
  PropertyType<T> Remove(Descriptor<T> aProperty,
                         bool* aFoundResult = nullptr)
  {
    void* ptr = RemoveInternal(aProperty, aFoundResult);
    return ReinterpretHelper<T>::FromPointer(ptr);
  }

  /**
   * Remove and destroy a property value. This requires a linear search
   * through the properties of the frame. If the frame has no such
   * property, nothing happens.
   */
  template<typename T>
  void Delete(Descriptor<T> aProperty, const nsIFrame* aFrame)
  {
    DeleteInternal(aProperty, aFrame);
  }

  /**
   * Remove and destroy all property values for the frame.
   */
  void DeleteAll(const nsIFrame* aFrame);

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  friend class ::nsIFrame;

  // Prevent copying of FrameProperties; we should always return/pass around
  // references to it, not copies!
  FrameProperties(const FrameProperties&) = delete;
  FrameProperties& operator=(const FrameProperties&) = delete;

  void SetInternal(UntypedDescriptor aProperty, void* aValue,
                   const nsIFrame* aFrame);

  void AddInternal(UntypedDescriptor aProperty, void* aValue);

  inline void*
  GetInternal(UntypedDescriptor aProperty, bool* aFoundResult) const;

  void* RemoveInternal(UntypedDescriptor aProperty, bool* aFoundResult);

  void DeleteInternal(UntypedDescriptor aProperty, const nsIFrame* aFrame);

  template<typename T>
  struct ReinterpretHelper
  {
    static_assert(sizeof(PropertyType<T>) <= sizeof(void*),
                  "size of the value must never be larger than a pointer");

    static void* ToPointer(PropertyType<T> aValue)
    {
      void* ptr = nullptr;
      memcpy(&ptr, &aValue, sizeof(aValue));
      return ptr;
    }

    static PropertyType<T> FromPointer(void* aPtr)
    {
      PropertyType<T> value;
      memcpy(&value, &aPtr, sizeof(value));
      return value;
    }
  };

  template<typename T>
  struct ReinterpretHelper<T*>
  {
    static void* ToPointer(T* aValue)
    {
      return static_cast<void*>(aValue);
    }

    static T* FromPointer(void* aPtr)
    {
      return static_cast<T*>(aPtr);
    }
  };

  /**
   * Stores a property descriptor/value pair.
   */
  struct PropertyValue {
    PropertyValue() : mProperty(nullptr), mValue(nullptr) {}
    PropertyValue(UntypedDescriptor aProperty, void* aValue)
      : mProperty(aProperty), mValue(aValue) {}

    void DestroyValueFor(const nsIFrame* aFrame) {
      if (mProperty->mDestructor) {
        mProperty->mDestructor(mValue);
      } else if (mProperty->mDestructorWithFrame) {
        mProperty->mDestructorWithFrame(aFrame, mValue);
      }
    }

    UntypedDescriptor mProperty;
    void* mValue;
  };

  /**
   * Used with an array of PropertyValues to allow lookups that compare
   * only on the FramePropertyDescriptor.
   */
  class PropertyComparator {
  public:
    bool Equals(const PropertyValue& a, const PropertyValue& b) const {
      return a.mProperty == b.mProperty;
    }
    bool Equals(UntypedDescriptor a, const PropertyValue& b) const {
      return a == b.mProperty;
    }
    bool Equals(const PropertyValue& a, UntypedDescriptor b) const {
      return a.mProperty == b;
    }
  };

  AutoTArray<PropertyValue,1> mProperties;
#ifdef DEBUG
  uint32_t mMaxLength;
#endif
};

inline void*
FrameProperties::GetInternal(UntypedDescriptor aProperty,
                             bool* aFoundResult) const
{
  MOZ_ASSERT(aProperty, "Null property?");

  nsTArray<PropertyValue>::index_type index =
    mProperties.IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex) {
    if (aFoundResult) {
      *aFoundResult = false;
    }
    return nullptr;
  }

  if (aFoundResult) {
    *aFoundResult = true;
  }
  return mProperties.ElementAt(index).mValue;
}

} // namespace mozilla

#endif /* FRAMEPROPERTIES_H_ */
