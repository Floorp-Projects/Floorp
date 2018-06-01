/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Services = require("Services");
const defer = require("devtools/shared/defer");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

function handleThreadState(toolbox, event, packet) {
  // Suppress interrupted events by default because the thread is
  // paused/resumed a lot for various actions.
  if (event === "paused" && packet.why.type === "interrupted") {
    return;
  }

  // TODO: Bug 1225492, we continue emitting events on the target
  // like we used to, but we should emit these only on the
  // threadClient now.
  toolbox.target.emit("thread-" + event);

  if (event === "paused") {
    toolbox.highlightTool("jsdebugger");

    if (packet.why.type === "debuggerStatement" ||
       packet.why.type === "breakpoint" ||
       packet.why.type === "exception") {
      toolbox.raise();
      toolbox.selectTool("jsdebugger", packet.why.type);
    }
  } else if (event === "resumed") {
    toolbox.unhighlightTool("jsdebugger");
  }
}

function attachThread(toolbox) {
  const deferred = defer();

  const target = toolbox.target;
  const { form: { chromeDebugger, actor } } = target;

  // Sourcemaps are always turned off when using the new debugger
  // frontend. This is because it does sourcemapping on the
  // client-side, so the server should not do it.
  let useSourceMaps = false;
  let autoBlackBox = false;
  let ignoreFrameEnvironment = false;
  const newDebuggerEnabled = Services.prefs.getBoolPref("devtools.debugger.new-debugger-frontend");
  if (!newDebuggerEnabled) {
    useSourceMaps = Services.prefs.getBoolPref("devtools.debugger.source-maps-enabled");
    autoBlackBox = Services.prefs.getBoolPref("devtools.debugger.auto-black-box");
  } else {
    ignoreFrameEnvironment = true;
  }

  const threadOptions = { useSourceMaps, autoBlackBox, ignoreFrameEnvironment };

  const handleResponse = (res, threadClient) => {
    if (res.error) {
      deferred.reject(new Error("Couldn't attach to thread: " + res.error));
      return;
    }
    threadClient.addListener("paused", handleThreadState.bind(null, toolbox));
    threadClient.addListener("resumed", handleThreadState.bind(null, toolbox));

    if (!threadClient.paused) {
      deferred.reject(
        new Error("Thread in wrong state when starting up, should be paused")
      );
    }

    // These flags need to be set here because the client sends them
    // with the `resume` request. We make sure to do this before
    // resuming to avoid another interrupt. We can't pass it in with
    // `threadOptions` because the resume request will override them.
    threadClient.pauseOnExceptions(
      Services.prefs.getBoolPref("devtools.debugger.pause-on-exceptions"),
      Services.prefs.getBoolPref("devtools.debugger.ignore-caught-exceptions")
    );

    threadClient.resume(res => {
      if (res.error === "wrongOrder") {
        const box = toolbox.getNotificationBox();
        box.appendNotification(
          L10N.getStr("toolbox.resumeOrderWarning"),
          "wrong-resume-order",
          "",
          box.PRIORITY_WARNING_HIGH
        );
      }

      deferred.resolve(threadClient);
    });
  };

  if (target.isTabActor) {
    // Attaching a tab, a browser process, or a WebExtensions add-on.
    target.activeTab.attachThread(threadOptions, handleResponse);
  } else if (target.isAddon) {
    // Attaching a legacy addon.
    target.client.attachAddon(actor, res => {
      target.client.attachThread(res.threadActor, handleResponse);
    });
  } else {
    // Attaching an old browser debugger or a content process.
    target.client.attachThread(chromeDebugger, handleResponse);
  }

  return deferred.promise;
}

function detachThread(threadClient) {
  threadClient.removeListener("paused");
  threadClient.removeListener("resumed");
}

module.exports = { attachThread, detachThread };
