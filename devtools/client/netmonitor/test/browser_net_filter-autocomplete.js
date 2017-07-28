/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test autocomplete based on filtering flags and requests
 */
const REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" +
         "&cookies=1" },
  { url: "sjs_content-type-test-server.sjs?fmt=css&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=js&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=font" },
  { url: "sjs_content-type-test-server.sjs?fmt=image" },
  { url: "sjs_content-type-test-server.sjs?fmt=audio" },
  { url: "sjs_content-type-test-server.sjs?fmt=video" },
  { url: "sjs_content-type-test-server.sjs?fmt=gzip" },
  { url: "sjs_status-codes-test-server.sjs?sts=304" },
];

function testAutocompleteContents(expected, document) {
  expected.forEach(function (item, i) {
    is(
      document
        .querySelector(
          `.devtools-autocomplete-listbox .autocomplete-item:nth-child(${i + 1})`
        )
        .textContent,
      item,
      `${expected[i]} found`
    );
  });
}

add_task(async function () {
  let { monitor } = await initNetMonitor(FILTERING_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");

  // Let the requests load completely before the autocomplete tests begin
  // as autocomplete values also rely on the network requests.
  let waitNetwork = waitForNetworkEvents(monitor, REQUESTS.length);
  loadCommonFrameScript();
  await performRequestsInContent(REQUESTS);
  await waitNetwork;

  EventUtils.synthesizeMouseAtCenter(
    document.querySelector(".devtools-filterinput"), {}, window);
  // Empty Mouse click should keep autocomplete hidden
  ok(!document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup still hidden");

  document.querySelector(".devtools-filterinput").focus();
  // Typing a char should invoke a autocomplete
  EventUtils.synthesizeKey("s", {});
  ok(document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup Created");
  testAutocompleteContents([
    "scheme:",
    "set-cookie-domain:",
    "set-cookie-name:",
    "set-cookie-value:",
    "size:",
    "status-code:",
  ], document);

  EventUtils.synthesizeKey("c", {});
  testAutocompleteContents(["scheme:"], document);
  EventUtils.synthesizeKey("VK_TAB", {});
  // Tab selection should hide autocomplete
  ok(document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup alive with content values");
  testAutocompleteContents(["scheme:http"], document);

  EventUtils.synthesizeKey("VK_RETURN", {});
  is(document.querySelector(".devtools-filterinput").value,
    "scheme:http", "Value correctly set after Enter");
  ok(!document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup hidden after keyboard Enter key");

  // Space separated tokens
  // The last token where autocomplete is availabe shall generate the popup
  EventUtils.synthesizeKey(" p", {});
  testAutocompleteContents(["protocol:"], document);

  // The new value of the text box should be previousTokens + latest value selected
  // First return selects "protocol:"
  EventUtils.synthesizeKey("VK_RETURN", {});
  // Second return selects "protocol:HTTP/1.1"
  EventUtils.synthesizeKey("VK_RETURN", {});
  is(document.querySelector(".devtools-filterinput").value,
    "scheme:http protocol:HTTP/1.1",
    "Tokenized click generates correct value in input box");

  // Explicitly type in `flag:` renders autocomplete with values
  EventUtils.synthesizeKey(" status-code:", {});
  testAutocompleteContents(["status-code:200", "status-code:304"], document);

  // Typing the exact value closes autocomplete
  EventUtils.synthesizeKey("304", {});
  ok(!document.querySelector(".devtools-autocomplete-popup"),
    "Typing the exact value closes autocomplete");

  // Check if mime-type has been correctly parsed out and values also get autocomplete
  EventUtils.synthesizeKey(" mime-type:au", {});
  testAutocompleteContents(["mime-type:audio/ogg"], document);

  // The negative filter flags
  EventUtils.synthesizeKey(" -", {});
  testAutocompleteContents([
    "-cause:",
    "-domain:",
    "-has-response-header:",
    "-is:",
    "-larger-than:",
    "-method:",
    "-mime-type:",
    "-protocol:",
    "-regexp:",
    "-remote-ip:",
    "-scheme:",
    "-set-cookie-domain:",
    "-set-cookie-name:",
    "-set-cookie-value:",
    "-size:",
    "-status-code:",
    "-transferred-larger-than:",
    "-transferred:",
  ], document);

  // Autocomplete for negative filtering
  EventUtils.synthesizeKey("is:", {});
  testAutocompleteContents(["-is:cached", "-is:from-cache", "-is:running"], document);

  await teardown(monitor);
});
