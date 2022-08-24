"use strict";

const TEST_PATH =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "dummy_page.html";

const TOPIC = "browsing-context-attached";

async function observeAttached(callback) {
  let attached = new Map();

  function observer(subject, topic, data) {
    is(topic, TOPIC, "observing correct topic");
    ok(BrowsingContext.isInstance(subject), "subject to be a BrowsingContext");
    is(typeof data, "string", "data to be a String");
    info(`*** bc id=${subject.id}, why=${data}`);
    attached.set(subject.id, { browsingContext: subject, why: data });
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
    attached.has(win.browsingContext.id),
    "got notification for window's chrome browsing context"
  );
  is(
    attached.get(win.browsingContext.id).why,
    "attach",
    "expected reason for chrome browsing context"
  );

  ok(
    attached.has(win.gBrowser.selectedBrowser.browsingContext.id),
    "got notification for toplevel browsing context"
  );
  is(
    attached.get(win.gBrowser.selectedBrowser.browsingContext.id).why,
    "attach",
    "expected reason for toplevel browsing context"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function toplevelForNewTab() {
  let tab;

  let attached = await observeAttached(async () => {
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  });

  ok(
    !attached.has(window.browsingContext.id),
    "no notification for the current window's chrome browsing context"
  );
  ok(
    attached.has(tab.linkedBrowser.browsingContext.id),
    "got notification for toplevel browsing context"
  );
  is(
    attached.get(tab.linkedBrowser.browsingContext.id).why,
    "attach",
    "expected reason for toplevel browsing context"
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
    !attached.has(window.browsingContext.id),
    "no notification for the current window's chrome browsing context"
  );
  ok(
    !attached.has(tab.linkedBrowser.browsingContext.id),
    "no notification for toplevel browsing context"
  );
  ok(
    attached.has(browsingContext.id),
    "got notification for frame's browsing context"
  );
  is(
    attached.get(browsingContext.id).why,
    "attach",
    "expected reason for frame's browsing context"
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
    attached.has(firstContext.id),
    "got notification for initial toplevel browsing context"
  );
  is(
    attached.get(firstContext.id).why,
    "attach",
    "expected reason for initial toplevel browsing context"
  );

  attached = await observeAttached(async () => {
    await loadURI(TEST_PATH);
  });
  const secondContext = tab.linkedBrowser.browsingContext;
  ok(
    attached.has(secondContext.id),
    "got notification for replaced toplevel browsing context"
  );
  isnot(secondContext, firstContext, "browsing context to be replaced");
  is(
    attached.get(secondContext.id).why,
    "replace",
    "expected reason for replaced toplevel browsing context"
  );
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
    attached.has(thirdContext.id),
    "got notification for replaced toplevel browsing context"
  );
  isnot(thirdContext, secondContext, "browsing context to be replaced");
  is(
    attached.get(thirdContext.id).why,
    "replace",
    "expected reason for replaced toplevel browsing context"
  );
  is(
    thirdContext.browserId,
    secondContext.browserId,
    "browserId has been kept"
  );

  BrowserTestUtils.removeTab(tab);
});
