/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the network actor uses the LongStringActor

const { DebuggerServer } = require("devtools/server/debugger-server");
const LONG_STRING_LENGTH = 400;
const LONG_STRING_INITIAL_LENGTH = 400;
let ORIGINAL_LONG_STRING_LENGTH, ORIGINAL_LONG_STRING_INITIAL_LENGTH;

add_task(async function() {
  const tab = await addTab(URL_ROOT + "network_requests_iframe.html");

  const target = await getTargetForTab(tab);
  const { client } = target;
  const consoleClient = target.activeConsole;

  await consoleClient.startListeners(["NetworkActivity"]);

  // Override the default long string settings to lower values.
  // This is done from the parent process's DebuggerServer as the LongString
  // actor is being created from the parent process as network requests are
  // watched from the parent process.
  ORIGINAL_LONG_STRING_LENGTH = DebuggerServer.LONG_STRING_LENGTH;
  ORIGINAL_LONG_STRING_INITIAL_LENGTH =
    DebuggerServer.LONG_STRING_INITIAL_LENGTH;

  DebuggerServer.LONG_STRING_LENGTH = LONG_STRING_LENGTH;
  DebuggerServer.LONG_STRING_INITIAL_LENGTH = LONG_STRING_INITIAL_LENGTH;

  info("test network POST request");

  const onNetworkEvent = consoleClient.once("serverNetworkEvent");
  const updates = [];
  let netActor = null;
  const onAllNetworkEventUpdateReceived = new Promise(resolve => {
    const onNetworkEventUpdate = packet => {
      updates.push(packet.updateType);
      assertNetworkEventUpdate(netActor, packet);

      if (
        updates.includes("responseContent") &&
        updates.includes("eventTimings")
      ) {
        client.off("networkEventUpdate", onNetworkEventUpdate);
        resolve();
      }
    };
    client.on("networkEventUpdate", onNetworkEventUpdate);
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    content.wrappedJSObject.testXhrPost();
  });

  info("Waiting for networkEvent");
  const netEvent = await onNetworkEvent;
  netActor = assertNetworkEvent(client, consoleClient, netEvent);

  info("Waiting for all networkEventUpdate");
  await onAllNetworkEventUpdateReceived;
  const requestHeaders = await consoleClient.getRequestHeaders(netActor);
  assertRequestHeaders(requestHeaders);
  const requestCookies = await consoleClient.getRequestCookies(netActor);
  assertRequestCookies(requestCookies);
  const requestPostData = await consoleClient.getRequestPostData(netActor);
  assertRequestPostData(requestPostData);
  const responseHeaders = await consoleClient.getResponseHeaders(netActor);
  assertResponseHeaders(responseHeaders);
  const responseCookies = await consoleClient.getResponseCookies(netActor);
  assertResponseCookies(responseCookies);
  const responseContent = await consoleClient.getResponseContent(netActor);
  assertResponseContent(responseContent);
  const eventTimings = await consoleClient.getEventTimings(netActor);
  assertEventTimings(eventTimings);

  await target.destroy();

  DebuggerServer.LONG_STRING_LENGTH = ORIGINAL_LONG_STRING_LENGTH;
  DebuggerServer.LONG_STRING_INITIAL_LENGTH = ORIGINAL_LONG_STRING_INITIAL_LENGTH;
});

function assertNetworkEvent(client, consoleClient, packet) {
  info("checking the network event packet");

  const netActor = packet.eventActor;

  checkObject(netActor, {
    actor: /[a-z]/,
    startedDateTime: /^\d+\-\d+\-\d+T.+$/,
    url: /data\.json/,
    method: "POST",
  });

  return netActor.actor;
}

