/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the network locations form of the Connect page.
 * Check that a network location can be added and removed.
 */

const TEST_NETWORK_LOCATION = "localhost:1111";

add_task(async function() {
  const { document, tab } = await openAboutDebugging();

  const sidebarItems = document.querySelectorAll(".js-sidebar-item");
  const connectSidebarItem = [...sidebarItems].find(element => {
    return element.textContent === "Connect";
  });
  ok(connectSidebarItem, "Sidebar contains a Connect item");

  info("Click on the Connect item in the sidebar");
  connectSidebarItem.click();

  info("Wait until Connect page is displayed");
  await waitUntil(() => document.querySelector(".js-connect-page"));

  let networkLocations = document.querySelectorAll(".js-network-location");
  is(networkLocations.length, 0, "By default, no network locations are displayed");

  addNetworkLocation(TEST_NETWORK_LOCATION, document);

  info("Wait until the new network location is visible in the list");
  await waitUntil(() => document.querySelectorAll(".js-network-location").length === 1);
  networkLocations = document.querySelectorAll(".js-network-location");

  const networkLocationValue =
    networkLocations[0].querySelector(".js-network-location-value");
  is(networkLocationValue.textContent, TEST_NETWORK_LOCATION,
    "Added network location has the expected value");

  removeNetworkLocation(TEST_NETWORK_LOCATION, document);

  info("Wait until the new network location is removed from the list");
  await waitUntil(() => document.querySelectorAll(".js-network-location").length === 0);

  await removeTab(tab);
});

function addNetworkLocation(location, document) {
  info("Setting a value in the network form input");
  const networkLocationInput =
    document.querySelector(".js-network-form-input");
  networkLocationInput.focus();
  EventUtils.sendString(location, networkLocationInput.ownerGlobal);

  info("Click on network form submit button");
  const networkLocationSubmitButton =
    document.querySelector(".js-network-form-submit-button");
  networkLocationSubmitButton.click();
}

function removeNetworkLocation(location, document) {
  const networkLocation = getNetworkLocation(location, document);
  ok(networkLocation, "Network location container found.");

  info("Click on the remove button for the provided network location");
  const removeButton =
    networkLocation.querySelector(".js-network-location-remove-button");
  removeButton.click();
}

function getNetworkLocation(location, document) {
  info("Find the container for network location: " + location);
  const networkLocations = document.querySelectorAll(".js-network-location");
  return [...networkLocations].find(element => {
    return element.querySelector(".js-network-location-value").textContent === location;
  });
}
