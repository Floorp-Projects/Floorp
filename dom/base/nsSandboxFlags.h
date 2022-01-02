/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
 * This constant denotes the lack of a sandbox attribute/directive.
 */
const unsigned long SANDBOXED_NONE = 0x0;

/**
 * This flag prevents content from navigating browsing contexts other than
 * itself, browsing contexts nested inside it, the top-level browsing context
 * and browsing contexts that it has opened.
 * As it is always on for sandboxed browsing contexts, it is used implicitly
 * within the code by checking that the overall flags are non-zero.
 * It is only uesd directly when the sandbox flags are initially set up.
 */
const unsigned long SANDBOXED_NAVIGATION = 0x1;

/**
 * This flag prevents content from creating new auxiliary browsing contexts,
 * e.g. using the target attribute, or the window.open() method.
 */
const unsigned long SANDBOXED_AUXILIARY_NAVIGATION = 0x2;

/**
 * This flag prevents content from navigating their top-level browsing
 * context.
 */
const unsigned long SANDBOXED_TOPLEVEL_NAVIGATION = 0x4;

/**
 * This flag prevents content from instantiating plugins, whether using the
 * embed element, the object element, or through navigation of a nested browsing
 * context, unless those plugins can be secured.
 */
const unsigned long SANDBOXED_PLUGINS = 0x8;

/**
 * This flag forces content into a unique origin, thus preventing it from
 * accessing other content from the same origin.
 * This flag also prevents script from reading from or writing to the
 * document.cookie IDL attribute, and blocks access to localStorage.
 */
const unsigned long SANDBOXED_ORIGIN = 0x10;

/**
 * This flag blocks form submission.
 */
const unsigned long SANDBOXED_FORMS = 0x20;

/**
 * This flag blocks the document from acquiring pointerlock.
 */
const unsigned long SANDBOXED_POINTER_LOCK = 0x40;

/**
 * This flag blocks script execution.
 */
const unsigned long SANDBOXED_SCRIPTS = 0x80;

/**
 * This flag blocks features that trigger automatically, such as
 * automatically playing a video or automatically focusing a form control.
 */
const unsigned long SANDBOXED_AUTOMATIC_FEATURES = 0x100;

/**
 * This flag prevents URL schemes that use storage areas from being able to
 * access the origin's data.
 */
// We don't have an explicit representation of this one, apparently?
// const unsigned long SANDBOXED_STORAGE_AREA_URLS = 0x200;

/**
 * This flag blocks the document from changing document.domain.
 */
const unsigned long SANDBOXED_DOMAIN = 0x400;

/**
 * This flag prevents content from using window.alert(), window.confirm(),
 * window.print(), window.prompt() and the beforeunload event from putting up
 * dialogs.
 */
const unsigned long SANDBOXED_MODALS = 0x800;

/**
 * This flag prevents content from escaping the sandbox by ensuring that any
 * auxiliary browsing context it creates inherits the content's active
 * sandboxing flag set.
 */
const unsigned long SANDBOX_PROPAGATES_TO_AUXILIARY_BROWSING_CONTEXTS = 0x1000;

/**
 * This flag prevents locking screen orientation.
 */
const unsigned long SANDBOXED_ORIENTATION_LOCK = 0x2000;

/**
 * This flag disables the Presentation API.
 */
const unsigned long SANDBOXED_PRESENTATION = 0x4000;

/**
 * This flag disables access to the first-party storage area by user activation.
 */
const unsigned long SANDBOXED_STORAGE_ACCESS = 0x8000;

/**
 * This flag prevents content from navigating their top-level browsing
 * context only when the user hasn't interacted with the browser.
 */
const unsigned long SANDBOXED_TOPLEVEL_NAVIGATION_USER_ACTIVATION = 0x20000;

/**
 * This flag disables content from initiating or instantiating downloads,
 * whether through downloading hyperlinks or through navigation that gets
 * handled as a download.
 */
const unsigned long SANDBOXED_ALLOW_DOWNLOADS = 0x10000;

const unsigned long SANDBOX_ALL_FLAGS = 0xFFFFF;
#endif
