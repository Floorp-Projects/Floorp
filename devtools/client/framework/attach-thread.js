/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Services = require("Services");
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

async function attachThread(toolbox) {
  const target = toolbox.target;
  const threadOptions = {
    autoBlackBox: false,
    ignoreFrameEnvironment: true,
    pauseOnExceptions:
      Services.prefs.getBoolPref("devtools.debugger.pause-on-exceptions"),
    ignoreCaughtExceptions:
      Services.prefs.getBoolPref("devtools.debugger.ignore-caught-exceptions"),
  };

  const [, threadClient] = await target.attachThread(threadOptions);
  if (!threadClient.paused) {
    throw new Error("Thread in wrong state when starting up, should be paused.");
  }

  try {
    await threadClient.resume();
  } catch (ex) {
    // Interpret a possible error thrown by ThreadActor.resume
    if (ex.error === "wrongOrder") {
      const box = toolbox.getNotificationBox();
      box.appendNotification(
        L10N.getStr("toolbox.resumeOrderWarning"),
        "wrong-resume-order",
        "",
        box.PRIORITY_WARNING_HIGH
      );
    } else {
      throw ex;
    }
  }
  threadClient.addListener("paused", handleThreadState.bind(null, toolbox));
  threadClient.addListener("resumed", handleThreadState.bind(null, toolbox));
  return threadClient;
}

function detachThread(threadClient) {
  threadClient.removeListener("paused");
  threadClient.removeListener("resumed");
}

module.exports = { attachThread, detachThread };
