/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Specialized .h file to be used by both JS and C++ code.

#ifndef builtin_TypedObjectConstants_h
#define builtin_TypedObjectConstants_h

///////////////////////////////////////////////////////////////////////////
// Values to be returned by SetFromTypedArrayApproach

#define JS_SETTYPEDARRAY_SAME_TYPE 0
#define JS_SETTYPEDARRAY_OVERLAPPING 1
#define JS_SETTYPEDARRAY_DISJOINT 2

///////////////////////////////////////////////////////////////////////////
// Slots for objects using the typed array layout

#define JS_TYPEDARRAYLAYOUT_BUFFER_SLOT 0
#define JS_TYPEDARRAYLAYOUT_LENGTH_SLOT 1
#define JS_TYPEDARRAYLAYOUT_BYTEOFFSET_SLOT 2

///////////////////////////////////////////////////////////////////////////
// Slots and flags for ArrayBuffer objects

#define JS_ARRAYBUFFER_FLAGS_SLOT 3

#define JS_ARRAYBUFFER_DETACHED_FLAG 0x8

///////////////////////////////////////////////////////////////////////////
// Slots for typed prototypes

#define JS_TYPROTO_SLOTS 0

///////////////////////////////////////////////////////////////////////////
// Slots for type objects
//
// Some slots apply to all type objects and some are specific to
// particular kinds of type objects. For simplicity we use the same
// number of slots no matter what kind of type descriptor we are
// working with, even though this is mildly wasteful.

// Slots on all type objects
#define JS_DESCR_SLOT_KIND 0         // Atomized string representation
#define JS_DESCR_SLOT_STRING_REPR 1  // Atomized string representation
#define JS_DESCR_SLOT_ALIGNMENT 2    // Alignment in bytes
#define JS_DESCR_SLOT_SIZE 3         // Size in bytes, else 0
#define JS_DESCR_SLOT_OPAQUE 4       // Atomized string representation
#define JS_DESCR_SLOT_TYPROTO 5      // Prototype for instances, if any
#define JS_DESCR_SLOT_ARRAYPROTO 6   // Lazily created prototype for arrays
#define JS_DESCR_SLOT_TRACE_LIST 7   // List of references for use in tracing
#define JS_DESCR_SLOT_FLAGS 8        // int32 bitvector of JS_DESCR_FLAG_*

// Slots on scalars, references
#define JS_DESCR_SLOT_TYPE 9  // Type code

// Slots on array descriptors
#define JS_DESCR_SLOT_ARRAY_ELEM_TYPE 9
#define JS_DESCR_SLOT_ARRAY_LENGTH 10

// Slots on struct type objects
#define JS_DESCR_SLOT_STRUCT_FIELD_NAMES 9
#define JS_DESCR_SLOT_STRUCT_FIELD_TYPES 10
#define JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS 11
#define JS_DESCR_SLOT_STRUCT_FIELD_MUTS 12

// Maximum number of slots for any descriptor
#define JS_DESCR_SLOTS 13

#define JS_DESCR_FLAG_ALLOW_CONSTRUCT 1  // Allow structure to be constructed

// These constants are for use exclusively in JS code. In C++ code,
// prefer TypeRepresentation::Scalar etc, which allows you to
// write a switch which will receive a warning if you omit a case.
#define JS_TYPEREPR_SCALAR_KIND 1
#define JS_TYPEREPR_REFERENCE_KIND 2
#define JS_TYPEREPR_STRUCT_KIND 3
#define JS_TYPEREPR_ARRAY_KIND 4

// These constants are for use exclusively in JS code. In C++ code,
// prefer Scalar::Int8 etc, which allows you to write a switch which will
// receive a warning if you omit a case.
#define JS_SCALARTYPEREPR_INT8 0
#define JS_SCALARTYPEREPR_UINT8 1
#define JS_SCALARTYPEREPR_INT16 2
#define JS_SCALARTYPEREPR_UINT16 3
#define JS_SCALARTYPEREPR_INT32 4
#define JS_SCALARTYPEREPR_UINT32 5
#define JS_SCALARTYPEREPR_FLOAT32 6
#define JS_SCALARTYPEREPR_FLOAT64 7
#define JS_SCALARTYPEREPR_UINT8_CLAMPED 8
#define JS_SCALARTYPEREPR_BIGINT64 9
#define JS_SCALARTYPEREPR_BIGUINT64 10

// These constants are for use exclusively in JS code. In C++ code,
// prefer ReferenceTypeRepresentation::TYPE_ANY etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_REFERENCETYPEREPR_ANY 0
#define JS_REFERENCETYPEREPR_OBJECT 1
#define JS_REFERENCETYPEREPR_WASM_ANYREF 2
#define JS_REFERENCETYPEREPR_STRING 3

#endif
