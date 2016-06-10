"use strict";

const BASE_ORIGIN = "http://example.com";
const URI = BASE_ORIGIN +
  "/browser/browser/components/contextualidentity/test/browser/empty_file.html";

add_task(function* () {
  info("Opening a new container tab...");

  let tab = gBrowser.addTab(URI, { userContextId: 1 });
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);

  info("Create a HTMLAnchorElement...");
  let position = yield ContentTask.spawn(browser, URI,
    function(URI) {
      let anchor = content.document.createElement("a");
      anchor.setAttribute('id', 'clickMe');
      anchor.setAttribute("href", URI);
      anchor.appendChild(content.document.createTextNode("click me!"));
      content.document.body.appendChild(anchor);
    }
  );

  info("Synthesize a mouse click and wait for a new tab...");
  let newTab = yield new Promise((resolve, reject) => {
    gBrowser.tabContainer.addEventListener("TabOpen", function onTabOpen(openEvent) {
      gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);
      resolve(openEvent.target);
    })

    BrowserTestUtils.synthesizeMouseAtCenter("#clickMe", { button: 1 }, browser);
  });

  is(newTab.getAttribute("usercontextid"), 1, "Correct UserContextId?");

  yield BrowserTestUtils.removeTab(tab);
  yield BrowserTestUtils.removeTab(newTab);
});
