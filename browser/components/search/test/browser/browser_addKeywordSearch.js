var testData = [
  { desc: "No path", action: "http://example.com/", param: "q" },
  {
    desc: "With path",
    action: "http://example.com/new-path-here/",
    param: "q",
  },
  { desc: "No action", action: "", param: "q" },
  {
    desc: "With Query String",
    action: "http://example.com/search?oe=utf-8",
    param: "q",
  },
];

add_task(async function() {
  const TEST_URL =
    "http://example.org/browser/browser/components/search/test/browser/test.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let count = 0;
  for (let method of ["GET", "POST"]) {
    for (let { desc, action, param } of testData) {
      info(`Running ${method} keyword test '${desc}'`);
      let id = `keyword-form-${count++}`;
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let contextMenuPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );

      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [{ action, param, method, id }],
        async function(args) {
          let doc = content.document;
          let form = doc.createElement("form");
          form.id = args.id;
          form.method = args.method;
          form.action = args.action;
          let element = doc.createElement("input");
          element.setAttribute("type", "text");
          element.setAttribute("name", args.param);
          form.appendChild(element);
          doc.body.appendChild(form);
        }
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        `#${id} > input`,
        { type: "contextmenu", button: 2 },
        tab.linkedBrowser
      );
      await contextMenuPromise;
      let url = action || tab.linkedBrowser.currentURI.spec;
      let actor = gContextMenu.actor;

      let data = await actor.getSearchFieldBookmarkData(
        gContextMenu.targetIdentifier
      );
      if (method == "GET") {
        ok(
          data.spec.endsWith(`${param}=%s`),
          `Check expected url for field named ${param} and action ${action}`
        );
      } else {
        is(
          data.spec,
          url,
          `Check expected url for field named ${param} and action ${action}`
        );
        is(
          data.postData,
          `${param}%3D%25s`,
          `Check expected POST data for field named ${param} and action ${action}`
        );
      }

      let popupHiddenPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      contextMenu.hidePopup();
      await popupHiddenPromise;
    }
  }

  BrowserTestUtils.removeTab(tab);
});
