/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * With e10s, plugins must run in their own process. This means we have
 * three processes at a minimum when we're running a plugin:
 *
 * 1) The main browser, or "chrome" process
 * 2) The content process hosting the plugin instance
 * 3) The plugin process
 *
 * If the plugin process crashes, we cannot be sure if the chrome process
 * will hear about it first, or the content process will hear about it
 * first. Because of how IPC works, that's really up to the operating system,
 * and we assume any guarantees about it, so we have to account for both
 * possibilities.
 *
 * This test exercises the browser's reaction to both possibilities.
 */

const CRASH_URL = "http://example.com/browser/browser/base/content/test/plugins/plugin_crashCommentAndURL.html";
const CRASHED_MESSAGE = "BrowserPlugins:NPAPIPluginProcessCrashed";

/**
 * In order for our test to work, we need to be able to put a plugin
 * in a very specific state. Specifically, we need it to match the
 * :-moz-handler-crashed pseudoselector. The only way I can find to
 * do that is by actually crashing the plugin. So we wait for the
 * plugin to crash and show the "please" state (since that will
 * only show if both the message from the parent has been received
 * AND the PluginCrashed event has fired).
 *
 * Once in that state, we try to rewind the clock a little bit - we clear
 * out the crashData cache in the PluginContent with a message, and we also
 * override the pluginFallbackState of the <object> to fool PluginContent
 * into believing that the plugin is in a particular state.
 *
 * @param browser
 *        The browser that has loaded the CRASH_URL that we need to
 *        prepare to be in the special state.
 * @param pluginFallbackState
 *        The value we should override the <object>'s pluginFallbackState
 *        with.
 * @return Promise
 *        The Promise resolves when the plugin has officially been put into
 *        the crash reporter state, and then "rewound" to have the "status"
 *        attribute of the statusDiv removed. The resolved Promise returns
 *        the run ID for the crashed plugin. It rejects if we never get into
 *        the crash reporter state.
 */
function preparePlugin(browser, pluginFallbackState) {
  return ContentTask.spawn(browser, pluginFallbackState, function* (contentPluginFallbackState) {
    let plugin = content.document.getElementById("plugin");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    // CRASH_URL will load a plugin that crashes immediately. We
    // wait until the plugin has finished being put into the crash
    // state.
    let statusDiv;
    yield ContentTaskUtils.waitForCondition(() => {
      statusDiv = plugin.ownerDocument
                        .getAnonymousElementByAttribute(plugin, "anonid",
                                                        "submitStatus");
      return statusDiv && statusDiv.getAttribute("status") == "please";
    }, "Timed out waiting for plugin to be in crash report state");

    // "Rewind", by wiping out the status attribute...
    statusDiv.removeAttribute("status");
    // Somehow, I'm able to get away with overriding the getter for
    // this XPCOM object. Probably because I've got chrome privledges.
    Object.defineProperty(plugin, "pluginFallbackType", {
      get: function() {
        return contentPluginFallbackState;
      }
    });
    return plugin.runID;
  }).then((runID) => {
    browser.messageManager.sendAsyncMessage("BrowserPlugins:Test:ClearCrashData");
    return runID;
  });
}

add_task(function* setup() {
  // Bypass click-to-play
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  // Clear out any minidumps we create from plugins - we really don't care
  // about them.
  let crashObserver = (subject, topic, data) => {
    if (topic != "plugin-crashed") {
      return;
    }

    let propBag = subject.QueryInterface(Ci.nsIPropertyBag2);
    let minidumpID = propBag.getPropertyAsAString("pluginDumpID");

    let minidumpDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    minidumpDir.append("minidumps");

    let pluginDumpFile = minidumpDir.clone();
    pluginDumpFile.append(minidumpID + ".dmp");

    let extraFile = minidumpDir.clone();
    extraFile.append(minidumpID + ".extra");

    ok(pluginDumpFile.exists(), "Found minidump");
    ok(extraFile.exists(), "Found extra file");

    pluginDumpFile.remove(false);
    extraFile.remove(false);
  };

  Services.obs.addObserver(crashObserver, "plugin-crashed", false);
  // plugins.testmode will make BrowserPlugins:Test:ClearCrashData work.
  Services.prefs.setBoolPref("plugins.testmode", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("plugins.testmode");
    Services.obs.removeObserver(crashObserver, "plugin-crashed");
  });
});

/**
 * In this case, the chrome process hears about the crash first.
 */
