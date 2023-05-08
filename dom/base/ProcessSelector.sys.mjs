/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Fills up aProcesses until max and then selects randomly from the available
// ones.
export function RandomSelector() {}

RandomSelector.prototype = {
  classID: Components.ID("{c616fcfd-9737-41f1-aa74-cee72a38f91b}"),
  QueryInterface: ChromeUtils.generateQI(["nsIContentProcessProvider"]),

  provideProcess(aType, aProcesses, aMaxCount) {
    if (aProcesses.length < aMaxCount) {
      return Ci.nsIContentProcessProvider.NEW_PROCESS;
    }

    return Math.floor(Math.random() * aMaxCount);
  },
};

// Fills up aProcesses until max and then selects one from the available
// ones that host the least number of tabs.
export function MinTabSelector() {}

MinTabSelector.prototype = {
  classID: Components.ID("{2dc08eaf-6eef-4394-b1df-a3a927c1290b}"),
  QueryInterface: ChromeUtils.generateQI(["nsIContentProcessProvider"]),

  provideProcess(aType, aProcesses, aMaxCount) {
    let min = Number.MAX_VALUE;
    let candidate = Ci.nsIContentProcessProvider.NEW_PROCESS;

    // The reason for not directly using aProcesses.length here is because if
    // we keep processes alive for testing but want a test to use only single
    // content process we can just keep relying on dom.ipc.processCount = 1
    // this way.
    let numIters = Math.min(aProcesses.length, aMaxCount);

    for (let i = 0; i < numIters; i++) {
      let process = aProcesses[i];
      let tabCount = process.tabCount;
      if (tabCount < min) {
        min = tabCount;
        candidate = i;
      }
    }

    // If all current processes have at least one tab and we have not yet
    // reached the maximum, spawn a new process.
    if (min > 0 && aProcesses.length < aMaxCount) {
      return Ci.nsIContentProcessProvider.NEW_PROCESS;
    }

    // Otherwise we use candidate.
    return candidate;
  },
};
