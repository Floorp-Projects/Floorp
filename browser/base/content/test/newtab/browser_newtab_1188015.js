/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

gDirectorySource = "data:application/json," + JSON.stringify({
  "directory": [{
    url: "http://example1.com/",
    enhancedImageURI: "data:image/png;base64,helloWORLD2",
    title: "title1",
    type: "affiliate",
    titleBgColor: "green"
  }]
});

function runTests() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PRELOAD_PREF);
  });

  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  // Make the page have a directory link
  yield setLinks([]);
  yield addNewTabPageTab();
  let titleNode = getCell(0).node.querySelector(".newtab-title");
  is(titleNode.style.backgroundColor, "green", "title bg color is green");
}
