/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains XPCOM code that bootstraps an SDK-based add-on
// by loading its harness-options.json, registering all its resource
// directories, executing its loader, and then executing its program's
// main() function.
