/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import('resource://gre/modules/Services.jsm');

// Fills up aProcesses until max and then selects randomly from the available
// ones.
function RandomSelector() {
}

RandomSelector.prototype = {
  classID:          Components.ID("{c616fcfd-9737-41f1-aa74-cee72a38f91b}"),
  QueryInterface:   ChromeUtils.generateQI([Ci.nsIContentProcessProvider]),

  provideProcess(aType, aOpener, aProcesses, aCount, aMaxCount) {
    if (aCount < aMaxCount) {
      return Ci.nsIContentProcessProvider.NEW_PROCESS;
    }

    let startIdx = Math.floor(Math.random() * aMaxCount);
    let curIdx = startIdx;

    do {
      if (aProcesses[curIdx].opener === aOpener) {
        return curIdx;
      }

      curIdx = (curIdx + 1) % aMaxCount;
    } while (curIdx !== startIdx);

    return Ci.nsIContentProcessProvider.NEW_PROCESS;
  },
};

// Fills up aProcesses until max and then selects one from the available
// ones that host the least number of tabs.
function MinTabSelector() {
}

MinTabSelector.prototype = {
  classID:          Components.ID("{2dc08eaf-6eef-4394-b1df-a3a927c1290b}"),
  QueryInterface:   ChromeUtils.generateQI([Ci.nsIContentProcessProvider]),

  provideProcess(aType, aOpener, aProcesses, aCount, aMaxCount) {
    if (aCount < aMaxCount) {
      return Ci.nsIContentProcessProvider.NEW_PROCESS;
    }

    let min = Number.MAX_VALUE;
    let candidate = Ci.nsIContentProcessProvider.NEW_PROCESS;

    // Note, that at this point aMaxCount is in the valid range and
    // the reason for not using aCount here is because if we keep
    // processes alive for testing but want a test to use only single
    // content process we can just keep relying on dom.ipc.processCount = 1
    // this way.
    for (let i = 0; i < aMaxCount; i++) {
      let process = aProcesses[i];
      let tabCount = process.tabCount;
      if (process.opener === aOpener && tabCount < min) {
        min = tabCount;
        candidate = i;
      }
    }

    return candidate;
  },
};

var components = [RandomSelector, MinTabSelector];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
