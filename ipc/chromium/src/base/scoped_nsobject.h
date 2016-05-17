/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_NSOBJECT_H_
#define BASE_SCOPED_NSOBJECT_H_

#import <Foundation/Foundation.h>
#include "base/basictypes.h"

// scoped_nsobject<> is patterned after scoped_ptr<>, but maintains ownership
// of an NSObject subclass object.  Style deviations here are solely for
// compatibility with scoped_ptr<>'s interface, with which everyone is already
// familiar.
//
// When scoped_nsobject<> takes ownership of an object (in the constructor or
// in reset()), it takes over the caller's existing ownership claim.  The
// caller must own the object it gives to scoped_nsobject<>, and relinquishes
// an ownership claim to that object.  scoped_nsobject<> does not call
// -retain.
template<typename NST>
class scoped_nsobject {
 public:
  typedef NST* element_type;

  explicit scoped_nsobject(NST* object = nil)
      : object_(object) {
  }

  ~scoped_nsobject() {
    [object_ release];
  }

  void reset(NST* object = nil) {
    [object_ release];
    object_ = object;
  }

  bool operator==(NST* that) const {
    return object_ == that;
  }

  bool operator!=(NST* that) const {
    return object_ != that;
  }

  operator NST*() const {
    return object_;
  }

  NST* get() const {
    return object_;
  }

  void swap(scoped_nsobject& that) {
    NST* temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // scoped_nsobject<>::release() is like scoped_ptr<>::release.  It is NOT
  // a wrapper for [object_ release].  To force a scoped_nsobject<> object to
  // call [object_ release], use scoped_nsobject<>::reset().
  NST* release() {
    NST* temp = object_;
    object_ = nil;
    return temp;
  }

 private:
  NST* object_;

  DISALLOW_COPY_AND_ASSIGN(scoped_nsobject);
};

#endif  // BASE_SCOPED_NSOBJECT_H_
