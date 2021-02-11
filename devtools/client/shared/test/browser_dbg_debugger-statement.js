/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the behavior of the debugger statement.
 */

// Use distinct origins in order to use distinct processes when fission is enabled
const TAB_URL = URL_ROOT_COM + "doc_inline-debugger-statement.html";
const IFRAME_URL = URL_ROOT_ORG + "doc_inline-debugger-statement.html";

add_task(async () => {
  const tab = await addTab(TAB_URL);
  const tabBrowsingContext = tab.linkedBrowser.browsingContext;

  const iframeBrowsingContext = await SpecialPowers.spawn(
    tabBrowsingContext,
    [IFRAME_URL],
    async function(url) {
      const iframe = content.document.createElement("iframe");
      const onLoad = new Promise(r =>
        iframe.addEventListener("load", r, { once: true })
      );
      iframe.src = url;
      content.document.body.appendChild(iframe);
      await onLoad;
      return iframe.browsingContext;
    }
  );

  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const { client } = target;

  info("## Test debugger statement against the top level tab document");
  // This function, by calling the debugger statement function, will bump the increment
  const threadFront = await testEarlyDebuggerStatement(
    client,
    tabBrowsingContext,
    target
  );
  await testDebuggerStatement(client, tabBrowsingContext, threadFront, 1);

  info("## Test debugger statement againt a distinct origin iframe");
  if (isFissionEnabled()) {
    // We have to use the watcher in order to create the frame target
    // and also have to attach to it in order to later be able to
    // create the thread front
    const watcherFront = await target.getWatcherFront();
    await watcherFront.watchTargets("frame");
    const iframeTarget = await target.getBrowsingContextTarget(
      iframeBrowsingContext.id
    );
    await iframeTarget.attach();

    // This function, by calling the debugger statement function, will bump the increment
    const iframeThreadFront = await testEarlyDebuggerStatement(
      client,
      iframeBrowsingContext,
      iframeTarget
    );
    await testDebuggerStatement(
      client,
      iframeBrowsingContext,
      iframeThreadFront,
      1
    );
  } else {
    // But in this case, the increment will be 0 as the previous call to `testEarlyDebuggerStatement`
    // bumped the tab's document increment and not the iframe's one.
    await testDebuggerStatement(client, iframeBrowsingContext, threadFront, 0);
  }

  await target.destroy();
});

async function testEarlyDebuggerStatement(
  client,
  browsingContext,
  targetFront
) {
  const onPaused = function(packet) {
    ok(false, "Pause shouldn't be called before we've attached!");
  };

  // using the DevToolsClient to listen to the pause packet, as the
  // threadFront is not yet attached.
  client.on("paused", onPaused);

  // This should continue without nesting an event loop and calling
  // the onPaused hook, because we haven't attached yet.
  const increment = await SpecialPowers.spawn(
    browsingContext,
    [],
    async function() {
      content.wrappedJSObject.runDebuggerStatement();
      // Pile up another setTimeout in order to guarantee that the other one ran
      await new Promise(r => content.setTimeout(r));
      return content.wrappedJSObject.increment;
    }
  );
  is(increment, 1, "As the thread wasn't paused, setTimeout worked");

  client.off("paused", onPaused);

  // Now attach
  const threadFront = await targetFront.attachThread();
  ok(true, "Pause wasn't called before we've attached.");

  return threadFront;
}

async function testDebuggerStatement(
  client,
  browsingContext,
  threadFront,
  incrementOriginalValue
) {
  const onPaused = threadFront.once("paused");

  // Reach around the debugging protocol and execute the debugger statement.
  // Not that this will be paused and spawn will only resolve once
  // the thread will be resumed
  const onResumed = SpecialPowers.spawn(browsingContext, [], function() {
    content.wrappedJSObject.runDebuggerStatement();
  });

  info("Waiting for paused event");
  await onPaused;
  ok(true, "The pause handler was triggered on a debugger statement.");

  // Pile up another setTimeout in order to guarantee that the other did not run
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(r => setTimeout(r, 1000));

  let increment = await SpecialPowers.spawn(
    browsingContext,
    [],
    async function() {
      return content.wrappedJSObject.increment;
    }
  );
  is(
    increment,
    incrementOriginalValue,
    "setTimeout are frozen while the thread is paused"
  );

  await threadFront.resume();
  await onResumed;

  increment = await SpecialPowers.spawn(browsingContext, [], async function() {
    // Pile up another setTimeout in order to guarantee that the other did run
    await new Promise(r => content.setTimeout(r));
    return content.wrappedJSObject.increment;
  });
  is(
    increment,
    incrementOriginalValue + 1,
    "setTimeout are resumed after the thread is resumed"
  );
}
