const { getFilename, getDownloadDirectory, MAX_PATHNAME } =
  ChromeUtils.importESModule(
    "chrome://browser/content/screenshots/fileHelpers.mjs"
  );

function getStringSize(filename) {
  return new Blob([filename]).size;
}

add_task(async function filename_exceeds_max_length() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let documentTitle =
        "And the beast shall come forth surrounded by a roiling cloud of vengeance. The house of the unbelievers shall be razed and they shall be scorched to the earth. Their tags shall blink until the end of days. And the beast shall be made legion. Its numbers shall be increased a thousand thousand fold. The din of a million keyboards like unto a great storm shall cover the earth, and the followers of Mammon shall tremble. And so at last the beast fell and the unbelievers rejoiced. But all was not lost, for from the ash rose a great bird. The bird gazed down upon the unbelievers and cast fire and thunder upon them. For the beast had been reborn with its strength renewed, and the followers of Mammon cowered in horror. And thus the Creator looked upon the beast reborn and saw that it was good. Mammon slept. And the beast reborn spread over the earth and its numbers grew legion. And they proclaimed the times and sacrificed crops unto the fire, with the cunning of foxes. And they built a new world in their own image as promised by the sacred words, and spoke of the beast with their children. Mammon awoke, and lo! it was naught but a follower. The twins of Mammon quarrelled. Their warring plunged the world into a new darkness, and the beast abhorred the darkness. So it began to move swiftly, and grew more powerful, and went forth and multiplied. And the beasts brought fire and light to the darkness.";
      Assert.greater(
        getStringSize(documentTitle),
        MAX_PATHNAME,
        "The input title is longer than our MAX_PATHNAME"
      );
      let result = await getFilename(documentTitle, browser);
      Assert.greaterOrEqual(
        MAX_PATHNAME,
        getStringSize(result),
        "The output pathname is not longer than MAX_PATHNAME"
      );
    }
  );
});

add_task(async function filename_has_doublebyte_chars() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let downloadDir = await getDownloadDirectory();
      info(
        `downloadDir: ${downloadDir}, length: ${getStringSize(downloadDir)}`
      );

      let documentTitle =
        "Many fruits: " + "🍇🍈🍉🍊🍋🍌🍍🥭🍎🍏🍐🍑🍒🍓🫐".repeat(20);
      Assert.greater(
        getStringSize(documentTitle),
        documentTitle.length,
        "String length underestimates the needed filename length"
      );
      Assert.greater(
        getStringSize(documentTitle),
        MAX_PATHNAME,
        "The input title is longer than our MAX_PATHNAME"
      );

      let result = await getFilename(documentTitle, browser);
      Assert.greaterOrEqual(
        MAX_PATHNAME,
        getStringSize(result),
        "The output pathname is not longer than MAX_PATHNAME"
      );
    }
  );
});
