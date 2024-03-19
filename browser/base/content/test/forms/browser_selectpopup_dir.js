const PAGE = `
<!doctype html>
<select style="direction: rtl">
  <option>ABC</option>
  <option>DEFG</option>
</select>
`;

add_task(async function () {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function () {
      let popup = await openSelectPopup("click");
      is(popup.style.direction, "rtl", "Should be the right dir");
    }
  );
});
