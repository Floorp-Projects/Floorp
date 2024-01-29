/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ObjectCommand

add_task(async function testObjectRelease() {
  const tab = await addTab("data:text/html;charset=utf-8,Test page<script>var foo = { bar: 42 };</script>");

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const { objectCommand } = commands;

  const evaluationResponse = await commands.scriptCommand.execute(
    "window.foo"
  );

  // Execute a second time so that the WebConsoleActor set this._lastConsoleInputEvaluation to another value
  // and so we prevent freeing `window.foo` 
  await commands.scriptCommand.execute("");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    is(content.wrappedJSObject.foo.bar, 42);
    const weakRef = Cu.getWeakReference(content.wrappedJSObject.foo);

    // Hold off the weak reference on SpecialPowsers so that it can be accessed in the next SpecialPowers.spawn
    SpecialPowers.weakRef = weakRef;

    // Nullify this variable so that it should be freed
    // unless the DevTools inspection still hold it in memory
    content.wrappedJSObject.foo = null;

    Cu.forceGC();
    Cu.forceCC();

    ok(SpecialPowers.weakRef.get(), "The 'foo' object can't be freed because of DevTools keeping a reference on it");
  });

  info("Release the server side actors which are keeping the object in memory");
  const objectFront = evaluationResponse.result;
  await commands.objectCommand.releaseObjects([objectFront]);

  ok(objectFront.isDestroyed(), "The passed object front has been destroyed");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      Cu.forceGC();
      Cu.forceCC();
      return !SpecialPowers.weakRef.get();
    }, "Wait for JS object to be freed", 500);

    ok(!SpecialPowers.weakRef.get(), "The 'foo' object has been freed");
  });

  await commands.destroy();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testMultiTargetObjectRelease() {
   // This test fails with EFT disabled
  if (!isEveryFrameTargetEnabled()) {
    return;
  }

  const tab = await addTab(`data:text/html;charset=utf-8,Test page<iframe src="data:text/html,bar">/iframe>`);

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const [,iframeTarget] = commands.targetCommand.getAllTargets(commands.targetCommand.ALL_TYPES);
  is(iframeTarget.url, "data:text/html,bar");

  const { objectCommand } = commands;

  const evaluationResponse1 = await commands.scriptCommand.execute(
    "window"
  );
  const evaluationResponse2 = await commands.scriptCommand.execute(
    "window", {
      selectedTargetFront: iframeTarget,
    }
  );
  const object1 = evaluationResponse1.result;
  const object2 = evaluationResponse2.result;
  isnot(object1, object2, "The two window object fronts are different");
  isnot(object1.targetFront, object2.targetFront, "The two window object fronts relates to two distinct targets");
  is(object2.targetFront, iframeTarget, "The second object relates to the iframe target");

  await commands.objectCommand.releaseObjects([object1, object2]);
  ok(object1.isDestroyed(), "The first object front is destroyed");
  ok(object2.isDestroyed(), "The second object front is destroyed");

  await commands.destroy();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testWorkerObjectRelease() {
  const workerUrl = `data:text/javascript,const foo = {}`;
  const tab = await addTab(`data:text/html;charset=utf-8,Test page<script>const worker = new Worker("${workerUrl}")</script>`);

  const commands = await CommandsFactory.forTab(tab);
  commands.targetCommand.listenForWorkers = true;
  await commands.targetCommand.startListening();

  const [,workerTarget] = commands.targetCommand.getAllTargets(commands.targetCommand.ALL_TYPES);
  is(workerTarget.url, workerUrl);

  const { objectCommand } = commands;

  const evaluationResponse = await commands.scriptCommand.execute(
    "foo", {
      selectedTargetFront: workerTarget,
    }
  );
  const object = evaluationResponse.result;
  is(object.targetFront, workerTarget, "The 'foo' object relates to the worker target");

  await commands.objectCommand.releaseObjects([object]);
  ok(object.isDestroyed(), "The object front is destroyed");

  await commands.destroy();
  BrowserTestUtils.removeTab(tab);
});
