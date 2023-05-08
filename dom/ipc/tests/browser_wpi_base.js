// This test is fission-only! Make that clear before continuing, to avoid
// confusing failures.
ok(
  Services.appinfo.fissionAutostart,
  "this test requires fission to function!"
);

requestLongerTimeout(2);

const WebContentIsolationStrategy = {
  IsolateNothing: 0,
  IsolateEverything: 1,
  IsolateHighValue: 2,
};

const COM_ORIGIN = "https://example.com";
const ORG_ORIGIN = "https://example.org";
const MOZ_ORIGIN = "https://www.mozilla.org";

// Helper for building document-builder.sjs URLs which have specific headers &
// HTML content.
function documentURL(origin, headers, html) {
  let params = new URLSearchParams();
  params.append("html", html.trim());
  for (const [key, value] of Object.entries(headers)) {
    params.append("headers", `${key}:${value}`);
  }
  return `${origin}/document-builder.sjs?${params.toString()}`;
}

async function testTreeRemoteTypes(name, testpage) {
  // Use document-builder.sjs to build up the expected document tree.
  function buildURL(path, page) {
    let html = `<h1>${path}</h1>`;
    for (let i = 0; i < page.children.length; ++i) {
      const inner = buildURL(`${path}[${i}]`, page.children[i]);
      html += `<iframe src=${JSON.stringify(inner)}></iframe>`;
    }
    return documentURL(page.origin, page.headers, html);
  }
  const url = buildURL(name, testpage);

  // Load the tab and confirm that properties of the loaded documents match
  // expectation.
  await BrowserTestUtils.withNewTab(url, async browser => {
    let stack = [
      {
        path: name,
        bc: browser.browsingContext,
        ...testpage,
      },
    ];

    while (stack.length) {
      const { path, bc, remoteType, children, origin } = stack.pop();
      is(
        Services.scriptSecurityManager.createContentPrincipal(
          bc.currentWindowGlobal.documentURI,
          {}
        ).originNoSuffix,
        origin,
        `Frame ${path} has expected originNoSuffix`
      );
      is(
        bc.currentWindowGlobal.domProcess.remoteType,
        remoteType,
        `Frame ${path} has expected remote type`
      );
      is(
        bc.children.length,
        children.length,
        `Frame ${path} has the expected number of children`
      );
      for (let i = 0; i < bc.children.length; ++i) {
        stack.push({
          path: `${path}[${i}]`,
          bc: bc.children[i],
          ...children[i],
        });
      }
    }
  });
}

function mkTestPage({
  comRemoteType,
  orgRemoteType,
  mozRemoteType,
  topOrigin,
  topHeaders = {},
  frameHeaders = {},
}) {
  const topRemoteType = {
    [COM_ORIGIN]: comRemoteType,
    [ORG_ORIGIN]: orgRemoteType,
    [MOZ_ORIGIN]: mozRemoteType,
  }[topOrigin];

  const innerChildren = [
    {
      origin: COM_ORIGIN,
      headers: frameHeaders,
      remoteType: comRemoteType,
      children: [],
    },
    {
      origin: ORG_ORIGIN,
      headers: frameHeaders,
      remoteType: orgRemoteType,
      children: [],
    },
    {
      origin: MOZ_ORIGIN,
      headers: frameHeaders,
      remoteType: mozRemoteType,
      children: [],
    },
  ];

  return {
    origin: topOrigin,
    headers: topHeaders,
    remoteType: topRemoteType,
    children: [
      {
        origin: COM_ORIGIN,
        headers: frameHeaders,
        remoteType: comRemoteType,
        children: [...innerChildren],
      },
      {
        origin: ORG_ORIGIN,
        headers: frameHeaders,
        remoteType: orgRemoteType,
        children: [...innerChildren],
      },
      {
        origin: MOZ_ORIGIN,
        headers: frameHeaders,
        remoteType: mozRemoteType,
        children: [...innerChildren],
      },
    ],
  };
}

