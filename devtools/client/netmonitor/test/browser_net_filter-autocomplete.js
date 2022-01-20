/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test autocomplete based on filtering flags and requests
 */
const REQUESTS = [
  {
    url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample",
  },
  {
    url:
      "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" +
      "&cookies=1",
  },
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
  expected.forEach(function(item, i) {
    is(
      document.querySelector(
        `.devtools-autocomplete-listbox .autocomplete-item:nth-child(${i + 1})`
      ).textContent,
      item,
      `${expected[i]} found`
    );
  });
}

add_task(async function() {
  const { monitor } = await initNetMonitor(FILTERING_URL, { requestCount: 1 });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");

  // Let the requests load completely before the autocomplete tests begin
  // as autocomplete values also rely on the network requests.
  const waitNetwork = waitForNetworkEvents(monitor, REQUESTS.length);
  await performRequestsInContent(REQUESTS);
  await waitNetwork;

  EventUtils.synthesizeMouseAtCenter(
    document.querySelector(".devtools-filterinput"),
    {},
    document.defaultView
  );
  // Empty Mouse click should keep autocomplete hidden
  ok(
    !document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup still hidden"
  );

  document.querySelector(".devtools-filterinput").focus();

  // Typing numbers that corresponds to status codes should invoke an autocomplete
  EventUtils.sendString("2");
  ok(
    document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup Created"
  );
  testAutocompleteContents(
    [
      "status-code:200",
      "status-code:201",
      "status-code:202",
      "status-code:203",
      "status-code:204",
      "status-code:205",
      "status-code:206",
    ],
    document
  );
  EventUtils.synthesizeKey("KEY_Enter");
  is(
    document.querySelector(".devtools-filterinput").value,
    "status-code:200",
    "Value correctly set after Enter"
  );
  ok(
    !document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup hidden after keyboard Enter key"
  );

  // Space separated tokens
  // The last token where autocomplete is available shall generate the popup
  EventUtils.sendString(" s");
  ok(
    document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup Created"
  );
  testAutocompleteContents(
    [
      "scheme:",
      "set-cookie-domain:",
      "set-cookie-name:",
      "set-cookie-value:",
      "size:",
      "status-code:",
    ],
    document
  );

  EventUtils.sendString("c");
  testAutocompleteContents(["scheme:"], document);
  EventUtils.synthesizeKey("KEY_Tab");
  // Tab selection should hide autocomplete
  ok(
    document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup alive with content values"
  );
  testAutocompleteContents(["scheme:http"], document);

  EventUtils.synthesizeKey("KEY_Enter");
  is(
    document.querySelector(".devtools-filterinput").value,
    "status-code:200 scheme:http",
    "Value correctly set after Enter"
  );
  ok(
    !document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup hidden after keyboard Enter key"
  );

  // Space separated tokens
  // The last token where autocomplete is available shall generate the popup
  EventUtils.sendString(" p");
  testAutocompleteContents(["protocol:"], document);

  // The new value of the text box should be previousTokens + latest value selected
  // First return selects "protocol:"
  EventUtils.synthesizeKey("KEY_Enter");
  // Second return selects "protocol:HTTP/1.1"
  EventUtils.synthesizeKey("KEY_Enter");
  is(
    document.querySelector(".devtools-filterinput").value,
    "status-code:200 scheme:http protocol:HTTP/1.1",
    "Tokenized click generates correct value in input box"
  );

  // Explicitly type in `flag:` renders autocomplete with values
  EventUtils.sendString(" status-code:");
  testAutocompleteContents(["status-code:200", "status-code:304"], document);

  // Typing the exact value closes autocomplete
  EventUtils.sendString("304");
  ok(
    !document.querySelector(".devtools-autocomplete-popup"),
    "Typing the exact value closes autocomplete"
  );

  // Check if mime-type has been correctly parsed out and values also get autocomplete
  EventUtils.sendString(" mime-type:text");
  testAutocompleteContents(
    ["mime-type:text/css", "mime-type:text/html", "mime-type:text/plain"],
    document
  );

  // The negative filter flags
  EventUtils.sendString(" -");
  testAutocompleteContents(
    [
      "-cause:",
      "-domain:",
      "-has-response-header:",
      "-initiator:",
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
    ],
    document
  );

  // Autocomplete for negative filtering
  EventUtils.sendString("is:");
  testAutocompleteContents(
    ["-is:cached", "-is:from-cache", "-is:running"],
    document
  );

  await teardown(monitor);
});
