/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const EXAMPLE_COM_URL = "https://example.com/browser/dom/midi/tests/";
const EXAMPLE_ORG_URL = "https://example.org/browser/dom/midi/tests/";
const PAGE1 = "port_ids_page_1.html";
const PAGE2 = "port_ids_page_2.html";

// Return the MIDI port id of the first input port for the given URL and page
function id_for_tab(url, page) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: url + page,
      waitForLoad: true,
    },
    async function (browser) {
      return SpecialPowers.spawn(browser, [""], function () {
        return content.wrappedJSObject.get_first_input_id();
      });
    }
  );
}

add_task(async function () {
  let com_page1;
  let com_page1_reload;
  let org_page1;
  let org_page2;

  [com_page1, com_page1_reload, org_page1, org_page2] = await Promise.all([
    id_for_tab(EXAMPLE_COM_URL, PAGE1),
    id_for_tab(EXAMPLE_COM_URL, PAGE1),
    id_for_tab(EXAMPLE_ORG_URL, PAGE1),
    id_for_tab(EXAMPLE_ORG_URL, PAGE2),
  ]);
  Assert.equal(
    com_page1,
    com_page1_reload,
    "MIDI port ids should be the same when reloading the same page"
  );
  Assert.notEqual(
    com_page1,
    org_page1,
    "MIDI port ids should be different in different origins"
  );
  Assert.equal(
    org_page1,
    org_page2,
    "MIDI port ids should be the same in the same origin"
  );
});
