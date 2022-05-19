/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const EXAMPLE_ORG_URL = "https://example.org/browser/dom/midi/tests/";
const PAGE = "refresh_port_list.html";

async function get_port_num(browser) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [""], function() {
    return content.wrappedJSObject.get_num_ports();
  });
}

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    EXAMPLE_ORG_URL + PAGE
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let ports_num = await get_port_num(gBrowser.selectedBrowser);
  Assert.equal(ports_num, 4, "There are four ports");

  const finished = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(gBrowser.selectedTab);
  await finished;

  let refreshed_ports_num = await get_port_num(gBrowser.selectedBrowser);
  Assert.equal(refreshed_ports_num, 5, "There are five ports");

  gBrowser.removeTab(gBrowser.selectedTab);
});