function assertNetworkEventUpdate(netActor, packet) {
  info("received networkEventUpdate " + packet.updateType);
  is(packet.from, netActor, "networkEventUpdate actor");

  let expectedPacket = null;

  switch (packet.updateType) {
    case "requestHeaders":
    case "responseHeaders":
      ok(packet.headers > 0, "headers > 0");
      ok(packet.headersSize > 0, "headersSize > 0");
      break;
    case "requestCookies":
      expectedPacket = {
        cookies: 3,
      };
      break;
    case "requestPostData":
      ok(packet.dataSize > 0, "dataSize > 0");
      ok(!packet.discardRequestBody, "discardRequestBody");
      break;
    case "responseStart":
      expectedPacket = {
        response: {
          httpVersion: /^HTTP\/\d\.\d$/,
          status: "200",
          statusText: "OK",
          headersSize: /^\d+$/,
          discardResponseBody: false,
        },
      };
      break;
    case "securityInfo":
      expectedPacket = {
        state: "insecure",
      };
      break;
    case "responseCookies":
      expectedPacket = {
        cookies: 0,
      };
      break;
    case "responseContent":
      expectedPacket = {
        mimeType: "application/json",
        contentSize: /^\d+$/,
        discardResponseBody: false,
      };
      break;
    case "eventTimings":
      expectedPacket = {
        totalTime: /^\d+$/,
      };
      break;
    default:
      ok(false, "unknown network event update type: " + packet.updateType);
      return;
  }

  if (expectedPacket) {
    info("checking the packet content");
    checkObject(packet, expectedPacket);
  }
}

function assertRequestHeaders(response) {
  info("checking request headers");

  ok(response.headers.length > 0, "request headers > 0");
  ok(response.headersSize > 0, "request headersSize > 0");

  checkHeadersOrCookies(response.headers, {
    Referer: /network_requests_iframe\.html/,
    Cookie: /bug768096/,
  });
}

function assertRequestCookies(response) {
  info("checking request cookies");

  is(response.cookies.length, 3, "request cookies length");

  checkHeadersOrCookies(response.cookies, {
    foobar: "fooval",
    omgfoo: "bug768096",
    badcookie: "bug826798=st3fan",
  });
}

function assertRequestPostData(response) {
  info("checking request POST data");

  checkObject(response, {
    postData: {
      text: {
        type: "longString",
        initial: /^Hello world! foobaz barr.+foobaz barrfo$/,
        length: 563,
        actor: /[a-z]/,
      },
    },
    postDataDiscarded: false,
  });

  is(
    response.postData.text.initial.length,
    LONG_STRING_INITIAL_LENGTH,
    "postData text initial length"
  );
}

function assertResponseHeaders(response) {
  info("checking response headers");

  ok(response.headers.length > 0, "response headers > 0");
  ok(response.headersSize > 0, "response headersSize > 0");

  checkHeadersOrCookies(response.headers, {
    "content-type": /^application\/(json|octet-stream)$/,
    "content-length": /^\d+$/,
    "x-very-short": "hello world",
    "x-very-long": {
      type: "longString",
      length: 521,
      initial: /^Lorem ipsum.+\. Donec vitae d$/,
      actor: /[a-z]/,
    },
  });
}

function assertResponseCookies(response) {
  info("checking response cookies");

  is(response.cookies.length, 0, "response cookies length");
}

function assertResponseContent(response) {
  info("checking response content");

  checkObject(response, {
    content: {
      text: {
        type: "longString",
        initial: /^\{ id: "test JSON data"(.|\r|\n)+ barfoo ba$/g,
        length: 1070,
        actor: /[a-z]/,
      },
    },
    contentDiscarded: false,
  });

  is(
    response.content.text.initial.length,
    LONG_STRING_INITIAL_LENGTH,
    "content initial length"
  );
}

function assertEventTimings(response) {
  info("checking event timings");

  checkObject(response, {
    timings: {
      blocked: /^-1|\d+$/,
      dns: /^-1|\d+$/,
      connect: /^-1|\d+$/,
      send: /^-1|\d+$/,
      wait: /^-1|\d+$/,
      receive: /^-1|\d+$/,
    },
    totalTime: /^\d+$/,
  });
}
