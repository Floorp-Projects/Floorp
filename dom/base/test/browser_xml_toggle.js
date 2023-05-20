const URL = `data:text/xml,
<?xml version="1.0" encoding="UTF-8"?>
<note>
  <to>Tove</to>
  <from>Jani</from>
  <heading>Reminder</heading>
  <body>Don't forget me this weekend!</body>
</note>
`;

add_task(async function xml_pretty_print_toggle() {
  await BrowserTestUtils.withNewTab(URL, async function (browser) {
    await SpecialPowers.spawn(browser, [], () => {
      let summary =
        content.document.documentElement.openOrClosedShadowRoot.querySelector(
          "summary"
        );
      let details = summary.parentNode;
      ok(details.open, "Should be open");
      summary.click();
      ok(!details.open, "Should be closed");
    });
  });
});
