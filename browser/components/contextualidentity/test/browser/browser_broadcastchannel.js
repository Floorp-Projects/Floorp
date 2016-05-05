let { classes: Cc, interfaces: Ci } = Components;

const BASE_ORIGIN = "http://example.com";
const URI = BASE_ORIGIN +
  "/browser/browser/components/contextualidentity/test/browser/empty_file.html";

// opens `uri' in a new tab with the provided userContextId and focuses it.
// returns the newly opened tab
function* openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = gBrowser.addTab(uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerDocument.defaultView.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

add_task(function* setup() {
  // make sure userContext is enabled.
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true]
    ]}, resolve);
  });
});

add_task(function* test() {
  let receiver = yield* openTabInUserContext(URI, 2);

  let channelName = "contextualidentity-broadcastchannel";

  // reflect the received message on title
  let receiveMsg = ContentTask.spawn(receiver.browser, channelName,
      function (name) {
        return new Promise(resolve => {
          content.window.bc = new content.window.BroadcastChannel(name);
          content.window.bc.onmessage = function (e) {
            content.document.title += e.data;
            resolve();
          }
        });
      });

  let sender1 = yield* openTabInUserContext(URI, 1);
  let sender2 = yield* openTabInUserContext(URI, 2);
  sender1.message = "Message from user context #1";
  sender2.message = "Message from user context #2";

  // send a message from a tab in different user context first
  // then send a message from a tab in the same user context
  for (let sender of [sender1, sender2]) {
    yield ContentTask.spawn(
        sender.browser,
        { name: channelName, message: sender.message },
        function (opts) {
          let bc = new content.window.BroadcastChannel(opts.name);
          bc.postMessage(opts.message);
        });
  }

  // make sure we have received a message
  yield receiveMsg;

  // Since sender1 sends before sender2, if the title is exactly
  // sender2's message, sender1's message must've been blocked
  is(receiver.browser.contentDocument.title, sender2.message,
      "should only receive messages from the same user context");

  gBrowser.removeTab(sender1.tab);
  gBrowser.removeTab(sender2.tab);
  gBrowser.removeTab(receiver.tab);
});
