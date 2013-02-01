/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

define(['./traditional2', './async2'], function () {
  var traditional2 = require('./traditional2');
  return {
    name: 'async1',
    traditional1Name: traditional2.traditional1Name,
    traditional2Name: traditional2.name,
    async2Name: require('./async2').name,
    async2Traditional2Name: require('./async2').traditional2Name
  };
});
