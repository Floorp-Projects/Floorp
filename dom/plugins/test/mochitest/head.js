
/**
 * Waits for a tab switch.
 */
function waitTabSwitched() {
  return new Promise(resolve => {
    gBrowser.addEventListener("TabSwitchDone", function onSwitch() {
      gBrowser.removeEventListener("TabSwitchDone", onSwitch);
      executeSoon(resolve);
    });
  });
}

/**
 * Waits a specified number of miliseconds.
 *
 * Usage:
 *    let wait = yield waitForMs(2000);
 *    ok(wait, "2 seconds should now have elapsed");
 *
 * @param aMs the number of miliseconds to wait for
 * @returns a Promise that resolves to true after the time has elapsed
 */
function waitForMs(aMs) {
  return new Promise((resolve) => {
    setTimeout(done, aMs);
    function done() {
      resolve(true);
    }
  });
}

/**
 * Platform string helper for nativeVerticalWheelEventMsg
 */
function getPlatform() {
  if (navigator.platform.indexOf("Win") == 0) {
    return "windows";
  }
  if (navigator.platform.indexOf("Mac") == 0) {
    return "mac";
  }
  if (navigator.platform.indexOf("Linux") == 0) {
    return "linux";
  }
  return "unknown";
}

/**
 * Returns a native wheel scroll event id for dom window
 * uitls sendNativeMouseScrollEvent.
 */
function nativeVerticalWheelEventMsg() {
  switch (getPlatform()) {
    case "windows": return 0x020A; // WM_MOUSEWHEEL
    case "mac": return 0; // value is unused, can be anything
    case "linux": return 4; // value is unused, pass GDK_SCROLL_SMOOTH anyway
  }
  throw "Native wheel events not supported on platform " + getPlatform();
}

/**
 * Waits for the first dom "scroll" event.
 */
function waitScrollStart(aTarget) {
  return new Promise((resolve, reject) => {
    aTarget.addEventListener("scroll", function listener(event) {
      aTarget.removeEventListener("scroll", listener, true);
      resolve(event);
    }, true);
  });
}

/**
 * Waits for the last dom "scroll" event which generally indicates
 * a scroll operation is complete. To detect this the helper waits
 * 1 second intervals checking for scroll events from aTarget. If
 * a scroll event is not received during that time, it considers
 * the scroll operation complete. Not super accurate, be careful.
 */
function waitScrollFinish(aTarget) {
  return new Promise((resolve, reject) => {
    let recent = false;
    let count = 0;
    function listener(event) {
      recent = true;
    }
    aTarget.addEventListener("scroll", listener, true);
    setInterval(function () {
      // one second passed and we didn't receive a scroll event.
      if (!recent) {
        aTarget.removeEventListener("scroll", listener, true);
        resolve();
        return;
      }
      recent = false;
      // ten seconds
      if (count > 10) {
        aTarget.removeEventListener("scroll", listener, true);
        reject();
      }
    }, 1000);
  });
}

/**
 * Set a plugin activation state. See nsIPluginTag for
 * supported states. Affected plugin default to the first
 * test plugin.
 */
function setTestPluginEnabledState(aState, aPluginName) {
  let name = aPluginName || "Test Plug-in";
  SpecialPowers.setTestPluginEnabledState(aState, name);
}

/**
 * Returns the chrome side nsIPluginTag for this plugin, helper for
 * setTestPluginEnabledState.
 */
function getTestPlugin(aName) {
  let pluginName = aName || "Test Plug-in";
  let ph = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
  let tags = ph.getPluginTags();

  // Find the test plugin
  for (let i = 0; i < tags.length; i++) {
    if (tags[i].name == pluginName)
      return tags[i];
  }
  ok(false, "Unable to find plugin");
  return null;
}

