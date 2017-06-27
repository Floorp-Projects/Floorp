/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ROWS = "browser.newtabpage.rows";
const PREF_NEWTAB_COLUMNS = "browser.newtabpage.columns";

function getCellsCount() {
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    return content.gGrid.cells.length;
  });
}

add_task(async function() {
  let testValues = [
    {row: 0, column: 0},
    {row: -1, column: -1},
    {row: -1, column: 0},
    {row: 0, column: -1},
    {row: 2, column: 4},
    {row: 2, column: 5},
  ];

  // Expected length of grid
  let expectedValues = [1, 1, 1, 1, 8, 10];

   // Values before setting new pref values (12 is the default value -> 6 x 2)
  let previousValues = [12, 1, 1, 1, 1, 8];

  await addNewTabPageTab();
  let existingTab = gBrowser.selectedTab;

  for (let i = 0; i < expectedValues.length; i++) {
    let existingTabGridLength = await getCellsCount();
    is(existingTabGridLength, previousValues[i],
      "Grid length of existing page before update is correctly.");

    await pushPrefs([PREF_NEWTAB_ROWS, testValues[i].row]);
    await pushPrefs([PREF_NEWTAB_COLUMNS, testValues[i].column]);

    existingTabGridLength = await getCellsCount();
    is(existingTabGridLength, expectedValues[i],
      "Existing page grid is updated correctly.");

    await addNewTabPageTab();
    let newTab = gBrowser.selectedTab;
    let newTabGridLength = await getCellsCount();
    is(newTabGridLength, expectedValues[i],
      "New page grid is updated correctly.");

    await BrowserTestUtils.removeTab(newTab);
  }

  gBrowser.removeTab(existingTab);
});

