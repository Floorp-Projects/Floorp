/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

exports.name = 'traditional1'

var async1 = require('./async1');

exports.traditional2Name = async1.traditional2Name;
exports.traditional1Name = async1.traditional1Name;
exports.async2Name = async1.async2Name;
exports.async2Traditional2Name = async1.async2Traditional2Name;
