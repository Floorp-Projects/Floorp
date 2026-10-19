// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  ValidationExtensionPolicy,
  ValidationNativeWindow,
} from "./helpers/validation-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function validationErrors() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as ValidationNativeWindow;
  const fixtures = [],
    tabs = [];
  const program =
    `const results=[];for(const call of [()=>chrome.runtime.getURL(),()=>chrome.runtime.getURL(9),()=>chrome.tabs.get('bad')]){try{call();results.push({returned:true});}catch(error){results.push({name:error.name,typeError:error instanceof TypeError,error:error instanceof Error});}}document.title=JSON.stringify(results);`;
  try {
    for (const manifestVersion of [2, 3]) {
      for (const mode of ["converted", "ordinary", "mismatched"]) {
        const original = "oooooooooooooooooooooooooooooooo";
        const id = mode === "ordinary"
          ? "ordinary-validation@example.test"
          : original + "@chromium-extension";
        const manifest = {
          manifest_version: manifestVersion,
          name: "Native API validation fixture",
          version: "1.0",
          browser_specific_settings: {
            gecko: {
              id,
              ...(mode === "ordinary" ? {} : {
                floorp_chrome_extension: {
                  schema_version: 1,
                  original_id: mode === "mismatched"
                    ? "pppppppppppppppppppppppppppppppp"
                    : original,
                },
              }),
            },
          },
        };
        const path = PathUtils.join(
          PathUtils.tempDir,
          "floorp-validation-" + Services.uuid.generateUUID() + ".xpi",
        );
        const encode = (value: string) => new TextEncoder().encode(value);
        await IOUtils.write(
          path,
          zipSync({
            "manifest.json": encode(JSON.stringify(manifest)),
            "control.html": encode('<script src="control.js"></script>'),
            "control.js": encode(program),
          }),
        );
        const addon = await AddonManager.installTemporaryAddon(
          new FileUtils.File(path),
        );
        fixtures.push({ addon, path });
        const policy = WebExtensionPolicy.getByID(id) as
          | ValidationExtensionPolicy
          | undefined;
        assert(policy, "installed native extension policy");
        const tab = window.gBrowser.addTab(policy.getURL("control.html"), {
          triggeringPrincipal: policy.extension.principal,
        });
        tabs.push(tab);
        const end = Date.now() + 10000;
        while (
          !tab.linkedBrowser.contentTitle.startsWith("[") &&
          Date.now() < end
        ) {
          await new Promise((resolve) => setTimeout(resolve, 20));
        }
        const results = JSON.parse(tab.linkedBrowser.contentTitle);
        assertEquals(
          results.length,
          3,
          "native schema rejects missing and wrong type arguments",
        );
        for (const result of results) {
          assert(
            !result.returned && result.error,
            "synchronous validation exception belongs to calling realm",
          );
          assertEquals(
            result.name,
            mode === "converted" ? "TypeError" : "Error",
            manifestVersion + " " + mode + " validation class",
          );
          assertEquals(
            result.typeError,
            mode === "converted",
            "ordinary Firefox validation remains unchanged",
          );
        }
        window.gBrowser.removeTab(tab);
        tabs.pop();
        await addon.uninstall();
        await IOUtils.remove(path);
        fixtures.pop();
      }
    }
  } finally {
    for (const tab of tabs) if (tab.parentNode) window.gBrowser.removeTab(tab);
    for (const { addon, path } of fixtures) {
      await addon.uninstall();
      await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}

export function runAllTests() {
  return runTests("ChromeValidationNative.test.mts", [
    {
      name: "Chrome document schema TypeError and ordinary Firefox isolation",
      fn: validationErrors,
    },
  ]);
}
