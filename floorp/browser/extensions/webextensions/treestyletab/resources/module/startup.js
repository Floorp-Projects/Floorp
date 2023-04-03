/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import '/extlib/l10n.js';

import {
  configs,
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as Permissions from '/common/permissions.js';

window.addEventListener('DOMContentLoaded', () => {
  document.querySelector('#title').textContent = document.title = `${browser.i18n.getMessage('extensionName')} ${browser.runtime.getManifest().version}`;
  document.querySelector('#syncTabsToDeviceOptionsLink').href = `${Constants.kSHORTHAND_URIS.options}#syncTabsToDeviceOptions`;

  Permissions.bindToCheckbox(
    Permissions.ALL_URLS,
    document.querySelector('#allUrlsPermissionGranted'),
    { onChanged: (granted) => configs.skipCollapsedTabsForTabSwitchingShortcuts = granted }
  );
  Permissions.bindToCheckbox(
    Permissions.BOOKMARKS,
    document.querySelector('#bookmarksPermissionGranted')
  );

  document.documentElement.classList.add('initialized');
}, { once: true });
