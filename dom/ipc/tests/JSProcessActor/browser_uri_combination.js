add_task(function specify_both() {
  // Specifying both should throw.

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerProcessActor("TestProcessActor", {
      parent: {
        moduleURI: "resource://testing-common/TestProcessActorParent.jsm",
      },
      child: {
        moduleURI: "resource://testing-common/TestProcessActorChild.jsm",
        esModuleURI: "resource://testing-common/TestProcessActorChild.sys.mjs",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerProcessActor("TestProcessActor", {
      parent: {
        esModuleURI: "resource://testing-common/TestProcessActorParent.sys.mjs",
      },
      child: {
        moduleURI: "resource://testing-common/TestProcessActorChild.jsm",
        esModuleURI: "resource://testing-common/TestProcessActorChild.sys.mjs",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerProcessActor("TestProcessActor", {
      parent: {
        moduleURI: "resource://testing-common/TestProcessActorParent.jsm",
        esModuleURI: "resource://testing-common/TestProcessActorParent.sys.mjs",
      },
      child: {
        moduleURI: "resource://testing-common/TestProcessActorChild.jsm",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");

  SimpleTest.doesThrow(() => {
    ChromeUtils.registerProcessActor("TestProcessActor", {
      parent: {
        moduleURI: "resource://testing-common/TestProcessActorParent.jsm",
        esModuleURI: "resource://testing-common/TestProcessActorParent.sys.mjs",
      },
      child: {
        esModuleURI: "resource://testing-common/TestProcessActorChild.sys.mjs",
      },
    });
  }, "Should throw if both moduleURI and esModuleURI are specified.");
});

add_task(function specify_mixed() {
  // Mixing JSM and ESM should work.

  try {
    ChromeUtils.registerProcessActor("TestProcessActor", {
      parent: {
        moduleURI: "resource://testing-common/TestProcessActorParent.jsm",
      },
      child: {
        esModuleURI: "resource://testing-common/TestProcessActorChild.sys.mjs",
      },
    });
  } finally {
    ChromeUtils.unregisterProcessActor("TestProcessActor");
  }

  try {
    ChromeUtils.registerProcessActor("TestProcessActor", {
      parent: {
        esModuleURI: "resource://testing-common/TestProcessActorParent.sys.mjs",
      },
      child: {
        moduleURI: "resource://testing-common/TestProcessActorChild.jsm",
      },
    });
  } finally {
    ChromeUtils.unregisterProcessActor("TestProcessActor");
  }
});
