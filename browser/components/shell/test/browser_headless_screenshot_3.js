"use strict";

add_task(async function() {
  // Test invalid URL arguments (either no argument or too many arguments).
  await testFileCreationNegative(["-screenshot"], "screenshot.png");
  await testFileCreationNegative(
    [
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "http://mochi.test:8888/headless.html",
      "-screenshot",
    ],
    "screenshot.png"
  );

  // Test all four basic variations of the "window-size" argument.
  await testFileCreationPositive(
    [
      "-url",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
      "-window-size",
      "800",
    ],
    "screenshot.png"
  );
  await testFileCreationPositive(
    [
      "-url",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
      "-window-size=800",
    ],
    "screenshot.png"
  );
  await testFileCreationPositive(
    [
      "-url",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
      "--window-size",
      "800",
    ],
    "screenshot.png"
  );
  await testFileCreationPositive(
    [
      "-url",
      "http://mochi.test:8888/browser/browser/components/shell/test/headless.html",
      "-screenshot",
      "--window-size=800",
    ],
    "screenshot.png"
  );
});
