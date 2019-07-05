/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure disableSearchAddon config works as expected in the source editor.

const isMacOS = Services.appinfo.OS === "Darwin";
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/sourceeditor.properties"
);

const FIND_KEY = L10N.getStr("find.key");
const REPLACE_KEY = L10N.getStr(
  isMacOS ? "replaceAllMac.key" : "replaceAll.key"
);

add_task(async function() {
  const { ed, win } = await setup({
    disableSearchAddon: true,
  });

  const edDoc = ed.container.contentDocument;
  const edWin = edDoc.defaultView;

  await promiseWaitForFocus();
  ed.focus();

  synthesizeKeyShortcut(FIND_KEY, edWin);
  const searchInput = edDoc.querySelector("input[type=search]");
  ok(!searchInput, "the search input is not displayed");

  synthesizeKeyShortcut(REPLACE_KEY, edWin);
  const replaceInput = edDoc.querySelector(".CodeMirror-dialog > input");
  ok(!replaceInput, "the replace input is not displayed");

  teardown(ed, win);
});
