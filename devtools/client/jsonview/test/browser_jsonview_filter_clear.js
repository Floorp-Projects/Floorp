/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "array_json.json";
const filterClearButton = "button.devtools-searchinput-clear";

function clickAndWaitForFilterClear(selector) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [selector], sel => {
    content.document.querySelector(sel).click();

    const rows = Array.from(
      content.document.querySelectorAll(".jsonPanelBox .treeTable .treeRow")
    );

    // If there are no hiddens rows
    if (!rows.some(r => r.classList.contains("hidden"))) {
      info("The view has been cleared, no need for mutationObserver");
      return Promise.resolve();
    }

    return new Promise(resolve => {
      // Wait until we have 0 hidden rows
      const observer = new content.MutationObserver(function (mutations) {
        info("New mutation ! ");
        info(`${mutations}`);
        for (let i = 0; i < mutations.length; i++) {
          const mutation = mutations[i];
          info(`type: ${mutation.type}`);
          info(`attributeName: ${mutation.attributeName}`);
          info(`attributeNamespace: ${mutation.attributeNamespace}`);
          info(`oldValue: ${mutation.oldValue}`);
          if (mutation.attributeName == "class") {
            if (!rows.some(r => r.classList.contains("hidden"))) {
              info("resolving mutationObserver");
              observer.disconnect();
              resolve();
              break;
            }
          }
        }
      });

      const parent = content.document.querySelector("tbody");
      observer.observe(parent, { attributes: true, subtree: true });
    });
  });
}

add_task(async function () {
  info("Test filter input is cleared when pressing the clear button");

  await addJsonViewTab(TEST_JSON_URL);

  // Type "honza" in the filter input
  const count = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 6, "There must be expected number of rows");
  await sendString("honza", ".jsonPanelBox .searchBox");
  await waitForFilter();

  const filterInputValue = await getFilterInputValue();
  is(filterInputValue, "honza", "Filter input shoud be filled");

  // Check the json is filtered
  const hiddenCount = await getElementCount(
    ".jsonPanelBox .treeTable .treeRow.hidden"
  );
  is(hiddenCount, 4, "There must be expected number of hidden rows");

  info("Click on the close button");
  await clickAndWaitForFilterClear(filterClearButton);

  // Check the json is not filtered and the filter input is empty
  const newfilterInputValue = await getFilterInputValue();
  is(newfilterInputValue, "", "Filter input should be empty");
  const newCount = await getElementCount(
    ".jsonPanelBox .treeTable .treeRow.hidden"
  );
  is(newCount, 0, "There must be expected number of rows");
});

function getFilterInputValue() {
  return getElementAttr(".devtools-filterinput", "value");
}
