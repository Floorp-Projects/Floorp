let { classes: Cc, interfaces: Ci } = Components;

const BASE_ORIGIN = "http://example.com";
const URI = BASE_ORIGIN +
  "/browser/browser/components/contextualidentity/test/browser/empty_file.html";

// opens `uri' in a new tab with the provided userContextId and focuses it.
// returns the newly opened tab
async function openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = BrowserTestUtils.addTab(gBrowser, uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

add_task(async function setup() {
  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true]
  ]});
});

add_task(async function test() {
  let receiver = await openTabInUserContext(URI, 2);

  let channelName = "contextualidentity-broadcastchannel";

  // reflect the received message on title
  await ContentTask.spawn(receiver.browser, channelName,
    function(name) {
      content.window.testPromise = new content.window.Promise(resolve => {
        content.window.bc = new content.window.BroadcastChannel(name);
        content.window.bc.onmessage = function(e) {
          content.document.title += e.data;
          resolve();
        }
      });
    }
  );

  let sender1 = await openTabInUserContext(URI, 1);
  let sender2 = await openTabInUserContext(URI, 2);
  sender1.message = "Message from user context #1";
  sender2.message = "Message from user context #2";

  // send a message from a tab in different user context first
  // then send a message from a tab in the same user context
  for (let sender of [sender1, sender2]) {
    await ContentTask.spawn(
        sender.browser,
        { name: channelName, message: sender.message },
        function(opts) {
          let bc = new content.window.BroadcastChannel(opts.name);
          bc.postMessage(opts.message);
        });
  }

  // Since sender1 sends before sender2, if the title is exactly
  // sender2's message, sender1's message must've been blocked
  await ContentTask.spawn(receiver.browser, sender2.message,
    async function(message) {
      await content.window.testPromise.then(function() {
        is(content.document.title, message,
           "should only receive messages from the same user context");
      });
    }
  );

  gBrowser.removeTab(sender1.tab);
  gBrowser.removeTab(sender2.tab);
  gBrowser.removeTab(receiver.tab);
});
