/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
const URL = "data:text/html;charset=utf-8,Test toggling devtools using keyboard shortcuts";

add_task(function*() {
  // Test with ACCEL+SHIFT+I / ACCEL+ALT+I (MacOSX) ; modifiers should match :
  // - toolbox-key-toggle in browser/devtools/framework/toolbox-window.xul
  // - key_devToolboxMenuItem in browser/base/content/browser.xul
  info('Test toggle using CTRL+SHIFT+I/CMD+ALT+I');
  yield testToggle('I', {
    accelKey : true,
    shiftKey : !navigator.userAgent.match(/Mac/),
    altKey : navigator.userAgent.match(/Mac/),
  });
  // Test with F12 ; no modifiers
  info('Test toggle using F12');
  yield testToggle('VK_F12', {});
});

function* testToggle(key, modifiers) {
  let tab = yield addTab(URL + " ; key : '" + key + "'");
  yield gDevTools.showToolbox(TargetFactory.forTab(tab));

  yield testToggleDockedToolbox(tab, key, modifiers);

  yield testToggleDetachedToolbox(tab, key, modifiers);

  yield cleanup();
}

function* testToggleDockedToolbox (tab, key, modifiers) {
  let toolbox = getToolboxForTab(tab);

  isnot(toolbox.hostType, devtools.Toolbox.HostType.WINDOW, "Toolbox is docked in the main window");

  info('verify docked toolbox is destroyed when using toggle key');
  let onToolboxDestroyed = once(gDevTools, "toolbox-destroyed");
  EventUtils.synthesizeKey(key, modifiers);
  yield onToolboxDestroyed;
  ok(true, "Docked toolbox is destroyed when using a toggle key");

  info('verify new toolbox is created when using toggle key');
  let onToolboxReady = once(gDevTools, "toolbox-ready");
  EventUtils.synthesizeKey(key, modifiers);
  yield onToolboxReady;
  ok(true, "Toolbox is created by using when toggle key");
}

function* testToggleDetachedToolbox (tab, key, modifiers) {
  let toolbox = getToolboxForTab(tab);

  info('change the toolbox hostType to WINDOW');
  yield toolbox.switchHost(devtools.Toolbox.HostType.WINDOW);
  is(toolbox.hostType, devtools.Toolbox.HostType.WINDOW, "Toolbox opened on separate window");

  let toolboxWindow = toolbox._host._window;
  info('Wait for focus on the toolbox window')
  yield new Promise(resolve => waitForFocus(resolve, toolboxWindow));

  info('Focus main window')
  let onMainWindowFocus = once(window, "focus");
  window.focus();
  yield onMainWindowFocus;
  ok(true, "Main window focused");

  info('verify windowed toolbox is focused when using toggle key from the main window')
  let onToolboxWindowFocus = once(toolboxWindow, "focus");
  EventUtils.synthesizeKey(key, modifiers);
  yield onToolboxWindowFocus;
  ok(true, "Toolbox focused and not destroyed");

  info('verify windowed toolbox is destroyed when using toggle key from its own window')
  let onToolboxDestroyed = once(gDevTools, "toolbox-destroyed");
  EventUtils.synthesizeKey(key, modifiers, toolboxWindow);
  yield onToolboxDestroyed;
  ok(true, "Toolbox destroyed");
}

function getToolboxForTab(tab) {
  return gDevTools.getToolbox(TargetFactory.forTab(tab));
}

function* cleanup(toolbox) {
  Services.prefs.setCharPref("devtools.toolbox.host", devtools.Toolbox.HostType.BOTTOM);
  gBrowser.removeCurrentTab();
}
