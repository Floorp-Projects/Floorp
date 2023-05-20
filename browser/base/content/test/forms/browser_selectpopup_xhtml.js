const PAGE = `<?xml version="1.0"?>
<html id="main-window"
        xmlns:html="http://www.w3.org/1999/xhtml"
        xmlns="http://www.w3.org/1999/xhtml">
<head/>
<body>
  <html:select>
    <html:option>abc</html:option>
    <html:optgroup>
      <html:option>defg</html:option>
    </html:optgroup>
  </html:select>
</body>
</html>
`;

add_task(async function () {
  const url = "data:application/xhtml+xml," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let popup = await openSelectPopup("click");
      let menuitems = popup.querySelectorAll("menuitem");
      is(menuitems.length, 2, "Should've properly detected two menu items");
      is(menuitems[0].textContent, "abc", "Option text should be correct");
      is(menuitems[1].textContent, "defg", "Second text should be correct");
      ok(
        !!popup.querySelector("menucaption"),
        "Should've created a caption for the optgroup"
      );
    }
  );
});
