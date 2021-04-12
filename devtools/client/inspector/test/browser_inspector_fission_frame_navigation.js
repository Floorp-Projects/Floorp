/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const EXAMPLE_COM_URI =
  "http://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_NET_URI =
  "http://example.net/document-builder.sjs?html=<div id=net>net";

const ORG_URL_ROOT = URL_ROOT.replace("example.com", "example.org");
const TEST_ORG_URI =
  ORG_URL_ROOT + "doc_inspector_fission_frame_navigation.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_ORG_URI);
  const tree = `
    id="root"
      iframe
        #document
          html
            head
            body
              id="org"`;
  // Note: the assertMarkupViewAsTree uses very high level APIs and is similar
  // to what should happen when a user interacts with the markup view.
  // It is important to avoid explicitly fetching walkers or node fronts during
  // the test, as it might cause reparenting of remote frames and make the test
  // succeed without actually testing that the feature works correctly.
  await assertMarkupViewAsTree(tree, "#root", inspector);

  await navigateIframeTo(inspector, EXAMPLE_COM_URI);
  const treeAfterLoadingCom = `
    id="root"
      iframe
        #document
          html
            head
            body
              id="com"`;
  await assertMarkupViewAsTree(treeAfterLoadingCom, "#root", inspector);

  await navigateIframeTo(inspector, EXAMPLE_NET_URI);
  const treeAfterLoadingNet = `
    id="root"
      iframe
        #document
          html
            head
            body
              id="net"`;
  await assertMarkupViewAsTree(treeAfterLoadingNet, "#root", inspector);
});

/**
 * This test will check the behavior when navigating a frame which is not
 * visible in the markup view, because its parentNode has not been expanded yet.
 *
 * We expect a root-node resource to be emitted, but ideally we should not
 * initialize the walker and inspector actors for the target of this root-node
 * resource.
 */
add_task(async function navigateFrameNotExpandedInMarkupView() {
  if (!isFissionEnabled()) {
    // This test only makes sense with Fission and remote frames, otherwise the
    // root-node resource will not be emitted for a frame navigation.
    return;
  }

  const { inspector } = await openInspectorForURL(TEST_ORG_URI);
  const resourceWatcher = inspector.toolbox.resourceWatcher;

  // At this stage the expected layout of the markup view is
  // v html     (expanded)
  //   v body   (expanded)
  //     > p    (collapsed)
  //     > div  (collapsed)
  //
  // The iframe we are about to navigate is therefore hidden and we are not
  // watching it - ie, it is not in the list of known NodeFronts/Actors.
  const resource = await navigateIframeTo(inspector, EXAMPLE_COM_URI);

  is(
    resource.resourceType,
    resourceWatcher.TYPES.ROOT_NODE,
    "A resource with resourceType ROOT_NODE was received when navigating"
  );

  // This highlights what doesn't work with the current approach.
  // Since the root-node resource is a NodeFront watched via the WalkerFront,
  // watching it for a new remote frame target implies initializing the
  // inspector & walker fronts.
  //
  // If the resource was not a front (eg ContentDOMreference?) and was emitted
  // by the target actor instead of the walker actor, the client can decide if
  // the inspector and walker fronts should be initialized or not.
  //
  // In this test scenario, the children of the iframe were not known by the
  // inspector before this navigation. Per the explanation above, the new target
  // should not have an already instantiated inspector front.
  //
  // This should be fixed when implementing the RootNode resource on the server
  // in https://bugzilla.mozilla.org/show_bug.cgi?id=1644190
  todo(
    !resource.targetFront.getCachedFront("inspector"),
    "The inspector front for the new target should not be initialized"
  );
});

async function navigateIframeTo(inspector, url) {
  info("Navigate the test iframe to " + url);

  const { commands } = inspector;
  const { resourceWatcher } = inspector.toolbox;
  const onTargetProcessed = waitForTargetProcessed(commands, url);

  const onNewRoot = waitForNextResource(
    resourceWatcher,
    resourceWatcher.TYPES.ROOT_NODE,
    {
      ignoreExistingResources: true,
    }
  );

  info("Update the src attribute of the iframe tag");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [url], function(_url) {
    content.document.querySelector("iframe").setAttribute("src", _url);
  });

  info("Wait for frameLoad/newRoot to resolve");
  const newRootResult = await onNewRoot;

  info("Wait for pending children updates");
  await inspector.markup._waitForChildren();

  if (isFissionEnabled()) {
    info("Wait until the new target has been processed by TargetCommand");
    await onTargetProcessed;
  }

  // Note: the newRootResult changes when the test runs with or without fission.
  return newRootResult;
}

/**
 * Returns a promise that waits until the provided commands's TargetCommand has fully
 * processed a target with the provided URL.
 * This will avoid navigating again before the new resource watchers have fully
 * attached to the new target.
 */
function waitForTargetProcessed(commands, url) {
  return new Promise(resolve => {
    const onTargetProcessed = targetFront => {
      if (targetFront.url !== encodeURI(url)) {
        return;
      }
      commands.targetCommand.off(
        "processed-available-target",
        onTargetProcessed
      );
      resolve();
    };
    commands.targetCommand.on("processed-available-target", onTargetProcessed);
  });
}
