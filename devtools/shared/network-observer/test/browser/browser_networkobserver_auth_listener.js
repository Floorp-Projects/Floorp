/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

const TEST_URL = URL_ROOT + "doc_network-observer.html";
const AUTH_URL = URL_ROOT + `sjs_network-auth-listener-test-server.sjs`;

// Correct credentials for sjs_network-auth-listener-test-server.sjs.
const USERNAME = "guest";
const PASSWORD = "guest";
const BAD_PASSWORD = "bad";

// NetworkEventOwner which will cancel all auth prompt requests.
class AuthCancellingOwner extends NetworkEventOwner {
  hasAuthPrompt = false;

  onAuthPrompt(authDetails, authCallbacks) {
    this.hasAuthPrompt = true;
    authCallbacks.cancelAuthPrompt();
  }
}

// NetworkEventOwner which will forward all auth prompt requests to the browser.
class AuthForwardingOwner extends NetworkEventOwner {
  hasAuthPrompt = false;

  onAuthPrompt(authDetails, authCallbacks) {
    this.hasAuthPrompt = true;
    authCallbacks.forwardAuthPrompt();
  }
}

// NetworkEventOwner which will answer provided credentials to auth prompts.
class AuthCredentialsProvidingOwner extends NetworkEventOwner {
  hasAuthPrompt = false;

  constructor(channel, username, password) {
    super();

    this.channel = channel;
    this.username = username;
    this.password = password;
  }

  async onAuthPrompt(authDetails, authCallbacks) {
    this.hasAuthPrompt = true;

    // Providing credentials immediately can lead to intermittent failures.
    // TODO: Investigate and remove.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 100));

    await authCallbacks.provideAuthCredentials(this.username, this.password);
  }

  addResponseContent(content) {
    super.addResponseContent();
    this.responseContent = content.text;
  }
}

