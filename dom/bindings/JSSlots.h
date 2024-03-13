/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines various reserved slot indices used by JavaScript
 * reflections of DOM objects.
 */
#ifndef mozilla_dom_DOMSlots_h
#define mozilla_dom_DOMSlots_h

// We use slot 0 for holding the raw object.  This is safe for both
// globals and non-globals.
// NOTE: This is baked into the Ion JIT as 0 in codegen for LGetDOMProperty and
// LSetDOMProperty. Those constants need to be changed accordingly if this value
// changes.
#define DOM_OBJECT_SLOT 0

// The total number of slots non-proxy DOM objects use by default.
// Specific objects may have more for storing cached values.
#define DOM_INSTANCE_RESERVED_SLOTS 1

// Interface objects store a number of reserved slots equal to
// INTERFACE_OBJECT_INFO_RESERVED_SLOT + number of legacy factory functions,
// with a maximum of js::FunctionExtended::NUM_EXTENDED_SLOTS.
// INTERFACE_OBJECT_INFO_RESERVED_SLOT contains the DOMInterfaceInfo.
// INTERFACE_OBJECT_FIRST_LEGACY_FACTORY_FUNCTION and higher contain the
// JSObjects for the legacy factory functions.
enum {
  INTERFACE_OBJECT_INFO_RESERVED_SLOT = 0,
  INTERFACE_OBJECT_FIRST_LEGACY_FACTORY_FUNCTION,
};
// See js::FunctionExtended::NUM_EXTENDED_SLOTS.
#define INTERFACE_OBJECT_MAX_SLOTS 3

// Legacy factory functions store a JSNativeHolder in the
// LEGACY_FACTORY_FUNCTION_NATIVE_HOLDER_RESERVED_SLOT slot.
enum { LEGACY_FACTORY_FUNCTION_NATIVE_HOLDER_RESERVED_SLOT = 0 };

// Interface prototype objects store a number of reserved slots equal to
// DOM_INTERFACE_PROTO_SLOTS_BASE or DOM_INTERFACE_PROTO_SLOTS_BASE + 1 if a
// slot for the unforgeable holder is needed.
#define DOM_INTERFACE_PROTO_SLOTS_BASE 0

// The slot index of raw pointer of dom object stored in observable array exotic
// object. We need this in order to call the OnSet* and OnDelete* callbacks.
#define OBSERVABLE_ARRAY_DOM_INTERFACE_SLOT 0

// The slot index of backing list stored in observable array exotic object.
#define OBSERVABLE_ARRAY_BACKING_LIST_OBJECT_SLOT 1

#endif /* mozilla_dom_DOMSlots_h */
