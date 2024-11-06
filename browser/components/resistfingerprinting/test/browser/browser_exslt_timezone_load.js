/**
 * Bug 1891690 - A test case for verifying EXSLT date will use Atlantic/Reykjavik
 *               timezone (GMT and "real" equivalent to UTC) after fingerprinting
 *               resistance is enabled.
 */

function getTimeZoneOnTab(tab) {
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

    SpecialPowers.Cu.getJSTestingFunctions().setTimeZone("PST8PDT");

    const parser = new DOMParser();
    const xsltProcessor = new XSLTProcessor();
    const xslStylesheet = parser.parseFromString(xslText, "application/xml");
    xsltProcessor.importStylesheet(xslStylesheet);
    const xmlDoc = parser.parseFromString("<test />", "application/xml");
    const styledDoc = xsltProcessor.transformToDocument(xmlDoc);
    const time = styledDoc.firstChild.textContent;

    SpecialPowers.Cu.getJSTestingFunctions().setTimeZone(undefined);

    return time
      .match(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}([+-]\d{2}:\d{2})/)
      .pop();
  };

  const extractTimeExpr = `(${extractTime.toString()})();`;

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extractTimeExpr],
    async funccode => content.eval(funccode)
  );
}

async function getTimeZone(enabled) {
  const overrides = enabled ? "+JSDateTimeUTC" : "-JSDateTimeUTC";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      ["privacy.fingerprintingProtection.overrides", overrides],
    ],
  });

  SpecialPowers.Cu.getJSTestingFunctions().setTimeZone("PST8PDT");

  // Open a tab for extracting the time zone from XSLT.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_dummy.html",
  });

  const timeZone = await getTimeZoneOnTab(tab);

  BrowserTestUtils.removeTab(tab);
  SpecialPowers.Cu.getJSTestingFunctions().setTimeZone(undefined);
  await SpecialPowers.popPrefEnv();

  return timeZone;
}

let realTimeZone = "";
add_setup(async () => {
  realTimeZone = await getTimeZone(false);
});

async function run_test(enabled) {
  const timeZone = await getTimeZone(enabled);
  const expected = enabled ? "+00:00" : realTimeZone;

  ok(timeZone.endsWith(expected), `Timezone is ${expected}.`);
}

add_task(() => run_test(true));
add_task(() => run_test(false));