add_task(async function testAuthRequestWithoutListener() {
  cleanupAuthManager();
  const tab = await addTab(TEST_URL);

  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== AUTH_URL,
    onNetworkEvent: event => {
      const owner = new AuthForwardingOwner();
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  const onAuthPrompt = waitForAuthPrompt(tab);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [AUTH_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  info("Wait for a network event to be created");
  await BrowserTestUtils.waitForCondition(() => events.length >= 1);
  is(events.length, 1, "Received the expected number of network events");

  info("Wait for the auth prompt to be displayed");
  await onAuthPrompt;
  ok(
    getTabAuthPrompts(tab).length == 1,
    "The auth prompt was not blocked by the network observer"
  );

  // The event owner should have been called for ResponseStart and EventTimings
  assertEventOwner(events[0], {
    hasResponseStart: true,
    hasEventTimings: true,
    hasServerTimings: true,
  });

  networkObserver.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function testAuthRequestWithForwardingListener() {
  cleanupAuthManager();
  const tab = await addTab(TEST_URL);

  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== AUTH_URL,
    onNetworkEvent: event => {
      info("waitForNetworkEvents received a new event");
      const owner = new AuthForwardingOwner();
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  info("Enable the auth prompt listener for this network observer");
  networkObserver.setAuthPromptListenerEnabled(true);

  const onAuthPrompt = waitForAuthPrompt(tab);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [AUTH_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  info("Wait for a network event to be received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 1);
  is(events.length, 1, "Received the expected number of network events");

  // The auth prompt should still be displayed since the network event owner
  // forwards the auth notification immediately.
  info("Wait for the auth prompt to be displayed");
  await onAuthPrompt;
  ok(
    getTabAuthPrompts(tab).length == 1,
    "The auth prompt was not blocked by the network observer"
  );

  // The event owner should have been called for ResponseStart, EventTimings and
  // AuthPrompt
  assertEventOwner(events[0], {
    hasResponseStart: true,
    hasEventTimings: true,
    hasAuthPrompt: true,
    hasServerTimings: true,
  });

  networkObserver.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function testAuthRequestWithCancellingListener() {
  cleanupAuthManager();
  const tab = await addTab(TEST_URL);

  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== AUTH_URL,
    onNetworkEvent: event => {
      const owner = new AuthCancellingOwner();
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  info("Enable the auth prompt listener for this network observer");
  networkObserver.setAuthPromptListenerEnabled(true);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [AUTH_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  info("Wait for a network event to be received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 1);
  is(events.length, 1, "Received the expected number of network events");

  await BrowserTestUtils.waitForCondition(
    () => events[0].hasResponseContent && events[0].hasSecurityInfo
  );

  // The auth prompt should not be displayed since the authentication was
  // cancelled.
  ok(
    !getTabAuthPrompts(tab).length,
    "The auth prompt was cancelled by the network event owner"
  );

  assertEventOwner(events[0], {
    hasResponseStart: true,
    hasResponseContent: true,
    hasEventTimings: true,
    hasServerTimings: true,
    hasAuthPrompt: true,
    hasSecurityInfo: true,
  });

  networkObserver.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function testAuthRequestWithWrongCredentialsListener() {
  cleanupAuthManager();
  const tab = await addTab(TEST_URL);

  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== AUTH_URL,
    onNetworkEvent: (event, channel) => {
      const owner = new AuthCredentialsProvidingOwner(
        channel,
        USERNAME,
        BAD_PASSWORD
      );
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  info("Enable the auth prompt listener for this network observer");
  networkObserver.setAuthPromptListenerEnabled(true);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [AUTH_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  info("Wait for all network events to be received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 1);
  is(events.length, 1, "Received the expected number of network events");

  // Wait for authPrompt to be handled
  await BrowserTestUtils.waitForCondition(() => events[0].hasAuthPrompt);

  // The auth prompt should not be displayed since the authentication was
  // fulfilled.
  ok(
    !getTabAuthPrompts(tab).length,
    "The auth prompt was handled by the network event owner"
  );

  assertEventOwner(events[0], {
    hasAuthPrompt: true,
    hasResponseStart: true,
    hasEventTimings: true,
    hasServerTimings: true,
  });

  networkObserver.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function testAuthRequestWithCredentialsListener() {
  cleanupAuthManager();
  const tab = await addTab(TEST_URL);

  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== AUTH_URL,
    onNetworkEvent: (event, channel) => {
      const owner = new AuthCredentialsProvidingOwner(
        channel,
        USERNAME,
        PASSWORD
      );
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  info("Enable the auth prompt listener for this network observer");
  networkObserver.setAuthPromptListenerEnabled(true);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [AUTH_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  // TODO: At the moment, providing credentials will result in additional
  // network events collected by the NetworkObserver, whereas we would expect
  // to keep the same event.
  // For successful auth prompts, we receive an additional event.
  // The last event will contain the responseContent flag.
  info("Wait for all network events to be received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 2);
  is(events.length, 2, "Received the expected number of network events");

  // Since the auth prompt was canceled we should also receive the security
  // information and the response content.
  await BrowserTestUtils.waitForCondition(
    () => events[1].hasResponseContent && events[1].hasSecurityInfo
  );

  // The auth prompt should not be displayed since the authentication was
  // fulfilled.
  ok(
    !getTabAuthPrompts(tab).length,
    "The auth prompt was handled by the network event owner"
  );

  assertEventOwner(events[1], {
    hasResponseStart: true,
    hasEventTimings: true,
    hasSecurityInfo: true,
    hasServerTimings: true,
    hasResponseContent: true,
  });

  is(events[1].responseContent, "success", "Auth prompt was successful");

  networkObserver.destroy();
  gBrowser.removeTab(tab);
});

function assertEventOwner(event, expectedFlags) {
  is(
    event.hasResponseStart,
    !!expectedFlags.hasResponseStart,
    "network event has the expected ResponseStart flag"
  );
  is(
    event.hasEventTimings,
    !!expectedFlags.hasEventTimings,
    "network event has the expected EventTimings flag"
  );
  is(
    event.hasAuthPrompt,
    !!expectedFlags.hasAuthPrompt,
    "network event has the expected AuthPrompt flag"
  );
  is(
    event.hasResponseCache,
    !!expectedFlags.hasResponseCache,
    "network event has the expected ResponseCache flag"
  );
  is(
    event.hasResponseContent,
    !!expectedFlags.hasResponseContent,
    "network event has the expected ResponseContent flag"
  );
  is(
    event.hasSecurityInfo,
    !!expectedFlags.hasSecurityInfo,
    "network event has the expected SecurityInfo flag"
  );
  is(
    event.hasServerTimings,
    !!expectedFlags.hasServerTimings,
    "network event has the expected ServerTimings flag"
  );
}

function getTabAuthPrompts(tab) {
  const tabDialogBox = gBrowser.getTabDialogBox(tab.linkedBrowser);
  return tabDialogBox
    .getTabDialogManager()
    ._dialogs.filter(
      d => d.frameContentWindow?.Dialog.args.promptType == "promptUserAndPass"
    );
}

function waitForAuthPrompt(tab) {
  return PromptTestUtils.waitForPrompt(tab.linkedBrowser, {
    modalType: Services.prompt.MODAL_TYPE_TAB,
    promptType: "promptUserAndPass",
  });
}

// Cleanup potentially stored credentials before running any test.
function cleanupAuthManager() {
  const authManager = SpecialPowers.Cc[
    "@mozilla.org/network/http-auth-manager;1"
  ].getService(SpecialPowers.Ci.nsIHttpAuthManager);
  authManager.clearAll();
}
