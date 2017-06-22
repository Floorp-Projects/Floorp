let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_largeAllocation.js", this);

add_task(async function() {
  info("Test 1 - On win32 - no forceEnable pref");
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable the header if it is disabled
      ["dom.largeAllocationHeader.enabled", true],
      // Increase processCount.webLargeAllocation to avoid any races where
      // processes aren't being cleaned up quickly enough.
      ["dom.ipc.processCount.webLargeAllocation", 20]
    ]
  });

  await largeAllocSuccessTests();
});

