"use strict";

const TOPIC = "browsing-context-discarded";

add_task(async function toplevel() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let bc = await SpecialPowers.spawn(
    win.gBrowser.selectedBrowser,
    [],
    () => content.docShell.browsingContext
  );

  let promise = BrowserUtils.promiseObserved(TOPIC, subject => subject === bc);
  await BrowserTestUtils.closeWindow(win);
  await promise;

  // If we make it here, then we've received the notification.
  ok(true, "got top-level notification");
});

add_task(async function subframe() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let bc = await SpecialPowers.spawn(win.gBrowser.selectedBrowser, [], () => {
    let iframe = content.document.createElement("iframe");
    content.document.body.appendChild(iframe);
    iframe.contentWindow.location = "https://example.com/";
    return iframe.browsingContext;
  });

  let promise = BrowserUtils.promiseObserved(TOPIC, subject => subject === bc);
  await BrowserTestUtils.closeWindow(win);
  await promise;

  // If we make it here, then we've received the notification.
  ok(true, "got subframe notification");
});
