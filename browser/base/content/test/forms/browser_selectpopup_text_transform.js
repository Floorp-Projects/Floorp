const PAGE = `
<!doctype html>
<select style="text-transform: uppercase">
  <option>abc</option>
  <option>defg</option>
</select>
`;

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.select.customstyling", true]],
  });
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let popup = await openSelectPopup("click");
      let menuitems = popup.querySelectorAll("menuitem");
      is(menuitems[0].textContent, "abc", "Option text should be lowercase");
      is(menuitems[1].textContent, "defg", "Option text should be lowercase");

      let optionStyle = getComputedStyle(menuitems[0]);
      is(
        optionStyle.textTransform,
        "uppercase",
        "Option text should be transformed to uppercase"
      );

      optionStyle = getComputedStyle(menuitems[1]);
      is(
        optionStyle.textTransform,
        "uppercase",
        "Option text should be transformed to uppercase"
      );
    }
  );
});
