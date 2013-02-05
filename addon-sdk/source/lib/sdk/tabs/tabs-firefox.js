/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

// TODO: BUG 792670 - remove dependency below
const { browserWindows } = require('../windows');
const { tabs } = require('../windows/tabs-firefox');

Object.defineProperties(tabs, {
  open: { value: function open(options) {
    if (options.inNewWindow)
        // `tabs` option is under review and may be removed.
        return browserWindows.open({ tabs: [ options ] });
    // Open in active window if new window was not required.
    return browserWindows.activeWindow.tabs.open(options);
  }}
});

// Workaround for bug 674195. Freezing objects from other compartments fail,
// so we use `Object.freeze` from the same component as objects
// `hasOwnProperty`. Since `hasOwnProperty` here will be from other component
// we override it to support our workaround.
module.exports = Object.create(tabs, {
  isPrototypeOf: { value: Object.prototype.isPrototypeOf }
});
