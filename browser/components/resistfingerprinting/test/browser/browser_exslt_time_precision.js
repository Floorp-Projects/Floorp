/**
 * Bug 1912129 - A test case for verifying EXSLT date will report second-precise
 *               time fingerprinting resistance is enabled.
 */

function getTime(tab) {
  const extractTime = function () {
    const xslText = `
    <xsl:stylesheet version="1.0"
                    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                    xmlns:date="http://exslt.org/dates-and-times"
                    extension-element-prefixes="date">
      <xsl:output method="text" />
      <xsl:template match="/">
        <xsl:value-of select="date:date-time()" />
      </xsl:template>
    </xsl:stylesheet>`;

    const parser = new DOMParser();
    const xsltProcessor = new XSLTProcessor();
    const xslStylesheet = parser.parseFromString(xslText, "application/xml");
    xsltProcessor.importStylesheet(xslStylesheet);
    const xmlDoc = parser.parseFromString("<test />", "application/xml");
    const styledDoc = xsltProcessor.transformToDocument(xmlDoc);
    const time = styledDoc.firstChild.textContent;

    return time;
  };

  const extractTimeExpr = `(${extractTime.toString()})();`;

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extractTimeExpr],
    async funccode => content.eval(funccode)
  );
}

add_task(async function test_new_window() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      ["privacy.fingerprintingProtection.overrides", "+ReduceTimerPrecision"],
    ],
  });

  // Open a tab for extracting the time from XSLT.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_dummy.html",
    forceNewProcess: true,
  });

  for (let i = 0; i < 10; i++) {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(res => setTimeout(res, 25));

    // The regex could be a lot shorter (e.g. /\.(\d{3})/) but I wrote the whole
    // thing to make sure the time is in the expected format and to allow us
    // to re-use this regex in the future if we need to.
    // Note: Date format is not locale dependent.
    const regex = /\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.(\d{3})[-+]\d{2}:\d{2}/;
    const time = await getTime(tab);
    const [, milliseconds] = time.match(regex);

    is(milliseconds, "000", "Date's precision was reduced to seconds.");
  }

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
