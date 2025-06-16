ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
});

const httpsTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function loadAndCheck(file, displayInline, downloadFile = null) {
  // Get the downloads list and add a view to listen for a download to be added.
  // We do this even if we aren't going to download anything, so we notice if a
  // download is started.
  let download;
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadView = {
    async onDownloadAdded(aDownload) {
      info("download added");
      ok(downloadFile, "Should be expecting a download");
      download = aDownload;

      // Clean up the download from the list
      downloadList.remove(aDownload);
      await aDownload.finalize(true);
    },
  };
  await downloadList.addView(downloadView);

  // Open the new URL and perform the load.
  await BrowserTestUtils.withNewTab(
    `${httpsTestRoot}/${file}`,
    async browser => {
      is(
        browser.browsingContext.children.length,
        displayInline ? 1 : 0,
        `Should ${displayInline ? "not " : ""}have a child frame`
      );

      await SpecialPowers.spawn(
        browser,
        [displayInline],
        async displayInline => {
          let obj = content.document.querySelector("object");
          is(
            obj.displayedType,
            displayInline
              ? Ci.nsIObjectLoadingContent.TYPE_DOCUMENT
              : Ci.nsIObjectLoadingContent.TYPE_FALLBACK,
            `should be displaying TYPE_${displayInline ? "DOCUMENT" : "FALLBACK"}`
          );
        }
      );
    }
  );

  // Clean up our download observer.
  await downloadList.removeView(downloadView);
  if (downloadFile) {
    is(
      download.source.url,
      `${httpsTestRoot}/${downloadFile}`,
      "Download has the correct source"
    );
  } else {
    is(download, undefined, "Should not have seen a download");
  }
}

add_task(async function test_pdf_object_attachment() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.navigation.object_embed.allow_retargeting", false],
      ["browser.download.open_pdf_attachments_inline", false],
    ],
  });

  // PDF attachment should display inline.
  await loadAndCheck("file_pdf_object_attachment.html", true);
});

add_task(async function test_img_object_attachment() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.navigation.object_embed.allow_retargeting", false]],
  });

  // Image attachment should display inline.
  await loadAndCheck("file_img_object_attachment.html", true);
});

add_task(async function test_svg_object_attachment() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.navigation.object_embed.allow_retargeting", false]],
  });

  // SVG attachment should fail to load.
  await loadAndCheck("file_svg_object_attachment.html", false);
});

add_task(async function test_html_object_attachment() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.navigation.object_embed.allow_retargeting", false]],
  });

  // HTML attachment should fail to load.
  await loadAndCheck("file_html_object_attachment.html", false);
});

add_task(async function test_pdf_object_attachment_allow_retargeting() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.navigation.object_embed.allow_retargeting", true],
      ["browser.download.open_pdf_attachments_inline", false],
    ],
  });

  // Even if `allow_retargeting` is enabled, we always display PDFs inline.
  await loadAndCheck("file_pdf_object_attachment.html", true);
});

add_task(async function test_img_object_attachment_allow_retargeting() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.navigation.object_embed.allow_retargeting", true]],
  });

  // Even if `allow_retargeting` is enabled, we always display images inline.
  await loadAndCheck("file_img_object_attachment.html", true);
});

add_task(async function test_svg_object_attachment_allow_retargeting() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.navigation.object_embed.allow_retargeting", true]],
  });

  // SVG attachments are downloaded if allow_retargeting is set.
  await loadAndCheck(
    "file_svg_object_attachment.html",
    false,
    "file_svg_attachment.svg"
  );
});

add_task(async function test_html_object_attachment_allow_retargeting() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.navigation.object_embed.allow_retargeting", true]],
  });

  // HTML attachments are downloaded if allow_retargeting is set.
  await loadAndCheck(
    "file_html_object_attachment.html",
    false,
    "file_html_attachment.html"
  );
});
