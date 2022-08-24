"use strict";

const TOPIC = "browsing-context-discarded";

async function observeDiscarded(browsingContexts, callback) {
  let discarded = [];

  let promise = BrowserUtils.promiseObserved(TOPIC, subject => {
    ok(BrowsingContext.isInstance(subject), "subject to be a BrowsingContext");
    discarded.push(subject);

    return browsingContexts.every(item => discarded.includes(item));
  });
  await callback();
  await promise;

  return discarded;
}

add_task(async function toplevelForNewWindow() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let browsingContext = win.gBrowser.selectedBrowser.browsingContext;

  await observeDiscarded([win.browsingContext, browsingContext], async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function toplevelForNewTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browsingContext = tab.linkedBrowser.browsingContext;

  let discarded = await observeDiscarded([browsingContext], () => {
    BrowserTestUtils.removeTab(tab);
  });

  ok(
    !discarded.includes(window.browsingContext),
    "no notification for the current window's chrome browsing context"
  );
});

add_task(async function subframe() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browsingContext = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    let iframe = content.document.createElement("iframe");
    content.document.body.appendChild(iframe);
    iframe.contentWindow.location = "https://example.com/";
    return iframe.browsingContext;
  });

  let discarded = await observeDiscarded([browsingContext], async () => {
    await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      let iframe = content.document.querySelector("iframe");
      iframe.remove();
    });
  });

  ok(
    !discarded.includes(tab.browsingContext),
    "no notification for toplevel browsing context"
  );

  BrowserTestUtils.removeTab(tab);
});
