/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../framework/test/shared-head.js */

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

const BASE_PATH = "../../../../devtools/client/webconsole/new-console-output/test/fixtures";
const TEMP_FILE_PATH = OS.Path.join(`${BASE_PATH}/stub-generators`, "test-tempfile.js");

let cachedPackets = {};

function getCleanedPacket(key, packet) {
  if(Object.keys(cachedPackets).includes(key)) {
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
  if(stubPackets.has(safeKey)) {

    let existingPacket = stubPackets.get(safeKey);
    res = Object.assign({}, packet, {
      from: existingPacket.from
    });

    // Clean root timestamp.
    if(res.timestamp) {
      res.timestamp = existingPacket.timestamp;
    }

    if (res.message) {
      // Clean timeStamp on the message prop.
      res.message.timeStamp = existingPacket.message.timeStamp;
      if (res.message.timer) {
        // Clean timer properties on the message.
        // Those properties are found on console.time and console.timeEnd calls,
        // and those time can vary, which is why we need to clean them.
        if (res.message.timer.started) {
          res.message.timer.started = existingPacket.message.timer.started;
        }
        if (res.message.timer.duration) {
          res.message.timer.duration = existingPacket.message.timer.duration;
        }
      }

      if(Array.isArray(res.message.arguments)) {
        // Clean actor ids on each message.arguments item.
        res.message.arguments.forEach((argument, i) => {
          if (argument && argument.actor) {
            argument.actor = existingPacket.message.arguments[i].actor;
          }
        });
      }
    }

    if (res.result) {
      // Clean actor ids on evaluation result messages.
      res.result.actor = existingPacket.result.actor;
      if (res.result.preview) {
        if(res.result.preview.timestamp) {
          // Clean timestamp there too.
          res.result.preview.timestamp = existingPacket.result.preview.timestamp;
        }
      }
    }

    if (res.exception) {
      // Clean actor ids on exception messages.
      res.exception.actor = existingPacket.exception.actor;
      if (res.exception.preview) {
        if(res.exception.preview.timestamp) {
          // Clean timestamp there too.
          res.exception.preview.timestamp = existingPacket.exception.preview.timestamp;
        }
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
    }

  } else {
    res = packet;
  }

  cachedPackets[key] = res;
  return res;
}

function formatPacket(key, packet) {
  return `
stubPackets.set("${key}", ${JSON.stringify(getCleanedPacket(key, packet), null, "\t")});
`;
}

function formatStub(key, packet) {
  let prepared = prepareMessage(
    getCleanedPacket(key, packet),
    {getNextId: () => "1"}
  );

  return `
stubPreparedMessages.set("${key}", new ConsoleMessage(${JSON.stringify(prepared, null, "\t")}));
`;
}

function formatNetworkStub(key, packet) {
  let actor = packet.eventActor;
  let networkInfo = {
    _type: "NetworkEvent",
    timeStamp: actor.timeStamp,
    node: null,
    actor: actor.actor,
    discardRequestBody: true,
    discardResponseBody: true,
    startedDateTime: actor.startedDateTime,
    request: {
      url: actor.url,
      method: actor.method,
    },
    isXHR: actor.isXHR,
    cause: actor.cause,
    response: {},
    timings: {},
    // track the list of network event updates
    updates: [],
    private: actor.private,
    fromCache: actor.fromCache,
    fromServiceWorker: actor.fromServiceWorker
  };
  let prepared = prepareMessage(networkInfo, {getNextId: () => "1"});
  return `
stubPreparedMessages.set("${key}", new NetworkEventMessage(${JSON.stringify(prepared, null, "\t")}));
`;
}

function formatFile(stubs) {
  return `/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * THIS FILE IS AUTOGENERATED. DO NOT MODIFY BY HAND. RUN TESTS IN FIXTURES/ TO UPDATE.
 */

const { ConsoleMessage, NetworkEventMessage } = require("devtools/client/webconsole/new-console-output/types");

let stubPreparedMessages = new Map();
let stubPackets = new Map();

${stubs.preparedMessages.join("")}
${stubs.packets.join("")}

module.exports = {
  stubPreparedMessages,
  stubPackets,
}`;
}
