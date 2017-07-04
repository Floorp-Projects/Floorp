/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../../framework/test/shared-head.js */
/* exported generateConsoleApiStubs, generateCssMessageStubs,
            generateEvaluationResultStubs, generateNetworkEventStubs,
            generatePageErrorStubs, BASE_PATH */
"use strict";

// shared-head.js handles imports, constants, and utility functions
// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webconsole.new-frontend-enabled");
});

const { prepareMessage } = require("devtools/client/webconsole/new-console-output/utils/messages");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index.js");
const {
  consoleApi,
  cssMessage,
  evaluationResult,
  networkEvent,
  pageError,
} = require("devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/stub-snippets.js");

const BASE_PATH =
  "../../../../devtools/client/webconsole/new-console-output/test/fixtures";

let cachedPackets = {};

function getCleanedPacket(key, packet) {
  if (Object.keys(cachedPackets).includes(key)) {
    return cachedPackets[key];
  }

  // Strip escaped characters.
  let safeKey = key
    .replace(/\\n/g, "\n")
    .replace(/\\r/g, "\r")
    .replace(/\\\"/g, `\"`)
    .replace(/\\\'/g, `\'`);

  // If the stub already exist, we want to ignore irrelevant properties
  // (actor, timeStamp, timer, ...) that might changed and "pollute"
  // the diff resulting from this stub generation.
  let res;
  if (stubPackets.has(safeKey)) {
    let existingPacket = stubPackets.get(safeKey);
    res = Object.assign({}, packet, {
      from: existingPacket.from
    });

    // Clean root timestamp.
    if (res.timestamp) {
      res.timestamp = existingPacket.timestamp;
    }

    if (res.timeStamp) {
      res.timeStamp = existingPacket.timeStamp;
    }

    if (res.startedDateTime) {
      res.startedDateTime = existingPacket.startedDateTime;
    }

    if (res.actor) {
      res.actor = existingPacket.actor;
    }

    if (res.message) {
      // Clean timeStamp on the message prop.
      res.message.timeStamp = existingPacket.message.timeStamp;
      if (res.message.timer) {
        // Clean timer properties on the message.
        // Those properties are found on console.time and console.timeEnd calls,
        // and those time can vary, which is why we need to clean them.
        if (res.message.timer.duration) {
          res.message.timer.duration = existingPacket.message.timer.duration;
        }
      }

      if (Array.isArray(res.message.arguments)) {
        res.message.arguments.forEach((argument, i) => {
          let existingArgument = existingPacket.message.arguments[i];

          // Clean actor ids on each message.arguments item.
          if (argument && argument.actor) {
            argument.actor = existingArgument.actor;
          }

          // `window`'s properties count can vary from OS to OS, so we
          // clean the `ownPropertyLength` property from the grip.
          if (argument && argument.class === "Window") {
            argument.ownPropertyLength = existingArgument.ownPropertyLength;
          }
        });
      }
    }

    if (res.result) {
      // Clean actor ids on evaluation result messages.
      res.result.actor = existingPacket.result.actor;
      if (res.result.preview) {
        if (res.result.preview.timestamp) {
          // Clean timestamp there too.
          res.result.preview.timestamp = existingPacket.result.preview.timestamp;
        }
      }
    }

    if (res.exception) {
      // Clean actor ids on exception messages.
      res.exception.actor = existingPacket.exception.actor;
      if (res.exception.preview) {
        if (res.exception.preview.timestamp) {
          // Clean timestamp there too.
          res.exception.preview.timestamp = existingPacket.exception.preview.timestamp;
        }

        if (
          typeof res.exception.preview.message === "object"
          && res.exception.preview.message.type === "longString"
        ) {
          res.exception.preview.message.actor =
            existingPacket.exception.preview.message.actor;
        }
      }

      if (
        typeof res.exceptionMessage === "object"
        && res.exceptionMessage.type === "longString"
      ) {
        res.exceptionMessage.actor =
          existingPacket.exceptionMessage.actor;
      }
    }

    if (res.eventActor) {
      // Clean actor ids, timeStamp and startedDateTime on network messages.
      res.eventActor.actor = existingPacket.eventActor.actor;
      res.eventActor.startedDateTime = existingPacket.eventActor.startedDateTime;
      res.eventActor.timeStamp = existingPacket.eventActor.timeStamp;
    }

    if (res.pageError) {
      // Clean timeStamp on pageError messages.
      res.pageError.timeStamp = existingPacket.pageError.timeStamp;

      if (
        typeof res.pageError.errorMessage === "object"
        && res.pageError.errorMessage.type === "longString"
      ) {
        res.pageError.errorMessage.actor = existingPacket.pageError.errorMessage.actor;
      }
    }

    if (res.packet) {
      let override = {};
      let keys = ["totalTime", "from", "contentSize", "transferredSize"];
      keys.forEach(x => {
        if (res.packet[x] !== undefined) {
          override[x] = existingPacket.packet[key];
        }
      });
      res.packet = Object.assign({}, res.packet, override);
    }

    if (res.networkInfo) {
      if (res.networkInfo.timeStamp) {
        res.networkInfo.timeStamp = existingPacket.networkInfo.timeStamp;
      }

      if (res.networkInfo.startedDateTime) {
        res.networkInfo.startedDateTime = existingPacket.networkInfo.startedDateTime;
      }

      if (res.networkInfo.totalTime) {
        res.networkInfo.totalTime = existingPacket.networkInfo.totalTime;
      }

      if (res.networkInfo.actor) {
        res.networkInfo.actor = existingPacket.networkInfo.actor;
      }

      if (res.networkInfo.request && res.networkInfo.request.headersSize) {
        res.networkInfo.request.headersSize =
          existingPacket.networkInfo.request.headersSize;
      }

      if (
        res.networkInfo.response
        && res.networkInfo.response.headersSize !== undefined
      ) {
        res.networkInfo.response.headersSize =
          existingPacket.networkInfo.response.headersSize;
      }
      if (res.networkInfo.response && res.networkInfo.response.bodySize !== undefined) {
        res.networkInfo.response.bodySize =
          existingPacket.networkInfo.response.bodySize;
      }
      if (
        res.networkInfo.response
        && res.networkInfo.response.transferredSize !== undefined
      ) {
        res.networkInfo.response.transferredSize =
          existingPacket.networkInfo.response.transferredSize;
      }
    }
  } else {
    res = packet;
  }

  cachedPackets[key] = res;
  return res;
}

function formatPacket(key, packet) {
  let stringifiedPacket = JSON.stringify(getCleanedPacket(key, packet), null, 2);
  return `stubPackets.set("${key}", ${stringifiedPacket});`;
}

function formatStub(key, packet) {
  let prepared = prepareMessage(
    getCleanedPacket(key, packet),
    {getNextId: () => "1"}
  );
  let stringifiedMessage = JSON.stringify(prepared, null, 2);
  return `stubPreparedMessages.set("${key}", new ConsoleMessage(${stringifiedMessage}));`;
}

function formatNetworkEventStub(key, packet) {
  let cleanedPacket = getCleanedPacket(key, packet);
  let networkInfo = cleanedPacket.networkInfo ? cleanedPacket.networkInfo : cleanedPacket;

  let prepared = prepareMessage(
    networkInfo,
    {getNextId: () => "1"}
  );
  let stringifiedMessage = JSON.stringify(prepared, null, 2);
  return `stubPreparedMessages.set("${key}", ` +
    `new NetworkEventMessage(${stringifiedMessage}));`;
}

function formatFile(stubs, type) {
  return `/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-len */

"use strict";

/*
 * THIS FILE IS AUTOGENERATED. DO NOT MODIFY BY HAND. RUN TESTS IN FIXTURES/ TO UPDATE.
 */

const { ${type} } =
  require("devtools/client/webconsole/new-console-output/types");

let stubPreparedMessages = new Map();
let stubPackets = new Map();
${stubs.preparedMessages.join("\n\n")}

${stubs.packets.join("\n\n")}

module.exports = {
  stubPreparedMessages,
  stubPackets,
};
`;
}

function* generateConsoleApiStubs() {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-console-api.html";

  let stubs = {
    preparedMessages: [],
    packets: [],
  };

  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  let {ui} = toolbox.getCurrentPanel().hud;
  ok(ui.jsterm, "jsterm exists");
  ok(ui.newConsoleOutput, "newConsoleOutput exists");

  for (let [key, {keys, code}] of consoleApi) {
    let received = new Promise(resolve => {
      let i = 0;
      let listener = (type, res) => {
        stubs.packets.push(formatPacket(keys[i], res));
        stubs.preparedMessages.push(formatStub(keys[i], res));
        if (++i === keys.length) {
          toolbox.target.client.removeListener("consoleAPICall", listener);
          resolve();
        }
      };
      toolbox.target.client.addListener("consoleAPICall", listener);
    });

    yield ContentTask.spawn(
      gBrowser.selectedBrowser,
      [key, code],
      function ([subKey, subCode]) {
        let script = content.document.createElement("script");
        // eslint-disable-next-line no-unsanitized/property
        script.innerHTML = `function triggerPacket() {${subCode}}`;
        content.document.body.appendChild(script);
        content.wrappedJSObject.triggerPacket();
        script.remove();
      }
    );

    yield received;
  }

  yield closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
}

function* generateCssMessageStubs() {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-css-message.html";

  let stubs = {
    preparedMessages: [],
    packets: [],
  };

  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");

  for (let [key, code] of cssMessage) {
    let received = new Promise(resolve => {
      /* CSS errors are considered as pageError on the server */
      toolbox.target.client.addListener("pageError", function onPacket(e, packet) {
        toolbox.target.client.removeListener("pageError", onPacket);
        info("Received css message:" + e + " " + JSON.stringify(packet, null, "\t"));

        let message = prepareMessage(packet, {getNextId: () => 1});
        stubs.packets.push(formatPacket(message.messageText, packet));
        stubs.preparedMessages.push(formatStub(message.messageText, packet));
        resolve();
      });
    });

    yield ContentTask.spawn(
      gBrowser.selectedBrowser,
      [key, code],
      function ([subKey, subCode]) {
        let style = content.document.createElement("style");
        // eslint-disable-next-line no-unsanitized/property
        style.innerHTML = subCode;
        content.document.body.appendChild(style);
      }
    );

    yield received;
  }

  yield closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
}

function* generateEvaluationResultStubs() {
  const TEST_URI = "data:text/html;charset=utf-8,stub generation";

  let stubs = {
    preparedMessages: [],
    packets: [],
  };

  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");

  for (let [key, code] of evaluationResult) {
    const packet = yield new Promise(resolve => {
      toolbox.target.activeConsole.evaluateJS(code, resolve);
    });
    stubs.packets.push(formatPacket(key, packet));
    stubs.preparedMessages.push(formatStub(key, packet));
  }

  yield closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
}

function* generateNetworkEventStubs() {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-network-event.html";

  let stubs = {
    preparedMessages: [],
    packets: [],
  };

  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  let {ui} = toolbox.getCurrentPanel().hud;

  for (let [key, {keys, code}] of networkEvent) {
    let onNetwork = new Promise(resolve => {
      let i = 0;
      toolbox.target.activeConsole.on("networkEvent", function onNetworkEvent(type, res) {
        stubs.packets.push(formatPacket(keys[i], res));
        stubs.preparedMessages.push(formatNetworkEventStub(keys[i], res));
        if (++i === keys.length) {
          toolbox.target.activeConsole.off("networkEvent", onNetworkEvent);
          resolve();
        }
      });
    });

    let onNetworkUpdate = new Promise(resolve => {
      let i = 0;
      ui.jsterm.hud.on("network-message-updated", function onNetworkUpdated(event, res) {
        let updateKey = `${keys[i++]} update`;
        // We cannot ensure the form of the network update packet, some properties
        // might be in another order than in the original packet.
        // Hand-picking only what we need should prevent this.
        const packet = {
          networkInfo: {
            _type: res.networkInfo._type,
            actor: res.networkInfo.actor,
            request: res.networkInfo.request,
            response: res.networkInfo.response,
            totalTime: res.networkInfo.totalTime,
          }
        };

        stubs.packets.push(formatPacket(updateKey, packet));
        stubs.preparedMessages.push(formatNetworkEventStub(updateKey, res));
        if (i === keys.length) {
          ui.jsterm.hud.off("network-message-updated", onNetworkUpdated);
          resolve();
        }
      });
    });

    yield ContentTask.spawn(
      gBrowser.selectedBrowser,
      [key, code],
      function ([subKey, subCode]) {
        let script = content.document.createElement("script");
        // eslint-disable-next-line no-unsanitized/property
        script.innerHTML = `function triggerPacket() {${subCode}}`;
        content.document.body.appendChild(script);
        content.wrappedJSObject.triggerPacket();
        script.remove();
      }
    );

    yield Promise.all([onNetwork, onNetworkUpdate]);
  }

  yield closeTabAndToolbox();
  return formatFile(stubs, "NetworkEventMessage");
}

function* generatePageErrorStubs() {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-console-api.html";

  let stubs = {
    preparedMessages: [],
    packets: [],
  };

  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");

  for (let [key, code] of pageError) {
    let received = new Promise(resolve => {
      toolbox.target.client.addListener("pageError", function onPacket(e, packet) {
        toolbox.target.client.removeListener("pageError", onPacket);
        stubs.packets.push(formatPacket(key, packet));
        stubs.preparedMessages.push(formatStub(key, packet));
        resolve();
      });
    });

    // On e10s, the exception is triggered in child process
    // and is ignored by test harness
    // expectUncaughtException should be called for each uncaught exception.
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }

    yield ContentTask.spawn(
      gBrowser.selectedBrowser,
      [key, code],
      function ([subKey, subCode]) {
        let script = content.document.createElement("script");
        // eslint-disable-next-line no-unsanitized/property
        script.innerHTML = subCode;
        content.document.body.appendChild(script);
        script.remove();
      }
    );

    yield received;
  }

  yield closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
}
