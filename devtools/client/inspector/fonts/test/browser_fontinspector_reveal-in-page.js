/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that fonts usage can be revealed in the page using the FontsHighlighter.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  // Enable the feature, since it's off by default for now.
  await pushPref("devtools.inspector.fonthighlighter.enabled", true);

  // Make sure the toolbox is tall enough to accomodate all fonts, otherwise mouseover
  // events simulation will fail.
  await pushPref("devtools.toolbox.footer.height", 500);

  const { tab, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  const fontEls = getUsedFontsEls(viewDoc);

  // The number of window selection change events we expect to get as we hover over each
  // font in the list. Waiting for those events is how we know that text-runs were
  // highlighted in the page.
  // The reason why these numbers vary is because the highlighter may create more than
  // 1 selection range object, depending on the number of text-runs found.
  const expectedSelectionChangeEvents = [1, 2, 2, 1, 1];

  for (let i = 0; i < fontEls.length; i++) {
    info(`Mousing over and out of font number ${i} in the list`);

    // Simulating a mouse over event on the font name and expecting a selectionchange.
    const nameEl = fontEls[i].querySelector(".font-name");
    let onEvents = waitForNSelectionEvents(tab, expectedSelectionChangeEvents[i]);
    EventUtils.synthesizeMouse(nameEl, 2, 2, {type: "mouseover"}, viewDoc.defaultView);
    await onEvents;

    ok(true,
      `${expectedSelectionChangeEvents[i]} selectionchange events detected on mouseover`);

    // Simulating a mouse out event on the font name and expecting a selectionchange.
    const otherEl = fontEls[i].querySelector(".font-preview");
    onEvents = waitForNSelectionEvents(tab, 1);
    EventUtils.synthesizeMouse(otherEl, 2, 2, {type: "mouseover"}, viewDoc.defaultView);
    await onEvents;

    ok(true, "1 selectionchange events detected on mouseout");
  }
});

async function waitForNSelectionEvents(tab, numberOfTimes) {
  await ContentTask.spawn(tab.linkedBrowser, numberOfTimes, async function(n) {
    const win = content.wrappedJSObject;

    await new Promise(resolve => {
      let received = 0;
      win.document.addEventListener("selectionchange", function listen() {
        received++;

        if (received === n) {
          win.document.removeEventListener("selectionchange", listen);
          resolve();
        }
      });
    });
  });
}
