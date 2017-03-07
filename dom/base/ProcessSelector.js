/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');

const BASE_PREF = "dom.ipc.processCount"
const PREF_BRANCH = BASE_PREF + ".";

// Utilities:
function getMaxContentParents(processType) {
  // If the pref doesn't exist, get the default number of processes.
  // If there's no pref, use only one process.
  return Services.prefs.getIntPref(PREF_BRANCH + processType,
                                   Services.prefs.getIntPref(BASE_PREF, 1));
}

// Fills up aProcesses until max and then selects randomly from the available
// ones.
function RandomSelector() {
}

RandomSelector.prototype = {
  classID:          Components.ID("{c616fcfd-9737-41f1-aa74-cee72a38f91b}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIContentProcessProvider]),

  provideProcess(aType, aOpener, aProcesses, aCount) {
    let maxContentParents = getMaxContentParents(aType);
    if (aCount < maxContentParents) {
      return Ci.nsIContentProcessProvider.NEW_PROCESS;
    }

    let startIdx = Math.floor(Math.random() * maxContentParents);
    let curIdx = startIdx;

    do {
      if (aProcesses[curIdx].opener === aOpener) {
        return curIdx;
      }

      curIdx = (curIdx + 1) % maxContentParents;
    } while (curIdx !== startIdx);

    return Ci.nsIContentProcessProvider.NEW_PROCESS;
  },
};

var components = [RandomSelector];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
