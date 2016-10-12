/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {interfaces: Ci, classes: Cc, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

var gProfD = do_get_profile().QueryInterface(Ci.nsILocalFile);
