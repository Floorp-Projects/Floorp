/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Constant flags that describe how a document is sandboxed according to the
 * HTML5 spec.
 */

#ifndef nsSandboxFlags_h___
#define nsSandboxFlags_h___

/**
 * This flag prevents content from navigating browsing contexts other than
 * the sandboxed browsing context itself (or browsing contexts further
 * nested inside it), and the top-level browsing context.
 */
const unsigned long SANDBOXED_NAVIGATION  = 0x1;

/**
 * This flag prevents content from navigating their top-level browsing
 * context.
 */
const unsigned long SANDBOXED_TOPLEVEL_NAVIGATION = 0x2;

/**
 * This flag prevents content from instantiating plugins, whether using the
 * embed element, the object element, the applet element, or through
 * navigation of a nested browsing context, unless those plugins can be
 * secured.
 */
const unsigned long SANDBOXED_PLUGINS = 0x4;

/**
 * This flag forces content into a unique origin, thus preventing it from
 * accessing other content from the same origin.
 * This flag also prevents script from reading from or writing to the
 * document.cookie IDL attribute, and blocks access to localStorage.
 */
const unsigned long SANDBOXED_ORIGIN = 0x8;

/**
 * This flag blocks form submission.
 */
const unsigned long SANDBOXED_FORMS = 0x10;

/**
 * This flag blocks script execution.
 */
const unsigned long SANDBOXED_SCRIPTS = 0x20;

/**
 * This flag blocks features that trigger automatically, such as
 * automatically playing a video or automatically focusing a form control.
 */
const unsigned long SANDBOXED_AUTOMATIC_FEATURES = 0x40;

/**
 * This flag blocks the document from acquiring pointerlock.
 */
const unsigned long SANDBOXED_POINTER_LOCK = 0x80;

/**
 * This flag blocks the document from changing document.domain.
 */
const unsigned long SANDBOXED_DOMAIN = 0x100;
#endif
