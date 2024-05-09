/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LogManager } from "resource://gre/modules/LogManager.sys.mjs";
// See Bug 1889052
// eslint-disable-next-line mozilla/use-console-createInstance
import { Log } from "resource://gre/modules/Log.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  cancelIdleCallback: "resource://gre/modules/Timer.sys.mjs",
  requestIdleCallback: "resource://gre/modules/Timer.sys.mjs",
});

const loggerNames = ["SessionStore"];

export const sessionStoreLogger = Log.repository.getLogger("SessionStore");
sessionStoreLogger.manageLevelFromPref("browser.sessionstore.loglevel");

class SessionLogManager extends LogManager {
  #idleCallbackId = null;
  #observers = new Set();

  QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);

  constructor(options = {}) {
    super(options);

    Services.obs.addObserver(this, "sessionstore-windows-restored");
    this.#observers.add("sessionstore-windows-restored");

    lazy.AsyncShutdown.profileBeforeChange.addBlocker(
      "SessionLogManager: finalize and flush any logs to disk",
      () => {
        return this.stop();
      }
    );
  }

  async stop() {
    if (this.#observers.has("sessionstore-windows-restored")) {
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      this.#observers.delete("sessionstore-windows-restored");
    }
    await this.requestLogFlush(true);
    this.finalize();
  }

  observe(subject, topic, _) {
    switch (topic) {
      case "sessionstore-windows-restored":
        // this represents the moment session restore is nominally complete
        // and is a good time to ensure any log messages are flushed to disk
        Services.obs.removeObserver(this, "sessionstore-windows-restored");
        this.#observers.delete("sessionstore-windows-restored");
        this.requestLogFlush();
        break;
    }
  }

  async requestLogFlush(immediate = false) {
    if (this.#idleCallbackId && !immediate) {
      return;
    }
    if (this.#idleCallbackId) {
      lazy.cancelIdleCallback(this.#idleCallbackId);
      this.#idleCallbackId = null;
    }
    if (!immediate) {
      await new Promise(resolve => {
        this.#idleCallbackId = lazy.requestIdleCallback(resolve);
      });
      this.#idleCallbackId = null;
    }
    await this.resetFileLog();
  }
}

export const logManager = new SessionLogManager({
  prefRoot: "browser.sessionstore.",
  logNames: loggerNames,
  logFilePrefix: "sessionrestore",
  logFileSubDirectoryEntries: ["sessionstore-logs"],
  testTopicPrefix: "sessionrestore:log-manager:",
});
