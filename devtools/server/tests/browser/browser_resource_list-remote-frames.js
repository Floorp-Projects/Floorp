/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's allocation point in the chrome
 * process.
 */

"use strict";

add_task(async function() {
  await pushPref("fission.autostart", true);
  const tabTarget = await addTabTarget(MAIN_DOMAIN + "doc_iframe.html");
  await testLocalListFrames(tabTarget);
  await testBrowserListFrames(tabTarget);
});

async function testLocalListFrames(tabTarget) {
  // at this point, tabTarget is the tab with two iframes, one nested inside
  // the other.
  const { frames } = await tabTarget.listRemoteFrames();
  is(frames.length, 2, "Got two frames");

  // Since we do not have access to remote frames yet this will return null.
  // This test should be updated when we have access to remote frames.
  for (const frame of frames) {
    const frameTarget = await frame.getTarget();
    is(frameTarget, null, "We cannot get remote iframe fronts yet");
  }

  // However we can confirm that the newly created iframe is there.
  const browser = gBrowser.selectedBrowser;
  const oopID = await ContentTask.spawn(browser, {}, async () => {
    const oop = content.document.querySelector("iframe");
    return oop.frameLoader.browsingContext.id;
  });
  ok(
    frames.find(f => f.id === oopID),
    "tabTarget.listRemoteFrames returns the oop frame descriptor"
  );
}
async function testBrowserListFrames(tabTarget) {
  // Now, we can test against the entire browser. getMainProcess will return
  // a target for the parentProcess, and will be able to enumerate over all
  // the tabs, the remote iframe, and the pair of frames, one nested inside the other.
  const target = await tabTarget.client.mainRoot.getMainProcess();
  await getFrames(target, tabTarget);
}

async function getFrames(target) {
  const { frames } = await target.listRemoteFrames();

  // Connect to the tab which is being debugged, this is equivilant to the
  // browsing context of the tabTarget. The difference between tabTarget's browsing
  // context, and this browsing context, is we have less information in tabTarget,
  // since it is requesting information from a content process. From the parent
  // we can access information like the URL.
  const descriptor = frames.find(f => f.url && f.url.includes("doc_iframe"));

  const front = await descriptor.getTarget();
  ok(front.hasActor("console"), "Got the console actor");
  ok(front.hasActor("thread"), "Got the thread actor");
  // Ensure sending at least one request to an actor...
  const consoleFront = await front.getFront("console");
  const { result } = await consoleFront.evaluateJS("var a = 42; a");
  is(result, 42, "console.eval worked");

  // Although we can get metadata about the child frames,
  // since we do not have access to remote frames yet, this will return null.
  // This test should be updated when we have access to remote frames.
  const childFrames = frames.filter(d => d.parentID === descriptor.id);
  for (const frame of childFrames) {
    const frameTarget = await frame.getTarget();
    is(frameTarget, null, "We cannot get remote iframe fronts yet");
  }

  getFirstFrameAgain(front, descriptor, target);
}

// Assert that calling descriptor.getTarget returns the same actor.
async function getFirstFrameAgain(firstTargetFront, descriptor, target) {
  const targetFront = await descriptor.getTarget();

  is(
    targetFront,
    firstTargetFront,
    "Second call to getTarget with the same id returns the same form"
  );

  const { frames } = await target.listRemoteFrames();
  const secondDescriptor = frames.find(f => f.id === descriptor.id);
  const secondTargetFront = await secondDescriptor.getTarget();
  is(
    secondTargetFront,
    firstTargetFront,
    "Second call to listFrames with the same id returns the same form"
  );
}
