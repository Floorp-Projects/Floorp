// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_REF_COUNTED_H_
#define BASE_REF_COUNTED_H_

#include "base/atomic_ref_count.h"
#include "base/thread_collision_warner.h"

namespace base {

namespace subtle {

class RefCountedBase {
 protected:
  RefCountedBase();
  ~RefCountedBase();

  void AddRef();

  // Returns true if the object should self-delete.
  bool Release();

 private:
  int ref_count_;
#ifndef NDEBUG
  bool in_dtor_;
#endif

  DFAKE_MUTEX(add_release_)

  DISALLOW_COPY_AND_ASSIGN(RefCountedBase);
};

class RefCountedThreadSafeBase {
 protected:
  RefCountedThreadSafeBase();
  ~RefCountedThreadSafeBase();

  void AddRef();

  // Returns true if the object should self-delete.
  bool Release();

 private:
  AtomicRefCount ref_count_;
#ifndef NDEBUG
  bool in_dtor_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RefCountedThreadSafeBase);
};



}  // namespace subtle

//
// A base class for reference counted classes.  Otherwise, known as a cheap
// knock-off of WebKit's RefCounted<T> class.  To use this guy just extend your
// class from it like so:
//
//   class MyFoo : public base::RefCounted<MyFoo> {
//    ...
//   };
//
template <class T>
class RefCounted : public subtle::RefCountedBase {
 public:
  RefCounted() { }
  ~RefCounted() { }

  void AddRef() {
    subtle::RefCountedBase::AddRef();
  }

  void Release() {
    if (subtle::RefCountedBase::Release()) {
      delete static_cast<T*>(this);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RefCounted<T>);
};

//
// A thread-safe variant of RefCounted<T>
//
//   class MyFoo : public base::RefCountedThreadSafe<MyFoo> {
//    ...
//   };
//
template <class T>
class RefCountedThreadSafe : public subtle::RefCountedThreadSafeBase {
 public:
  RefCountedThreadSafe() { }
  ~RefCountedThreadSafe() { }

  void AddRef() {
    subtle::RefCountedThreadSafeBase::AddRef();
  }

  void Release() {
    if (subtle::RefCountedThreadSafeBase::Release()) {
      delete static_cast<T*>(this);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RefCountedThreadSafe<T>);
};

//
// A wrapper for some piece of data so we can place other things in
// scoped_refptrs<>.
//
template<typename T>
class RefCountedData : public base::RefCounted< base::RefCountedData<T> > {
 public:
  RefCountedData() : data() {}
  explicit RefCountedData(const T& in_value) : data(in_value) {}

  T data;
};

}  // namespace base

//
// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     ...
//     foo = NULL;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how scoped_refptr<T> acts like a pointer to T.
// Given two scoped_refptr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references NULL.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
template <class T>
class scoped_refptr {
 public:
  scoped_refptr() : ptr_(NULL) {
  }

  MOZ_IMPLICIT scoped_refptr(T* p) : ptr_(p) {
    if (ptr_)
      ptr_->AddRef();
  }

  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_)
      ptr_->AddRef();
  }

  ~scoped_refptr() {
    if (ptr_)
      ptr_->Release();
  }

  T* get() const { return ptr_; }
  operator T*() const { return ptr_; }
  T* operator->() const { return ptr_; }

  scoped_refptr<T>& operator=(T* p) {
    // AddRef first so that self assignment should work
    if (p)
      p->AddRef();
    if (ptr_ )
      ptr_ ->Release();
    ptr_ = p;
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  void swap(T** pp) {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(scoped_refptr<T>& r) {
    swap(&r.ptr_);
  }

 protected:
  T* ptr_;
};

#endif  // BASE_REF_COUNTED_H_
