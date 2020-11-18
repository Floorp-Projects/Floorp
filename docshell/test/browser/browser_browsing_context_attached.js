"use strict";

const TEST_PATH =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "dummy_page.html";

const TOPIC = "browsing-context-attached";

async function observeAttached(callback) {
  let attached = [];
  function observer(subject, topic) {
    is(topic, TOPIC, "observing correct topic");
    ok(subject instanceof BrowsingContext, "subject to be a BrowsingContext");
    info(`*** bc id: ${subject.id}`);
    attached.push(subject);
  }
  Services.obs.addObserver(observer, TOPIC);
  try {
    await callback();
    return attached;
  } finally {
    Services.obs.removeObserver(observer, TOPIC);
  }
}

add_task(async function toplevelForNewWindow() {
  let win;

  let attached = await observeAttached(async () => {
    win = await BrowserTestUtils.openNewBrowserWindow();
  });

  ok(
    attached.includes(win.browsingContext),
    "got notification for window's chrome browsing context"
  );
  ok(
    attached.includes(win.gBrowser.selectedBrowser.browsingContext),
    "got notification for toplevel browsing context"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function toplevelForNewTab() {
  let tab;

  let attached = await observeAttached(async () => {
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  });

  ok(
    !attached.includes(window.browsingContext),
    "no notification for the current window's chrome browsing context"
  );
  ok(
    attached.includes(tab.linkedBrowser.browsingContext),
    "got notification for toplevel browsing context"
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function subframe() {
  let browsingContext;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let attached = await observeAttached(async () => {
    browsingContext = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      let iframe = content.document.createElement("iframe");
      content.document.body.appendChild(iframe);
      iframe.contentWindow.location = "https://example.com/";
      return iframe.browsingContext;
    });
  });

  ok(
    !attached.includes(window.browsingContext),
    "no notification for the current window's chrome browsing context"
  );
  ok(
    !attached.includes(tab.linkedBrowser.browsingContext),
    "no notification for toplevel browsing context"
  );
  ok(
    attached.includes(browsingContext),
    "got notification for frame's browsing context"
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function toplevelReplacedBy() {
  let tab;

  let attached = await observeAttached(async () => {
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  });

  const firstContext = tab.linkedBrowser.browsingContext;
  ok(
    attached.includes(firstContext),
    "got notification for initial toplevel browsing context"
  );

  attached = await observeAttached(async () => {
    await loadURI(TEST_PATH);
  });
  const secondContext = tab.linkedBrowser.browsingContext;
  ok(
    attached.includes(secondContext),
    "got notification for replaced toplevel browsing context"
  );
  isnot(secondContext, firstContext, "browsing context to be replaced");
  is(
    secondContext.browserId,
    firstContext.browserId,
    "browserId has been kept"
  );

  attached = await observeAttached(async () => {
    await loadURI("about:robots");
  });
  const thirdContext = tab.linkedBrowser.browsingContext;
  ok(
    attached.includes(thirdContext),
    "got notification for replaced toplevel browsing context"
  );
  isnot(thirdContext, secondContext, "browsing context to be replaced");
  is(
    thirdContext.browserId,
    secondContext.browserId,
    "browserId has been kept"
  );

  BrowserTestUtils.removeTab(tab);
});
