/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_456342_sample.xhtml";

const EXPECTED_IDS = new Set(["searchTerm"]);

const EXPECTED_XPATHS = new Set([
  "/xhtml:html/xhtml:body/xhtml:form/xhtml:p[2]/xhtml:input",
  "/xhtml:html/xhtml:body/xhtml:form/xhtml:p[3]/xhtml:input[@name='fill-in']",
  "/xhtml:html/xhtml:body/xhtml:form/xhtml:p[4]/xhtml:input[@name='mistyped']",
  "/xhtml:html/xhtml:body/xhtml:form/xhtml:p[5]/xhtml:textarea[@name='textarea_pass']",
]);

/**
 * Bug 456342 - Restore values from non-standard input field types.
 */
add_task(async function test_restore_nonstandard_input_values() {
  // Add tab with various non-standard input field types.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Fill in form values.
  let expectedValue = Math.random();
  await setFormElementValues(browser, { value: expectedValue });

  // Remove tab and check collected form data.
  await promiseRemoveTabAndSessionState(tab);
  let undoItems = JSON.parse(ss.getClosedTabData(window));
  let savedFormData = undoItems[0].state.formdata;

  let foundIds = 0;
  for (let id of Object.keys(savedFormData.id)) {
    ok(EXPECTED_IDS.has(id), `Check saved ID "${id}" was expected`);
    is(savedFormData.id[id], expectedValue, `Check saved value for #${id}`);
    foundIds++;
  }

  let foundXpaths = 0;
  for (let exp of Object.keys(savedFormData.xpath)) {
    ok(EXPECTED_XPATHS.has(exp), `Check saved xpath "${exp}" was expected`);
    is(savedFormData.xpath[exp], expectedValue, `Check saved value for ${exp}`);
    foundXpaths++;
  }

  is(foundIds, EXPECTED_IDS.size, "Check number of fields saved by ID");
  is(
    foundXpaths,
    EXPECTED_XPATHS.size,
    "Check number of fields saved by xpath"
  );
});

function setFormElementValues(browser, data) {
  return sendMessage(browser, "ss-test:setFormElementValues", data);
}
