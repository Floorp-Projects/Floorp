/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* ()
{
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, null, false);

  let browser = tab.linkedBrowser;
  browser.stop(); // stop the about:blank load

  let writeDomainURL = encodeURI("data:text/html,<script>document.write(document.domain);</script>");

  let tests = [
    {
      name: "view background image",
      url: "http://mochi.test:8888/",
      element: "body",
      go: function () {
        return ContentTask.spawn(gBrowser.selectedBrowser, { writeDomainURL: writeDomainURL }, function* (arg) {
          let contentBody = content.document.body;
          contentBody.style.backgroundImage = "url('" + arg.writeDomainURL + "')";

          return "context-viewbgimage";
        });
      },
      verify: function () {
        return ContentTask.spawn(gBrowser.selectedBrowser, { }, function* (arg) {
          return [content.document.body.textContent, "no domain was inherited for view background image"];
        });
      }
    },
    {
      name: "view image",
      url: "http://mochi.test:8888/",
      element: "img",
      go: function () {
        return ContentTask.spawn(gBrowser.selectedBrowser, { writeDomainURL: writeDomainURL }, function* (arg) {
          let doc = content.document;
          let img = doc.createElement("img");
          img.setAttribute("src", arg.writeDomainURL);
          doc.body.insertBefore(img, doc.body.firstChild);

          return "context-viewimage";
        });
      },
      verify: function () {
        return ContentTask.spawn(gBrowser.selectedBrowser, { }, function* (arg) {
          return [content.document.body.textContent, "no domain was inherited for view image"];
        });
      }
    },
    {
      name: "show only this frame",
      url: "http://mochi.test:8888/",
      element: "iframe",
      go: function () {
        return ContentTask.spawn(gBrowser.selectedBrowser, { writeDomainURL: writeDomainURL }, function* (arg) {
          let doc = content.document;
          let iframe = doc.createElement("iframe");
          iframe.setAttribute("src", arg.writeDomainURL);
          doc.body.insertBefore(iframe, doc.body.firstChild);

          // Wait for the iframe to load.
          return new Promise(resolve => {
            iframe.addEventListener("load", function onload() {
              iframe.removeEventListener("load", onload, true);
              resolve("context-showonlythisframe");
            }, true);
          });
        });
      },
      verify: function () {
        return ContentTask.spawn(gBrowser.selectedBrowser, { writeDomainURL: writeDomainURL }, function* (arg) {
          return [content.document.body.textContent, "no domain was inherited for 'show only this frame'"];
        });
      }
    }
  ];

  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");

  for (let test of tests) {
    let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gBrowser.loadURI(test.url);
    yield loadedPromise;

    info("Run subtest " + test.name);
    let commandToRun = yield test.go();

    let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
    yield BrowserTestUtils.synthesizeMouse(test.element, 3, 3,
          { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
    yield popupShownPromise;

    let loadedAfterCommandPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    document.getElementById(commandToRun).click();
    yield loadedAfterCommandPromise;

    let result = yield test.verify();
    ok(!result[0], result[1]);

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
    contentAreaContextMenu.hidePopup();
    yield popupHiddenPromise;
  }

  gBrowser.removeCurrentTab();
});
