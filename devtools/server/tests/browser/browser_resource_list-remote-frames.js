/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's allocation point in the chrome
 * process.
 */

"use strict";

add_task(async function() {
  const tabTarget = await addTabTarget(MAIN_DOMAIN + "doc_iframe.html");
  await testLocalListFrames(tabTarget);
  await testBrowserListFrames(tabTarget);
});

async function testLocalListFrames(tabTarget) {
  // at this point, tabTarget is the tab with two iframes, one nested inside
  // the other.
  // we should move this to descriptorFront.listRemoteFrames once we have
  // tabDescriptors
  const { frames } = await tabTarget.listRemoteFrames();
  is(frames.length, 3, "Got three frames");

  info("Check that we can connect to the remote targets");
  const frameTargets = [];
  for (const frame of frames) {
    const frameTarget = await frame.getTarget();
    ok(frameTarget && frameTarget.actor, "Valid frame target retrieved");
    frameTargets.push(frameTarget);
  }

  // Check that we retrieved frame targets for the expected URLs.
  const expectedUrls = [
    "data:text/html,<iframe src='data:text/html,foo'></iframe>",
    "data:text/html,foo",
    "http://example.com/browser/devtools/server/tests/browser/doc_iframe_content.html",
  ];
  for (const url of expectedUrls) {
    ok(
      frameTargets.find(target => target.url === url),
      "Found a frame target for the expected url " + url
    );
  }
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
  ok(descriptor, "we have a descriptor with the url 'doc_iframe'");

  const front = await descriptor.getTarget();
  ok(front.hasActor("console"), "Got the console actor");
  ok(front.hasActor("thread"), "Got the thread actor");
  // Ensure sending at least one request to an actor...
  const consoleFront = await front.getFront("console");
  const { result } = await consoleFront.evaluateJSAsync("var a = 42; a");
  is(result, 42, "console.eval worked");

  info("Check that we can connect to the remote frames");
  const childFrames = frames.filter(d => d.parentID === descriptor.id);
  for (const frame of childFrames) {
    const frameTarget = await frame.getTarget();
    ok(frameTarget && frameTarget.actor, "Valid frame target retrieved");
  }

  await getFirstFrameAgain(front, descriptor, target);
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
  const secondDescriptor = frames.find(f => {
    return f.id === descriptor.id;
  });
  const secondTargetFront = await secondDescriptor.getTarget();
  is(
    secondTargetFront,
    firstTargetFront,
    "Second call to listFrames with the same id returns the same form"
  );
}
