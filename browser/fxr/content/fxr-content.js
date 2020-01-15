/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script is for code associated with an FxR PC window that
 * must run in a content process. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Send this notification from the content process so that the
// ExtensionPolicyService can register this content frame and be ready to load
// content scripts
Services.obs.notifyObservers(this, "tab-content-frameloader-created");
