var testData = [
  { desc: "No path", action: "https://example.com/", param: "q" },
  {
    desc: "With path",
    action: "https://example.com/new-path-here/",
    param: "q",
  },
  { desc: "No action", action: "", param: "q" },
  {
    desc: "With Query String",
    action: "https://example.com/search?oe=utf-8",
    param: "q",
  },
  {
    desc: "With Unicode Query String",
    action: "https://example.com/searching",
    param: "q",
    testHiddenUnicode: true,
  },
];

add_task(async function () {
  const TEST_URL =
    "https://example.org/browser/browser/components/search/test/browser/test.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let count = 0;
  for (let method of ["GET", "POST"]) {
    for (let { desc, action, param, testHiddenUnicode = false } of testData) {
      info(`Running ${method} keyword test '${desc}'`);
      let id = `keyword-form-${count++}`;
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let contextMenuPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );

      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [{ action, param, method, id, testHiddenUnicode }],
        async function (args) {
          let doc = content.document;
          let form = doc.createElement("form");
          form.id = args.id;
          form.method = args.method;
          form.action = args.action;
          let element = doc.createElement("input");
          element.setAttribute("type", "text");
          element.setAttribute("name", args.param);
          form.appendChild(element);
          if (args.testHiddenUnicode) {
            form.insertAdjacentHTML(
              "beforeend",
              `<input name="utf8✓" type="hidden" value="✓">`
            );
          }
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
        if (testHiddenUnicode) {
          ok(
            data.spec.includes(`utf8%E2%9C%93=%E2%9C%93`),
            `Check the unicode param is correctly encoded`
          );
        }
      } else {
        is(
          data.spec,
          url,
          `Check expected url for field named ${param} and action ${action}`
        );
        if (testHiddenUnicode) {
          is(
            data.postData,
            `utf8%u2713%3D%u2713&q%3D%25s`,
            `Check expected POST data for field named ${param} and action ${action}`
          );
        } else {
          is(
            data.postData,
            `${param}%3D%25s`,
            `Check expected POST data for field named ${param} and action ${action}`
          );
        }
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
