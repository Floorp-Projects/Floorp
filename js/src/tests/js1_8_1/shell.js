/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: This only turns on 1.8.1 in shell builds.  The browser requires the
//       futzing in js/src/tests/browser.js (which only turns on 1.8, the most
//       the browser supports).
if (typeof version != 'undefined')
{
  version(181);
}

