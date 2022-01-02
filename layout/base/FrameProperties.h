/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FRAMEPROPERTIES_H_
#define FRAMEPROPERTIES_H_

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

class nsIFrame;

namespace mozilla {

struct FramePropertyDescriptorUntyped {
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
      : mDestructor(aDtor), mDestructorWithFrame(aDtorWithFrame) {}
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
 * aProperty in the FrameProperties methods.
 */
template <typename T>
struct FramePropertyDescriptor : public FramePropertyDescriptorUntyped {
  typedef void Destructor(T* aPropertyValue);
  typedef void DestructorWithFrame(const nsIFrame* aFrame, T* aPropertyValue);

  template <Destructor Dtor>
  static constexpr const FramePropertyDescriptor<T> NewWithDestructor() {
    return {Destruct<Dtor>, nullptr};
  }

  template <DestructorWithFrame Dtor>
  static constexpr const FramePropertyDescriptor<T>
  NewWithDestructorWithFrame() {
    return {nullptr, DestructWithFrame<Dtor>};
  }

  static constexpr const FramePropertyDescriptor<T> NewWithoutDestructor() {
    return {nullptr, nullptr};
  }

 private:
  constexpr FramePropertyDescriptor(UntypedDestructor* aDtor,
                                    UntypedDestructorWithFrame* aDtorWithFrame)
      : FramePropertyDescriptorUntyped(aDtor, aDtorWithFrame) {}

  template <Destructor Dtor>
  static void Destruct(void* aPropertyValue) {
    Dtor(static_cast<T*>(aPropertyValue));
  }

  template <DestructorWithFrame Dtor>
  static void DestructWithFrame(const nsIFrame* aFrame, void* aPropertyValue) {
    Dtor(aFrame, static_cast<T*>(aPropertyValue));
  }
};

// SmallValueHolder<T> is a placeholder intended to be used as template
// argument of FramePropertyDescriptor for types which can fit directly into our
// internal value slot (i.e. types that can fit in 64 bits). This class should
// never be defined, so that we won't use it for unexpected purpose by mistake.
template <typename T>
class SmallValueHolder;

namespace detail {

template <typename T>
struct FramePropertyTypeHelper {
  typedef T* Type;
};
template <typename T>
struct FramePropertyTypeHelper<SmallValueHolder<T>> {
  typedef T Type;
};

}  // namespace detail

/**
 * The FrameProperties class is optimized for storing 0 or 1 properties on
 * a given frame. Storing very large numbers of properties on a single
 * frame will not be efficient.
 */
class FrameProperties {
 public:
  template <typename T>
  using Descriptor = const FramePropertyDescriptor<T>*;
  using UntypedDescriptor = const FramePropertyDescriptorUntyped*;

  template <typename T>
  using PropertyType = typename detail::FramePropertyTypeHelper<T>::Type;

  explicit FrameProperties() = default;

  ~FrameProperties() {
    MOZ_ASSERT(mProperties.Length() == 0, "forgot to delete properties");
  }

  /**
   * Return true if we have no properties, otherwise return false.
   */
  bool IsEmpty() const { return mProperties.IsEmpty(); }

  /**
   * Set a property value. This requires a linear search through
   * the properties of the frame. Any existing value for the property
   * is destroyed.
   */
  template <typename T>
  void Set(Descriptor<T> aProperty, PropertyType<T> aValue,
           const nsIFrame* aFrame) {
    uint64_t v = ReinterpretHelper<T>::ToInternalValue(aValue);
    SetInternal(aProperty, v, aFrame);
  }

  /**
   * Add a property value; the descriptor MUST NOT already be present.
   */
  template <typename T>
  void Add(Descriptor<T> aProperty, PropertyType<T> aValue) {
    MOZ_ASSERT(!Has(aProperty), "duplicate frame property");
    uint64_t v = ReinterpretHelper<T>::ToInternalValue(aValue);
    AddInternal(aProperty, v);
  }

