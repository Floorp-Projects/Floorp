/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { getTargetWindow } = require("../content/mod");
const { getTabContentWindow, isTab } = require("./utils");
const { viewFor } = require("../view/core");

if (require('../system/xul-app').name == 'Fennec') {
  module.exports = require('./tab-fennec');
}
else {
  module.exports = require('./tab-firefox');
}

getTargetWindow.when(isTab, tab => getTabContentWindow(tab));

getTargetWindow.when(x => x instanceof module.exports.Tab,
  tab => getTabContentWindow(viewFor(tab)));
