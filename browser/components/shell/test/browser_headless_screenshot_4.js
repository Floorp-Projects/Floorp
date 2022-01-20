"use strict";

add_task(async function() {
  // Test other variations of the "window-size" argument.
  await testWindowSizePositive(800, 600);
  await testWindowSizePositive(1234);
  await testFileCreationNegative(
    [
      "-url",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
      "-window-size",
      "hello",
    ],
    "screenshot.png"
  );
  await testFileCreationNegative(
    [
      "-url",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
      "-window-size",
      "800,",
    ],
    "screenshot.png"
  );
});
