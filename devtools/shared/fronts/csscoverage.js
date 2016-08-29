/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {cssUsageSpec} = require("devtools/shared/specs/csscoverage");
const protocol = require("devtools/shared/protocol");
const {custom} = protocol;

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools-shared/locale/csscoverage.properties");

loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);

/**
 * Allow: let foo = l10n.lookup("csscoverageFoo");
 */
const l10n = exports.l10n = {
  lookup: (msg) => L10N.getStr(msg)
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
const CSSUsageFront = protocol.FrontClassWithSpec(cssUsageSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.cssUsageActor;
    this.manage(this);
  },

  _onStateChange: protocol.preEvent("state-change", function (ev) {
    isRunning = ev.isRunning;
    ev.target = target;

    if (isRunning) {
      let gnb = chromeWindow.document.getElementById("global-notificationbox");
      notification = gnb.getNotificationWithValue("csscoverage-running");

      if (notification == null) {
        let notifyStop = reason => {
          if (reason == "removed") {
            this.stop();
          }
        };

        let msg = l10n.lookup("csscoverageRunningReply");
        notification = gnb.appendNotification(msg, "csscoverage-running",
                                              "",
                                              gnb.PRIORITY_INFO_HIGH,
                                              null,
                                              notifyStop);
      }
    } else {
      if (notification) {
        notification.remove();
        notification = undefined;
      }

      gDevTools.showToolbox(target, "styleeditor");
      target = undefined;
    }
  }),

  /**
   * Server-side start is above. Client-side start adds a notification box
   */
  start: custom(function (newChromeWindow, newTarget, noreload = false) {
    target = newTarget;
    chromeWindow = newChromeWindow;

    return this._start(noreload);
  }, {
    impl: "_start"
  }),

  /**
   * Server-side start is above. Client-side start adds a notification box
   */
  toggle: custom(function (newChromeWindow, newTarget) {
    target = newTarget;
    chromeWindow = newChromeWindow;

    return this._toggle();
  }, {
    impl: "_toggle"
  }),

  /**
   * We count STARTING and STOPPING as 'running'
   */
  isRunning: function () {
    return isRunning;
  }
});

exports.CSSUsageFront = CSSUsageFront;

const knownFronts = new WeakMap();

/**
 * Create a CSSUsageFront only when needed (returns a promise)
 * For notes on target.makeRemote(), see
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1016330#c7
 */
exports.getUsage = function (trgt) {
  return trgt.makeRemote().then(() => {
    let front = knownFronts.get(trgt.client);
    if (front == null && trgt.form.cssUsageActor != null) {
      front = new CSSUsageFront(trgt.client, trgt.form);
      knownFronts.set(trgt.client, front);
    }
    return front;
  });
};
