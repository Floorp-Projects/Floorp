/**
 * Bug 1891690 - A test case for verifying EXSLT date will use Atlantic/Reykjavik
 *               timezone (GMT and "real" equivalent to UTC) after fingerprinting
 *               resistance is enabled.
 */

function getTimeZone(tab) {
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

async function run_test(enabled) {
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

  const timeZone = await getTimeZone(tab);

  const expected = enabled ? "+00:00" : "-07:00";
  ok(timeZone.endsWith(expected), `Timezone is ${expected}.`);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
}

add_task(() => run_test(true));
add_task(() => run_test(false));
