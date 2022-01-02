/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that autocomplete can be disabled.

const TESTCASE_URI = TEST_BASE_HTTP + "autocomplete.html";

// Pref which decides if CSS autocompletion is enabled in Style Editor or not.
const AUTOCOMPLETION_PREF = "devtools.styleeditor.autocompletion-enabled";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);
  const editor = await ui.editors[0].getSourceEditor();
  editor.sourceEditor.setOption("autocomplete", false);

  is(
    editor.sourceEditor.getOption("autocomplete"),
    false,
    "Autocompletion option does not exist"
  );
  ok(
    !editor.sourceEditor.getAutocompletionPopup(),
    "Autocompletion popup does not exist"
  );
});

add_task(async function() {
  Services.prefs.setBoolPref(AUTOCOMPLETION_PREF, false);
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);
  const editor = await ui.editors[0].getSourceEditor();

  is(
    editor.sourceEditor.getOption("autocomplete"),
    false,
    "Autocompletion option does not exist"
  );
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(AUTOCOMPLETION_PREF);
});
