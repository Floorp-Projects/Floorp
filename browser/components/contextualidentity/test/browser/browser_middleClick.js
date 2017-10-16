"use strict";

const BASE_ORIGIN = "http://example.com";
const URI = BASE_ORIGIN +
  "/browser/browser/components/contextualidentity/test/browser/empty_file.html";

add_task(async function() {
  info("Opening a new container tab...");

  let tab = BrowserTestUtils.addTab(gBrowser, URI, { userContextId: 1 });
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Create a HTMLAnchorElement...");
  await ContentTask.spawn(browser, URI,
    function(uri) {
      let anchor = content.document.createElement("a");
      anchor.setAttribute("id", "clickMe");
      anchor.setAttribute("href", uri);
      anchor.appendChild(content.document.createTextNode("click me!"));
      content.document.body.appendChild(anchor);
    }
  );

  info("Synthesize a mouse click and wait for a new tab...");
  let newTab = await new Promise((resolve, reject) => {
    gBrowser.tabContainer.addEventListener("TabOpen", function(openEvent) {
      resolve(openEvent.target);
    }, {once: true});

    BrowserTestUtils.synthesizeMouseAtCenter("#clickMe", { button: 1 }, browser);
  });

  is(newTab.getAttribute("usercontextid"), 1, "Correct UserContextId?");

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(newTab);
});
