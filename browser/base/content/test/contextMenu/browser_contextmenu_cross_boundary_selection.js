/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE = `
  data:text/html,
  <div>OuterText<div>
  <div id="host">
    <template shadowrootmode="open">
      <span id="innerText">InnerText</span>
    </template>
  </div>
  `;
/**
 * Tests that right click on a cross boundary selection shows the context menu
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.shadowdom.selection_across_boundary.enabled", true]],
  });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async function (browser) {
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let awaitPopupShown = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );

      let awaitPopupHidden = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );

      await SpecialPowers.spawn(browser, [], () => {
        let host = content.document.getElementById("host");
        content.getSelection().setBaseAndExtent(
          content.document.body,
          0,
          host.shadowRoot.getElementById("innerText").firstChild,
          3 // Only select the first three characters of the inner text
        );
      });

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "div", // Click on the div for OuterText
        {
          type: "contextmenu",
          button: 2,
        },
        browser
      );

      await awaitPopupShown;

      const allVisibleMenuItems = Array.from(contextMenu.children)
        .filter(elem => {
          return !elem.hidden;
        })
        .map(elem => elem.id);

      ok(
        allVisibleMenuItems.includes("context-copy"),
        "copy button should exist"
      );

      contextMenu.hidePopup();
      await awaitPopupHidden;
    }
  );
});
