let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_largeAllocation.js", this);

// Force-enabling the Large-Allocation header
add_task(async function() {
  info("Test 1 - force enabling the Large-Allocation header");
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable the header if it is disabled
      ["dom.largeAllocationHeader.enabled", true],
      // Force-enable process creation with large-allocation, such that non
      // win32 builds can test the behavior.
      ["dom.largeAllocation.forceEnable", true],
      // Increase processCount.webLargeAllocation to avoid any races where
      // processes aren't being cleaned up quickly enough.
      ["dom.ipc.processCount.webLargeAllocation", 20]
    ]
  });

  await largeAllocSuccessTests();
});

add_task(async function() {
  info("Test 2 - not force enabling the Large-Allocation header");
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable the header if it is disabled
      ["dom.largeAllocationHeader.enabled", true],
      // Force-enable process creation with large-allocation, such that non
      // win32 builds can test the behavior.
      ["dom.largeAllocation.forceEnable", false],
      // Increase processCount.webLargeAllocation to avoid any races where
      // processes aren't being cleaned up quickly enough.
      ["dom.ipc.processCount.webLargeAllocation", 20]
    ]
  });

  await largeAllocFailTests();
});
