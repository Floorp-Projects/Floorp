/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT_SSL + "chunked_json.sjs";

add_task(async function() {
  info("Test chunked JSON started");

  await addJsonViewTab(TEST_JSON_URL, {
    appReadyState: "interactive",
    docReadyState: "loading",
  });

  is(
    await getElementCount(".rawdata.is-active"),
    1,
    "The Raw Data tab is selected."
  );

  // Write some text and check that it is displayed.
  await write("[");
  await checkText();

  // Repeat just in case.
  await write("1,");
  await checkText();

  is(
    await getElementCount("button.prettyprint"),
    0,
    "There is no pretty print button during load"
  );

  await selectJsonViewContentTab("json");
  is(
    await getElementText(".jsonPanelBox > .panelContent"),
    "",
    "There is no JSON tree"
  );

  await selectJsonViewContentTab("headers");
  ok(
    await getElementText(".headersPanelBox .netInfoHeadersTable"),
    "The headers table has been filled."
  );

  // Write some text without being in Raw Data, then switch tab and check.
  await write("2");
  await selectJsonViewContentTab("rawdata");
  await checkText();

  // Add an array, when counting rows we will ensure it has been expanded automatically.
  await write(",[3]]");
  await checkText();

  // Close the connection.

  // When the ready state of the JSON View app changes, it triggers the
  // custom event "AppReadyStateChange".
  const appReady = BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "AppReadyStateChange",
    true,
    null,
    true
  );
  await server("close");
  await appReady;

  is(await getElementCount(".json.is-active"), 1, "The JSON tab is selected.");

  is(
    await getElementCount(".jsonPanelBox .treeTable .treeRow"),
    4,
    "There is a tree with 4 rows."
  );

  await selectJsonViewContentTab("rawdata");
  await checkText();

  is(
    await getElementCount("button.prettyprint"),
    1,
    "There is a pretty print button."
  );
  await clickJsonNode("button.prettyprint");
  await checkText(JSON.stringify(JSON.parse(data), null, 2));
});

let data = " ";
async function write(text) {
  data += text;
  const onJsonViewUpdated = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return new Promise(resolve => {
        const observer = new content.MutationObserver(() => resolve());
        observer.observe(content.wrappedJSObject.JSONView.json, {
          characterData: true,
        });
      });
    }
  );
  await server("write", text);
  await onJsonViewUpdated;
}
async function checkText(text = data) {
  is(await getElementText(".textPanelBox .data"), text, "Got the right text.");
}

function server(action, value) {
  return new Promise(resolve => {
    const xhr = new XMLHttpRequest();
    xhr.open("GET", TEST_JSON_URL + "?" + action + "=" + value);
    xhr.addEventListener("load", resolve, { once: true });
    xhr.send();
  });
}
