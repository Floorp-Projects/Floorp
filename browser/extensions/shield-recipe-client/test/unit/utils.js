"use strict";
/* eslint-disable no-unused-vars */

Cu.import("resource://gre/modules/Preferences.jsm");

const preferenceBranches = {
  user: Preferences,
  default: new Preferences({defaultBranch: true}),
};

// duplicated from test/browser/head.js until we move everything over to mochitests.
function withMockPreferences(testFunction) {
  return async function inner(...args) {
    const prefManager = new MockPreferences();
    try {
      await testFunction(...args, prefManager);
    } finally {
      prefManager.cleanup();
    }
  };
}

class MockPreferences {
  constructor() {
    this.oldValues = {user: {}, default: {}};
  }

  set(name, value, branch = "user") {
    this.preserve(name, branch);
    preferenceBranches[branch].set(name, value);
  }

  preserve(name, branch) {
    if (!(name in this.oldValues[branch])) {
      this.oldValues[branch][name] = preferenceBranches[branch].get(name, undefined);
    }
  }

  cleanup() {
    for (const [branchName, values] of Object.entries(this.oldValues)) {
      const preferenceBranch = preferenceBranches[branchName];
      for (const [name, value] of Object.entries(values)) {
        if (value !== undefined) {
          preferenceBranch.set(name, value);
        } else {
          preferenceBranch.reset(name);
        }
      }
    }
  }
}
