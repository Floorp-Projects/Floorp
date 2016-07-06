/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ShapedObject_h
#define vm_ShapedObject_h

#include "jsobj.h"

namespace js {

/*
 * Shaped objects extend the base implementation of an object with a shape
 * field.  Subclasses of ShapedObject ascribe meaning to this field.
 *
 * ShapedObject is only created as the base class of some other class.  It's
 * never created as a most-derived class.
 */
class ShapedObject : public JSObject
{
  protected:
    // Property layout description and other state.
    GCPtrShape shape_;

  public:
    // Set the shape of an object. This pointer is valid for native objects and
    // some non-native objects. After creating an object, the objects for which
    // the shape pointer is invalid need to overwrite this pointer before a GC
    // can occur.
    void initShape(Shape* shape) {
        this->shape_.init(shape);
    }

    void setShape(Shape* shape) {
        this->shape_ = shape;
    }

    Shape* shape() const { return this->shape_; }

    static size_t offsetOfShape() { return offsetof(ShapedObject, shape_); }

  private:
    static void staticAsserts() {
        static_assert(offsetof(ShapedObject, shape_) == offsetof(shadow::Object, shape),
                      "shadow shape must match actual shape");
    }
};

} // namespace js

#endif /* vm_ShapedObject_h */
