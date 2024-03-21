/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// NOTE that this test expects "font.minimum-size.x-western=9" to be set
// in the manifest.

const PAGE = `
<!doctype html>
<body lang="en-US">
<select>
  <option style="font-size:24px">A</option>
  <option style="font-size:6px">BCD</option>
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
      let menuitems = popup.querySelectorAll("menuitem");
      is(
        getComputedStyle(menuitems[0]).fontSize,
        "24px",
        "font-size should be honored"
      );
      is(
        getComputedStyle(menuitems[1]).fontSize,
        "9px",
        "minimum font-size should be honored"
      );
    }
  );
});