const heuristics = [
  {
    name: "coop",
    setup_com: async expected => {
      // Set the COOP header, and load
      await testTreeRemoteTypes(
        "com_set_coop",
        mkTestPage({
          topOrigin: COM_ORIGIN,
          topHeaders: { "Cross-Origin-Opener-Policy": "same-origin" },
          comRemoteType: expected.com_high,
          orgRemoteType: expected.org_normal,
          mozRemoteType: expected.moz_normal,
        })
      );
    },
    run_extra_test: async expected => {
      // Load with both the COOP and COEP headers set.
      await testTreeRemoteTypes(
        "com_coop_coep",
        mkTestPage({
          topOrigin: COM_ORIGIN,
          topHeaders: {
            "Cross-Origin-Opener-Policy": "same-origin",
            "Cross-Origin-Embedder-Policy": "require-corp",
          },
          frameHeaders: {
            "Cross-Origin-Embedder-Policy": "require-corp",
            "Cross-Origin-Resource-Policy": "cross-origin",
          },
          comRemoteType: expected.com_coop_coep,
          orgRemoteType: expected.org_coop_coep,
          mozRemoteType: expected.moz_coop_coep,
        })
      );
    },
  },
  {
    name: "hasSavedLogin",
    setup_com: async expected => {
      // add .com to the password manager
      let LoginInfo = new Components.Constructor(
        "@mozilla.org/login-manager/loginInfo;1",
        Ci.nsILoginInfo,
        "init"
      );
      await Services.logins.addLoginAsync(
        new LoginInfo(COM_ORIGIN, "", null, "username", "password", "", "")
      );

      // Init login detection service to trigger fetching logins
      let loginDetection = Cc[
        "@mozilla.org/login-detection-service;1"
      ].createInstance(Ci.nsILoginDetectionService);
      loginDetection.init();

      await TestUtils.waitForCondition(() => {
        let x = loginDetection.isLoginsLoaded();
        return x;
      }, "waiting for loading logins from the password manager");
    },
  },
  {
    name: "isLoggedIn",
    setup_com: async expected => {
      let p = new Promise(resolve => {
        Services.obs.addObserver(function obs() {
          Services.obs.removeObserver(
            obs,
            "passwordmgr-form-submission-detected"
          );
          resolve();
        }, "passwordmgr-form-submission-detected");
      });

      const TEST_URL = documentURL(
        COM_ORIGIN,
        {},
        `<form>
          <input value="username">
          <input type="password" value="password">
          <input type="submit">
        </form>`
      );

      // submit the form to simulate the login behavior
      await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
        await SpecialPowers.spawn(browser, [], async () => {
          content.document.querySelector("form").submit();
        });
      });
      await p;
    },
  },
];

async function do_tests(expected) {
  for (let heuristic of heuristics) {
    info(`Starting ${heuristic.name} test`);
    // Clear all site-specific data, as we don't want to have any high-value site
    // permissions from any previous iterations.
    await new Promise(resolve =>
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve)
    );

    // Loads for basic URLs with no special headers set.
    await testTreeRemoteTypes(
      "basic_com",
      mkTestPage({
        topOrigin: COM_ORIGIN,
        comRemoteType: expected.com_normal,
        orgRemoteType: expected.org_normal,
        mozRemoteType: expected.moz_normal,
      })
    );

    await testTreeRemoteTypes(
      "basic_org",
      mkTestPage({
        topOrigin: ORG_ORIGIN,
        comRemoteType: expected.com_normal,
        orgRemoteType: expected.org_normal,
        mozRemoteType: expected.moz_normal,
      })
    );

    info(`Setting up ${heuristic.name} test`);
    await heuristic.setup_com(expected);

    // Load again after the heuristic is triggered
    info(`Running ${heuristic.name} tests after setup`);
    await testTreeRemoteTypes(
      `com_after_${heuristic.name}`,
      mkTestPage({
        topOrigin: COM_ORIGIN,
        comRemoteType: expected.com_high,
        orgRemoteType: expected.org_normal,
        mozRemoteType: expected.moz_normal,
      })
    );

    // Load again with a .org toplevel
    await testTreeRemoteTypes(
      `org_after_${heuristic.name}`,
      mkTestPage({
        topOrigin: ORG_ORIGIN,
        comRemoteType: expected.com_high,
        orgRemoteType: expected.org_normal,
        mozRemoteType: expected.moz_normal,
      })
    );

    // Run heuristic dependent tests
    if (heuristic.run_extra_test) {
      info(`Running extra tests for ${heuristic.name}`);
      await heuristic.run_extra_test(expected);
    }
  }
}
