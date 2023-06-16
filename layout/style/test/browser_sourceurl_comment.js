add_task(async function () {
  // Test text and expected results.
  let test_cases = [
    ["/*# sourceURL=here*/", "here"],
    ["/*# sourceURL=here  */", "here"],
    ["/*@ sourceURL=here*/", "here"],
    ["/*@ sourceURL=there*/ /*# sourceURL=here*/", "here"],
    ["/*# sourceURL=here there  */", "here"],

    ["/*# sourceURL=  here  */", ""],
    ["/*# sourceURL=*/", ""],
    ["/*# sourceUR=here  */", ""],
    ["/*! sourceURL=here  */", ""],
    ["/*# sourceURL = here  */", ""],
    ["/*   # sourceURL=here   */", ""],
  ];

  let page = "<!DOCTYPE HTML>\n<html>\n<head>\n";
  for (let i = 0; i < test_cases.length; ++i) {
    page += `<style type="text/css"> #x${i} { color: red; }${test_cases[i][0]}</style>\n`;
  }
  page += "</head><body>some text</body></html>";

  let uri = "data:text/html;base64," + btoa(page);
  info(`URI is ${uri}`);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: uri,
    },
    async function (browser) {
      await SpecialPowers.spawn(browser, [test_cases], function (tests) {
        for (let i = 0; i < content.document.styleSheets.length; ++i) {
          let sheet = content.document.styleSheets[i];

          info(`Checking sheet #${i}`);
          is(sheet.sourceURL, tests[i][1], `correct source URL for sheet ${i}`);
        }
      });
    }
  );
});
