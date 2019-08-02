/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * We cannot yet use proper JS modules within webextensions, as support for them
 * is highly experimental and highly instable. So we end up just including all
 * the JS files we need as separate background scripts, and since they all are
 * executed within the same context, this works for our in-browser deployment.
 *
 * However, this code is tracked outside of mozilla-central, and we work on
 * shipping this code in other products, like android-components as well.
 * Because of that, we have automated tests running within that repository. To
 * make our lives easier, we add `module.exports` statements to the JS source
 * files, so we can easily import their contents into our NodeJS-based test
 * suite.
 *
 * This works fine, but obviously, `module` is not defined when running
 * in-browser. So let's use this empty object as a shim, so we don't run into
 * runtime exceptions because of that.
 */
var module = {};
