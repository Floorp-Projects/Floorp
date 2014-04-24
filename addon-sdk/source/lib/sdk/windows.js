/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'stable',
  'engines': {
    'Firefox': '*',
    'Fennec': '*'
  }
};

const { isBrowser } = require('./window/utils');
const { modelFor } = require('./model/core');
const { viewFor } = require('./view/core');


if (require('./system/xul-app').is('Fennec')) {
  module.exports = require('./windows/fennec');
}
else {
  module.exports = require('./windows/firefox');
}


const browsers = module.exports.browserWindows;

//
modelFor.when(isBrowser, view => {
  for (let model of browsers) {
    if (viewFor(model) === view)
      return model;
  }
  return null;
});
