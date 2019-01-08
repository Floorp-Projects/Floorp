/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-mocks.js", this);

/**
 * Test that the initial route is /runtime/this-firefox
 */
add_task(async function() {
  info("Check root route redirects to 'This Firefox'");
  const { document, tab } = await openAboutDebugging();
  is(document.location.hash, "#/runtime/this-firefox");

  await removeTab(tab);
});

/**
 * Test that the routes in about:debugging show the proper views
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  info("Check 'This Firefox' route");
  document.location.hash = "#/runtime/this-firefox";
  await waitUntil(() => document.querySelector(".js-runtime-page"));
  const infoLabel = document.querySelector(".js-runtime-info").textContent;
  // NOTE: when using USB Mocks, we see only "Firefox" as the device name
  ok(infoLabel.includes("Firefox"), "Runtime is displayed as Firefox");
  ok(!infoLabel.includes(" on "), "Runtime is not associated to any device");

  info("Check 'Connect' page");
  document.location.hash = "#/connect";
  await waitUntil(() => document.querySelector(".js-connect-page"));
  ok(true, "Connect page has been shown");

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
  await waitUntil(() => document.querySelector(".js-runtime-page"));
  const runtimeLabel = document.querySelector(".js-runtime-info").textContent;
  ok(runtimeLabel.includes("Lorem ipsum"), "Runtime is displayed with the mocked name");

  await removeTab(tab);
});

/**
 * Test that an invalid route redirects to / (currently This Firefox page)
 */
add_task(async function() {
  info("Check an invalid route redirects to root");
  const { document, tab } = await openAboutDebugging();

  info("Waiting for a non-runtime page to load");
  document.location.hash = "#/connect";
  await waitUntil(() => document.querySelector(".js-connect-page"));

  info("Update hash & wait for a redirect to root ('This Firefox')");
  document.location.hash = "#/lorem-ipsum";
  await waitUntil(() => document.querySelector(".js-runtime-page"));
  is(document.location.hash, "#/runtime/this-firefox", "Redirected to root");

  await removeTab(tab);
});
