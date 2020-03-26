/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 585991.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    foo = {
      item0: "value0",
      item1: "value1",
    };
  </script>
</head>
<body>Autocomplete popup delete key usage test</body>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  info("web console opened");

  const { autocompletePopup: popup } = jsterm;
  await setInputValueForAutocompletion(hud, "foo.i");

  ok(popup.isOpen, "popup is open");

  info("press Delete");
  const onPopupClose = popup.once("popup-closed");
  const onTimeout = wait(1000).then(() => "timeout");
  EventUtils.synthesizeKey("KEY_Delete");

  const result = await Promise.race([onPopupClose, onTimeout]);

  is(result, "timeout", "The timeout won the race");
  ok(popup.isOpen, "popup is open after hitting delete key");

  await closeAutocompletePopup(hud);
});
