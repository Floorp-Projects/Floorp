/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  getCleanedPacket,
  getStubFilePath,
  writeStubsToFile,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/stub-generators/test-css-message.html";
const STUB_FILE = "cssMessage.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generateCssMessageStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(
      getStubFilePath(STUB_FILE, env, true),
      generatedStubs
    );
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = require(getStubFilePath(STUB_FILE));
  const FAILURE_MSG =
    "The cssMessage stubs file needs to be updated by running " +
    "`mach test devtools/client/webconsole/test/browser/" +
    "browser_webconsole_stubs_css_message.js --headless " +
    "--setenv WEBCONSOLE_STUBS_UPDATE=true`";

  if (generatedStubs.size !== existingStubs.stubPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const packetStr = JSON.stringify(packet, null, 2);
    const existingPacketStr = JSON.stringify(
      existingStubs.stubPackets.get(key),
      null,
      2
    );
    is(packetStr, existingPacketStr, `"${key}" packet has expected value`);
    failed = failed || packetStr !== existingPacketStr;
  }

  if (failed) {
    ok(false, FAILURE_MSG);
  } else {
    ok(true, "Stubs are up to date");
  }
});

async function generateCssMessageStubs() {
  const stubs = new Map();
  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const webConsoleFront = await toolbox.target.getFront("console");

  for (const code of getCommands()) {
    const received = new Promise(resolve => {
      /* CSS errors are considered as pageError on the server */
      webConsoleFront.once("pageError", function onPacket(packet) {
        info(
          "Received css message: pageError " +
            JSON.stringify(packet, null, "\t")
        );

        const key = packet.pageError.errorMessage;
        stubs.set(key, getCleanedPacket(key, packet));
        resolve();
      });
    });

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [code], function(
      subCode
    ) {
      content.docShell.cssErrorReportingEnabled = true;
      const style = content.document.createElement("style");
      style.append(content.document.createTextNode(subCode));
      content.document.body.append(style);
    });

    await received;
  }

  await closeTabAndToolbox().catch(() => {});
  return stubs;
}

function getCommands() {
  return [
    `
  p {
    such-unknown-property: wow;
  }
  `,
    `
  p {
    padding-top: invalid value;
  }
  `,
  ];
}
