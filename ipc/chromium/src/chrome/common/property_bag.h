// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROPERTY_BAG_H_
#define CHROME_COMMON_PROPERTY_BAG_H_

#include <map>

#include "base/basictypes.h"
#include "base/linked_ptr.h"

class PropertyAccessorBase;

// A property bag holds a generalized list of arbitrary metadata called
// properties. Each property is a class type derived from PropertyBag::Prop
// that can bet set and retrieved.
//
// The property bag is not read or written directly. Instead, callers go
// through a PropertyAccessor. The Accessor generates the unique IDs that
// identify different properties, so uniquely identify a property. The Accessor
// is templatized to also provide typesafety to the callers.
//
// Example:
//   // Note: you don't want to use Singleton for your Accessor if you're using
//   // a simple type like int or string as the data, since it will enforce that
//   // there is only one singleton for that type, which will conflict. If
//   // you're putting in some struct that's unique to you, go ahead.
//   PropertyAccessor<int>* my_accessor() const {
//     static PropertyAccessor<int>* accessor = NULL;
//     if (!accessor) accessor = new PropertyAccessor<int>;
//     return accessor;
//   }
//
//   void doit(SomeObjectThatImplementsPropertyBag* object) {
//     PropertyAccessor<int>* accessor = my_accessor();
//     int* property = accessor.GetProperty(object);
//     if (property)
//       ... use property ...
//
//     accessor.SetProperty(object, 22);
//   }
class PropertyBag {
 public:
  // The type that uniquely identifies a property type.
  typedef int PropID;
  enum { NULL_PROP_ID = -1 };  // Invalid property ID.

  // Properties are all derived from this class. They must be deletable and
  // copyable
  class Prop {
   public:
    virtual ~Prop() {}

    // Copies the property and returns a pointer to the new one. The caller is
    // responsible for managing the lifetime.
    virtual Prop* copy() = 0;
  };

  PropertyBag();
  PropertyBag(const PropertyBag& other);
  virtual ~PropertyBag();

  PropertyBag& operator=(const PropertyBag& other);

 private:
  friend class PropertyAccessorBase;

  typedef std::map<PropID, linked_ptr<Prop> > PropertyMap;

  // Used by the PropertyAccessor to set the property with the given ID.
  // Ownership of the given pointer will be transferred to us. Any existing
  // property matching the given ID will be deleted.
  void SetProperty(PropID id, Prop* prop);

  // Used by the PropertyAccessor to retrieve the property with the given ID.
  // The returned pointer will be NULL if there is no match. Ownership of the
  // pointer will stay with the property bag.
  Prop* GetProperty(PropID id);
  const Prop* GetProperty(PropID id) const;

  // Deletes the property with the given ID from the bag if it exists.
  void DeleteProperty(PropID id);

  PropertyMap props_;

  // Copy and assign is explicitly allowed for this class.
};

// PropertyAccessorBase -------------------------------------------------------

// Manages getting the unique IDs to identify a property. Callers should use
// PropertyAccessor below instead.
class PropertyAccessorBase {
 public:
  PropertyAccessorBase();
  virtual ~PropertyAccessorBase() {}

  // Removes our property, if any, from the given property bag.
  void DeleteProperty(PropertyBag* bag) {
    bag->DeleteProperty(prop_id_);
  }

 protected:
  void SetPropertyInternal(PropertyBag* bag, PropertyBag::Prop* prop) {
    bag->SetProperty(prop_id_, prop);
  }
  PropertyBag::Prop* GetPropertyInternal(PropertyBag* bag) {
    return bag->GetProperty(prop_id_);
  }
  const PropertyBag::Prop* GetPropertyInternal(const PropertyBag* bag) const {
    return bag->GetProperty(prop_id_);
  }

 private:
  // Identifier for this property.
  PropertyBag::PropID prop_id_;

  DISALLOW_COPY_AND_ASSIGN(PropertyAccessorBase);
};

// PropertyAccessor -----------------------------------------------------------

// Provides typesafe accessor functions for a property bag, and manages the
// unique identifiers for properties via the PropertyAccessorBase.
//
// Note that class T must be derived from PropertyBag::Prop.
template<class T>
class PropertyAccessor : public PropertyAccessorBase {
 public:
  PropertyAccessor() : PropertyAccessorBase() {}
  virtual ~PropertyAccessor() {}

  // Takes ownership of the |prop| pointer.
  void SetProperty(PropertyBag* bag, const T& prop) {
    SetPropertyInternal(bag, new Container(prop));
  }

  // Returns our property in the given bag or NULL if there is no match. The
  // returned pointer's ownership will stay with the property bag.
  T* GetProperty(PropertyBag* bag) {
    PropertyBag::Prop* prop = GetPropertyInternal(bag);
    if (!prop)
      return NULL;
    return static_cast<Container*>(prop)->get();
  }
  const T* GetProperty(const PropertyBag* bag) const {
    const PropertyBag::Prop* prop = GetPropertyInternal(bag);
    if (!prop)
      return NULL;
    return static_cast<const Container*>(prop)->get();
  }

  // See also DeleteProperty on thn PropertyAccessorBase.

 private:
  class Container : public PropertyBag::Prop {
   public:
    Container(const T& data) : data_(data) {}

    T* get() { return &data_; }
    const T* get() const { return &data_; }

   private:
    virtual Prop* copy() {
      return new Container(data_);
    }

    T data_;
  };

  DISALLOW_COPY_AND_ASSIGN(PropertyAccessor);
};

#endif  // CHROME_COMMON_PROPERTY_BAG_H_
