const PAGE = `
<!doctype html>
<select style="width: 600px">
  <option>ABC</option>
  <option>DEFG</option>
</select>
`;

function tick() {
  return new Promise(r =>
    requestAnimationFrame(() => requestAnimationFrame(r))
  );
}

add_task(async function () {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let popup = await openSelectPopup("click");
      let arrowSB = popup.shadowRoot.querySelector(".menupopup-arrowscrollbox");
      is(
        arrowSB.getBoundingClientRect().width,
        600,
        "Should be the right size"
      );

      // Trigger a layout change that would cause us to layout the popup again,
      // and change our menulist to be zero-size so that the anchor rect
      // codepath is used. We should still use the anchor rect to expand our
      // size.
      await tick();

      popup.closest("menulist").style.width = "0";
      popup.style.minWidth = "2px";

      await tick();

      is(
        arrowSB.getBoundingClientRect().width,
        600,
        "Should be the right size"
      );
    }
  );
});
