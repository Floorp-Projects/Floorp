add_task(async function() {
  let uri = "http://example.com/browser/layout/style/test/sourcemap_css.html";
  info(`URI is ${uri}`);

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: uri
  }, async function(browser) {
    await ContentTask.spawn(browser, null, function* () {
      let seenSheets = 0;

      for (let i = 0; i < content.document.styleSheets.length; ++i) {
        let sheet = content.document.styleSheets[i];

        info(`Checking ${sheet.href}`);
        if (/mapped\.css/.test(sheet.href)) {
          is(sheet.sourceMapURL, "mapped.css.map", "X-SourceMap header took effect");
          seenSheets |= 1;
        } else if (/mapped2\.css/.test(sheet.href)) {
          is(sheet.sourceMapURL, "mapped2.css.map", "SourceMap header took effect");
          seenSheets |= 2;
        } else {
          ok(false, "sheet does not have source map URL");
        }
      }

        is(seenSheets, 3, "seen all source-mapped sheets");
    });
  });
});
