// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, runTests } from "../../../test/utils/test_harness.ts";

async function testPackagedIconDecoder(): Promise<void> {
  if (Services.appinfo.OS !== "WINNT") return;

  // Exercise the actual packaged module and image without writing a profile,
  // creating shortcuts, or changing the current browser's window icons.
  const { TaskbarTabsUtils } = ChromeUtils.importESModule(
    "moz-src:///browser/components/taskbartabs/TaskbarTabsUtils.sys.mjs",
  );
  const image = await TaskbarTabsUtils._imageFromLocalURI(
    Services.io.newURI("chrome://browser/content/icons/floorp-moegi.png"),
  );
  assert(image.width > 0 && image.height > 0, "the bundled icon must decode");
}

export async function runAllTests(): Promise<void> {
  await runTests("custom-icon-decode.test.ts", [
    {
      name: "packaged Windows icon decoder loads an image",
      fn: testPackagedIconDecoder,
    },
  ]);
}
