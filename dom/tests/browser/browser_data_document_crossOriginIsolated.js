"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const PATH = DIRPATH + "file_coop_coep.html";

const ORIGIN = "https://test1.example.com";
const URL = `${ORIGIN}/${PATH}`;

add_task(async function () {
  await BrowserTestUtils.withNewTab(URL, async function (browser) {
    BrowserTestUtils.startLoadingURIString(browser, URL);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(browser, [ORIGIN], async origin => {
      is(
        content.window.origin,
        origin,
        `Opened a tab and navigated to ${origin}`
      );

      ok(
        content.window.crossOriginIsolated,
        `Should have been cross-origin-isolated env`
      );

      let hostIds = [];
      function createShadowDOMAndTriggerSlotChange(host) {
        var shadow = host.attachShadow({ mode: "closed" });

        let promise = new Promise(resolve => {
          shadow.addEventListener("slotchange", function () {
            hostIds.push(host.id);
            resolve();
          });
        });

        shadow.innerHTML = "<slot></slot>";

        host.appendChild(host.ownerDocument.createElement("span"));

        return promise;
      }

      let host1 = content.document.getElementById("host1");

      let dataDoc = content.document.implementation.createHTMLDocument();
      dataDoc.body.innerHTML = "<div id='host2'></div>";
      let host2 = dataDoc.body.firstChild;

      let host3 = content.document.getElementById("host3");

      let promises = [];
      promises.push(createShadowDOMAndTriggerSlotChange(host1));
      promises.push(createShadowDOMAndTriggerSlotChange(host2));
      promises.push(createShadowDOMAndTriggerSlotChange(host3));

      await Promise.all(promises);

      is(hostIds.length, 3, `Got 3 slot change events`);
      is(hostIds[0], "host1", `The first one was host1`);
      is(hostIds[1], "host2", `The second one was host2`);
      is(hostIds[2], "host3", `The third one was host3`);
    });
  });
});
