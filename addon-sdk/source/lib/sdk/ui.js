/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Firefox': '> 28'
  }
};

lazyRequire(this, './ui/button/action', 'ActionButton');
lazyRequire(this, './ui/button/toggle', 'ToggleButton');
lazyRequire(this, './ui/sidebar', 'Sidebar');
lazyRequire(this, './ui/frame', 'Frame');
lazyRequire(this, './ui/toolbar', 'Toolbar');

module.exports = Object.freeze({
  get ActionButton() { return ActionButton; },
  get ToggleButton() { return ToggleButton; },
  get Sidebar() { return Sidebar; },
  get Frame() { return Frame; },
  get Toolbar() { return Toolbar; },
});
