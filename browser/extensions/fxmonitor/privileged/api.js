/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ExtensionAPI */

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

let FirefoxMonitorContainer = {};

this.fxmonitor = class extends ExtensionAPI {
  getAPI(context) {
    Services.scriptloader.loadSubScript(context.extension.getURL("privileged/FirefoxMonitor.jsm"),
                                        FirefoxMonitorContainer);
    return {
      fxmonitor: {
        async start() {
          await FirefoxMonitorContainer.FirefoxMonitor.init(context.extension);
        },
      },
    };
  }

  onShutdown(shutdownReason) {
    if (Services.startup.shuttingDown || !FirefoxMonitorContainer.FirefoxMonitor) {
      return;
    }

    FirefoxMonitorContainer.FirefoxMonitor.stopObserving();
  }
};
