const PAGE = `
<!doctype html>
<select style="width: 600px">
  <option>ABC</option>
  <option>DEFG</option>
</select>
`;

add_task(async function() {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      let popup = await openSelectPopup("click");
      let arrowSB = popup.shadowRoot.querySelector(".menupopup-arrowscrollbox");
      is(
        arrowSB.getBoundingClientRect().width,
        600,
        "Should be the right size"
      );
    }
  );
});
