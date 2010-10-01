function test() {
  waitForExplicitFinish();

  function observer(win, topic, data) {
    if (topic != "main-pane-loaded")
      return;

    Services.obs.removeObserver(observer, "main-pane-loaded");
    runTest(win);
  }
  Services.obs.addObserver(observer, "main-pane-loaded", false);

  openDialog("chrome://browser/content/preferences/preferences.xul", "Preferences",
             "chrome,titlebar,toolbar,centerscreen,dialog=no", "paneMain");
}

function runTest(win) {
  let doc = win.document;
  let pbAutoStartPref = doc.getElementById("browser.privatebrowsing.autostart");
  let startupPref = doc.getElementById("browser.startup.page");
  let menu = doc.getElementById("browserStartupPage");
  let option = doc.getElementById("browserStartupLastSession");
  let defOption = doc.getElementById("browserStartupHomePage");
  let otherOption = doc.getElementById("browserStartupBlank");

  ok(!pbAutoStartPref.value, "Sanity check");
  is(startupPref.value, startupPref.defaultValue, "Sanity check");

  // First, check to make sure that setting pbAutoStartPref disables the menu item
  pbAutoStartPref.value = true;
  is(option.getAttribute("disabled"), "true", "Setting private browsing to autostart " +
     "should disable the 'Show my tabs and windows from last time' option");
  pbAutoStartPref.value = false;

  // Now ensure the correct behavior when pbAutoStartPref is set with option enabled
  startupPref.value = option.getAttribute("value");
  is(menu.selectedItem, option, "Sanity check");
  pbAutoStartPref.value = true;
  is(option.getAttribute("disabled"), "true", "Setting private browsing to autostart " +
     "should disable the 'Show my tabs and windows from last time' option");
  is(menu.selectedItem, defOption, "The 'Show home page' option should be selected");
  is(startupPref.value, option.getAttribute("value"), "But the value of the startup " +
     "pref itself shouldn't change");
  menu.selectedItem = otherOption;
  menu.doCommand();
  is(startupPref.value, otherOption.getAttribute("value"), "And we should be able to " +
     "chnage it!");
  pbAutoStartPref.value = false;

  // Now, ensure that with 'Show my windows and tabs from last time' enabled, toggling
  // pbAutoStartPref would restore that value in the menulist.
  startupPref.value = option.getAttribute("value");
  is(menu.selectedItem, option, "Sanity check");
  pbAutoStartPref.value = true;
  is(menu.selectedItem, defOption, "The 'Show home page' option should be selected");
  pbAutoStartPref.value = false;
  is(menu.selectedItem, option, "The correct value should be restored");

  // cleanup
  [pbAutoStartPref, startupPref].forEach(function (pref) {
    if (pref.hasUserValue)
      pref.reset();
  });

  win.close();
  finish();
}
