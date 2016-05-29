/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FRAMEPROPERTYTABLE_H_
#define FRAMEPROPERTYTABLE_H_

#include "mozilla/MemoryReporting.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/unused.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

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
  MOZ_CONSTEXPR FramePropertyDescriptorUntyped(
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
  static MOZ_CONSTEXPR const FramePropertyDescriptor<T> NewWithDestructor()
  {
    return { Destruct<Dtor>, nullptr };
  }

  template<DestructorWithFrame Dtor>
  static MOZ_CONSTEXPR
  const FramePropertyDescriptor<T> NewWithDestructorWithFrame()
  {
    return { nullptr, DestructWithFrame<Dtor> };
  }

  static MOZ_CONSTEXPR const FramePropertyDescriptor<T> NewWithoutDestructor()
  {
    return { nullptr, nullptr };
  }

private:
  MOZ_CONSTEXPR FramePropertyDescriptor(
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
 * The FramePropertyTable is optimized for storing 0 or 1 properties on
 * a given frame. Storing very large numbers of properties on a single
 * frame will not be efficient.
 * 
 * Property values are passed as void* but do not actually have to be
 * valid pointers. You can use NS_INT32_TO_PTR/NS_PTR_TO_INT32 to
 * store int32_t values. Null/zero values can be stored and retrieved.
 * Of course, the destructor function (if any) must handle such values
 * correctly.
 */
class FramePropertyTable {
public:
  template<typename T>
  using Descriptor = const FramePropertyDescriptor<T>*;
  using UntypedDescriptor = const FramePropertyDescriptorUntyped*;

  template<typename T>
  using PropertyType = typename detail::FramePropertyTypeHelper<T>::Type;

  FramePropertyTable() : mLastFrame(nullptr), mLastEntry(nullptr)
  {
  }
  ~FramePropertyTable()
  {
    DeleteAll();
  }

  /**
   * Set a property value on a frame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through
   * the properties of that frame. Any existing value for the property
   * is destroyed.
   */
  template<typename T>
  void Set(const nsIFrame* aFrame, Descriptor<T> aProperty,
           PropertyType<T> aValue)
  {
    void* ptr = ReinterpretHelper<T>::ToPointer(aValue);
    SetInternal(aFrame, aProperty, ptr);
  }

  /**
   * @return true if @aProperty is set for @aFrame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through the
   * properties of that frame.
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
   */
  template<typename T>
  bool Has(const nsIFrame* aFrame, Descriptor<T> aProperty)
  {
    bool foundResult = false;
    mozilla::Unused << GetInternal(aFrame, aProperty, &foundResult);
    return foundResult;
  }

  /**
   * Get a property value for a frame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through
   * the properties of that frame. If the frame has no such property,
   * returns zero-filled result, which means null for pointers and
   * zero for integers and floating point types.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame has a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  template<typename T>
  PropertyType<T> Get(const nsIFrame* aFrame, Descriptor<T> aProperty,
                      bool* aFoundResult = nullptr)
  {
    void* ptr = GetInternal(aFrame, aProperty, aFoundResult);
    return ReinterpretHelper<T>::FromPointer(ptr);
  }
  /**
   * Remove a property value for a frame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through
   * the properties of that frame. The old property value is returned
   * (and not destroyed). If the frame has no such property,
   * returns zero-filled result, which means null for pointers and
   * zero for integers and floating point types.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame had a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  template<typename T>
  PropertyType<T> Remove(const nsIFrame* aFrame, Descriptor<T> aProperty,
                         bool* aFoundResult = nullptr)
  {
    void* ptr = RemoveInternal(aFrame, aProperty, aFoundResult);
    return ReinterpretHelper<T>::FromPointer(ptr);
  }
  /**
   * Remove and destroy a property value for a frame. This requires one
   * hashtable lookup (using the frame as the key) and a linear search
   * through the properties of that frame. If the frame has no such
   * property, nothing happens.
   */
  template<typename T>
  void Delete(const nsIFrame* aFrame, Descriptor<T> aProperty)
  {
    DeleteInternal(aFrame, aProperty);
  }
  /**
   * Remove and destroy all property values for a frame. This requires one
   * hashtable lookup (using the frame as the key).
   */
  void DeleteAllFor(const nsIFrame* aFrame);
  /**
   * Remove and destroy all property values for all frames.
   */
  void DeleteAll();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  void SetInternal(const nsIFrame* aFrame, UntypedDescriptor aProperty,
                   void* aValue);

  void* GetInternal(const nsIFrame* aFrame, UntypedDescriptor aProperty,
                    bool* aFoundResult);

  void* RemoveInternal(const nsIFrame* aFrame, UntypedDescriptor aProperty,
                       bool* aFoundResult);

  void DeleteInternal(const nsIFrame* aFrame, UntypedDescriptor aProperty);

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
   * Stores a property descriptor/value pair. It can also be used to
   * store an nsTArray of PropertyValues.
   */
  struct PropertyValue {
    PropertyValue() : mProperty(nullptr), mValue(nullptr) {}
    PropertyValue(UntypedDescriptor aProperty, void* aValue)
      : mProperty(aProperty), mValue(aValue) {}

    bool IsArray() { return !mProperty && mValue; }
    nsTArray<PropertyValue>* ToArray()
    {
      NS_ASSERTION(IsArray(), "Must be array");
      return reinterpret_cast<nsTArray<PropertyValue>*>(&mValue);
    }

    void DestroyValueFor(const nsIFrame* aFrame) {
      if (mProperty->mDestructor) {
        mProperty->mDestructor(mValue);
      } else if (mProperty->mDestructorWithFrame) {
        mProperty->mDestructorWithFrame(aFrame, mValue);
      }
    }

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
      size_t n = 0;
      // We don't need to measure mProperty because it always points to static
      // memory.  As for mValue:  if it's a single value we can't measure it,
      // because the type is opaque;  if it's an array, we measure the array
      // storage, but we can't measure the individual values, again because
      // their types are opaque.
      if (IsArray()) {
        nsTArray<PropertyValue>* array = ToArray();
        n += array->ShallowSizeOfExcludingThis(aMallocSizeOf);
      }
      return n;
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

  /**
   * Our hashtable entry. The key is an nsIFrame*, the value is a
   * PropertyValue representing one or more property/value pairs.
   */
  class Entry : public nsPtrHashKey<const nsIFrame>
  {
  public:
    explicit Entry(KeyTypePointer aKey) : nsPtrHashKey<const nsIFrame>(aKey) {}
    Entry(const Entry &toCopy) :
      nsPtrHashKey<const nsIFrame>(toCopy), mProp(toCopy.mProp) {}

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
      return mProp.SizeOfExcludingThis(aMallocSizeOf);
    }

    PropertyValue mProp;
  };

  static void DeleteAllForEntry(Entry* aEntry);

  // Note that mLastEntry points into mEntries, so we need to be careful about
  // not triggering a resize of mEntries, e.g. use RawRemoveEntry() instead of
  // RemoveEntry() in some places.
  nsTHashtable<Entry> mEntries;
  const nsIFrame* mLastFrame;
  Entry* mLastEntry;
};

/**
 * This class encapsulates the properties of a frame.
 */
class FrameProperties {
public:
  template<typename T> using Descriptor = FramePropertyTable::Descriptor<T>;
  template<typename T> using PropertyType = FramePropertyTable::PropertyType<T>;

  FrameProperties(FramePropertyTable* aTable, const nsIFrame* aFrame)
    : mTable(aTable), mFrame(aFrame) {}

  template<typename T>
  void Set(Descriptor<T> aProperty, PropertyType<T> aValue) const
  {
    mTable->Set(mFrame, aProperty, aValue);
  }

  template<typename T>
  bool Has(Descriptor<T> aProperty) const
  {
    return mTable->Has(mFrame, aProperty);
  }

  template<typename T>
  PropertyType<T> Get(Descriptor<T> aProperty,
                      bool* aFoundResult = nullptr) const
  {
    return mTable->Get(mFrame, aProperty, aFoundResult);
  }
  template<typename T>
  PropertyType<T> Remove(Descriptor<T> aProperty,
                         bool* aFoundResult = nullptr) const
  {
    return mTable->Remove(mFrame, aProperty, aFoundResult);
  }
  template<typename T>
  void Delete(Descriptor<T> aProperty)
  {
    mTable->Delete(mFrame, aProperty);
  }

private:
  FramePropertyTable* mTable;
  const nsIFrame* mFrame;
};

} // namespace mozilla

#endif /* FRAMEPROPERTYTABLE_H_ */
