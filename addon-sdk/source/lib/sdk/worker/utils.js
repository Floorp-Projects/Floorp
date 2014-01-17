/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'deprecated'
};

const {
  requiresAddonGlobal, attach, detach, destroy, WorkerHost
} = require('../content/utils');

exports.WorkerHost = WorkerHost;
exports.detach = detach;
exports.attach = attach;
exports.destroy = destroy;
exports.requiresAddonGlobal = requiresAddonGlobal;
