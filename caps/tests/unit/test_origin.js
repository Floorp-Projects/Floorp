var ssm = Services.scriptSecurityManager;
function makeURI(uri) {
  return Services.io.newURI(uri);
}

function checkThrows(f) {
  var threw = false;
  try {
    f();
  } catch (e) {
    threw = true;
  }
  Assert.ok(threw);
}

function checkCrossOrigin(a, b) {
  Assert.ok(!a.equals(b));
  Assert.ok(!a.equalsConsideringDomain(b));
  Assert.ok(!a.subsumes(b));
  Assert.ok(!a.subsumesConsideringDomain(b));
  Assert.ok(!b.subsumes(a));
  Assert.ok(!b.subsumesConsideringDomain(a));
}

function checkOriginAttributes(prin, attrs, suffix) {
  attrs = attrs || {};
  Assert.equal(
    prin.originAttributes.inIsolatedMozBrowser,
    attrs.inIsolatedMozBrowser || false
  );
  Assert.equal(prin.originSuffix, suffix || "");
  Assert.equal(ChromeUtils.originAttributesToSuffix(attrs), suffix || "");
  Assert.ok(
    ChromeUtils.originAttributesMatchPattern(prin.originAttributes, attrs)
  );
  if (!prin.isNullPrincipal && !prin.origin.startsWith("[")) {
    Assert.ok(ssm.createContentPrincipalFromOrigin(prin.origin).equals(prin));
  } else {
    checkThrows(() => ssm.createContentPrincipalFromOrigin(prin.origin));
  }
}

function checkSandboxOriginAttributes(arr, attrs, options) {
  options = options || {};
  var sandbox = Cu.Sandbox(arr, options);
  checkOriginAttributes(
    Cu.getObjectPrincipal(sandbox),
    attrs,
    ChromeUtils.originAttributesToSuffix(attrs)
  );
}

// utility function useful for debugging
// eslint-disable-next-line no-unused-vars
function printAttrs(name, attrs) {
  info(
    name +
      " {\n" +
      "\tuserContextId: " +
      attrs.userContextId +
      ",\n" +
      "\tinIsolatedMozBrowser: " +
      attrs.inIsolatedMozBrowser +
      ",\n" +
      "\tprivateBrowsingId: '" +
      attrs.privateBrowsingId +
      "',\n" +
      "\tfirstPartyDomain: '" +
      attrs.firstPartyDomain +
      "'\n}"
  );
}

function checkValues(attrs, values) {
  values = values || {};
  // printAttrs("attrs", attrs);
  // printAttrs("values", values);
  Assert.equal(attrs.userContextId, values.userContextId || 0);
  Assert.equal(
    attrs.inIsolatedMozBrowser,
    values.inIsolatedMozBrowser || false
  );
  Assert.equal(attrs.privateBrowsingId, values.privateBrowsingId || "");
  Assert.equal(attrs.firstPartyDomain, values.firstPartyDomain || "");
}

