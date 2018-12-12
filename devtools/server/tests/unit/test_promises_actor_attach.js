/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we can attach and detach to the PromisesActor under the correct
 * states.
 */

add_task(async function() {
  const { promisesFront } = await createMainProcessPromisesFront();
  await testAttach(promisesFront);
});

add_task(async function() {
  const { promisesFront } = await createTabPromisesFront();
  await testAttach(promisesFront);
});

async function testAttach(promises) {
  try {
    await promises.detach();
    ok(false, "Should not be able to detach when in a detached state.");
  } catch (e) {
    ok(true, "Expected detach to fail when already in a detached state.");
  }

  await promises.attach();
  ok(true, "Expected attach to succeed.");

  try {
    await promises.attach();
    ok(false, "Should not be able to attach when in an attached state.");
  } catch (e) {
    ok(true, "Expected attach to fail when already in an attached state.");
  }

  await promises.detach();
  ok(true, "Expected detach to succeed.");
}
