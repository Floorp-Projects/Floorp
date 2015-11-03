/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var toolbox;

add_task(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target);
  let root = toolbox.frame.contentDocument.documentElement;

  let platform = root.getAttribute("platform");
  let expectedPlatform = getPlatform();
  is(platform, expectedPlatform, ":root[platform] is correct");

  let theme = Services.prefs.getCharPref("devtools.theme");
  let className = "theme-" + theme;
  ok(root.classList.contains(className), ":root has " + className + " class (current theme)");

  yield toolbox.destroy();
});

function getPlatform() {
  let {OS} = Services.appinfo;
  if (OS == "WINNT") {
    return "win";
  } else if (OS == "Darwin") {
    return "mac";
  } else {
    return "linux";
  }
}
