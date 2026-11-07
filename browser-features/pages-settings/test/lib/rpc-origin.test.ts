// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  isDevLaunchPage,
  usesSettingsActor,
} from "../../../../libs/ui/settings-rpc-origin.ts";
import { devPort } from "../../../../libs/shared/dev-ports/defines.ts";

export async function runAllTests() {
  await runTests(
    "settings-rpc-origin",
    [
      ["http://localhost:5183/", "5183", true],
      ["http://127.0.0.1:5183/", "5183", true],
      ["http://localhost:5187/", "5187", true],
      ["about:hub#/features/design", "5183", false],
      ["about:welcome", "5187", false],
      ["chrome://noraneko-settings/content/index.html", "5183", false],
      ["http://localhost:5196/test/integration/index.html", "5183", false],
      ["https://example.com:5183/", "5183", false],
    ].map(([url, port, expected]) => ({
      name: `Transport for ${url}`,
      fn: () =>
        assertEquals(
          usesSettingsActor(String(url), String(port)),
          expected,
          "Use the document origin, not its script origin",
        ),
    })),
  );
  await runTests(
    "settings-dev-launch-page",
    [
      ["http://localhost:5183/", true],
      ["http://localhost:5283/", true],
      ["http://127.0.0.1:5283/", true],
      ["http://localhost:5196/test/integration/index.html", false],
      ["http://localhost:5203/", true],
      ["http://localhost:5200/", false],
      ["about:hub#/features/design", false],
      ["chrome://noraneko-settings/content/index.html", false],
      ["https://localhost:5283/", false],
      ["not a url", false],
    ].map(([url, expected]) => ({
      name: `Dev launch page ${url}`,
      fn: () =>
        assertEquals(
          isDevLaunchPage(String(url), devPort("settings", 0)),
          expected,
          "Only launch-topology ports are dev launch pages",
        ),
    })),
  );
}
