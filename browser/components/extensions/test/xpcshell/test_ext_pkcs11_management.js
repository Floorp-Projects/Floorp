"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  ctypes: "resource://gre/modules/ctypes.jsm",
  MockRegistry: "resource://testing-common/MockRegistry.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

do_get_profile();
let tmpDir = FileUtils.getDir("TmpD", ["PKCS11"]);
let slug = AppConstants.platform === "linux" ? "pkcs11-modules" : "PKCS11Modules";
tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
let baseDir = OS.Path.join(tmpDir.path, slug);
OS.File.makeDir(baseDir);

registerCleanupFunction(() => {
  tmpDir.remove(true);
});

function getPath(filename) {
  return OS.Path.join(baseDir, filename);
}

const testmodule = "../../../../../security/manager/ssl/tests/unit/pkcs11testmodule/" + ctypes.libraryName("pkcs11testmodule");

// This function was inspired by the native messaging test under
// toolkit/components/extensions

async function setupManifests(modules) {
  async function writeManifest(module) {
    let manifest = {
      name: module.name,
      description: module.description,
      path: module.path,
      type: "pkcs11",
      allowed_extensions: [module.id],
    };

    let manifestPath = getPath(`${module.name}.json`);
    await OS.File.writeAtomic(manifestPath, JSON.stringify(manifest));

    return manifestPath;
  }

  switch (AppConstants.platform) {
    case "macosx":
    case "linux":
      let dirProvider = {
        getFile(property) {
          if (property == "XREUserNativeManifests") {
            return tmpDir.clone();
          } else if (property == "XRESysNativeManifests") {
            return tmpDir.clone();
          }
          return null;
        },
      };

      Services.dirsvc.registerProvider(dirProvider);
      registerCleanupFunction(() => {
        Services.dirsvc.unregisterProvider(dirProvider);
      });

      for (let module of modules) {
        await writeManifest(module);
      }
      break;

    case "win":
      const REGKEY = String.raw`Software\Mozilla\PKCS11Modules`;

      let registry = new MockRegistry();
      registerCleanupFunction(() => {
        registry.shutdown();
      });

      for (let module of modules) {
        if (!OS.Path.winIsAbsolute(module.path)) {
          let cwd = await OS.File.getCurrentDirectory();
          module.path = OS.Path.join(cwd, module.path);
        }
        let manifestPath = await writeManifest(module);
        registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                          `${REGKEY}\\${module.name}`, "", manifestPath);
      }
      break;

    default:
      ok(false, `Loading of PKCS#11 modules is not supported on ${AppConstants.platform}`);
  }
}

add_task(async function test_pkcs11() {
  async function background() {
    try {
      let isInstalled = await browser.pkcs11.isModuleInstalled("testmodule");
      browser.test.assertFalse(isInstalled, "PKCS#11 module is not installed before we install it");
      await browser.pkcs11.installModule("testmodule", 0);
      isInstalled = await browser.pkcs11.isModuleInstalled("testmodule");
      browser.test.assertTrue(isInstalled, "PKCS#11 module is installed after we install it");
      let slots = await browser.pkcs11.getModuleSlots("testmodule");
      browser.test.assertEq("Test PKCS11 Slot", slots[0].name, "The first slot name matches the expected name");
      browser.test.assertEq("Test PKCS11 Slot 二", slots[1].name, "The second slot name matches the expected name");
      browser.test.assertTrue(slots[1].token, "The second slot has a token");
      browser.test.assertFalse(slots[2].token, "The third slot has no token");
      browser.test.assertEq("Test PKCS11 Tokeñ 2 Label", slots[1].token.name, "The token name matches the expected name");
      browser.test.assertEq("Test PKCS11 Manufacturer ID", slots[1].token.manufacturer, "The token manufacturer matches the expected manufacturer");
      browser.test.assertEq("0.0", slots[1].token.HWVersion, "The token hardware version matches the expected version");
      browser.test.assertEq("0.0", slots[1].token.FWVersion, "The token firmware version matches the expected version");
      browser.test.assertEq("", slots[1].token.serial, "The token has no serial number");
      browser.test.assertFalse(slots[1].token.isLoggedIn, "The token is not logged in");
      await browser.pkcs11.uninstallModule("testmodule");
      isInstalled = await browser.pkcs11.isModuleInstalled("testmodule");
      browser.test.assertFalse(isInstalled, "PKCS#11 module is no longer installed after we uninstall it");
      await browser.pkcs11.installModule("testmodule");
      isInstalled = await browser.pkcs11.isModuleInstalled("testmodule");
      browser.test.assertTrue(isInstalled, "Installing the PKCS#11 module without flags parameter succeeds");
      await browser.pkcs11.uninstallModule("testmodule");
      await browser.test.assertRejects(
        browser.pkcs11.isModuleInstalled("nonexistingmodule"),
        /No such PKCS#11 module nonexistingmodule/,
        "We cannot access modules if no JSON file exists");
      await browser.test.assertRejects(
        browser.pkcs11.isModuleInstalled("othermodule"),
        /No such PKCS#11 module othermodule/,
        "We cannot access modules if we're not listed in the module's manifest file's allowed_extensions key");
      await browser.test.assertRejects(
        browser.pkcs11.uninstallModule("internalmodule"),
        /No such PKCS#11 module internalmodule/,
        "We cannot uninstall the NSS Builtin Roots Module");
      browser.test.notifyPass("pkcs11");
    } catch (e) {
      browser.test.fail(`Error: ${String(e)} :: ${e.stack}`);
      browser.test.notifyFail("pkcs11 failed");
    }
  }

  await setupManifests([
    {
      name: "testmodule",
      description: "PKCS#11 Test Module",
      path: testmodule,
      id: "pkcs11@tests.mozilla.org",
    },
    {
      name: "othermodule",
      description: "PKCS#11 Test Module",
      path: testmodule,
      id: "other@tests.mozilla.org",
    },
    {
      name: "internalmodule",
      description: "Builtin Roots Module",
      path: ctypes.libraryName("nssckbi"),
      id: "pkcs11@tests.mozilla.org",
    },
  ]);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["pkcs11"],
      applications: {"gecko": {id: "pkcs11@tests.mozilla.org"}},
    },
    background: background,
  });
  await extension.startup();
  await extension.awaitFinish("pkcs11");
  await extension.unload();
});
