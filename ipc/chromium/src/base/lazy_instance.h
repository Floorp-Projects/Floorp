// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The LazyInstance<Type, Traits> class manages a single instance of Type,
// which will be lazily created on the first time it's accessed.  This class is
// useful for places you would normally use a function-level static, but you
// need to have guaranteed thread-safety.  The Type constructor will only ever
// be called once, even if two threads are racing to create the object.  Get()
// and Pointer() will always return the same, completely initialized instance.
// When the instance is constructed it is registered with AtExitManager.  The
// destructor will be called on program exit.
//
// LazyInstance is completely thread safe, assuming that you create it safely.
// The class was designed to be POD initialized, so it shouldn't require a
// static constructor.  It really only makes sense to declare a LazyInstance as
// a global variable using the base::LinkerInitialized constructor.
//
// LazyInstance is similar to Singleton, except it does not have the singleton
// property.  You can have multiple LazyInstance's of the same type, and each
// will manage a unique instance.  It also preallocates the space for Type, as
// to avoid allocating the Type instance on the heap.  This may help with the
// performance of creating the instance, and reducing heap fragmentation.  This
// requires that Type be a complete type so we can determine the size.
//
// Example usage:
//   static LazyInstance<MyClass> my_instance(base::LINKER_INITIALIZED);
//   void SomeMethod() {
//     my_instance.Get().SomeMethod();  // MyClass::SomeMethod()
//
//     MyClass* ptr = my_instance.Pointer();
//     ptr->DoDoDo();  // MyClass::DoDoDo
//   }

#ifndef BASE_LAZY_INSTANCE_H_
#define BASE_LAZY_INSTANCE_H_

#include "base/atomicops.h"
#include "base/basictypes.h"

namespace base {

template <typename Type>
struct DefaultLazyInstanceTraits {
  static void New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    // The parenthesis is very important here to force POD type initialization.
    new (instance) Type();
  }
  static void Delete(void* instance) {
    // Explicitly call the destructor.
    reinterpret_cast<Type*>(instance)->~Type();
  }
};

// We pull out some of the functionality into a non-templated base, so that we
// can implement the more complicated pieces out of line in the .cc file.
class LazyInstanceHelper {
 protected:
  enum {
    STATE_EMPTY    = 0,
    STATE_CREATING = 1,
    STATE_CREATED  = 2
  };

  explicit LazyInstanceHelper(LinkerInitialized x) { /* state_ is 0 */ }
  // Declaring a destructor (even if it's empty) will cause MSVC to register a
  // static initializer to register the empty destructor with atexit().

  // Make sure that instance is created, creating or waiting for it to be
  // created if neccessary.  Constructs with |ctor| in the space provided by
  // |instance| and registers dtor for destruction at program exit.
  void EnsureInstance(void* instance, void (*ctor)(void*), void (*dtor)(void*));

  base::subtle::Atomic32 state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LazyInstanceHelper);
};

template <typename Type, typename Traits = DefaultLazyInstanceTraits<Type> >
class LazyInstance : public LazyInstanceHelper {
 public:
  explicit LazyInstance(LinkerInitialized x) : LazyInstanceHelper(x) { }
  // Declaring a destructor (even if it's empty) will cause MSVC to register a
  // static initializer to register the empty destructor with atexit().

  Type& Get() {
    return *Pointer();
  }

  Type* Pointer() {
    Type* instance = reinterpret_cast<Type*>(&buf_);

    // We will hopefully have fast access when the instance is already created.
    if (base::subtle::NoBarrier_Load(&state_) != STATE_CREATED)
      EnsureInstance(instance, Traits::New, Traits::Delete);

    return instance;
  }

 private:
  int8_t buf_[sizeof(Type)];  // Preallocate the space for the Type instance.

  DISALLOW_COPY_AND_ASSIGN(LazyInstance);
};

}  // namespace base

#endif  // BASE_LAZY_INSTANCE_H_
