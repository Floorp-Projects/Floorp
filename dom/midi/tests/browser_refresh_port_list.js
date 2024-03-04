/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const EXAMPLE_ORG_URL = "https://example.org/browser/dom/midi/tests/";
const PAGE = "refresh_port_list.html";

async function get_access() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.get_access();
  });
}

async function reset_access() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.reset_access();
  });
}

async function get_num_ports() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.get_num_ports();
  });
}

async function add_port() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return content.wrappedJSObject.add_port();
  });
}

async function remove_port() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return content.wrappedJSObject.remove_port();
  });
}

async function force_refresh() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return content.wrappedJSObject.force_refresh();
  });
}

add_task(async function () {
  gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    EXAMPLE_ORG_URL + PAGE
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  await get_access(gBrowser.selectedBrowser);
  let ports_num = await get_num_ports(gBrowser.selectedBrowser);
  Assert.equal(ports_num, 4, "We start with four ports");
  await add_port(gBrowser.selectedBrowser);
  ports_num = await get_num_ports(gBrowser.selectedBrowser);
  Assert.equal(ports_num, 5, "One port is added manually");
  // This causes the test service to refresh the ports the next time a refresh
  // is requested, it will happen after we reload the tab later on and will add
  // back the port that we're removing on the next line.
  await force_refresh(gBrowser.selectedBrowser);
  await remove_port(gBrowser.selectedBrowser);
  ports_num = await get_num_ports(gBrowser.selectedBrowser);
  Assert.equal(ports_num, 4, "One port is removed manually");

  await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

  await get_access(gBrowser.selectedBrowser);
  let refreshed_ports_num = await get_num_ports(gBrowser.selectedBrowser);
  Assert.equal(refreshed_ports_num, 5, "One port is added by the refresh");

  gBrowser.removeTab(gBrowser.selectedTab);
});
