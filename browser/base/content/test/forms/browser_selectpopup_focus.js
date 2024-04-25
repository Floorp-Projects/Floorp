// Empty select to make sure that we click on the menulist button.
const PAGE = `
<!doctype html>
<select style="padding: 0">
  <option></option>
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
      await openSelectPopup("click");
      await SpecialPowers.spawn(browser, [], () => {
        is(
          content.document.activeElement,
          content.document.querySelector("select"),
          "Select is the active element"
        );
        ok(
          content.document.querySelector("select").matches(":focus"),
          "Select matches :focus"
        );
      });
      await hideSelectPopup();
    }
  );
});
