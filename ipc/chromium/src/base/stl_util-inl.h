/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// STL utility functions.  Usually, these replace built-in, but slow(!),
// STL functions with more efficient versions.

#ifndef BASE_STL_UTIL_INL_H_
#define BASE_STL_UTIL_INL_H_

#include <string.h>  // for memcpy
#include <functional>
#include <set>
#include <string>
#include <vector>
#include <cassert>

// Clear internal memory of an STL object.
// STL clear()/reserve(0) does not always free internal memory allocated
// This function uses swap/destructor to ensure the internal memory is freed.
template<class T> void STLClearObject(T* obj) {
  T tmp;
  tmp.swap(*obj);
  obj->reserve(0);  // this is because sometimes "T tmp" allocates objects with
                    // memory (arena implementation?).  use reserve()
                    // to clear() even if it doesn't always work
}

// Reduce memory usage on behalf of object if it is using more than
// "bytes" bytes of space.  By default, we clear objects over 1MB.
template <class T> inline void STLClearIfBig(T* obj, size_t limit = 1<<20) {
  if (obj->capacity() >= limit) {
    STLClearObject(obj);
  } else {
    obj->clear();
  }
}

// Reserve space for STL object.
// STL's reserve() will always copy.
// This function avoid the copy if we already have capacity
template<class T> void STLReserveIfNeeded(T* obj, int new_size) {
  if (obj->capacity() < new_size)   // increase capacity
    obj->reserve(new_size);
  else if (obj->size() > new_size)  // reduce size
    obj->resize(new_size);
}

// STLDeleteContainerPointers()
//  For a range within a container of pointers, calls delete
//  (non-array version) on these pointers.
// NOTE: for these three functions, we could just implement a DeleteObject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of algorithm.h, which seems expensive.
// For hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin,
                                ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// STLDeleteContainerPairPointers()
//  For a range within a container of pairs, calls delete
//  (non-array version) on BOTH items in the pairs.
// NOTE: Like STLDeleteContainerPointers, it is important that this deletes
// behind the iterator because if both the key and value are deleted, the
// container may call the hash function on the iterator when it is advanced,
// which could result in the hash function trying to dereference a stale
// pointer.
template <class ForwardIterator>
void STLDeleteContainerPairPointers(ForwardIterator begin,
                                    ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
    delete temp->second;
  }
}

// STLDeleteContainerPairFirstPointers()
//  For a range within a container of pairs, calls delete (non-array version)
//  on the FIRST item in the pairs.
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
template <class ForwardIterator>
void STLDeleteContainerPairFirstPointers(ForwardIterator begin,
                                         ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
  }
}

// STLDeleteContainerPairSecondPointers()
//  For a range within a container of pairs, calls delete
//  (non-array version) on the SECOND item in the pairs.
template <class ForwardIterator>
void STLDeleteContainerPairSecondPointers(ForwardIterator begin,
                                          ForwardIterator end) {
  while (begin != end) {
    delete begin->second;
    ++begin;
  }
}

template<typename T>
inline void STLAssignToVector(std::vector<T>* vec,
                              const T* ptr,
                              size_t n) {
  vec->resize(n);
  memcpy(&vec->front(), ptr, n*sizeof(T));
}

/***** Hack to allow faster assignment to a vector *****/

// This routine speeds up an assignment of 32 bytes to a vector from
// about 250 cycles per assignment to about 140 cycles.
//
// Usage:
//      STLAssignToVectorChar(&vec, ptr, size);
//      STLAssignToString(&str, ptr, size);

inline void STLAssignToVectorChar(std::vector<char>* vec,
                                  const char* ptr,
                                  size_t n) {
  STLAssignToVector(vec, ptr, n);
}

inline void STLAssignToString(std::string* str, const char* ptr, size_t n) {
  str->resize(n);
  memcpy(&*str->begin(), ptr, n);
}

// To treat a possibly-empty vector as an array, use these functions.
// If you know the array will never be empty, you can use &*v.begin()
// directly, but that is allowed to dump core if v is empty.  This
// function is the most efficient code that will work, taking into
// account how our STL is actually implemented.  THIS IS NON-PORTABLE
// CODE, so call us instead of repeating the nonportable code
// everywhere.  If our STL implementation changes, we will need to
// change this as well.

template<typename T>
inline T* vector_as_array(std::vector<T>* v) {
# ifdef NDEBUG
  return &*v->begin();
# else
  return v->empty() ? NULL : &*v->begin();
# endif
}

template<typename T>
inline const T* vector_as_array(const std::vector<T>* v) {
# ifdef NDEBUG
  return &*v->begin();
# else
  return v->empty() ? NULL : &*v->begin();
# endif
}