add_task(function* testChromeHearsPluginCrashFirst() {
  // Open a remote window so that we can run this test even if e10s is not
  // enabled by default.
  let win = yield BrowserTestUtils.openNewBrowserWindow({remote: true});
  let browser = win.gBrowser.selectedBrowser;

  browser.loadURI(CRASH_URL);
  yield BrowserTestUtils.browserLoaded(browser);

  // In this case, we want the <object> to match the -moz-handler-crashed
  // pseudoselector, but we want it to seem still active, because the
  // content process is not yet supposed to know that the plugin has
  // crashed.
  let runID = yield preparePlugin(browser,
                                  Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE);

  // Send the message down to PluginContent.jsm saying that the plugin has
  // crashed, and that we have a crash report.
  let mm = browser.messageManager;
  mm.sendAsyncMessage(CRASHED_MESSAGE,
                      { pluginName: "", runID, state: "please" });

  yield ContentTask.spawn(browser, null, function* () {
    // At this point, the content process should have heard the
    // plugin crash message from the parent, and we are OK to emit
    // the PluginCrashed event.
    let plugin = content.document.getElementById("plugin");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let statusDiv = plugin.ownerDocument
                          .getAnonymousElementByAttribute(plugin, "anonid",
                                                          "submitStatus");

    if (statusDiv.getAttribute("status") == "please") {
      Assert.ok(false, "Did not expect plugin to be in crash report mode yet.");
      return;
    }

    // Now we need the plugin to seem crashed to PluginContent.jsm, without
    // actually crashing the plugin again. We hack around this by overriding
    // the pluginFallbackType again.
    Object.defineProperty(plugin, "pluginFallbackType", {
      get: function() {
        return Ci.nsIObjectLoadingContent.PLUGIN_CRASHED;
      },
    });

    let event = new content.PluginCrashedEvent("PluginCrashed", {
      pluginName: "",
      pluginDumpID: "",
      browserDumpID: "",
      submittedCrashReport: false,
      bubbles: true,
      cancelable: true,
    });

    plugin.dispatchEvent(event);
    Assert.equal(statusDiv.getAttribute("status"), "please",
      "Should have been showing crash report UI");
  });
  yield BrowserTestUtils.closeWindow(win);
});

/**
 * In this case, the content process hears about the crash first.
 */
add_task(function* testContentHearsCrashFirst() {
  // Open a remote window so that we can run this test even if e10s is not
  // enabled by default.
  let win = yield BrowserTestUtils.openNewBrowserWindow({remote: true});
  let browser = win.gBrowser.selectedBrowser;

  browser.loadURI(CRASH_URL);
  yield BrowserTestUtils.browserLoaded(browser);

  // In this case, we want the <object> to match the -moz-handler-crashed
  // pseudoselector, and we want the plugin to seem crashed, since the
  // content process in this case has heard about the crash first.
  let runID = yield preparePlugin(browser,
                                  Ci.nsIObjectLoadingContent.PLUGIN_CRASHED);

  yield ContentTask.spawn(browser, null, function* () {
    // At this point, the content process has not yet heard from the
    // parent about the crash report. Let's ensure that by making sure
    // we're not showing the plugin crash report UI.
    let plugin = content.document.getElementById("plugin");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let statusDiv = plugin.ownerDocument
                          .getAnonymousElementByAttribute(plugin, "anonid",
                                                          "submitStatus");

    if (statusDiv.getAttribute("status") == "please") {
      Assert.ok(false, "Did not expect plugin to be in crash report mode yet.");
    }

    let event = new content.PluginCrashedEvent("PluginCrashed", {
      pluginName: "",
      pluginDumpID: "",
      browserDumpID: "",
      submittedCrashReport: false,
      bubbles: true,
      cancelable: true,
    });

    plugin.dispatchEvent(event);

    Assert.notEqual(statusDiv.getAttribute("status"), "please",
      "Should not yet be showing crash report UI");
  });

  // Now send the message down to PluginContent.jsm that the plugin has
  // crashed...
  let mm = browser.messageManager;
  mm.sendAsyncMessage(CRASHED_MESSAGE,
                      { pluginName: "", runID, state: "please"});

  yield ContentTask.spawn(browser, null, function* () {
    // At this point, the content process will have heard the message
    // from the parent and reacted to it. We should be showing the plugin
    // crash report UI now.
    let plugin = content.document.getElementById("plugin");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let statusDiv = plugin.ownerDocument
                          .getAnonymousElementByAttribute(plugin, "anonid",
                                                          "submitStatus");

    Assert.equal(statusDiv.getAttribute("status"), "please",
      "Should have been showing crash report UI");
  });

  yield BrowserTestUtils.closeWindow(win);
});
