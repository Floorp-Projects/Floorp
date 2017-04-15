/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require('chrome');
const { Class } = require('../core/heritage');
const { EventTarget } = require('../event/target');
const { Branch } = require('./service');
const { emit, off } = require('../event/core');
const { when: unload } = require('../system/unload');

const prefTargetNS = require('../core/namespace').ns();

const PrefsTarget = Class({
  extends: EventTarget,
  initialize: function(options) {
    options = options || {};
    EventTarget.prototype.initialize.call(this, options);

    let branchName = options.branchName || '';
    let branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService).
        getBranch(branchName).
        QueryInterface(Ci.nsIPrefBranch2);
    prefTargetNS(this).branch = branch;

    // provides easy access to preference values
    this.prefs = Branch(branchName);

    // start listening to preference changes
    let observer = prefTargetNS(this).observer = onChange.bind(this);
    branch.addObserver('', observer);

    // Make sure to destroy this on unload
    unload(destroy.bind(this));
  }
});
exports.PrefsTarget = PrefsTarget;

/* HELPERS */

function onChange(subject, topic, name) {
  if (topic === 'nsPref:changed') {
    emit(this, name, name);
    emit(this, '', name);
  }
}

function destroy() {
  off(this);

  // stop listening to preference changes
  let branch = prefTargetNS(this).branch;
  branch.removeObserver('', prefTargetNS(this).observer);
  prefTargetNS(this).observer = null;
}
