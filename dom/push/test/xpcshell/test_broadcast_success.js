/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService, PushServiceWebSocket } = serviceExports;
// Create the profile directory early to ensure pushBroadcastService
// is initialized with the correct path
do_get_profile();
const { BroadcastService } = ChromeUtils.import(
  "resource://gre/modules/PushBroadcastService.jsm",
  null
);
const { JSONFile } = ChromeUtils.import("resource://gre/modules/JSONFile.jsm");

const { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
const { broadcastHandler } = ChromeUtils.import(
  "resource://test/broadcast_handler.jsm"
);

const broadcastService = pushBroadcastService;
const assert = Assert;
const userAgentID = "bd744428-f125-436a-b6d0-dd0c9845837f";
const channelID = "0ef2ad4a-6c49-41ad-af6e-95d2425276bf";

function run_test() {
  setPrefs({
    userAgentID,
    alwaysConnect: true,
    requestTimeout: 1000,
    retryBaseInterval: 150,
  });
  run_next_test();
}

function getPushServiceMock() {
  return {
    subscribed: [],
    subscribeBroadcast(broadcastId, version) {
      this.subscribed.push([broadcastId, version]);
    },
  };
}

add_task(async function test_register_success() {
  await broadcastService._resetListeners();
  const db = PushServiceWebSocket.newPushDB();
  broadcastHandler.reset();
  const notifications = broadcastHandler.notifications;
  let socket;
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  await broadcastService.addListener("broadcast-test", "2018-02-01", {
    moduleURI: "resource://test/broadcast_handler.jsm",
    symbolName: "broadcastHandler",
  });

  PushServiceWebSocket._generateID = () => channelID;

  var broadcastSubscriptions = [];

  let handshakeDone;
  let handshakePromise = new Promise(resolve => (handshakeDone = resolve));
  await PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(data) {
          socket = this;
          deepEqual(
            data.broadcasts,
            { "broadcast-test": "2018-02-01" },
            "Handshake: doesn't consult listeners"
          );
          equal(data.messageType, "hello", "Handshake: wrong message type");
          ok(
            !data.uaid,
            "Should not send UAID in handshake without local subscriptions"
          );
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
            })
          );
          handshakeDone();
        },

        onBroadcastSubscribe(data) {
          broadcastSubscriptions.push(data);
        },
      });
    },
  });
  await handshakePromise;

  socket.serverSendMsg(
    JSON.stringify({
      messageType: "broadcast",
      broadcasts: {
        "broadcast-test": "2018-03-02",
      },
    })
  );

  await broadcastHandler.wasNotified;

  deepEqual(
    notifications,
    [
      [
        "2018-03-02",
        "broadcast-test",
        { phase: broadcastService.PHASES.BROADCAST },
      ],
    ],
    "Broadcast notification didn't get delivered"
  );

  deepEqual(
    await broadcastService.getListeners(),
    {
      "broadcast-test": "2018-03-02",
    },
    "Broadcast version wasn't updated"
  );

  await broadcastService.addListener("example-listener", "2018-03-01", {
    moduleURI: "resource://gre/modules/not-real-example.jsm",
    symbolName: "doesntExist",
  });

  deepEqual(broadcastSubscriptions, [
    {
      messageType: "broadcast_subscribe",
      broadcasts: { "example-listener": "2018-03-01" },
    },
  ]);
});

add_task(async function test_handle_hello_broadcasts() {
  PushService.uninit();
  await broadcastService._resetListeners();
  let db = PushServiceWebSocket.newPushDB();
  broadcastHandler.reset();
  let notifications = broadcastHandler.notifications;
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  await broadcastService.addListener("broadcast-test", "2018-02-01", {
    moduleURI: "resource://test/broadcast_handler.jsm",
    symbolName: "broadcastHandler",
  });

  PushServiceWebSocket._generateID = () => channelID;

  await PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(data) {
          deepEqual(
            data.broadcasts,
            { "broadcast-test": "2018-02-01" },
            "Handshake: doesn't consult listeners"
          );
          equal(data.messageType, "hello", "Handshake: wrong message type");
          ok(
            !data.uaid,
            "Should not send UAID in handshake without local subscriptions"
          );
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
              broadcasts: {
                "broadcast-test": "2018-02-02",
              },
            })
          );
        },

        onBroadcastSubscribe(data) {},
      });
    },
  });

  await broadcastHandler.wasNotified;

  deepEqual(
    notifications,
    [
      [
        "2018-02-02",
        "broadcast-test",
        { phase: broadcastService.PHASES.HELLO },
      ],
    ],
    "Broadcast notification on hello was delivered"
  );

  deepEqual(
    await broadcastService.getListeners(),
    {
      "broadcast-test": "2018-02-02",
    },
    "Broadcast version wasn't updated"
  );
});

