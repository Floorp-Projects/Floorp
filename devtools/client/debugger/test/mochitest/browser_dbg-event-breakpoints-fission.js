/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

 // Tests early event breakpoints and event breakpoints in a remote frame.

add_task(async function () {
    await pushPref(
    "devtools.debugger.features.event-listeners-breakpoints",
    true
  );

  const dbg = await initDebugger(
    "doc-event-breakpoints-fission.html",
    "event-breakpoints"
  );

  await selectSource(dbg, "event-breakpoints");
  await waitForSelectedSource(dbg, "event-breakpoints");

  await dbg.actions.addEventListenerBreakpoints([
    "event.mouse.click",
    "event.xhr.load",
    "timer.timeout.set"
  ]);

  info("Assert early timeout event breakpoint gets hit");
  const waitForReload = reloadBrowser();

  await waitForPaused(dbg);
  assertPauseLocation(dbg, 17, "doc-event-breakpoints-fission.html");
  await resume(dbg);

  await waitForReload;

  info("Assert event breakpoints work in remote frame");
  await invokeAndAssertBreakpoints(dbg);

  info("reload the iframe")
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.wrappedJSObject.reloadIframe()
  );
  info("Assert event breakpoints work in remote frame after reload");
  await invokeAndAssertBreakpoints(dbg);
});

async function invokeAndAssertBreakpoints(dbg) {
  invokeInTabRemoteFrame("clickHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 12);
  await resume(dbg);

  invokeInTabRemoteFrame("xhrHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 20);
  await resume(dbg);
}

function assertPauseLocation(dbg, line, url = "event-breakpoints.js") {
  const { location } = dbg.selectors.getVisibleSelectedFrame();
  const selectedSource = dbg.selectors.getSelectedSourceWithContent();

  is(location.sourceId, selectedSource.id, `Correct selected sourceId`);
  ok(selectedSource.url.includes(url), "Correct url");
  is(location.line, line, "Correct paused line");

  assertPausedLocation(dbg);
}

async function invokeInTabRemoteFrame(fnc, ...args) {
  info(`Invoking in tab remote frame: ${fnc}(${args.map(uneval).join(",")})`);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [fnc, args], function (_fnc, _args) {
    return SpecialPowers.spawn(
      content.document.querySelector("iframe"),
      [_fnc, _args],
      (__fnc, __args) => content.wrappedJSObject[__fnc](...__args)
    );
  });
}

