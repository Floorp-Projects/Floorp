/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { remoteClientManager } =
  require("devtools/client/shared/remote-debugging/remote-client-manager");
const { CONNECTION_TYPES } =
  require("devtools/client/shared/remote-debugging/constants");

add_task(async function testRemoteClientManager() {
  for (const type of Object.values(CONNECTION_TYPES)) {
    const fakeClient = createFakeClient();
    const runtimeInfo = {};
    const clientId = "clientId";
    const remoteId = remoteClientManager.getRemoteId(clientId, type);

    const connectionType = remoteClientManager.getConnectionTypeByRemoteId(remoteId);
    equal(connectionType, type,
      `[${type}]: Correct connection type was returned by getConnectionTypeByRemoteId`);

    equal(remoteClientManager.hasClient(clientId, type), false,
      `[${type}]: hasClient returns false if no client was set`);
    equal(remoteClientManager.getClient(clientId, type), null,
      `[${type}]: getClient returns null if no client was set`);
    equal(remoteClientManager.getClientByRemoteId(remoteId), null,
      `[${type}]: getClientByRemoteId returns null if no client was set`);
    equal(remoteClientManager.getRuntimeInfoByRemoteId(remoteId), null,
      `[${type}]: getRuntimeInfoByRemoteId returns null if no client was set`);

    remoteClientManager.setClient(clientId, type, fakeClient, runtimeInfo);
    equal(remoteClientManager.hasClient(clientId, type), true,
      `[${type}]: hasClient returns true`);
    equal(remoteClientManager.getClient(clientId, type), fakeClient,
      `[${type}]: getClient returns the correct client`);
    equal(remoteClientManager.getClientByRemoteId(remoteId), fakeClient,
      `[${type}]: getClientByRemoteId returns the correct client`);
    equal(remoteClientManager.getRuntimeInfoByRemoteId(remoteId), runtimeInfo,
      `[${type}]: getRuntimeInfoByRemoteId returns the correct object`);

    remoteClientManager.removeClient(clientId, type);
    equal(remoteClientManager.hasClient(clientId, type), false,
      `[${type}]: hasClient returns false after removing the client`);
    equal(remoteClientManager.getClient(clientId, type), null,
      `[${type}]: getClient returns null after removing the client`);
    equal(remoteClientManager.getClientByRemoteId(remoteId), null,
      `[${type}]: getClientByRemoteId returns null after removing the client`);
    equal(remoteClientManager.getRuntimeInfoByRemoteId(), null,
      `[${type}]: getRuntimeInfoByRemoteId returns null after removing the client`);
  }

  // Test various fallback scenarios for APIs relying on remoteId, when called without a
  // remoteId, we expect to get the information for the local this-firefox runtime.
  const { THIS_FIREFOX } = CONNECTION_TYPES;
  const thisFirefoxClient = createFakeClient();
  const thisFirefoxInfo = {};
  remoteClientManager.setClient(THIS_FIREFOX, THIS_FIREFOX, thisFirefoxClient,
    thisFirefoxInfo);

  equal(remoteClientManager.getClientByRemoteId(), thisFirefoxClient,
    `[fallback]: getClientByRemoteId returns this-firefox if remoteId is null`);
  equal(remoteClientManager.getRuntimeInfoByRemoteId(), thisFirefoxInfo,
    `[fallback]: getRuntimeInfoByRemoteId returns this-firefox if remoteId is null`);

  const otherRemoteId = remoteClientManager.getRemoteId("clientId", CONNECTION_TYPES.USB);
  equal(remoteClientManager.getClientByRemoteId(otherRemoteId), null,
    `[fallback]: getClientByRemoteId does not fallback if remoteId is non-null`);
  equal(remoteClientManager.getRuntimeInfoByRemoteId(otherRemoteId), null,
    `[fallback]: getRuntimeInfoByRemoteId does not fallback if remoteId is non-null`);
});

add_task(async function testRemoteClientManagerWithUnknownType() {
  const remoteId = remoteClientManager.getRemoteId("someClientId", "NotARealType");
  const connectionType = remoteClientManager.getConnectionTypeByRemoteId(remoteId);
  equal(connectionType, CONNECTION_TYPES.UNKNOWN,
    `Connection type UNKNOWN was returned by getConnectionTypeByRemoteId`);
});

function createFakeClient() {
  const EventEmitter = require("devtools/shared/event-emitter");

  const client = {};
  EventEmitter.decorate(client);

  // Define aliases expected by the remote-client-manager.
  client.addOneTimeListener = (evt, listener) => {
    return client.once(evt, listener);
  };
  client.addListener = (evt, listener) => {
    return client.on(evt, listener);
  };
  client.removeListener = (evt, listener) => {
    return client.off(evt, listener);
  };

  return client;
}
