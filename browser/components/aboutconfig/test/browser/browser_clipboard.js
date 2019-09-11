/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.copy.false", false],
      ["test.aboutconfig.copy.number", 10],
      ["test.aboutconfig.copy.spaces.1", " "],
      ["test.aboutconfig.copy.spaces.2", "  "],
      ["test.aboutconfig.copy.spaces.3", "   "],
      ["test.aboutconfig.copy.string", "010.5"],
    ],
  });
});

add_task(async function test_copy() {
  await AboutConfigTest.withNewTab(async function() {
    for (let [name, expectedString] of [
      [PREF_BOOLEAN_DEFAULT_TRUE, "true"],
      [PREF_BOOLEAN_USERVALUE_TRUE, "true"],
      [PREF_STRING_DEFAULT_EMPTY, ""],
      ["test.aboutconfig.copy.false", "false"],
      ["test.aboutconfig.copy.number", "10"],
      ["test.aboutconfig.copy.spaces.1", " "],
      ["test.aboutconfig.copy.spaces.2", "  "],
      ["test.aboutconfig.copy.spaces.3", "   "],
      ["test.aboutconfig.copy.string", "010.5"],
    ]) {
      // Limit the number of preferences shown so all the rows are visible.
      this.search(name);
      let row = this.getRow(name);

      // Triple click at any location in the name cell should select the name.
      await BrowserTestUtils.synthesizeMouseAtCenter(
        row.nameCell,
        { clickCount: 3 },
        this.browser
      );
      Assert.ok(row.nameCell.contains(this.window.getSelection().anchorNode));
      await SimpleTest.promiseClipboardChange(name, async () => {
        await BrowserTestUtils.synthesizeKey(
          "c",
          { accelKey: true },
          this.browser
        );
      });

      // Triple click at any location in the value cell should select the value.
      await BrowserTestUtils.synthesizeMouseAtCenter(
        row.valueCell,
        { clickCount: 3 },
        this.browser
      );
      let selection = this.window.getSelection();
      Assert.ok(row.valueCell.contains(selection.anchorNode));

      // The selection is never collapsed because of the <span> element, and
      // this makes sure that an empty string can be copied.
      Assert.ok(!selection.isCollapsed);
      await SimpleTest.promiseClipboardChange(expectedString, async () => {
        await BrowserTestUtils.synthesizeKey(
          "c",
          { accelKey: true },
          this.browser
        );
      });
    }
  });
});

add_task(async function test_copy_multiple() {
  await AboutConfigTest.withNewTab(async function() {
    // Lines are separated by a single LF character on all platforms.
    let expectedString =
      "test.aboutconfig.copy.false\tfalse\t\n" +
      "test.aboutconfig.copy.number\t10\t\n" +
      "test.aboutconfig.copy.spaces.1\t \t\n" +
      "test.aboutconfig.copy.spaces.2\t  \t\n" +
      "test.aboutconfig.copy.spaces.3\t   \t\n" +
      "test.aboutconfig.copy.string\t010.5";

    this.search("test.aboutconfig.copy.");
    let startRow = this.getRow("test.aboutconfig.copy.false");
    let endRow = this.getRow("test.aboutconfig.copy.string");
    let { width, height } = endRow.valueCell.getBoundingClientRect();

    // Drag from the top left of the first row to the bottom right of the last.
    await BrowserTestUtils.synthesizeMouse(
      startRow.nameCell,
      1,
      1,
      { type: "mousedown" },
      this.browser
    );
    await BrowserTestUtils.synthesizeMouse(
      endRow.valueCell,
      width - 1,
      height - 1,
      { type: "mousemove" },
      this.browser
    );
    await BrowserTestUtils.synthesizeMouse(
      endRow.valueCell,
      width - 1,
      height - 1,
      { type: "mouseup" },
      this.browser
    );

    await SimpleTest.promiseClipboardChange(expectedString, async () => {
      await BrowserTestUtils.synthesizeKey(
        "c",
        { accelKey: true },
        this.browser
      );
    });
  });
});
