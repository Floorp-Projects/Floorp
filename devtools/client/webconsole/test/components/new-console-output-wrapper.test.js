/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test utils.
const expect = require("expect");
const { stubPackets } =
  require("devtools/client/webconsole/test/fixtures/stubs/index");
const {
  getFirstMessage,
  getLastMessage,
  getMessageAt,
  getPrivatePacket,
} = require("devtools/client/webconsole/test/helpers");

const NewConsoleOutputWrapper =
  require("devtools/client/webconsole/new-console-output-wrapper");
const { messagesAdd } =
  require("devtools/client/webconsole/actions/messages");

function getNewConsoleOutputWrapper() {
  const hud = {
    proxy: {
      releaseActor: () => {},
      target: {
        activeTab: {
          ensureCSSErrorReportingEnabled: () => {}
        }
      },
    },
  };
  return new NewConsoleOutputWrapper(null, hud);
}

describe("NewConsoleOutputWrapper", () => {
  it("clears queues when dispatchMessagesClear is called", () => {
    const ncow = getNewConsoleOutputWrapper();
    ncow.queuedMessageAdds.push({fakePacket: "message"});
    ncow.queuedMessageUpdates.push({fakePacket: "message-update"});
    ncow.queuedRequestUpdates.push({fakePacket: "request-update"});

    ncow.dispatchMessagesClear();

    expect(ncow.queuedMessageAdds.length).toBe(0);
    expect(ncow.queuedMessageUpdates.length).toBe(0);
    expect(ncow.queuedRequestUpdates.length).toBe(0);
  });

  it("removes private packets from message queue on dispatchPrivateMessagesClear", () => {
    const ncow = getNewConsoleOutputWrapper();

    const publicLog = stubPackets.get("console.log('mymap')");
    ncow.queuedMessageAdds.push(
      getPrivatePacket("console.trace()"),
      publicLog,
      getPrivatePacket("XHR POST request"),
    );

    ncow.dispatchPrivateMessagesClear();

    expect(ncow.queuedMessageAdds).toEqual([publicLog]);
  });

  it("removes private packets from network update queue on dispatchPrivateMessagesClear",
    () => {
      const ncow = getNewConsoleOutputWrapper();

      const postId = Symbol();
      const getId = Symbol();

      // Add messages in the store to make sure that update to private requests are
      // removed from the queue.
      ncow.getStore().dispatch(messagesAdd([
        stubPackets.get("GET request"),
        {...getPrivatePacket("XHR GET request"), actor: getId},
      ]));

      // Add packet to the message queue to make sure that update to private requests are
      // removed from the queue.
      ncow.queuedMessageAdds.push(
        {...getPrivatePacket("XHR POST request"), actor: postId},
      );

      const publicNetworkUpdate = stubPackets.get("GET request update");
      ncow.queuedMessageUpdates.push(
        publicNetworkUpdate,
        {...getPrivatePacket("XHR GET request update"), networkInfo: {actor: getId}},
        {...getPrivatePacket("XHR POST request update"), networkInfo: {actor: postId}},
      );

      ncow.dispatchPrivateMessagesClear();

      expect(ncow.queuedMessageUpdates.length).toBe(1);
      expect(ncow.queuedMessageUpdates).toEqual([publicNetworkUpdate]);
    });

  it("removes private packets from network request queue on dispatchPrivateMessagesClear",
    () => {
      const ncow = getNewConsoleOutputWrapper();

      ncow.getStore().dispatch(messagesAdd([
        stubPackets.get("GET request"),
        getPrivatePacket("XHR GET request"),
        getPrivatePacket("XHR POST request"),
      ]));

      const state = ncow.getStore().getState();
      const publicId = getFirstMessage(state).id;
      const privateXhrGetId = getMessageAt(state, 1).id;
      const privateXhrPostId = getLastMessage(state).id;
      ncow.queuedRequestUpdates.push(
        {id: publicId},
        {id: privateXhrGetId},
        {id: privateXhrPostId},
      );
      // ncow.queuedRequestUpdates.push({fakePacket: "request-update"});

      ncow.dispatchPrivateMessagesClear();

      expect(ncow.queuedRequestUpdates.length).toBe(1);
      expect(ncow.queuedRequestUpdates).toEqual([{id: publicId}]);
    });
});
