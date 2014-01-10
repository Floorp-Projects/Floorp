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

exports.ActionButton = require('./ui/button/action').ActionButton;
exports.ToggleButton = require('./ui/button/toggle').ToggleButton;
exports.Sidebar = require('./ui/sidebar').Sidebar;
exports.Frame = require('./ui/frame').Frame;
exports.Toolbar = require('./ui/toolbar').Toolbar;
