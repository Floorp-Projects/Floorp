/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the network actor uses the LongStringActor

const { DevToolsServer } = require("devtools/server/devtools-server");
const LONG_STRING_LENGTH = 400;
const LONG_STRING_INITIAL_LENGTH = 400;
let ORIGINAL_LONG_STRING_LENGTH, ORIGINAL_LONG_STRING_INITIAL_LENGTH;

add_task(async function() {
  const tab = await addTab(URL_ROOT + "network_requests_iframe.html");

  const target = await createAndAttachTargetForTab(tab);

  // Avoid mocha to try to load these module and fail while doing it when running node tests
  const {
    ResourceWatcher,
  } = require("devtools/shared/resources/resource-watcher");

  const commands = await target.descriptorFront.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();
  const resourceWatcher = new ResourceWatcher(targetList);

  // Override the default long string settings to lower values.
  // This is done from the parent process's DevToolsServer as the LongString
  // actor is being created from the parent process as network requests are
  // watched from the parent process.
  ORIGINAL_LONG_STRING_LENGTH = DevToolsServer.LONG_STRING_LENGTH;
  ORIGINAL_LONG_STRING_INITIAL_LENGTH =
    DevToolsServer.LONG_STRING_INITIAL_LENGTH;

  DevToolsServer.LONG_STRING_LENGTH = LONG_STRING_LENGTH;
  DevToolsServer.LONG_STRING_INITIAL_LENGTH = LONG_STRING_INITIAL_LENGTH;

  info("test network POST request");
  const networkResource = await new Promise(resolve => {
    resourceWatcher
      .watchResources([resourceWatcher.TYPES.NETWORK_EVENT], {
        onAvailable: () => {},
        onUpdated: resourceUpdate => {
          resolve(resourceUpdate[0].resource);
        },
      })
      .then(() => {
        // Spawn the network request after we started watching
        SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
          content.wrappedJSObject.testXhrPost();
        });
      });
  });

  const netActor = networkResource.actor;
  ok(netActor, "We have a netActor:" + netActor);

  const webConsoleFront = await target.getFront("console");
  const requestHeaders = await webConsoleFront.getRequestHeaders(netActor);
  assertRequestHeaders(requestHeaders);
  const requestCookies = await webConsoleFront.getRequestCookies(netActor);
  assertRequestCookies(requestCookies);
  const requestPostData = await webConsoleFront.getRequestPostData(netActor);
  assertRequestPostData(requestPostData);
  const responseHeaders = await webConsoleFront.getResponseHeaders(netActor);
  assertResponseHeaders(responseHeaders);
  const responseCookies = await webConsoleFront.getResponseCookies(netActor);
  assertResponseCookies(responseCookies);
  const responseContent = await webConsoleFront.getResponseContent(netActor);
  assertResponseContent(responseContent);
  const eventTimings = await webConsoleFront.getEventTimings(netActor);
  assertEventTimings(eventTimings);

  await target.destroy();

  DevToolsServer.LONG_STRING_LENGTH = ORIGINAL_LONG_STRING_LENGTH;
  DevToolsServer.LONG_STRING_INITIAL_LENGTH = ORIGINAL_LONG_STRING_INITIAL_LENGTH;
});

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
