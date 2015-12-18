/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var toolbox;

add_task(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target);
  let doc = toolbox.frame.contentDocument;
  let root = doc.documentElement;

  let platform = root.getAttribute("platform");
  let expectedPlatform = getPlatform();
  is(platform, expectedPlatform, ":root[platform] is correct");

  let theme = Services.prefs.getCharPref("devtools.theme");
  let className = "theme-" + theme;
  ok(root.classList.contains(className),
     ":root has " + className + " class (current theme)");

  // Convert the xpath result into an array of strings
  // like `href="{URL}" type="text/css"`
  let sheetsIterator = doc.evaluate("processing-instruction('xml-stylesheet')",
                       doc, null, XPathResult.ANY_TYPE, null);
  let sheetsInDOM = [];
  let sheet;
  while (sheet = sheetsIterator.iterateNext()) {
    sheetsInDOM.push(sheet.data);
  }

  let sheetsFromTheme = gDevTools.getThemeDefinition(theme).stylesheets;
  info ("Checking for existence of " + sheetsInDOM.length + " sheets");
  for (let sheet of sheetsFromTheme) {
    ok(sheetsInDOM.some(s=>s.includes(sheet)), "There is a stylesheet for " + sheet);
  }

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
