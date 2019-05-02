/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the network locations form of the Connect page.
 * Check that a network location can be added and removed.
 */

const TEST_NETWORK_LOCATION = "localhost:1111";
const TEST_NETWORK_LOCATION_INVALID = "testnetwork";

add_task(async function() {
  const { document, tab } = await openAboutDebugging();

  await selectConnectPage(document);

  let networkLocations = document.querySelectorAll(".qa-network-location");
  is(networkLocations.length, 0, "By default, no network locations are displayed");

  info("Check whether error message should show if the input value is invalid");
  addNetworkLocation(TEST_NETWORK_LOCATION_INVALID, document);
  await waitUntil(() =>
    document.querySelector(".qa-connect-page__network-form__error-message"));

  info("Wait until the new network location is visible in the list");
  addNetworkLocation(TEST_NETWORK_LOCATION, document);
  await waitUntil(() => document.querySelectorAll(".qa-network-location").length === 1);
  await waitUntil(() =>
    !document.querySelector(".qa-connect-page__network-form__error-message"));

  networkLocations = document.querySelectorAll(".qa-network-location");
  const networkLocationValue =
    networkLocations[0].querySelector(".qa-network-location-value");
  is(networkLocationValue.textContent, TEST_NETWORK_LOCATION,
    "Added network location has the expected value");

  info("Check whether error message should show if the input value was duplicate");
  addNetworkLocation(TEST_NETWORK_LOCATION, document);
  await waitUntil(() =>
    document.querySelector(".qa-connect-page__network-form__error-message"));

  info("Wait until the new network location is removed from the list");
  removeNetworkLocation(TEST_NETWORK_LOCATION, document);
  await waitUntil(() => document.querySelectorAll(".qa-network-location").length === 0);

  await removeTab(tab);
});

function addNetworkLocation(location, document) {
  info("Setting a value in the network form input");
  const networkLocationInput =
    document.querySelector(".qa-network-form-input");
  networkLocationInput.value = "";
  networkLocationInput.focus();
  EventUtils.sendString(location, networkLocationInput.ownerGlobal);

  info("Click on network form submit button");
  const networkLocationSubmitButton =
    document.querySelector(".qa-network-form-submit-button");
  networkLocationSubmitButton.click();
}

function removeNetworkLocation(location, document) {
  const networkLocation = getNetworkLocation(location, document);
  ok(networkLocation, "Network location container found.");

  info("Click on the remove button for the provided network location");
  const removeButton =
    networkLocation.querySelector(".qa-network-location-remove-button");
  removeButton.click();
}

function getNetworkLocation(location, document) {
  info("Find the container for network location: " + location);
  const networkLocations = document.querySelectorAll(".qa-network-location");
  return [...networkLocations].find(element => {
    return element.querySelector(".qa-network-location-value").textContent === location;
  });
}
