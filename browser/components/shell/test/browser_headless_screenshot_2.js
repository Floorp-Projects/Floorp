"use strict";
add_task(async function() {
  // Test variations of the "screenshot" argument when a file path
  // isn't specified.
  await testFileCreationPositive(
    [
      "-screenshot",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
    ],
    "screenshot.png"
  );
  await testFileCreationPositive(
    [
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
    ],
    "screenshot.png"
  );
  await testFileCreationPositive(
    [
      "--screenshot",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
    ],
    "screenshot.png"
  );
  await testFileCreationPositive(
    [
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "--screenshot",
    ],
    "screenshot.png"
  );
});
