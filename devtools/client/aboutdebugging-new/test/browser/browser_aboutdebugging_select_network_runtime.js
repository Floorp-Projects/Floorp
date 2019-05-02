/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NETWORK_RUNTIME_HOST = "localhost:6080";
const NETWORK_RUNTIME_APP_NAME = "TestNetworkApp";
const NETWORK_RUNTIME_CHANNEL = "SomeChannel";
const NETWORK_RUNTIME_VERSION = "12.3";

// Test that network runtimes can be selected.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Prepare Network client mock");
  const networkClient = mocks.createNetworkRuntime(NETWORK_RUNTIME_HOST, {
    name: NETWORK_RUNTIME_APP_NAME,
  });
  networkClient.getDeviceDescription = () => {
    return {
      name: NETWORK_RUNTIME_APP_NAME,
      channel: NETWORK_RUNTIME_CHANNEL,
      version: NETWORK_RUNTIME_VERSION,
    };
  };

  info("Test addons in runtime page for Network client");
  await connectToRuntime(NETWORK_RUNTIME_HOST, document);
  await selectRuntime(NETWORK_RUNTIME_HOST, NETWORK_RUNTIME_APP_NAME, document);

  info("Check that the network runtime mock is properly displayed");
  const thisFirefoxRuntimeInfo = document.querySelector(".qa-runtime-name");
  ok(thisFirefoxRuntimeInfo, "Runtime info for this-firefox runtime is displayed");
  const runtimeInfoText = thisFirefoxRuntimeInfo.textContent;

  ok(runtimeInfoText.includes(NETWORK_RUNTIME_APP_NAME),
    "network runtime info shows the correct runtime name: " + runtimeInfoText);
  ok(runtimeInfoText.includes(NETWORK_RUNTIME_VERSION),
    "network runtime info shows the correct version number: " + runtimeInfoText);

  await removeTab(tab);
});