function run_test() {
  // Attributeless origins.
  Assert.equal(ssm.getSystemPrincipal().origin, "[System Principal]");
  checkOriginAttributes(ssm.getSystemPrincipal());
  var exampleOrg = ssm.createContentPrincipal(
    makeURI("http://example.org"),
    {}
  );
  Assert.equal(exampleOrg.origin, "http://example.org");
  checkOriginAttributes(exampleOrg);
  var exampleCom = ssm.createContentPrincipal(
    makeURI("https://www.example.com:123"),
    {}
  );
  Assert.equal(exampleCom.origin, "https://www.example.com:123");
  checkOriginAttributes(exampleCom);
  var nullPrin = Cu.getObjectPrincipal(new Cu.Sandbox(null));
  Assert.ok(
    /^moz-nullprincipal:\{([0-9]|[a-z]|\-){36}\}$/.test(nullPrin.origin)
  );
  checkOriginAttributes(nullPrin);
  var ipv6Prin = ssm.createContentPrincipal(
    makeURI("https://[2001:db8::ff00:42:8329]:123"),
    {}
  );
  Assert.equal(ipv6Prin.origin, "https://[2001:db8::ff00:42:8329]:123");
  checkOriginAttributes(ipv6Prin);
  var ipv6NPPrin = ssm.createContentPrincipal(
    makeURI("https://[2001:db8::ff00:42:8329]"),
    {}
  );
  Assert.equal(ipv6NPPrin.origin, "https://[2001:db8::ff00:42:8329]");
  checkOriginAttributes(ipv6NPPrin);
  var ep = Cu.getObjectPrincipal(
    Cu.Sandbox([exampleCom, nullPrin, exampleOrg])
  );
  checkOriginAttributes(ep);
  checkCrossOrigin(exampleCom, exampleOrg);
  checkCrossOrigin(exampleOrg, nullPrin);

  // nsEP origins should be in lexical order.
  Assert.equal(
    ep.origin,
    `[Expanded Principal [${exampleOrg.origin}, ${exampleCom.origin}, ${nullPrin.origin}]]`
  );

  // Make sure createContentPrincipal does what the rest of gecko does.
  Assert.ok(
    exampleOrg.equals(
      Cu.getObjectPrincipal(new Cu.Sandbox("http://example.org"))
    )
  );

  //
  // Test origin attributes.
  //

  // Just browser.
  var exampleOrg_browser = ssm.createContentPrincipal(
    makeURI("http://example.org"),
    { inIsolatedMozBrowser: true }
  );
  var nullPrin_browser = ssm.createNullPrincipal({
    inIsolatedMozBrowser: true,
  });
  checkOriginAttributes(
    exampleOrg_browser,
    { inIsolatedMozBrowser: true },
    "^inBrowser=1"
  );
  checkOriginAttributes(
    nullPrin_browser,
    { inIsolatedMozBrowser: true },
    "^inBrowser=1"
  );
  Assert.equal(exampleOrg_browser.origin, "http://example.org^inBrowser=1");

  // First party Uri
  var exampleOrg_firstPartyDomain = ssm.createContentPrincipal(
    makeURI("http://example.org"),
    { firstPartyDomain: "example.org" }
  );
  checkOriginAttributes(
    exampleOrg_firstPartyDomain,
    { firstPartyDomain: "example.org" },
    "^firstPartyDomain=example.org"
  );
  Assert.equal(
    exampleOrg_firstPartyDomain.origin,
    "http://example.org^firstPartyDomain=example.org"
  );

  // Just userContext.
  var exampleOrg_userContext = ssm.createContentPrincipal(
    makeURI("http://example.org"),
    { userContextId: 42 }
  );
  checkOriginAttributes(
    exampleOrg_userContext,
    { userContextId: 42 },
    "^userContextId=42"
  );
  Assert.equal(
    exampleOrg_userContext.origin,
    "http://example.org^userContextId=42"
  );

  checkSandboxOriginAttributes(null, {});
  checkSandboxOriginAttributes("http://example.org", {});
  checkSandboxOriginAttributes(
    "http://example.org",
    {},
    { originAttributes: {} }
  );
  checkSandboxOriginAttributes(["http://example.org"], {});
  checkSandboxOriginAttributes(
    ["http://example.org"],
    {},
    { originAttributes: {} }
  );

  // Check that all of the above are cross-origin.
  checkCrossOrigin(exampleOrg_browser, nullPrin_browser);
  checkCrossOrigin(exampleOrg_firstPartyDomain, exampleOrg);
  checkCrossOrigin(exampleOrg_userContext, exampleOrg);

  // Check Principal kinds.
  function checkKind(prin, kind) {
    Assert.equal(prin.isNullPrincipal, kind == "nullPrincipal");
    Assert.equal(prin.isContentPrincipal, kind == "contentPrincipal");
    Assert.equal(prin.isExpandedPrincipal, kind == "expandedPrincipal");
    Assert.equal(prin.isSystemPrincipal, kind == "systemPrincipal");
  }
  checkKind(ssm.createNullPrincipal({}), "nullPrincipal");
  checkKind(
    ssm.createContentPrincipal(makeURI("http://www.example.com"), {}),
    "contentPrincipal"
  );
  checkKind(
    Cu.getObjectPrincipal(
      Cu.Sandbox([
        ssm.createContentPrincipal(makeURI("http://www.example.com"), {}),
      ])
    ),
    "expandedPrincipal"
  );
  checkKind(ssm.getSystemPrincipal(), "systemPrincipal");

  //
  // Test Origin Attribute Manipulation
  //

  // check that we can create an empty origin attributes dict with default
  // members and values.
  var emptyAttrs = ChromeUtils.fillNonDefaultOriginAttributes({});
  checkValues(emptyAttrs);

  var uri = "http://example.org";
  var tests = [
    ["", {}],
    ["^userContextId=3", { userContextId: 3 }],
    ["^inBrowser=1", { inIsolatedMozBrowser: true }],
    ["^firstPartyDomain=example.org", { firstPartyDomain: "example.org" }],
  ];

  // check that we can create an origin attributes from an origin properly
  tests.forEach(t => {
    let attrs = ChromeUtils.createOriginAttributesFromOrigin(uri + t[0]);
    checkValues(attrs, t[1]);
    Assert.equal(ChromeUtils.originAttributesToSuffix(attrs), t[0]);
  });

  // check that we can create an origin attributes from a dict properly
  tests.forEach(t => {
    let attrs = ChromeUtils.fillNonDefaultOriginAttributes(t[1]);
    checkValues(attrs, t[1]);
    Assert.equal(ChromeUtils.originAttributesToSuffix(attrs), t[0]);
  });

  // each row in the dflt_tests array has these values:
  // [0] - the suffix used to create an origin attribute from
  // [1] - the expected result of creating an origin attributes from [0]
  // [2] - the expected result after setting userContextId to the default
  // [3] - the expected result of creating a suffix from [2]
  var dflt_tests = [
    ["", {}, {}, ""],
    ["^userContextId=3", { userContextId: 3 }, {}, ""],
  ];

  // check that we can set the userContextId to default properly
  dflt_tests.forEach(t => {
    let orig = ChromeUtils.createOriginAttributesFromOrigin(uri + t[0]);
    checkValues(orig, t[1]);
    let mod = orig;
    mod.userContextId = 0;
    checkValues(mod, t[2]);
    Assert.equal(ChromeUtils.originAttributesToSuffix(mod), t[3]);
  });

  // each row in the dflt2_tests array has these values:
  // [0] - the suffix used to create an origin attribute from
  // [1] - the expected result of creating an origin attributes from [0]
  // [2] - the expected result after setting firstPartyUri to the default
  // [3] - the expected result of creating a suffix from [2]
  var dflt2_tests = [
    ["", {}, {}, ""],
    ["^firstPartyDomain=foo.com", { firstPartyDomain: "foo.com" }, {}, ""],
  ];

  // check that we can set the userContextId to default properly
  dflt2_tests.forEach(t => {
    let orig = ChromeUtils.createOriginAttributesFromOrigin(uri + t[0]);
    checkValues(orig, t[1]);
    let mod = orig;
    mod.firstPartyDomain = "";
    checkValues(mod, t[2]);
    Assert.equal(ChromeUtils.originAttributesToSuffix(mod), t[3]);
  });

  var fileURI = makeURI("file:///foo/bar").QueryInterface(Ci.nsIFileURL);
  var fileTests = [
    [true, fileURI.spec],
    [false, "file://UNIVERSAL_FILE_URI_ORIGIN"],
  ];
  fileTests.forEach(t => {
    Services.prefs.setBoolPref("security.fileuri.strict_origin_policy", t[0]);
    var filePrin = ssm.createContentPrincipal(fileURI, {});
    Assert.equal(filePrin.origin, t[1]);
  });
  Services.prefs.clearUserPref("security.fileuri.strict_origin_policy");

  var aboutBlankURI = makeURI("about:blank");
  var aboutBlankPrin = ssm.createContentPrincipal(aboutBlankURI, {});
  Assert.ok(
    /^moz-nullprincipal:\{([0-9]|[a-z]|\-){36}\}$/.test(aboutBlankPrin.origin)
  );
}
