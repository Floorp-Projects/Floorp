"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  ctypes: "resource://gre/modules/ctypes.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

do_get_profile();
let tmpDir = FileUtils.getDir("TmpD", ["PKCS11"]);

registerCleanupFunction(() => {
  tmpDir.remove(true);
});

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// This function was inspired by the native messaging test under
// toolkit/components/extensions

add_task(async function test_pkcs11() {
  async function background() {
    try {
      let autoloadEnabled = await browser.pkcs11.autoload.get({});
      browser.test.assertFalse(
        autoloadEnabled.value,
        "autoload should not be enabled before it gets set"
      );
      // When the preference "security.osclientcerts.enabled" gets set to true, PSM dispatches a
      // background task to load the library. When it finishes, it will notify observers of the
      // topic "psm:load-os-client-certs-module-task-ran". This test observes that topic and
      // messages this extension, which then resolves the promise.
      let osclientcertsModuleLoadedPromise = new Promise((resolve, reject) => {
        browser.test.onMessage.addListener(topic => {
          if (topic === "psm:load-os-client-certs-module-task-ran") {
            resolve();
          } else {
            reject(
              `expected "psm:load-os-client-certs-module-task-ran", got "${topic}"`
            );
          }
        });
      });
      await browser.pkcs11.autoload.set({ value: true });
      await osclientcertsModuleLoadedPromise;
      await browser.test.assertRejects(
        browser.pkcs11.uninstallModule("osclientcerts"),
        /No such PKCS#11 module osclientcerts/,
        "uninstallModule should not work on the built-in osclientcerts module"
      );
      await browser.pkcs11.autoload.set({ value: false });

      await browser.test.assertRejects(
        browser.pkcs11.installModule("osclientcerts", 0),
        /No such PKCS#11 module osclientcerts/,
        "installModule should not work on the built-in osclientcerts module"
      );

      await browser.test.assertRejects(
        browser.pkcs11.isModuleInstalled("osclientcerts"),
        /No such PKCS#11 module osclientcerts/,
        "isModuleLoaded should not work on the built-in osclientcerts module"
      );

      await browser.test.assertRejects(
        browser.pkcs11.getModuleSlots("osclientcerts"),
        /No such PKCS#11 module osclientcerts/,
        "getModuleSlots should not work on the built-in osclientcerts module"
      );

      browser.test.notifyPass("pkcs11");
    } catch (e) {
      browser.test.fail(`Error: ${String(e)} :: ${e.stack}`);
      browser.test.notifyFail("pkcs11 failed");
    }
  }

  let libDir = FileUtils.getDir("GreBinD", []);
  await setupPKCS11Manifests(tmpDir, [
    {
      // This is included to be able to test that pkcs11.isModuleInstalled, pkcs11.installModule,
      // pkcs11.uninstallModule, and pkcs11.getModuleSlots fail if given the osclientcerts module.
      name: "osclientcerts",
      description: "OS Client Cert Module",
      path: OS.Path.join(libDir.path, ctypes.libraryName("osclientcerts")),
      id: "pkcs11@tests.mozilla.org",
    },
  ]);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["pkcs11"],
      applications: { gecko: { id: "pkcs11@tests.mozilla.org" } },
    },
    useAddonManager: "temporary",
    background: background,
  });

  let osclientcertsModuleLoadedObserver = (subject, topic) => {
    Services.obs.removeObserver(osclientcertsModuleLoadedObserver, topic);
    extension.sendMessage(topic);
  };
  Services.obs.addObserver(
    osclientcertsModuleLoadedObserver,
    "psm:load-os-client-certs-module-task-ran"
  );

  await promiseStartupManager();
  await extension.startup();
  await extension.awaitFinish("pkcs11");
  await extension.unload();
  await promiseShutdownManager();
});
