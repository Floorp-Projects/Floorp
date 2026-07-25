// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  loadUriInWebPanelBrowser,
  type WebPanelBrowserElement,
} from "../utils/web-panel-browser.ts";

function testWebPanelLoadUsesSupportedOptions(): void {
  const captured: { uri: nsIURI | null; options: object | null } = {
    uri: null,
    options: null,
  };
  const browser = {
    loadURI(uri: nsIURI, options?: object): void {
      captured.uri = uri;
      captured.options = options ?? null;
    },
  } as unknown as WebPanelBrowserElement;

  const targetURL = "https://example.com/panel-test";
  loadUriInWebPanelBrowser(browser, targetURL);

  assertEquals(
    captured.uri?.spec,
    targetURL,
    "should load the requested URI",
  );
  assert(captured.options !== null, "should pass load options");

  const options = captured.options as Record<string, unknown>;
  assert(
    options.triggeringPrincipal !== undefined,
    "should pass a triggering principal",
  );
  assert(
    !("remoteType" in options),
    "should not pass the removed remoteType option",
  );
  assert(
    !("remoteTypeOverride" in options),
    "should not force a remote type override for a web URL",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("webPanelBrowserLoad.test.ts", [
    {
      name: "web panel loads with Gecko-supported options",
      fn: testWebPanelLoadUsesSupportedOptions,
    },
  ]);
}
