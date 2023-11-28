add_task(async function test_hr() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.select.customstyling", true]],
  });

  const PAGE_CONTENT = `
<!doctype html>
<select>
<option>One</option>
<hr style="color: red; background-color: blue">
<option>Two</option>
</select>`;

  const pageUrl = "data:text/html," + encodeURIComponent(PAGE_CONTENT);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  const selectPopup = await openSelectPopup("click");
  const menulist = selectPopup.parentNode;

  const optionOne = selectPopup.children[0];
  const separator = selectPopup.children[1];
  const optionTwo = selectPopup.children[2];

  is(optionOne.textContent, "One", "First option has expected text content");

  is(separator.tagName, "menuseparator", "Separator is menuseparator");

  const separatorStyle = getComputedStyle(separator);

  is(
    separatorStyle.color,
    "rgb(255, 0, 0)",
    "Separator color is specified CSS color"
  );

  is(
    separatorStyle.backgroundColor,
    "rgba(0, 0, 0, 0)",
    "Separator background-color is not set to specified CSS color"
  );

  is(optionTwo.textContent, "Two", "Second option has expected text content");

  is(menulist.activeChild, optionOne, "First option is selected to start");

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(
    menulist.activeChild,
    optionTwo,
    "Second option is selected after arrow down"
  );

  BrowserTestUtils.removeTab(tab);
});
