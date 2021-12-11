// This test is fission-only! Make that clear before continuing, to avoid
// confusing failures.
ok(
  Services.appinfo.fissionAutostart,
  "this test requires fission to function!"
);

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

async function do_tests(expected) {
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

  // Load again after setting the COOP header
  await testTreeRemoteTypes(
    "com_after_coop",
    mkTestPage({
      topOrigin: COM_ORIGIN,
      comRemoteType: expected.com_high,
      orgRemoteType: expected.org_normal,
      mozRemoteType: expected.moz_normal,
    })
  );

  // Load again after setting the COOP header, with a .org toplevel
  await testTreeRemoteTypes(
    "org_after_coop",
    mkTestPage({
      topOrigin: ORG_ORIGIN,
      comRemoteType: expected.com_high,
      orgRemoteType: expected.org_normal,
      mozRemoteType: expected.moz_normal,
    })
  );

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
}

add_task(async function test_isolate_nothing() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatedMozillaDomains", "mozilla.org"],
      [
        "fission.webContentIsolationStrategy",
        WebContentIsolationStrategy.IsolateNothing,
      ],
    ],
  });

  await do_tests({
    com_normal: "web",
    org_normal: "web",
    moz_normal: "privilegedmozilla",
    com_high: "web",
    com_coop_coep: "webCOOP+COEP=https://example.com",
    org_coop_coep: "webCOOP+COEP=https://example.org",
    moz_coop_coep: "privilegedmozilla",
  });
});

add_task(async function test_isolate_everything() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatedMozillaDomains", "mozilla.org"],
      [
        "fission.webContentIsolationStrategy",
        WebContentIsolationStrategy.IsolateEverything,
      ],
    ],
  });

  await do_tests({
    com_normal: "webIsolated=https://example.com",
    org_normal: "webIsolated=https://example.org",
    moz_normal: "privilegedmozilla",
    com_high: "webIsolated=https://example.com",
    com_coop_coep: "webCOOP+COEP=https://example.com",
    org_coop_coep: "webCOOP+COEP=https://example.org",
    moz_coop_coep: "privilegedmozilla",
  });
});

add_task(async function test_isolate_high_value() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatedMozillaDomains", "mozilla.org"],
      [
        "fission.webContentIsolationStrategy",
        WebContentIsolationStrategy.IsolateHighValue,
      ],
    ],
  });

  await do_tests({
    com_normal: "web",
    org_normal: "web",
    moz_normal: "privilegedmozilla",
    com_high: "webIsolated=https://example.com",
    com_coop_coep: "webCOOP+COEP=https://example.com",
    org_coop_coep: "webCOOP+COEP=https://example.org",
    moz_coop_coep: "privilegedmozilla",
  });
});