  /**
   * @return true if @aProperty is set. This requires a linear search through
   * the properties of the frame.
   *
   * In most cases, this shouldn't be used outside of assertions, because if
   * you're doing a lookup anyway it would be far more efficient to call Get()
   * or Take() and check the aFoundResult outparam to find out whether the
   * property is set. Legitimate non-assertion uses include:
   *
   *   - Checking if a frame property is set in cases where that's all we want
   *     to know (i.e., we don't intend to read the actual value or remove the
   *     property).
   *
   *   - Calling Has() before Set() in cases where we don't want to overwrite
   *     an existing value for the frame property.
   */
  template <typename T>
  bool Has(Descriptor<T> aProperty) const {
    return mProperties.Contains(aProperty, PropertyComparator());
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
  template <typename T>
  PropertyType<T> Get(Descriptor<T> aProperty,
                      bool* aFoundResult = nullptr) const {
    uint64_t v = GetInternal(aProperty, aFoundResult);
    return ReinterpretHelper<T>::FromInternalValue(v);
  }

  /**
   * Remove a property value, and return it without destroying it.
   *
   * This requires a linear search through the properties of the frame.
   * If the frame has no such property, returns zero-filled result, which means
   * null for pointers and zero for integers and floating point types.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame had a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  template <typename T>
  PropertyType<T> Take(Descriptor<T> aProperty, bool* aFoundResult = nullptr) {
    uint64_t v = TakeInternal(aProperty, aFoundResult);
    return ReinterpretHelper<T>::FromInternalValue(v);
  }

  /**
   * Remove and destroy a property value. This requires a linear search through
   * the properties of the frame. If the frame has no such property, nothing
   * happens.
   */
  template <typename T>
  void Remove(Descriptor<T> aProperty, const nsIFrame* aFrame) {
    RemoveInternal(aProperty, aFrame);
  }

  /**
   * Call @aFunction for each property or until @aFunction returns false.
   */
  template <class F>
  void ForEach(F aFunction) const {
#ifdef DEBUG
    size_t len = mProperties.Length();
#endif
    for (const auto& prop : mProperties) {
      bool shouldContinue = aFunction(prop.mProperty, prop.mValue);
      MOZ_ASSERT(len == mProperties.Length(),
                 "frame property list was modified by ForEach callback!");
      if (!shouldContinue) {
        return;
      }
    }
  }

  /**
   * Remove and destroy all property values for the frame.
   */
  void RemoveAll(const nsIFrame* aFrame) {
    nsTArray<PropertyValue> toDelete = std::move(mProperties);
    for (auto& prop : toDelete) {
      prop.DestroyValueFor(aFrame);
    }
    MOZ_ASSERT(mProperties.IsEmpty(), "a property dtor added new properties");
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    // We currently report only the shallow size of the mProperties array.
    // As for the PropertyValue entries: we don't need to measure the mProperty
    // field of because it always points to static memory, and we can't measure
    // mValue because the type is opaque.
    // XXX Can we do better, e.g. with a method on the descriptor?
    return mProperties.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  // Prevent copying of FrameProperties; we should always return/pass around
  // references to it, not copies!
  FrameProperties(const FrameProperties&) = delete;
  FrameProperties& operator=(const FrameProperties&) = delete;

  inline void SetInternal(UntypedDescriptor aProperty, uint64_t aValue,
                          const nsIFrame* aFrame);

  inline void AddInternal(UntypedDescriptor aProperty, uint64_t aValue);

  inline uint64_t GetInternal(UntypedDescriptor aProperty,
                              bool* aFoundResult) const;

  inline uint64_t TakeInternal(UntypedDescriptor aProperty, bool* aFoundResult);

  inline void RemoveInternal(UntypedDescriptor aProperty,
                             const nsIFrame* aFrame);

  template <typename T>
  struct ReinterpretHelper {
    static_assert(sizeof(PropertyType<T>) <= sizeof(uint64_t),
                  "size of the value must never be larger than 64 bits");

    static uint64_t ToInternalValue(PropertyType<T> aValue) {
      uint64_t v = 0;
      memcpy(&v, &aValue, sizeof(aValue));
      return v;
    }

    static PropertyType<T> FromInternalValue(uint64_t aInternalValue) {
      PropertyType<T> value;
      memcpy(&value, &aInternalValue, sizeof(value));
      return value;
    }
  };

  /**
   * Stores a property descriptor/value pair.
   */
  struct PropertyValue {
    PropertyValue() : mProperty(nullptr), mValue(0) {}
    PropertyValue(UntypedDescriptor aProperty, uint64_t aValue)
        : mProperty(aProperty), mValue(aValue) {}

    // NOTE: This function converts our internal 64-bit-integer representation
    // to a pointer-type representation. This is lossy on 32-bit systems, but it
    // should be fine, as long as we *only* do this in cases where we're sure
    // that the stored property-value is in fact a pointer. And we should have
    // that assurance, since only pointer-typed frame properties are expected to
    // have a destructor
    void DestroyValueFor(const nsIFrame* aFrame) {
      if (mProperty->mDestructor) {
        mProperty->mDestructor(
            ReinterpretHelper<void*>::FromInternalValue(mValue));
      } else if (mProperty->mDestructorWithFrame) {
        mProperty->mDestructorWithFrame(
            aFrame, ReinterpretHelper<void*>::FromInternalValue(mValue));
      }
    }

    UntypedDescriptor mProperty;
    uint64_t mValue;
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

  nsTArray<PropertyValue> mProperties;
};

inline uint64_t FrameProperties::GetInternal(UntypedDescriptor aProperty,
                                             bool* aFoundResult) const {
  MOZ_ASSERT(aProperty, "Null property?");

  return mProperties.ApplyIf(
      aProperty, 0, PropertyComparator(),
      [&aFoundResult](const PropertyValue& aPV) -> uint64_t {
        if (aFoundResult) {
          *aFoundResult = true;
        }
        return aPV.mValue;
      },
      [&aFoundResult]() -> uint64_t {
        if (aFoundResult) {
          *aFoundResult = false;
        }
        return 0;
      });
}

inline void FrameProperties::SetInternal(UntypedDescriptor aProperty,
                                         uint64_t aValue,
                                         const nsIFrame* aFrame) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  mProperties.ApplyIf(
      aProperty, 0, PropertyComparator(),
      [&](PropertyValue& aPV) {
        aPV.DestroyValueFor(aFrame);
        aPV.mValue = aValue;
      },
      [&]() { mProperties.AppendElement(PropertyValue(aProperty, aValue)); });
}

inline void FrameProperties::AddInternal(UntypedDescriptor aProperty,
                                         uint64_t aValue) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  mProperties.AppendElement(PropertyValue(aProperty, aValue));
}

inline uint64_t FrameProperties::TakeInternal(UntypedDescriptor aProperty,
                                              bool* aFoundResult) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  auto index = mProperties.IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex) {
    if (aFoundResult) {
      *aFoundResult = false;
    }
    return 0;
  }

  if (aFoundResult) {
    *aFoundResult = true;
  }

  uint64_t result = mProperties.Elements()[index].mValue;
  mProperties.RemoveElementAt(index);

  return result;
}

inline void FrameProperties::RemoveInternal(UntypedDescriptor aProperty,
                                            const nsIFrame* aFrame) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  auto index = mProperties.IndexOf(aProperty, 0, PropertyComparator());
  if (index != nsTArray<PropertyValue>::NoIndex) {
    mProperties.Elements()[index].DestroyValueFor(aFrame);
    mProperties.RemoveElementAt(index);
  }
}

}  // namespace mozilla

#endif /* FRAMEPROPERTIES_H_ */
