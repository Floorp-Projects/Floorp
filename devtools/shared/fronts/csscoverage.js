/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {cssUsageSpec} = require("devtools/shared/specs/csscoverage");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/shared/locales/csscoverage.properties");

loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);

/**
 * Allow: let foo = l10n.lookup("csscoverageFoo");
 */
const l10n = exports.l10n = {
  lookup: (msg) => L10N.getStr(msg),
};

/**
 * Running more than one usage report at a time is probably bad for performance
 * and it isn't particularly useful, and it's confusing from a notification POV
 * so we only allow one.
 */
var isRunning = false;
var notification;
var target;
var chromeWindow;

/**
 * Front for CSSUsageActor
 */
class CSSUsageFront extends FrontClassWithSpec(cssUsageSpec) {
  constructor(client, form) {
    super(client, { actor: form.cssUsageActor });
    this.manage(this);
    this.before("state-change", this._onStateChange.bind(this));
  }

  _onStateChange(ev) {
    isRunning = ev.isRunning;
    ev.target = target;

    if (isRunning) {
      const gnb = chromeWindow.gNotificationBox;
      notification = gnb.getNotificationWithValue("csscoverage-running");

      if (notification == null) {
        const notifyStop = reason => {
          if (reason == "removed") {
            this.stop();
          }
        };

        const msg = l10n.lookup("csscoverageRunningReply");
        notification = gnb.appendNotification(msg, "csscoverage-running",
                                              "",
                                              gnb.PRIORITY_INFO_HIGH,
                                              null,
                                              notifyStop);
      }
    } else {
      if (notification) {
        notification.close();
        notification = undefined;
      }

      gDevTools.showToolbox(target, "styleeditor");
      target = undefined;
    }
  }

  /**
   * Server-side start is above. Client-side start adds a notification box
   */
  start(newChromeWindow, newTarget, noreload = false) {
    target = newTarget;
    chromeWindow = newChromeWindow;

    return super.start(noreload);
  }

  /**
   * Server-side start is above. Client-side start adds a notification box
   */
  toggle(newChromeWindow, newTarget) {
    target = newTarget;
    chromeWindow = newChromeWindow;

    return super.toggle();
  }

  /**
   * We count STARTING and STOPPING as 'running'
   */
  isRunning() {
    return isRunning;
  }
}

exports.CSSUsageFront = CSSUsageFront;
registerFront(CSSUsageFront);