// Return a mutable char* pointing to a string's internal buffer,
// which may not be null-terminated. Writing through this pointer will
// modify the string.
//
// string_as_array(&str)[i] is valid for 0 <= i < str.size() until the
// next call to a string method that invalidates iterators.
//
// As of 2006-04, there is no standard-blessed way of getting a
// mutable reference to a string's internal buffer. However, issue 530
// (http://www.open-std.org/JTC1/SC22/WG21/docs/lwg-active.html#530)
// proposes this as the method. According to Matt Austern, this should
// already work on all current implementations.
inline char* string_as_array(std::string* str) {
  // DO NOT USE const_cast<char*>(str->data())! See the unittest for why.
  return str->empty() ? NULL : &*str->begin();
}

// These are methods that test two hash maps/sets for equality.  These exist
// because the == operator in the STL can return false when the maps/sets
// contain identical elements.  This is because it compares the internal hash
// tables which may be different if the order of insertions and deletions
// differed.

template <class HashSet>
inline bool
HashSetEquality(const HashSet& set_a,
                const HashSet& set_b) {
  if (set_a.size() != set_b.size()) return false;
  for (typename HashSet::const_iterator i = set_a.begin();
       i != set_a.end();
       ++i) {
    if (set_b.find(*i) == set_b.end())
      return false;
  }
  return true;
}

template <class HashMap>
inline bool
HashMapEquality(const HashMap& map_a,
                const HashMap& map_b) {
  if (map_a.size() != map_b.size()) return false;
  for (typename HashMap::const_iterator i = map_a.begin();
       i != map_a.end(); ++i) {
    typename HashMap::const_iterator j = map_b.find(i->first);
    if (j == map_b.end()) return false;
    if (i->second != j->second) return false;
  }
  return true;
}

