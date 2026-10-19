// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until(read, label) {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  throw Error("Timed out waiting for " + label);
}
async function power() {
  const service = Cc["@mozilla.org/power/powermanagerservice;1"].getService(
    Ci.nsIPowerManagerService,
  );
  assert(
    typeof service.newWakeLockForChromeExtension === "function",
    "compiled native Chrome power backend is required",
  );
  const state = () => ({
    cpu: service.getWakeLockState("cpu"),
    screen: service.getWakeLockState("screen"),
  });
  assertEquals(
    JSON.stringify(state()),
    JSON.stringify({ cpu: "unlocked", screen: "unlocked" }),
    "isolated native wake-lock baseline",
  );
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { ExtensionPermissions } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionPermissions.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const fixtures = [],
    tabs = [],
    window = Services.wm.getMostRecentWindow("navigator:browser");
  const permission = { permissions: ["power"], origins: [] };
  const program =
    `const ready=()=>document.title=JSON.stringify({phase:'ready',namespace:typeof chrome.power,keys:chrome.power?Object.keys(chrome.power).sort():[],level:chrome.power?.Level});window.addEventListener('message',event=>{try{const command=event.data;if(!['system','display','release','invalid'].includes(command))return;let result,invalid=false,validation=[];if(command==='invalid'){for(const call of [()=>chrome.power.requestKeepAwake('none'),()=>chrome.power.requestKeepAwake(),()=>chrome.power.requestKeepAwake('system',1),()=>chrome.power.releaseKeepAwake(1)]){try{call();validation.push({returned:true});}catch(error){validation.push({name:error.name,typeError:error instanceof TypeError});}}invalid=validation.every(error=>error.name==='TypeError'&&error.typeError);}else if(command==='release')result=chrome.power.releaseKeepAwake();else result=chrome.power.requestKeepAwake(command);document.title=JSON.stringify({phase:command,returns:typeof result,invalid,validation});}catch(error){document.title='FAIL:'+error;}});ready();`;
  async function install(
    original,
    {
      ordinary = false,
      mismatch = false,
      optional = false,
      grant = true,
      withoutPermission = false,
    } = {},
  ) {
    const id = ordinary
        ? "ordinary-power@example.test"
        : original + "@chromium-extension",
      path = PathUtils.join(
        PathUtils.tempDir,
        "floorp-power-" + Services.uuid.generateUUID() + ".xpi",
      );
    if (optional && grant) await ExtensionPermissions.add(id, permission);
    const manifest = {
      manifest_version: 3,
      name: "Native power fixture",
      version: "1.0",
      permissions: optional || withoutPermission ? [] : ["power"],
      optional_permissions: optional ? ["power"] : [],
      background: { scripts: ["background.js"] },
      browser_specific_settings: {
        gecko: {
          id,
          ...(ordinary ? {} : {
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: mismatch
                ? "pppppppppppppppppppppppppppppppp"
                : original,
            },
          }),
        },
      },
    };
    const encode = (value) => new TextEncoder().encode(value);
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(JSON.stringify(manifest)),
        "background.js": encode(program),
        "control.html": encode('<script src="background.js"></script>'),
      }),
    );
    const addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(path),
    );
    const fixture = {
      addon,
      path,
      id,
      optional,
      extension: await until(
        () => ExtensionParent.GlobalManager.extensionMap.get(id),
        "power addon",
      ),
    };
    fixtures.push(fixture);
    fixture.background = await until(
      () => fixture.extension.backgroundContext,
      "power background",
    );
    await until(
      () => fixture.background.xulBrowser.contentTitle.startsWith("{"),
      "initial native namespace",
    );
    return fixture;
  }
  const read = (browser) =>
    until(() => {
      const title = browser.contentTitle;
      if (title.startsWith("FAIL:")) throw Error(title);
      return title.startsWith("{") && JSON.parse(title);
    }, "native power command");
  const command = async (browser, value) => {
    browser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          'content.document.title="WAIT";content.postMessage(' +
            JSON.stringify(value) +
            ',"*");',
        ),
      false,
    );
    const result = await read(browser);
    assertEquals(
      result.returns,
      "undefined",
      "Chrome sync-void return contract",
    );
    return result;
  };
  async function expect(cpu, screen, label) {
    await until(() => {
      const actual = state();
      return actual.cpu === cpu && actual.screen === screen;
    }, label);
  }
  try {
    const first = await install("llllllllllllllllllllllllllllllll"),
      second = await install("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", {
        optional: true,
      });
    for (
      const [original, options] of [
        ["nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn", { ordinary: true }],
        ["nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn", { mismatch: true }],
        ["oooooooooooooooooooooooooooooooo", { withoutPermission: true }],
        ["pppppppppppppppppppppppppppppppp", { optional: true, grant: false }],
      ] as const
    ) {
      const extra = await install(original, options);
      assertEquals(
        (await read(extra.background.xulBrowser)).namespace,
        "undefined",
        "ordinary, malformed, and unpermitted documents do not receive Chrome power",
      );
    }
    const a = first.background.xulBrowser,
      b = second.background.xulBrowser;
    assertEquals(
      JSON.stringify((await read(a)).keys),
      JSON.stringify(["Level", "releaseKeepAwake", "requestKeepAwake"]),
      "Chrome155 namespace members",
    );
    assertEquals(
      JSON.stringify((await read(a)).level),
      JSON.stringify({ SYSTEM: "system", DISPLAY: "display" }),
      "Chrome155 Level enumobject",
    );
    assert(
      (await command(a, "invalid")).invalid,
      "missing, invalid, and extra arguments synchronously throw caller TypeError",
    );
    await command(a, "system");
    await expect(
      "locked-background",
      "unlocked",
      "actual CPU system wake lock",
    );
    await command(a, "display");
    await expect(
      "locked-background",
      "locked-foreground",
      "display locks actual CPU and foreground screen",
    );
    await command(a, "display");
    await command(a, "system");
    await expect(
      "locked-background",
      "unlocked",
      "level replacement releases screen without losing CPU",
    );
    await command(b, "display");
    await command(a, "release");
    await expect(
      "locked-background",
      "locked-foreground",
      "another extension retains independently owned locks",
    );
    await command(b, "release");
    await expect(
      "unlocked",
      "unlocked",
      "repeated requests hold only one effective owner lock",
    );
    await command(a, "display");
    await first.extension.terminateBackground();
    assertEquals(
      first.extension.backgroundState,
      "stopped",
      "issuing background genuinely stopped",
    );
    await expect(
      "locked-background",
      "locked-foreground",
      "extension request survives background suspension",
    );
    const tab = window.gBrowser.addTab(
      first.extension.baseURI.resolve("control.html"),
      { triggeringPrincipal: first.extension.principal },
    );
    tabs.push(tab);
    await read(tab.linkedBrowser);
    await command(tab.linkedBrowser, "release");
    await expect(
      "unlocked",
      "unlocked",
      "different owning view releases extension request",
    );
    await command(b, "display");
    await ExtensionPermissions.remove(second.id, permission, second.extension);
    await expect(
      "unlocked",
      "unlocked",
      "permission revocation releases native locks",
    );
    await command(tab.linkedBrowser, "display");
    await first.addon.uninstall();
    first.addon = null;
    await expect(
      "unlocked",
      "unlocked",
      "uninstall releases both native locks",
    );
    await IOUtils.writeUTF8(
      PathUtils.join(PathUtils.tempDir, "floorp-native-power.json"),
      JSON.stringify({
        passed: true,
        states: state(),
        backgroundSuspension: true,
        permissionRevocation: true,
      }),
    );
  } finally {
    for (const tab of tabs) if (tab.isConnected) window.gBrowser.removeTab(tab);
    for (const fixture of fixtures) {
      await fixture.addon?.uninstall();
      await IOUtils.remove(fixture.path, { ignoreAbsent: true });
      if (fixture.optional) {
        await ExtensionPermissions.remove(fixture.id, permission);
      }
    }
  }
}
export async function runAllTests() {
  await runTests("ChromePowerNative.test.mts", [
    {
      name:
        "real native CPU and display wake locks have extension ownership and lifetime",
      fn: power,
    },
  ]);
}
