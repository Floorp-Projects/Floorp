/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 // Tests ime composition handling.

function synthesizeCompositionChange(string) {
  EventUtils.synthesizeCompositionChange({
    composition: {
      string,
      clauses: [
        { length: string.length, attr: Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE },
      ],
    },
    caret: { start: string.length, length: 0 },
    key: { key: string ? string[string.length - 1] : "KEY_Backspace" },
  });
  // Modifying composition should not open the popup.
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Popup should be closed");
}

add_task(async function test_composition() {
  gURLBar.focus();
  // Add at least one typed entry for the empty results set. Also clear history
  // so that this can be over the autofill threshold.
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits({
    uri: "http://mozilla.org/",
    transition: PlacesUtils.history.TRANSITIONS.TYPED,
  });

  info("The popup should not be shown during composition but, after compositionend, it should be.");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Popup should be closed");
  synthesizeCompositionChange("I");
  Assert.equal(gURLBar.value, "I", "Check urlbar value");
  synthesizeCompositionChange("In");
  Assert.equal(gURLBar.value, "In", "Check urlbar value");
  // Committing composition should open the popup.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeComposition({
      type: "compositioncommitasis",
      key: { key: "KEY_Enter" },
    });
  });
  Assert.equal(gURLBar.value, "In", "Check urlbar value");

  info("If composition starts while the popup is shown, the compositionstart event should close the popup.");
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open");
  synthesizeCompositionChange("t");
  Assert.equal(gURLBar.value, "Int", "Check urlbar value");
  synthesizeCompositionChange("te");
  Assert.equal(gURLBar.value, "Inte", "Check urlbar value");
  // Committing composition should open the popup.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeComposition({
      type: "compositioncommitasis",
      key: { key: "KEY_Enter" },
    });
  });
  Assert.equal(gURLBar.value, "Inte", "Check urlbar value");

  info("If composition is cancelled, the value shouldn't be changed.");
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open");
  synthesizeCompositionChange("r");
  Assert.equal(gURLBar.value, "Inter", "Check urlbar value");
  synthesizeCompositionChange("");
  Assert.equal(gURLBar.value, "Inte", "Check urlbar value");
  // Canceled compositionend should reopen the popup.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeComposition({
      type: "compositioncommit",
      data: "",
      key: { key: "KEY_Escape" },
    });
  });
  Assert.equal(gURLBar.value, "Inte", "Check urlbar value");

  info("If composition replaces some characters and canceled, the search string should be the latest value.");
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open");
  EventUtils.synthesizeKey("VK_LEFT", { shiftKey: true });
  EventUtils.synthesizeKey("VK_LEFT", { shiftKey: true });
  synthesizeCompositionChange("t");
  Assert.equal(gURLBar.value, "Int", "Check urlbar value");
  synthesizeCompositionChange("te");
  Assert.equal(gURLBar.value, "Inte", "Check urlbar value");
  synthesizeCompositionChange("");
  Assert.equal(gURLBar.value, "In", "Check urlbar value");
  // Canceled compositionend should search the result with the latest value.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeComposition({
      type: "compositioncommitasis",
      key: { key: "KEY_Escape" },
    });
  });
  Assert.equal(gURLBar.value, "In", "Check urlbar value");

  info("If all characters are removed, the popup should be closed.");
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Backspace", {});
    EventUtils.synthesizeKey("KEY_Backspace", {});
  });
  Assert.equal(gURLBar.value, "", "Check urlbar value");

  info("Composition which is canceled shouldn't cause opening the popup.");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Popup should be closed");
  synthesizeCompositionChange("I");
  Assert.equal(gURLBar.value, "I", "Check urlbar value");
  synthesizeCompositionChange("In");
  Assert.equal(gURLBar.value, "In", "Check urlbar value");
  synthesizeCompositionChange("");
  Assert.equal(gURLBar.value, "", "Check urlbar value");
  // Canceled compositionend shouldn't open the popup if it was closed.
  EventUtils.synthesizeComposition({
    type: "compositioncommitasis",
    key: { key: "KEY_Escape" },
  });
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Popup should be closed");
  Assert.equal(gURLBar.value, "", "Check urlbar value");

  info("Down key should open the popup even if the editor is empty.");
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeKey("KEY_ArrowDown", {});
  });
  Assert.equal(gURLBar.value, "", "Check urlbar value");

  info("If popup is open at starting composition, the popup should be reopened after composition anyway.");
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open");
  synthesizeCompositionChange("I");
  Assert.equal(gURLBar.value, "I", "Check urlbar value");
  synthesizeCompositionChange("In");
  Assert.equal(gURLBar.value, "In", "Check urlbar value");
  synthesizeCompositionChange("");
  Assert.equal(gURLBar.value, "", "Check urlbar value");
  // A canceled compositionend should open the popup if it was open.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeComposition({
      type: "compositioncommitasis",
      key: { key: "KEY_Escape" },
    });
  });
  Assert.equal(gURLBar.value, "", "Check urlbar value");

  info("Type normally, and hit escape, the popup should be closed.");
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open");
  EventUtils.synthesizeKey("I", {});
  EventUtils.synthesizeKey("n", {});
  // The old urlbar may close/reopen the popup and then ESC wouldn't act as
  // expected.
  if (!UrlbarPrefs.get("quantumbar")) {
    await UrlbarTestUtils.promiseSearchComplete(window);
  }
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape", {});
  });
  Assert.equal(gURLBar.value, "In", "Check urlbar value");
  // Clear typed chars.
  EventUtils.synthesizeKey("KEY_Backspace", {});
  EventUtils.synthesizeKey("KEY_Backspace", {});
  Assert.equal(gURLBar.value, "", "Check urlbar value");

  info("With autofill, compositionstart shouldn't open the popup");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Popup should be closed");
  synthesizeCompositionChange("M");
  Assert.equal(gURLBar.value, "M", "Check urlbar value");
  synthesizeCompositionChange("Mo");
  Assert.equal(gURLBar.value, "Mo", "Check urlbar value");
  // Committing composition should open the popup.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeComposition({
      type: "compositioncommitasis",
      key: { key: "KEY_Enter" },
    });
  });
  Assert.equal(gURLBar.value, "Mozilla.org/", "Check urlbar value");
});

