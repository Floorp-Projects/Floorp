/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head-mocks.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "head-mocks.js", this);

/**
 * Test that remote runtimes show the connection prompt,
 * but it's hidden in 'This Firefox'
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();
  mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });

  const { document, tab } = await openAboutDebugging();

  info("Checking This Firefox");
  ok(!document.querySelector(".js-connection-prompt-toggle-button"),
    "This Firefox does not contain the connection prompt button");

  info("Checking a USB runtime");
  mocks.emitUSBUpdate();
  await connectToRuntime("Fancy Phone", document);
  await selectRuntime("Fancy Phone", "Lorem ipsum", document);
  ok(!!document.querySelector(".js-connection-prompt-toggle-button"),
    "Runtime contains the connection prompt button");

  await removeTab(tab);
});
