add_task(function specify_both() {
  // Specifying both should throw.

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerWindowActor("TestWindow", {
      parent: {
        moduleURI: "resource://testing-common/TestWindowParent.jsm",
      },
      child: {
        moduleURI: "resource://testing-common/TestWindowChild.jsm",
        esModuleURI: "resource://testing-common/TestWindowChild.sys.mjs",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerWindowActor("TestWindow", {
      parent: {
        esModuleURI: "resource://testing-common/TestWindowParent.sys.mjs",
      },
      child: {
        moduleURI: "resource://testing-common/TestWindowChild.jsm",
        esModuleURI: "resource://testing-common/TestWindowChild.sys.mjs",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerWindowActor("TestWindow", {
      parent: {
        moduleURI: "resource://testing-common/TestWindowParent.jsm",
        esModuleURI: "resource://testing-common/TestWindowParent.sys.mjs",
      },
      child: {
        moduleURI: "resource://testing-common/TestWindowChild.jsm",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerWindowActor("TestWindow", {
      parent: {
        moduleURI: "resource://testing-common/TestWindowParent.jsm",
        esModuleURI: "resource://testing-common/TestWindowParent.sys.mjs",
      },
      child: {
        esModuleURI: "resource://testing-common/TestWindowChild.sys.mjs",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");
});

add_task(function specify_mixed() {
  // Mixing JSM and ESM should work.

  try {
    ChromeUtils.registerWindowActor("TestWindow", {
      parent: {
        moduleURI: "resource://testing-common/TestWindowParent.jsm",
      },
      child: {
        esModuleURI: "resource://testing-common/TestWindowChild.sys.mjs",
      },
    });
  } finally {
    ChromeUtils.unregisterWindowActor("TestWindow");
  }

  try {
    ChromeUtils.registerWindowActor("TestWindow", {
      parent: {
        esModuleURI: "resource://testing-common/TestWindowParent.sys.mjs",
      },
      child: {
        moduleURI: "resource://testing-common/TestWindowChild.jsm",
      },
    });
  } finally {
    ChromeUtils.unregisterWindowActor("TestWindow");
  }
});
