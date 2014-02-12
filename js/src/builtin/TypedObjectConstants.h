/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Specialized .h file to be used by both JS and C++ code.

#ifndef builtin_TypedObjectConstants_h
#define builtin_TypedObjectConstants_h

///////////////////////////////////////////////////////////////////////////
// Slots for type objects
//
// Some slots apply to all type objects and some are specific to
// particular kinds of type objects. For simplicity we use the same
// number of slots no matter what kind of type descriptor we are
// working with, even though this is mildly wasteful.

// Slots on all type objects
#define JS_DESCR_SLOT_TYPE_REPR          0  // Associated Type Representation
#define JS_DESCR_SLOT_ALIGNMENT          1  // Alignment in bytes
#define JS_DESCR_SLOT_SIZE               2  // Size in bytes, if sized, else 0

// Slots on scalars, references, and x4s
#define JS_DESCR_SLOT_TYPE               3  // Type code

// Slots on all array descriptors
#define JS_DESCR_SLOT_ARRAY_ELEM_TYPE    3

// Slots on sized array descriptors
#define JS_DESCR_SLOT_SIZED_ARRAY_LENGTH 4

// Slots on struct type objects
#define JS_DESCR_SLOT_STRUCT_FIELD_NAMES 3
#define JS_DESCR_SLOT_STRUCT_FIELD_TYPES 4
#define JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS 5

// Maximum number of slots for any descriptor
#define JS_DESCR_SLOTS                   6

///////////////////////////////////////////////////////////////////////////
// Slots for type representation objects
//
// Some slots apply to all type representations and some are specific
// to particular kinds of type representations. Because all type
// representations share the same class, however, they always have the
// same number of slots, though not all of them will be initialized or
// used in the same way.

// Slots on *all* type representations:
#define JS_TYPEREPR_SLOT_KIND      0 // One of the `kind` constants below
#define JS_TYPEREPR_SLOT_SIZE      1 // Size in bytes.
#define JS_TYPEREPR_SLOT_ALIGNMENT 2 // Alignment in bytes.

// Slots on sized arrays:
#define JS_TYPEREPR_SLOT_LENGTH    3 // Length of the array

// Slots on scalars, references, and X4s:
#define JS_TYPEREPR_SLOT_TYPE      3 // One of the constants below

// Maximum number of slots for any type representation
#define JS_TYPEREPR_SLOTS          4

// These constants are for use exclusively in JS code. In C++ code,
// prefer TypeRepresentation::Scalar etc, which allows you to
// write a switch which will receive a warning if you omit a case.
#define JS_TYPEREPR_UNSIZED_ARRAY_KIND  0
#define JS_TYPEREPR_MAX_UNSIZED_KIND    0    // Unsized kinds go above here
#define JS_TYPEREPR_SCALAR_KIND         1
#define JS_TYPEREPR_REFERENCE_KIND      2
#define JS_TYPEREPR_STRUCT_KIND         3
#define JS_TYPEREPR_SIZED_ARRAY_KIND    4
#define JS_TYPEREPR_X4_KIND             5

// These constants are for use exclusively in JS code. In C++ code,
// prefer ScalarTypeRepresentation::TYPE_INT8 etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_SCALARTYPEREPR_INT8          0
#define JS_SCALARTYPEREPR_UINT8         1
#define JS_SCALARTYPEREPR_INT16         2
#define JS_SCALARTYPEREPR_UINT16        3
#define JS_SCALARTYPEREPR_INT32         4
#define JS_SCALARTYPEREPR_UINT32        5
#define JS_SCALARTYPEREPR_FLOAT32       6
#define JS_SCALARTYPEREPR_FLOAT64       7
#define JS_SCALARTYPEREPR_UINT8_CLAMPED 8

// These constants are for use exclusively in JS code. In C++ code,
// prefer ReferenceTypeRepresentation::TYPE_ANY etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_REFERENCETYPEREPR_ANY        0
#define JS_REFERENCETYPEREPR_OBJECT     1
#define JS_REFERENCETYPEREPR_STRING     2

// These constants are for use exclusively in JS code.  In C++ code,
// prefer X4TypeRepresentation::TYPE_INT32 etc, since that allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_X4TYPEREPR_INT32         0
#define JS_X4TYPEREPR_FLOAT32       1

///////////////////////////////////////////////////////////////////////////
// Slots for typed objects

#define JS_TYPEDOBJ_SLOT_BYTEOFFSET       0
#define JS_TYPEDOBJ_SLOT_BYTELENGTH       1
#define JS_TYPEDOBJ_SLOT_OWNER            2
#define JS_TYPEDOBJ_SLOT_NEXT_VIEW        3
#define JS_TYPEDOBJ_SLOT_NEXT_BUFFER      4

#define JS_DATAVIEW_SLOTS              5 // Number of slots for data views

#define JS_TYPEDOBJ_SLOT_LENGTH           5 // Length of array (see (*) below)
#define JS_TYPEDOBJ_SLOT_TYPE_DESCR       6 // For typed objects, type descr

#define JS_TYPEDOBJ_SLOT_DATA             7 // private slot, based on alloc kind
#define JS_TYPEDOBJ_SLOTS                 7 // Number of slots for typed objs

// (*) The JS_TYPEDOBJ_SLOT_LENGTH slot stores the length for typed objects of
// sized and unsized array type. The slot contains 0 for non-arrays.
// The slot also contains 0 for *unattached* typed objects, no matter what
// type they have.

#endif
