"use strict";

/* exported createHttpServer, promiseConsoleOutput, setupPKCS11Manifests */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  MockRegistry: "resource://testing-common/MockRegistry.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
});

ExtensionTestUtils.init(this);

/**
 * Creates a new HttpServer for testing, and begins listening on the
 * specified port. Automatically shuts down the server when the test
 * unit ends.
 *
 * @param {integer} [port]
 *        The port to listen on. If omitted, listen on a random
 *        port. The latter is the preferred behavior.
 *
 * @returns {HttpServer}
 */
function createHttpServer(port = -1) {
  let server = new HttpServer();
  server.start(port);

  registerCleanupFunction(() => {
    return new Promise(resolve => {
      server.stop(resolve);
    });
  });

  return server;
}

var promiseConsoleOutput = async function(task) {
  const DONE = `=== console listener ${Math.random()} done ===`;

  let listener;
  let messages = [];
  let awaitListener = new Promise(resolve => {
    listener = msg => {
      if (msg == DONE) {
        resolve();
      } else {
        void (msg instanceof Ci.nsIConsoleMessage);
        messages.push(msg);
      }
    };
  });

  Services.console.registerListener(listener);
  try {
    let result = await task();

    Services.console.logStringMessage(DONE);
    await awaitListener;

    return { messages, result };
  } finally {
    Services.console.unregisterListener(listener);
  }
};

/**
 * Helper function for pkcs11 tests. Given a temporary directory and a list of
 * module specifications, writes the manifests for those modules so the
 * platform can load them.
 *
 * @param {nsIFile} [tmpDir]
 *        A temporary directory to put the manifests in.
 * @param {Array} [modules]
 *        An array of module specifications (see
 *        https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests#PKCS_11_manifests).
 */
async function setupPKCS11Manifests(tmpDir, modules) {
  let slug =
    AppConstants.platform === "linux" ? "pkcs11-modules" : "PKCS11Modules";
  tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  let baseDir = OS.Path.join(tmpDir.path, slug);
  OS.File.makeDir(baseDir);

  async function writeManifest(module) {
    let manifest = {
      name: module.name,
      description: module.description,
      path: module.path,
      type: "pkcs11",
      allowed_extensions: [module.id],
    };

    let manifestPath = OS.Path.join(baseDir, `${module.name}.json`);
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
        registry.setValue(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          `${REGKEY}\\${module.name}`,
          "",
          manifestPath
        );
      }
      break;

    default:
      ok(
        false,
        `Loading of PKCS#11 modules is not supported on ${AppConstants.platform}`
      );
  }
}
