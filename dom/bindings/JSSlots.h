/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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
#define DOM_OBJECT_SLOT 0

// We use slot 1 for holding the expando object. This is not safe for globals
// until bug 760095 is fixed, so that bug blocks converting Window to new
// bindings.
#define DOM_XRAY_EXPANDO_SLOT 1

// We use slot 2 for holding either a JS::ObjectValue which points to the cached
// SOW or JS::UndefinedValue if this class doesn't need SOWs. This is not safe
// for globals until bug 760095 is fixed, so that bug blocks converting Window
// to new bindings.
#define DOM_OBJECT_SLOT_SOW 2

// NOTE: This is baked into the Ion JIT as 0 in codegen for LGetDOMProperty and
// LSetDOMProperty. Those constants need to be changed accordingly if this value
// changes.
#define DOM_PROTO_INSTANCE_CLASS_SLOT 0

// Interface objects store a number of reserved slots equal to
// DOM_INTERFACE_SLOTS_BASE + number of named constructors.
#define DOM_INTERFACE_SLOTS_BASE (DOM_XRAY_EXPANDO_SLOT + 1)

// Interface prototype objects store a number of reserved slots equal to
// DOM_INTERFACE_PROTO_SLOTS_BASE or DOM_INTERFACE_PROTO_SLOTS_BASE + 1 if a
// slot for the unforgeable holder is needed.
#define DOM_INTERFACE_PROTO_SLOTS_BASE (DOM_XRAY_EXPANDO_SLOT + 1)

static_assert(DOM_PROTO_INSTANCE_CLASS_SLOT != DOM_XRAY_EXPANDO_SLOT,
              "Interface prototype object use both of these, so they must "
              "not be the same slot.");

#endif /* mozilla_dom_DOMSlots_h */
