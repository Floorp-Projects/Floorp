/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the initial route is /setup
 */
add_task(async function() {
  info("Check root route redirects to setup page");
  const { document, tab } = await openAboutDebugging();
  is(document.location.hash, "#/setup");

  await removeTab(tab);
});

/**
 * Test that the routes in about:debugging show the proper views and document.title
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  info("Check 'This Firefox' route");
  document.location.hash = "#/runtime/this-firefox";
  await waitUntil(() => document.querySelector(".qa-runtime-page"));
  const infoLabel = document.querySelector(".qa-runtime-name").textContent;
  // NOTE: when using USB Mocks, we see only "Firefox" as the device name
  ok(infoLabel.includes("Firefox"), "Runtime is displayed as Firefox");
  ok(!infoLabel.includes(" on "), "Runtime is not associated to any device");
  is(
    document.title,
    "Debugging - Runtime / this-firefox",
    "Checking title for 'runtime' page"
  );

  info("Check 'Setup' page");
  document.location.hash = "#/setup";
  await waitUntil(() => document.querySelector(".qa-connect-page"));
  ok(true, "Setup page has been shown");
  is(document.title, "Debugging - Setup", "Checking title for 'setup' page");

  info("Check 'USB device runtime' page");
  // connect to a mocked USB runtime
  mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });
  mocks.emitUSBUpdate();
  await connectToRuntime("Fancy Phone", document);
  // navigate to it via URL
  document.location.hash = "#/runtime/1337id";
  await waitUntil(() => document.querySelector(".qa-runtime-page"));
  const runtimeLabel = document.querySelector(".qa-runtime-name").textContent;
  is(
    document.title,
    "Debugging - Runtime / 1337id",
    "Checking title for 'runtime' page with USB device"
  );
  ok(
    runtimeLabel.includes("Lorem ipsum"),
    "Runtime is displayed with the mocked name"
  );

  await removeTab(tab);
});

/**
 * Test that an invalid route redirects to / (currently This Firefox page)
 */
add_task(async function() {
  info("Check an invalid route redirects to root");
  const { document, tab } = await openAboutDebugging();

  info("Waiting for a non setup page to load");
  document.location.hash = "#/runtime/this-firefox";
  await waitUntil(() => document.querySelector(".qa-runtime-page"));

  info("Update hash & wait for a redirect to root (connect page)");
  document.location.hash = "#/lorem-ipsum";
  await waitUntil(() => document.querySelector(".qa-connect-page"));
  is(document.title, "Debugging - Setup", "Checking title for 'setup' page");
  is(document.location.hash, "#/setup", "Redirected to root");

  await removeTab(tab);
});

/**
 * Test that routes from old about:debugging redirect to this Firefox.
 */
add_task(async function testOldAboutDebuggingRoutes() {
  info("Check that routes from old about:debugging redirect to this Firefox");
  const { document, tab } = await openAboutDebugging();

  const routes = ["addons", "tabs", "workers"];
  for (const route of routes) {
    info("Move to setup page before testing the route");
    document.location.hash = "#/setup";
    await waitUntil(() => document.querySelector(".qa-connect-page"));

    info(`Check that navigating to ${route} redirects to This Firefox`);
    document.location.hash = route;
    await waitUntil(() => document.querySelector(".qa-runtime-page"));
    is(
      document.location.hash,
      "#/runtime/this-firefox",
      `${route} was redirected to This Firefox`
    );
  }

  await removeTab(tab);
});
