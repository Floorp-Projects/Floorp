/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "chunked_json.sjs";

add_task(async function () {
  info("Test chunked JSON started");

  await addJsonViewTab(TEST_JSON_URL, {
    appReadyState: "interactive",
    docReadyState: "loading",
  });

  is(await getElementCount(".rawdata.is-active"), 1,
    "The Raw Data tab is selected.");

  // Write some text and check that it is displayed.
  await write("[");
  await checkText();

  // Repeat just in case.
  await write("1,");
  await checkText();

  is(await getElementCount("button.prettyprint"), 0,
    "There is no pretty print button during load");

  await selectJsonViewContentTab("json");
  is(await getElementText(".jsonPanelBox > .panelContent"), "", "There is no JSON tree");

  await selectJsonViewContentTab("headers");
  ok(await getElementText(".headersPanelBox .netInfoHeadersTable"),
    "The headers table has been filled.");

  // Write some text without being in Raw Data, then switch tab and check.
  await write("2");
  await selectJsonViewContentTab("rawdata");
  await checkText();

  // Another text check.
  await write("]");
  await checkText();

  // Close the connection.
  let appReady = waitForContentMessage("Test:JsonView:AppReadyStateChange");
  await server("close");
  await appReady;

  is(await getElementCount(".json.is-active"), 1, "The JSON tab is selected.");

  is(await getElementCount(".jsonPanelBox .treeTable .treeRow"), 2,
    "There is a tree with 2 rows.");

  await selectJsonViewContentTab("rawdata");
  await checkText();

  is(await getElementCount("button.prettyprint"), 1, "There is a pretty print button.");
  await clickJsonNode("button.prettyprint");
  await checkText(JSON.stringify(JSON.parse(data), null, 2));
});

let data = " ";
async function write(text) {
  data += text;
  let dataReceived = waitForContentMessage("Test:JsonView:NewDataReceived");
  await server("write", text);
  await dataReceived;
}
async function checkText(text = data) {
  is(await getElementText(".textPanelBox .data"), text, "Got the right text.");
}

function server(action, value) {
  return new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", TEST_JSON_URL + "?" + action + "=" + value);
    xhr.addEventListener("load", resolve, {once: true});
    xhr.send();
  });
}
