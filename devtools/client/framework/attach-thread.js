const {Cc, Ci, Cu} = require("chrome");
const Services = Cu.import("resource://gre/modules/Services.jsm", {}).Services;
const promise = require("promise");

function l10n(name) {
  const bundle = Services.strings.createBundle("chrome://devtools/locale/toolbox.properties");
  try {
    return bundle.GetStringFromName(name);
  } catch (e) {
    throw new Error("Failed loading l10n string: " + name);
  }
}

function handleThreadState(toolbox, event, packet) {
  // Suppress interrupted events by default because the thread is
  // paused/resumed a lot for various actions.
  if (event !== "paused" || packet.why.type !== "interrupted") {
    // TODO: Bug 1225492, we continue emitting events on the target
    // like we used to, but we should emit these only on the
    // threadClient now.
    toolbox.target.emit("thread-" + event);
  }

  if (event === "paused") {
    toolbox.highlightTool("jsdebugger");

    if (packet.why.type === 'debuggerStatement' ||
       packet.why.type === 'breakpoint' ||
       packet.why.type === 'exception') {
      toolbox.raise();
      toolbox.selectTool("jsdebugger");
    }
  } else if (event === "resumed") {
    toolbox.unhighlightTool("jsdebugger");
  }
}

function attachThread(toolbox) {
  let deferred = promise.defer();

  let target = toolbox.target;
  let { form: { chromeDebugger, actor } } = target;
  let threadOptions = {
    useSourceMaps: Services.prefs.getBoolPref("devtools.debugger.source-maps-enabled"),
    autoBlackBox: Services.prefs.getBoolPref("devtools.debugger.auto-black-box"),
    pauseOnExceptions: Services.prefs.getBoolPref("devtools.debugger.pause-on-exceptions"),
    ignoreCaughtExceptions: Services.prefs.getBoolPref("devtools.debugger.ignore-caught-exceptions")
  };

  let handleResponse = (res, threadClient) => {
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

    threadClient.resume(res => {
      if (res.error === "wrongOrder") {
        const box = toolbox.getNotificationBox();
        box.appendNotification(
          l10n("toolbox.resumeOrderWarning"),
          "wrong-resume-order",
          "",
          box.PRIORITY_WARNING_HIGH
        );
      }

      deferred.resolve(threadClient)
    });
  }

  if (target.isAddon) {
    // Attaching an addon
    target.client.attachAddon(actor, res => {
      target.client.attachThread(res.threadActor, handleResponse);
    });
  } else if (target.isTabActor) {
    // Attaching a normal thread
    target.activeTab.attachThread(threadOptions, handleResponse);
  } else {
    // Attaching the browser debugger
    target.client.attachThread(chromeDebugger, handleResponse);
  }

  return deferred.promise;
}

function detachThread(threadClient) {
  threadClient.removeListener("paused");
  threadClient.removeListener("resumed");
}

module.exports = { attachThread, detachThread };
