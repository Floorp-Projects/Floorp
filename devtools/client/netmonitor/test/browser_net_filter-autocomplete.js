/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  let { document, window } = monitor.panelWin;

  info("Starting test... ");

  EventUtils.synthesizeMouseAtCenter(
    document.querySelector(".devtools-filterinput"), {}, window);
  // Empty Mouse click should keep autocomplete hidden
  ok(!document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup Created");

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
  ok(!document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup Hidden");
  is(document.querySelector(".devtools-filterinput").value,
    "scheme:", "Value correctly set after TAB");

  // Space separated tokens
  EventUtils.synthesizeKey("https ", {});
  // Adding just a space should keep popup hidden
  ok(!document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup still hidden");

  // The last token where autocomplete is availabe shall generate the popup
  EventUtils.synthesizeKey("p", {});
  testAutocompleteContents(["protocol:"], document);

  // The new value of the text box should be previousTokens + latest value selected
  EventUtils.synthesizeKey("VK_RETURN", {});
  is(document.querySelector(".devtools-filterinput").value,
    "scheme:https protocol:", "Tokenized click generates correct value in input box");

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

  await teardown(monitor);
});
