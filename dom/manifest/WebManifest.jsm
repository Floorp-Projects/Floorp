/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/*exported EXPORTED_SYMBOLS, ManifestProcessor, ManifestObtainer*/
/*globals Components */
'use strict';
const {
  utils: Cu
} = Components;

this.EXPORTED_SYMBOLS = [
  'ManifestObtainer',
  'ManifestProcessor'
];

// Export public interfaces
for (let symbl of EXPORTED_SYMBOLS) {
  Cu.import(`resource://gre/modules/${symbl}.js`);
}
