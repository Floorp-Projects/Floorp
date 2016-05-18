/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* NOTE: no include guard; this file is meant to maybe be included multiple
   times.  It has a list of the sandbox keywords we support, with their
   corresponding sandbox flags. */

// Each entry has the sandbox keyword as a string, the corresponding nsGkAtoms
// atom name, and the corresponding sandbox flags.
SANDBOX_KEYWORD("allow-same-origin", allowsameorigin,  SANDBOXED_ORIGIN)
SANDBOX_KEYWORD("allow-forms", allowforms,  SANDBOXED_FORMS)
SANDBOX_KEYWORD("allow-scripts", allowscripts,
		SANDBOXED_SCRIPTS | SANDBOXED_AUTOMATIC_FEATURES)
SANDBOX_KEYWORD("allow-top-navigation", allowtopnavigation,
		SANDBOXED_TOPLEVEL_NAVIGATION)
SANDBOX_KEYWORD("allow-pointer-lock", allowpointerlock, SANDBOXED_POINTER_LOCK)
SANDBOX_KEYWORD("allow-orientation-lock", alloworientationlock,
		SANDBOXED_ORIENTATION_LOCK)
SANDBOX_KEYWORD("allow-popups", allowpopups, SANDBOXED_AUXILIARY_NAVIGATION)
SANDBOX_KEYWORD("allow-modals", allowmodals, SANDBOXED_MODALS)
SANDBOX_KEYWORD("allow-popups-to-escape-sandbox", allowpopupstoescapesandbox,
                SANDBOX_PROPAGATES_TO_AUXILIARY_BROWSING_CONTEXTS)
