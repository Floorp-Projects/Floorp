/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can nest event loops when needed in
// ThreadActor.prototype.unsafeSynchronize.

add_task(
  threadFrontTest(async ({ threadFront, client }) => {
    // Reach over the protocol connection and get a reference to the thread actor.
    // TODO: rewrite the test so we don't do this..
    const thread = client._transport._serverConnection.getActor(
      threadFront.actorID
    );

    test_nesting(thread);
  })
);

function test_nesting(thread) {
  const { resolve, promise: p } = defer();

  let currentStep = 0;

  executeSoon(function() {
    // Should be on the first step
    Assert.equal(++currentStep, 1);
    // We should have one nested event loop from unsfeSynchronize
    Assert.equal(thread._nestedEventLoops.size, 1);
    resolve(true);
  });

  Assert.equal(thread.unsafeSynchronize(p), true);

  // Should be on the second step
  Assert.equal(++currentStep, 2);
  // There shouldn't be any nested event loops anymore
  Assert.equal(thread._nestedEventLoops.size, 0);
}