add_task(async function test_broadcast_context() {
  await broadcastService._resetListeners();
  const db = PushServiceWebSocket.newPushDB();
  broadcastHandler.reset();
  registerCleanupFunction(() => {
    return db.drop().then(() => db.close());
  });

  const serviceId = "broadcast-test";
  const version = "2018-02-01";
  await broadcastService.addListener(serviceId, version, {
    moduleURI: "resource://test/broadcast_handler.jsm",
    symbolName: "broadcastHandler",
  });

  // PushServiceWebSocket._generateID = () => channelID;

  await PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(data) {},
      });
    },
  });

  // Simulate registration.
  PushServiceWebSocket.sendSubscribeBroadcast(serviceId, version);

  // Simulate broadcast reply received by PushWebSocketListener.
  const message = JSON.stringify({
    messageType: "broadcast",
    broadcasts: {
      [serviceId]: version,
    },
  });
  PushServiceWebSocket._wsOnMessageAvailable({}, message);
  await broadcastHandler.wasNotified;

  deepEqual(
    broadcastHandler.notifications,
    [[version, serviceId, { phase: broadcastService.PHASES.REGISTER }]],
    "Broadcast passes REGISTER context"
  );

  // Simulate broadcast reply, without previous registration.
  broadcastHandler.reset();
  PushServiceWebSocket._wsOnMessageAvailable({}, message);
  await broadcastHandler.wasNotified;

  deepEqual(
    broadcastHandler.notifications,
    [[version, serviceId, { phase: broadcastService.PHASES.BROADCAST }]],
    "Broadcast passes BROADCAST context"
  );
});

add_task(async function test_broadcast_unit() {
  const fakeListenersData = {
    abc: {
      version: "2018-03-04",
      sourceInfo: {
        moduleURI: "resource://gre/modules/abc.jsm",
        symbolName: "getAbc",
      },
    },
    def: {
      version: "2018-04-05",
      sourceInfo: {
        moduleURI: "resource://gre/modules/def.jsm",
        symbolName: "getDef",
      },
    },
  };
  const path = FileTestUtils.getTempFile("broadcast-listeners.json").path;

  const jsonFile = new JSONFile({ path });
  jsonFile.data = {
    listeners: fakeListenersData,
  };
  await jsonFile._save();

  const pushServiceMock = getPushServiceMock();

  const mockBroadcastService = new BroadcastService(pushServiceMock, path);
  const listeners = await mockBroadcastService.getListeners();
  deepEqual(listeners, {
    abc: "2018-03-04",
    def: "2018-04-05",
  });

  await mockBroadcastService.addListener("ghi", "2018-05-06", {
    moduleURI: "resource://gre/modules/ghi.jsm",
    symbolName: "getGhi",
  });

  deepEqual(pushServiceMock.subscribed, [["ghi", "2018-05-06"]]);

  await mockBroadcastService._saveImmediately();

  const newJSONFile = new JSONFile({ path });
  await newJSONFile.load();

  deepEqual(newJSONFile.data, {
    listeners: {
      ...fakeListenersData,
      ghi: {
        version: "2018-05-06",
        sourceInfo: {
          moduleURI: "resource://gre/modules/ghi.jsm",
          symbolName: "getGhi",
        },
      },
    },
    version: 1,
  });

  deepEqual(await mockBroadcastService.getListeners(), {
    abc: "2018-03-04",
    def: "2018-04-05",
    ghi: "2018-05-06",
  });
});

add_task(async function test_broadcast_initialize_sane() {
  const path = FileTestUtils.getTempFile("broadcast-listeners.json").path;
  const mockBroadcastService = new BroadcastService(getPushServiceMock(), path);
  deepEqual(
    await mockBroadcastService.getListeners(),
    {},
    "listeners should start out sane"
  );
  await mockBroadcastService._saveImmediately();
  let onDiskJSONFile = new JSONFile({ path });
  await onDiskJSONFile.load();
  deepEqual(
    onDiskJSONFile.data,
    { listeners: {}, version: 1 },
    "written JSON file has listeners and version fields"
  );

  await mockBroadcastService.addListener("ghi", "2018-05-06", {
    moduleURI: "resource://gre/modules/ghi.jsm",
    symbolName: "getGhi",
  });

  await mockBroadcastService._saveImmediately();

  onDiskJSONFile = new JSONFile({ path });
  await onDiskJSONFile.load();

  deepEqual(
    onDiskJSONFile.data,
    {
      listeners: {
        ghi: {
          version: "2018-05-06",
          sourceInfo: {
            moduleURI: "resource://gre/modules/ghi.jsm",
            symbolName: "getGhi",
          },
        },
      },
      version: 1,
    },
    "adding listeners to initial state is written OK"
  );
});

add_task(async function test_broadcast_reject_invalid_sourceinfo() {
  const path = FileTestUtils.getTempFile("broadcast-listeners.json").path;
  const mockBroadcastService = new BroadcastService(getPushServiceMock(), path);

  await assert.rejects(
    mockBroadcastService.addListener("ghi", "2018-05-06", {
      moduleName: "resource://gre/modules/ghi.jsm",
      symbolName: "getGhi",
    }),
    /moduleURI must be a string/,
    "rejects sourceInfo that doesn't have moduleURI"
  );
});

add_task(async function test_broadcast_reject_version_not_string() {
  await assert.rejects(
    broadcastService.addListener(
      "ghi",
      {},
      {
        moduleURI: "resource://gre/modules/ghi.jsm",
        symbolName: "getGhi",
      }
    ),
    /version should be a string/,
    "rejects version that isn't a string"
  );
});

add_task(async function test_broadcast_reject_version_empty_string() {
  await assert.rejects(
    broadcastService.addListener("ghi", "", {
      moduleURI: "resource://gre/modules/ghi.jsm",
      symbolName: "getGhi",
    }),
    /version should not be an empty string/,
    "rejects version that is an empty string"
  );
});
