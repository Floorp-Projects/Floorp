/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import * as Constants from '/common/constants.js';

const uri = decodeURIComponent(location.search.replace(/^\?/, ''));
const matched = uri && uri.match(Constants.kSHORTHAND_CUSTOM_URI);
if (matched) {
  const name = matched[1];
  const params = new URLSearchParams(matched[2] || '');
  const hash = matched[3] || '';
  const delimiter = params.toString() ? '?' : '';
  switch (name.toLowerCase()) {
    case 'group':
      location.href = `${Constants.kSHORTHAND_URIS.group}${delimiter}${params.toString()}${hash}`;
      break;

    case 'startup':
      location.href = `${Constants.kSHORTHAND_URIS.startup}${hash}`;
      break;

    case 'test-runner':
    case 'testrunner':
      location.href = `${Constants.kSHORTHAND_URIS.testRunner}${delimiter}${params.toString()}${hash}`;
      break;

    case 'options':
      location.href = `${Constants.kSHORTHAND_URIS.options}${delimiter}${params.toString()}${hash}`;
      break;

    case 'tabbar':
    case 'sidebar':
      if (!params.has('style')) {
        browser.runtime.sendMessage({
          type: 'treestyletab:get-config-value',
          keys: ['style']
        }).then(configs => {
          params.set('style', configs.style);
          location.href = `${Constants.kSHORTHAND_URIS.tabbar}?${params.toString()}${hash}`;
        });
      }
      else {
        location.href = `${Constants.kSHORTHAND_URIS.tabbar}${delimiter}${params.toString()}${hash}`;
      }
      break;
  }
}
