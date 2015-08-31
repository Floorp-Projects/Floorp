/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

gDirectorySource = "data:application/json," + JSON.stringify({
  "suggested": [{
    url: "http://example.com",
    imageURI: "data:image/png;base64,helloWORLD3",
    title: "title",
    type: "affiliate",
    frecent_sites: ["classroom.google.com", "codeacademy.org", "codecademy.com", "codeschool.com", "codeyear.com", "elearning.ut.ac.id", "how-to-build-websites.com", "htmlcodetutorial.com", "htmldog.com", "htmlplayground.com", "learn.jquery.com", "quackit.com", "roseindia.net", "teamtreehouse.com", "tizag.com", "tutorialspoint.com", "udacity.com", "w3schools.com", "webdevelopersnotes.com"]
  }]
});

function runTests() {
  Services.prefs.setBoolPref(PRELOAD_PREF, false);
  let origIsTopPlacesSite = NewTabUtils.isTopPlacesSite;
  NewTabUtils.isTopPlacesSite = () => true;

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PRELOAD_PREF);
    NewTabUtils.isTopPlacesSite = origIsTopPlacesSite;
  });

  function getData(cellNum) {
    let cell = getCell(cellNum);
    if (!cell.site)
      return null;
    let siteNode = cell.site.node;
    return {
      type: siteNode.getAttribute("type"),
      suggested: siteNode.querySelector(".newtab-suggested").innerHTML,
      sponsoredTag: siteNode.querySelector(".newtab-sponsored")
    };
  }

  let gBrowser = gWindow.gBrowser;

  // Trigger a suggested tile to show.
  yield setLinks("1,2,3,4,5,6,7,8");
  yield addNewTabPageTab();
  yield customizeNewTabPage("classic");
  let {type, suggested, sponsoredTag} = getData(0);
  isnot(suggested, "");
  is(type, "affiliate", "our suggested link is affiliate");
  is(gBrowser.tabs.length, 2, "We start with 2 tabs open");

  // Add a listener for new open tabs.
  let promise = new Promise((resolve, reject) => {
    let container = gBrowser.tabContainer;
    let onTabOpen = function(event) {
      container.removeEventListener("TabOpen", onTabOpen, false);
      let browser = gBrowser.getBrowserForTab(event.target);
      browser.addEventListener("load", () => {
        browser.removeEventListener("load", this, true);
        is(browser.currentURI.spec, "about:blank", "We opened 'about:blank' after clicking 'Learn more'");
        is(gBrowser.tabs.length, 3, "There are now 3 tabs open");
        is(gBrowser.getBrowserForTab(gBrowser.tabs[1]).currentURI.spec, "about:newtab", "about:newtab is still open");
        gBrowser.removeTab(gBrowser.tabs[2]); // Remove the additional opened tab.
        resolve();
      }, true);
    };
    container.addEventListener("TabOpen", onTabOpen, false);
  });

  // Clicking on 'Learn more' opens a new tab.
  sponsoredTag.click();
  let legalText = getCell(0).site.node.querySelector(".sponsored-explain");
  let learnMoreLink = legalText.getElementsByTagName("a")[0];
  learnMoreLink.href = "about:blank" // For testing purposes, don't use a real site.
  learnMoreLink.click();

  promise.then(TestRunner.next);
}
