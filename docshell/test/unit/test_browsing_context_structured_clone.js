"use strict";

add_task(async function test_BrowsingContext_structured_clone() {
  let browser = Services.appShell.createWindowlessBrowser(false);

  let frame = browser.document.createElement("iframe");

  await new Promise(r => {
    frame.onload = () => r();
    browser.document.body.appendChild(frame);
  });

  let { browsingContext } = frame;

  let sch = new StructuredCloneHolder("debug name", "<anonymized> debug name", {
    browsingContext,
  });

  let deserialize = () => sch.deserialize({}, true);

  // Check that decoding a live browsing context produces the correct
  // object.
  equal(
    deserialize().browsingContext,
    browsingContext,
    "Got correct browsing context from StructuredClone deserialize"
  );

  // Check that decoding a second time still succeeds.
  equal(
    deserialize().browsingContext,
    browsingContext,
    "Got correct browsing context from second StructuredClone deserialize"
  );

  // Destroy the browsing context and make sure that the decode fails
  // with a DataCloneError.
  //
  // Making sure the BrowsingContext is actually destroyed by the time
  // we do the second decode is a bit tricky. We obviously have clear
  // our local references to it, and give the GC a chance to reap them.
  // And we also, of course, have to destroy the frame that it belongs
  // to, or its frame loader and window global would hold it alive.
  //
  // Beyond that, we don't *have* to reload or destroy the parent
  // document, but we do anyway just to be safe.
  //

  frame.remove();
  frame = null;
  browsingContext = null;

  browser.document.location.reload();
  browser.close();

  // We will schedule a precise GC and do both GC and CC a few times, to make
  // sure we have completely destroyed the WindowGlobal actors (which keep
  // references to their BrowsingContexts) in order
  // to allow their (now snow-white) references to be collected.
  await schedulePreciseGCAndForceCC(3);

  // OK. We can be fairly confident that the BrowsingContext object
  // stored in our structured clone data has been destroyed. Make sure
  // that attempting to decode it again leads to the appropriate error.
  Assert.throws(
    deserialize,
    e => e.name === "DataCloneError",
    "Should get a DataCloneError when trying to decode a dead BrowsingContext"
  );
});
