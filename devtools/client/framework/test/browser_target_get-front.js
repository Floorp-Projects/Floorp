/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  const tab = await addTab("about:blank");
  const target = await TargetFactory.forTab(tab);
  await target.attach();

  const tab2 = await addTab("about:blank");
  const target2 = await TargetFactory.forTab(tab2);
  await target2.attach();

  info("Test the targetFront attribute for the root");
  const { client } = target;
  is(
    client.mainRoot.targetFront,
    null,
    "got null from the targetFront attribute for the root"
  );
  is(
    client.mainRoot.parentFront,
    null,
    "got null from the parentFront attribute for the root"
  );

  info("Test getting a front twice");
  const getAccessibilityFront = await target.getFront("accessibility");
  const getAccessibilityFront2 = await target.getFront("accessibility");
  is(
    getAccessibilityFront,
    getAccessibilityFront2,
    "got the same front when calling getFront twice"
  );
  is(
    getAccessibilityFront.targetFront,
    target,
    "got the correct targetFront attribute from the front"
  );
  is(
    getAccessibilityFront2.targetFront,
    target,
    "got the correct targetFront attribute from the front"
  );
  is(
    getAccessibilityFront.parentFront,
    target,
    "got the correct parentFront attribute from the front"
  );
  is(
    getAccessibilityFront2.parentFront,
    target,
    "got the correct parentFront attribute from the front"
  );

  info("Test getting a front on different targets");
  const target1Front = await target.getFront("accessibility");
  const target2Front = await target2.getFront("accessibility");
  is(
    target1Front !== target2Front,
    true,
    "got different fronts when calling getFront on different targets"
  );
  is(
    target1Front.targetFront !== target2Front.targetFront,
    true,
    "got different targetFront from different fronts from different targets"
  );
  is(
    target2Front.targetFront,
    target2,
    "got the correct targetFront attribute from the front"
  );

  info("Test async front retrieval");
  // use two fronts that are initialized one after the other.
  const asyncFront1 = target.getFront("performance");
  const asyncFront2 = target.getFront("performance");

  info("waiting on async fronts returns a real front");
  const awaitedAsyncFront1 = await asyncFront1;
  const awaitedAsyncFront2 = await asyncFront2;
  is(
    awaitedAsyncFront1,
    awaitedAsyncFront2,
    "got the same front when requesting the front first async then sync"
  );
  await target.destroy();
  await target2.destroy();

  info("destroying a front immediately is possible");
  await testDestroy();
});

async function testDestroy() {
  // initialize a clean target
  const tab = await addTab("about:blank");
  const target = await TargetFactory.forTab(tab);
  await target.attach();

  // do not wait for the front to finish loading
  target.getFront("performance");

  try {
    await target.destroy();
    ok(
      true,
      "calling destroy on an async front instantiated with getFront does not throw"
    );
  } catch (e) {
    ok(
      false,
      "calling destroy on an async front instantiated with getFront does not throw"
    );
  }
}
