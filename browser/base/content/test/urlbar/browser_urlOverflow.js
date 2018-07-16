/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testVal(aExpected, overflowSide = "") {
  info(`Testing ${aExpected}`);
  gURLBar.value = aExpected;

  gURLBar.focus();
  Assert.equal(gURLBar.scheme.value, "", "Check the scheme value");
  Assert.equal(getComputedStyle(gURLBar.scheme).visibility, "hidden",
               "Check the scheme box visibility");

  gURLBar.blur();
  await window.promiseDocumentFlushed(() => {});
  // The attribute doesn't always change, so we can't use waitForAttribute.
  await TestUtils.waitForCondition(
    () => gURLBar.getAttribute("textoverflow") === overflowSide);

  let scheme = aExpected.match(/^([a-z]+:\/{0,2})/)[1];
  // We strip http, so we should not show the scheme for it.
  if (scheme == "http://" && Services.prefs.getBoolPref("browser.urlbar.trimURLs", true))
    scheme = "";

  Assert.equal(gURLBar.selectionStart, gURLBar.selectionEnd,
               "Selection sanity check");
  Assert.equal(gURLBar.scheme.value, scheme, "Check the scheme value");
  let isOverflowed = gURLBar.inputField.scrollWidth > gURLBar.inputField.clientWidth;
  Assert.equal(isOverflowed, !!overflowSide, "Check The input field overflow");
  Assert.equal(gURLBar.getAttribute("textoverflow"), overflowSide,
               "Check the textoverflow attribute");
  if (overflowSide) {
    let side = gURLBar.inputField.scrollLeft == 0 ? "end" : "start";
    Assert.equal(side, overflowSide, "Check the overflow side");
    Assert.equal(getComputedStyle(gURLBar.scheme).visibility,
                 scheme && isOverflowed && overflowSide == "start" ? "visible" : "hidden",
                 "Check the scheme box visibility");
  }
}

add_task(async function() {
  registerCleanupFunction(function() {
    URLBarSetURI();
  });

  let lotsOfSpaces = new Array(200).fill("%20").join("");

  // اسماء.شبكة
  let rtlDomain = "\u0627\u0633\u0645\u0627\u0621\u002e\u0634\u0628\u0643\u0629";

  // Mix the direction of the tests to cover more cases, and to ensure the
  // textoverflow attribute changes every time, because tewtVal waits for that.
  await testVal(`https://mozilla.org/${lotsOfSpaces}/test/`, "end");
  await testVal(`https://mozilla.org/`);
  await testVal(`https://${rtlDomain}/${lotsOfSpaces}/test/`, "start");

  await testVal(`ftp://mozilla.org/${lotsOfSpaces}/test/`, "end");
  await testVal(`ftp://${rtlDomain}/${lotsOfSpaces}/test/`, "start");
  await testVal(`ftp://mozilla.org/`);
  await testVal(`http://${rtlDomain}/${lotsOfSpaces}/test/`, "start");
  await testVal(`http://mozilla.org/`);
  await testVal(`http://mozilla.org/${lotsOfSpaces}/test/`, "end");
  await testVal(`https://${rtlDomain}:80/${lotsOfSpaces}/test/`, "start");

  info("Test with formatting disabled");
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.urlbar.formatting.enabled", false],
    ["browser.urlbar.trimURLs", false]
  ]});

  await testVal(`https://mozilla.org/`);
  await testVal(`https://${rtlDomain}/${lotsOfSpaces}/test/`, "start");
  await testVal(`https://mozilla.org/${lotsOfSpaces}/test/`, "end");

  info("Test with trimURLs disabled");
  await testVal(`http://${rtlDomain}/${lotsOfSpaces}/test/`, "start");
});
