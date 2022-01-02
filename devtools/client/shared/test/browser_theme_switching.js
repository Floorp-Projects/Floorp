/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  // For some reason, mochitest spawn a very special default tab,
  // whose WindowGlobal is still the initial about:blank document.
  // This seems to be specific to mochitest, this doesn't reproduce
  // in regular firefox run. Even having about:blank as home page,
  // force loading another final about:blank document (which isn't the initial one)
  //
  // To workaround this, force opening a dedicated test tab
  const tab = await addTab("data:text/html;charset=utf-8,Test page");

  const toolbox = await gDevTools.showToolboxForTab(tab);
  const doc = toolbox.doc;
  const root = doc.documentElement;

  const platform = root.getAttribute("platform");
  const expectedPlatform = getPlatform();
  is(platform, expectedPlatform, ":root[platform] is correct");

  const theme = Services.prefs.getCharPref("devtools.theme");
  const className = "theme-" + theme;
  ok(
    root.classList.contains(className),
    ":root has " + className + " class (current theme)"
  );

  // Convert the xpath result into an array of strings
  // like `href="{URL}" type="text/css"`
  const sheetsIterator = doc.evaluate(
    "processing-instruction('xml-stylesheet')",
    doc,
    null,
    XPathResult.ANY_TYPE,
    null
  );
  const sheetsInDOM = [];

  /* eslint-disable no-cond-assign */
  let sheet;
  while ((sheet = sheetsIterator.iterateNext())) {
    sheetsInDOM.push(sheet.data);
  }
  /* eslint-enable no-cond-assign */

  const sheetsFromTheme = gDevTools.getThemeDefinition(theme).stylesheets;
  info("Checking for existence of " + sheetsInDOM.length + " sheets");
  for (const themeSheet of sheetsFromTheme) {
    ok(
      sheetsInDOM.some(s => s.includes(themeSheet)),
      "There is a stylesheet for " + themeSheet
    );
  }

  await toolbox.destroy();
});

function getPlatform() {
  const { OS } = Services.appinfo;
  if (OS == "WINNT") {
    return "win";
  } else if (OS == "Darwin") {
    return "mac";
  }
  return "linux";
}
