/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  formatPacket,
  formatStub,
  formatFile,
  getStubFilePath,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");
const { prepareMessage } = require("devtools/client/webconsole/utils/messages");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/stub-generators/test-css-message.html";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  const filePath = getStubFilePath("cssMessage.js", env);
  info(`${isStubsUpdate ? "Update" : "Check"} stubs at ${filePath}`);

  const generatedStubs = await generateCssMessageStubs();

  if (isStubsUpdate) {
    await OS.File.writeAtomic(filePath, generatedStubs);
    ok(true, `${filePath} was successfully updated`);
    return;
  }

  const repoStubFileContent = await OS.File.read(filePath, {
    encoding: "utf-8",
  });
  is(generatedStubs, repoStubFileContent, "stubs file is up to date");

  if (generatedStubs != repoStubFileContent) {
    ok(
      false,
      "The cssMessage stubs file needs to be updated by running " +
        "`mach test devtools/client/webconsole/test/browser/" +
        "browser_webconsole_stubs_css_message.js --headless " +
        "--setenv WEBCONSOLE_STUBS_UPDATE=true`"
    );
  }
});

async function generateCssMessageStubs() {
  const stubs = {
    preparedMessages: [],
    packets: [],
  };

  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");

  for (const code of getCommands()) {
    const received = new Promise(resolve => {
      /* CSS errors are considered as pageError on the server */
      toolbox.target.activeConsole.on("pageError", function onPacket(packet) {
        toolbox.target.activeConsole.off("pageError", onPacket);
        info(
          "Received css message: pageError " +
            JSON.stringify(packet, null, "\t")
        );

        const message = prepareMessage(packet, { getNextId: () => 1 });
        stubs.packets.push(formatPacket(message.messageText, packet));
        stubs.preparedMessages.push(formatStub(message.messageText, packet));
        resolve();
      });
    });

    await ContentTask.spawn(gBrowser.selectedBrowser, code, function(subCode) {
      content.docShell.cssErrorReportingEnabled = true;
      const style = content.document.createElement("style");
      style.append(content.document.createTextNode(subCode));
      content.document.body.append(style);
    });

    await received;
  }

  await closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
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