// The following functions are useful for cleaning up STL containers
// whose elements point to allocated memory.

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.  This function is suitable for use with a vector, set,
// hash_set, or any other STL container which defines sensible begin(), end(),
// and clear() methods.
//
// If container is NULL, this function is a no-op.
//
// As an alternative to calling STLDeleteElements() directly, consider
// STLElementDeleter (defined below), which ensures that your container's
// elements are deleted when the STLElementDeleter goes out of scope.
template <class T>
void STLDeleteElements(T *container) {
  if (!container) return;
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

// Given an STL container consisting of (key, value) pairs, STLDeleteValues
// deletes all the "value" components and clears the container.  Does nothing
// in the case it's given a NULL pointer.

template <class T>
void STLDeleteValues(T *v) {
  if (!v) return;
  for (typename T::iterator i = v->begin(); i != v->end(); ++i) {
    delete i->second;
  }
  v->clear();
}


// The following classes provide a convenient way to delete all elements or
// values from STL containers when they goes out of scope.  This greatly
// simplifies code that creates temporary objects and has multiple return
// statements.  Example:
//
// vector<MyProto *> tmp_proto;
// STLElementDeleter<vector<MyProto *> > d(&tmp_proto);
// if (...) return false;
// ...
// return success;

// Given a pointer to an STL container this class will delete all the element
// pointers when it goes out of scope.

template<class STLContainer> class STLElementDeleter {
 public:
  explicit STLElementDeleter(STLContainer *ptr) : container_ptr_(ptr) {}
  ~STLElementDeleter() { STLDeleteElements(container_ptr_); }
 private:
  STLContainer *container_ptr_;
};

// Given a pointer to an STL container this class will delete all the value
// pointers when it goes out of scope.

template<class STLContainer> class STLValueDeleter {
 public:
  explicit STLValueDeleter(STLContainer *ptr) : container_ptr_(ptr) {}
  ~STLValueDeleter() { STLDeleteValues(container_ptr_); }
 private:
  STLContainer *container_ptr_;
};


// Forward declare some callback classes in callback.h for STLBinaryFunction
template <class R, class T1, class T2>
class ResultCallback2;

// STLBinaryFunction is a wrapper for the ResultCallback2 class in callback.h
// It provides an operator () method instead of a Run method, so it may be
// passed to STL functions in <algorithm>.
//
// The client should create callback with NewPermanentCallback, and should
// delete callback after it is done using the STLBinaryFunction.

template <class Result, class Arg1, class Arg2>
class STLBinaryFunction : public std::binary_function<Arg1, Arg2, Result> {
 public:
  typedef ResultCallback2<Result, Arg1, Arg2> Callback;

  explicit STLBinaryFunction(Callback* callback)
    : callback_(callback) {
    assert(callback_);
  }

  Result operator() (Arg1 arg1, Arg2 arg2) {
    return callback_->Run(arg1, arg2);
  }

 private:
  Callback* callback_;
};

// STLBinaryPredicate is a specialized version of STLBinaryFunction, where the
// return type is bool and both arguments have type Arg.  It can be used
// wherever STL requires a StrictWeakOrdering, such as in sort() or
// lower_bound().
//
// templated typedefs are not supported, so instead we use inheritance.

template <class Arg>
class STLBinaryPredicate : public STLBinaryFunction<bool, Arg, Arg> {
 public:
  typedef typename STLBinaryPredicate<Arg>::Callback Callback;
  explicit STLBinaryPredicate(Callback* callback)
    : STLBinaryFunction<bool, Arg, Arg>(callback) {
  }
};

// Functors that compose arbitrary unary and binary functions with a
// function that "projects" one of the members of a pair.
// Specifically, if p1 and p2, respectively, are the functions that
// map a pair to its first and second, respectively, members, the
// table below summarizes the functions that can be constructed:
//
// * UnaryOperate1st<pair>(f) returns the function x -> f(p1(x))
// * UnaryOperate2nd<pair>(f) returns the function x -> f(p2(x))
// * BinaryOperate1st<pair>(f) returns the function (x,y) -> f(p1(x),p1(y))
// * BinaryOperate2nd<pair>(f) returns the function (x,y) -> f(p2(x),p2(y))
//
// A typical usage for these functions would be when iterating over
// the contents of an STL map. For other sample usage, see the unittest.

template<typename Pair, typename UnaryOp>
class UnaryOperateOnFirst
    : public std::unary_function<Pair, typename UnaryOp::result_type> {
 public:
  UnaryOperateOnFirst() {
  }

  explicit UnaryOperateOnFirst(const UnaryOp& f) : f_(f) {
  }

  typename UnaryOp::result_type operator()(const Pair& p) const {
    return f_(p.first);
  }

 private:
  UnaryOp f_;
};

template<typename Pair, typename UnaryOp>
UnaryOperateOnFirst<Pair, UnaryOp> UnaryOperate1st(const UnaryOp& f) {
  return UnaryOperateOnFirst<Pair, UnaryOp>(f);
}

template<typename Pair, typename UnaryOp>
class UnaryOperateOnSecond
    : public std::unary_function<Pair, typename UnaryOp::result_type> {
 public:
  UnaryOperateOnSecond() {
  }

  explicit UnaryOperateOnSecond(const UnaryOp& f) : f_(f) {
  }

  typename UnaryOp::result_type operator()(const Pair& p) const {
    return f_(p.second);
  }

 private:
  UnaryOp f_;
};

template<typename Pair, typename UnaryOp>
UnaryOperateOnSecond<Pair, UnaryOp> UnaryOperate2nd(const UnaryOp& f) {
  return UnaryOperateOnSecond<Pair, UnaryOp>(f);
}

template<typename Pair, typename BinaryOp>
class BinaryOperateOnFirst
    : public std::binary_function<Pair, Pair, typename BinaryOp::result_type> {
 public:
  BinaryOperateOnFirst() {
  }

  explicit BinaryOperateOnFirst(const BinaryOp& f) : f_(f) {
  }

  typename BinaryOp::result_type operator()(const Pair& p1,
                                            const Pair& p2) const {
    return f_(p1.first, p2.first);
  }

 private:
  BinaryOp f_;
};

template<typename Pair, typename BinaryOp>
BinaryOperateOnFirst<Pair, BinaryOp> BinaryOperate1st(const BinaryOp& f) {
  return BinaryOperateOnFirst<Pair, BinaryOp>(f);
}

template<typename Pair, typename BinaryOp>
class BinaryOperateOnSecond
    : public std::binary_function<Pair, Pair, typename BinaryOp::result_type> {
 public:
  BinaryOperateOnSecond() {
  }

  explicit BinaryOperateOnSecond(const BinaryOp& f) : f_(f) {
  }

  typename BinaryOp::result_type operator()(const Pair& p1,
                                            const Pair& p2) const {
    return f_(p1.second, p2.second);
  }

 private:
  BinaryOp f_;
};

template<typename Pair, typename BinaryOp>
BinaryOperateOnSecond<Pair, BinaryOp> BinaryOperate2nd(const BinaryOp& f) {
  return BinaryOperateOnSecond<Pair, BinaryOp>(f);
}

// Translates a set into a vector.
template<typename T>
std::vector<T> SetToVector(const std::set<T>& values) {
  std::vector<T> result;
  result.reserve(values.size());
  result.insert(result.begin(), values.begin(), values.end());
  return result;
}

#endif  // BASE_STL_UTIL_INL_H_
