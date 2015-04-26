/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let testData = [
  /* baseURI, field name, expected */
  [ 'http://example.com/', 'q', 'http://example.com/?q=%s' ],
  [ 'http://example.com/new-path-here/', 'q', 'http://example.com/new-path-here/?q=%s' ],
  [ '', 'q', 'http://example.org/browser/browser/base/content/test/general/dummy_page.html?q=%s' ],
  // Tests for proper behaviour when called on a form whose action contains a question mark.
  [ 'http://example.com/search?oe=utf-8', 'q', 'http://example.com/search?oe=utf-8&q=%s' ],
];

let mm = gBrowser.selectedBrowser.messageManager;

add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.org/browser/browser/base/content/test/general/dummy_page.html",
  }, function* (browser) {
    yield ContentTask.spawn(browser, null, function* () {
      let doc = content.document;
      let base = doc.createElement("base");
      doc.head.appendChild(base);
    });

    for (let [baseURI, fieldName, expected] of testData) {
      let popupShownPromise = BrowserTestUtils.waitForEvent(document.getElementById("contentAreaContextMenu"),
                                                            "popupshown");

      yield ContentTask.spawn(browser, { baseURI, fieldName }, function* (args) {
        let doc = content.document;

        let base = doc.querySelector('head > base');
        base.href = args.baseURI;

        let form = doc.createElement("form");
        form.id = "keyword-form";
        let element = doc.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("name", args.fieldName);
        form.appendChild(element);
        doc.body.appendChild(form);

        /* Open context menu so chrome can access the element */
        const domWindowUtils =
          content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                 .getInterface(Components.interfaces.nsIDOMWindowUtils);
        let rect = element.getBoundingClientRect();
        let left = rect.left + rect.width / 2;
        let top = rect.top + rect.height / 2;
        domWindowUtils.sendMouseEvent("contextmenu", left, top, 2,
                                      1, 0, false, 0, 0, true);
      });

      yield popupShownPromise;

      let target = gContextMenuContentData.popupNode;

      let urlCheck = new Promise((resolve, reject) => {
        let onMessage = (message) => {
          mm.removeMessageListener("ContextMenu:SearchFieldBookmarkData:Result", onMessage);

          is(message.data.spec, expected,
             `Bookmark spec for search field named ${fieldName} and baseURI ${baseURI} incorrect`);
          resolve();
        };
        mm.addMessageListener("ContextMenu:SearchFieldBookmarkData:Result", onMessage);

        mm.sendAsyncMessage("ContextMenu:SearchFieldBookmarkData", null, { target });
      });

      yield urlCheck;

      document.getElementById("contentAreaContextMenu").hidePopup();

      yield ContentTask.spawn(browser, null, function* () {
        let doc = content.document;
        doc.body.removeChild(doc.getElementById("keyword-form"));
      });
    }
  });
});
