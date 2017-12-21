var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
var ssm = Services.scriptSecurityManager;
function makeURI(uri) { return Services.io.newURI(uri); }

function checkThrows(f) {
  var threw = false;
  try { f(); } catch (e) { threw = true; }
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
  Assert.equal(prin.originAttributes.appId, attrs.appId || 0);
  Assert.equal(prin.originAttributes.inIsolatedMozBrowser, attrs.inIsolatedMozBrowser || false);
  Assert.equal(prin.originSuffix, suffix || "");
  Assert.equal(ChromeUtils.originAttributesToSuffix(attrs), suffix || "");
  Assert.ok(ChromeUtils.originAttributesMatchPattern(prin.originAttributes, attrs));
  if (!prin.isNullPrincipal && !prin.origin.startsWith("[")) {
    Assert.ok(ssm.createCodebasePrincipalFromOrigin(prin.origin).equals(prin));
  } else {
    checkThrows(() => ssm.createCodebasePrincipalFromOrigin(prin.origin));
  }
}

function checkSandboxOriginAttributes(arr, attrs, options) {
  options = options || {};
  var sandbox = Cu.Sandbox(arr, options);
  checkOriginAttributes(Cu.getObjectPrincipal(sandbox), attrs,
                        ChromeUtils.originAttributesToSuffix(attrs));
}

// utility function useful for debugging
function printAttrs(name, attrs) {
  info(name + " {\n" +
       "\tappId: " + attrs.appId + ",\n" +
       "\tuserContextId: " + attrs.userContextId + ",\n" +
       "\tinIsolatedMozBrowser: " + attrs.inIsolatedMozBrowser + ",\n" +
       "\tprivateBrowsingId: '" + attrs.privateBrowsingId + "',\n" +
       "\tfirstPartyDomain: '" + attrs.firstPartyDomain + "'\n}");
}


function checkValues(attrs, values) {
  values = values || {};
  // printAttrs("attrs", attrs);
  // printAttrs("values", values);
  Assert.equal(attrs.appId, values.appId || 0);
  Assert.equal(attrs.userContextId, values.userContextId || 0);
  Assert.equal(attrs.inIsolatedMozBrowser, values.inIsolatedMozBrowser || false);
  Assert.equal(attrs.privateBrowsingId, values.privateBrowsingId || "");
  Assert.equal(attrs.firstPartyDomain, values.firstPartyDomain || "");
}

function run_test() {
  // Attributeless origins.
  Assert.equal(ssm.getSystemPrincipal().origin, "[System Principal]");
  checkOriginAttributes(ssm.getSystemPrincipal());
  var exampleOrg = ssm.createCodebasePrincipal(makeURI("http://example.org"), {});
  Assert.equal(exampleOrg.origin, "http://example.org");
  checkOriginAttributes(exampleOrg);
  var exampleCom = ssm.createCodebasePrincipal(makeURI("https://www.example.com:123"), {});
  Assert.equal(exampleCom.origin, "https://www.example.com:123");
  checkOriginAttributes(exampleCom);
  var nullPrin = Cu.getObjectPrincipal(new Cu.Sandbox(null));
  Assert.ok(/^moz-nullprincipal:\{([0-9]|[a-z]|\-){36}\}$/.test(nullPrin.origin));
  checkOriginAttributes(nullPrin);
  var ipv6Prin = ssm.createCodebasePrincipal(makeURI("https://[2001:db8::ff00:42:8329]:123"), {});
  Assert.equal(ipv6Prin.origin, "https://[2001:db8::ff00:42:8329]:123");
  checkOriginAttributes(ipv6Prin);
  var ipv6NPPrin = ssm.createCodebasePrincipal(makeURI("https://[2001:db8::ff00:42:8329]"), {});
  Assert.equal(ipv6NPPrin.origin, "https://[2001:db8::ff00:42:8329]");
  checkOriginAttributes(ipv6NPPrin);
  var ep = Cu.getObjectPrincipal(Cu.Sandbox([exampleCom, nullPrin, exampleOrg]));
  checkOriginAttributes(ep);
  checkCrossOrigin(exampleCom, exampleOrg);
  checkCrossOrigin(exampleOrg, nullPrin);

  // nsEP origins should be in lexical order.
  Assert.equal(ep.origin, `[Expanded Principal [${exampleOrg.origin}, ${exampleCom.origin}, ${nullPrin.origin}]]`);

  // Make sure createCodebasePrincipal does what the rest of gecko does.
  Assert.ok(exampleOrg.equals(Cu.getObjectPrincipal(new Cu.Sandbox("http://example.org"))));

  //
  // Test origin attributes.
  //

  // Just app.
  var exampleOrg_app = ssm.createCodebasePrincipal(makeURI("http://example.org"), {appId: 42});
  var nullPrin_app = ssm.createNullPrincipal({appId: 42});
  checkOriginAttributes(exampleOrg_app, {appId: 42}, "^appId=42");
  checkOriginAttributes(nullPrin_app, {appId: 42}, "^appId=42");
  Assert.equal(exampleOrg_app.origin, "http://example.org^appId=42");

  // Just browser.
  var exampleOrg_browser = ssm.createCodebasePrincipal(makeURI("http://example.org"), {inIsolatedMozBrowser: true});
  var nullPrin_browser = ssm.createNullPrincipal({inIsolatedMozBrowser: true});
  checkOriginAttributes(exampleOrg_browser, {inIsolatedMozBrowser: true}, "^inBrowser=1");
  checkOriginAttributes(nullPrin_browser, {inIsolatedMozBrowser: true}, "^inBrowser=1");
  Assert.equal(exampleOrg_browser.origin, "http://example.org^inBrowser=1");

  // App and browser.
  var exampleOrg_appBrowser = ssm.createCodebasePrincipal(makeURI("http://example.org"), {inIsolatedMozBrowser: true, appId: 42});
  var nullPrin_appBrowser = ssm.createNullPrincipal({inIsolatedMozBrowser: true, appId: 42});
  checkOriginAttributes(exampleOrg_appBrowser, {appId: 42, inIsolatedMozBrowser: true}, "^appId=42&inBrowser=1");
  checkOriginAttributes(nullPrin_appBrowser, {appId: 42, inIsolatedMozBrowser: true}, "^appId=42&inBrowser=1");
  Assert.equal(exampleOrg_appBrowser.origin, "http://example.org^appId=42&inBrowser=1");

  // App and browser, different domain.
  var exampleCom_appBrowser = ssm.createCodebasePrincipal(makeURI("https://www.example.com:123"), {appId: 42, inIsolatedMozBrowser: true});
  checkOriginAttributes(exampleCom_appBrowser, {appId: 42, inIsolatedMozBrowser: true}, "^appId=42&inBrowser=1");
  Assert.equal(exampleCom_appBrowser.origin, "https://www.example.com:123^appId=42&inBrowser=1");

  // First party Uri
  var exampleOrg_firstPartyDomain = ssm.createCodebasePrincipal(makeURI("http://example.org"), {firstPartyDomain: "example.org"});
  checkOriginAttributes(exampleOrg_firstPartyDomain, { firstPartyDomain: "example.org" }, "^firstPartyDomain=example.org");
  Assert.equal(exampleOrg_firstPartyDomain.origin, "http://example.org^firstPartyDomain=example.org");

  // Make sure we don't crash when serializing principals with UNKNOWN_APP_ID.
  try {
    let binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].
                       createInstance(Ci.nsIObjectOutputStream);
    let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
    pipe.init(false, false, 0, 0xffffffff, null);
    binaryStream.setOutputStream(pipe.outputStream);
    binaryStream.writeCompoundObject(simplePrin, Ci.nsISupports, true); // eslint-disable-line no-undef
    binaryStream.close();
  } catch (e) {
    Assert.ok(true);
  }


  // Just userContext.
  var exampleOrg_userContext = ssm.createCodebasePrincipal(makeURI("http://example.org"), {userContextId: 42});
  checkOriginAttributes(exampleOrg_userContext, { userContextId: 42 }, "^userContextId=42");
  Assert.equal(exampleOrg_userContext.origin, "http://example.org^userContextId=42");

  // UserContext and App.
  var exampleOrg_userContextApp = ssm.createCodebasePrincipal(makeURI("http://example.org"), {appId: 24, userContextId: 42});
  var nullPrin_userContextApp = ssm.createNullPrincipal({appId: 24, userContextId: 42});
  checkOriginAttributes(exampleOrg_userContextApp, {appId: 24, userContextId: 42}, "^appId=24&userContextId=42");
  checkOriginAttributes(nullPrin_userContextApp, {appId: 24, userContextId: 42}, "^appId=24&userContextId=42");
  Assert.equal(exampleOrg_userContextApp.origin, "http://example.org^appId=24&userContextId=42");

  checkSandboxOriginAttributes(null, {});
  checkSandboxOriginAttributes("http://example.org", {});
  checkSandboxOriginAttributes("http://example.org", {}, {originAttributes: {}});
  checkSandboxOriginAttributes("http://example.org", {appId: 42}, {originAttributes: {appId: 42}});
  checkSandboxOriginAttributes(["http://example.org"], {});
  checkSandboxOriginAttributes(["http://example.org"], {}, {originAttributes: {}});
  checkSandboxOriginAttributes(["http://example.org"], {appId: 42}, {originAttributes: {appId: 42}});

  // Check that all of the above are cross-origin.
  checkCrossOrigin(exampleOrg_app, exampleOrg);
  checkCrossOrigin(exampleOrg_app, nullPrin_app);
  checkCrossOrigin(exampleOrg_browser, exampleOrg_app);
  checkCrossOrigin(exampleOrg_browser, nullPrin_browser);
  checkCrossOrigin(exampleOrg_appBrowser, exampleOrg_app);
  checkCrossOrigin(exampleOrg_appBrowser, nullPrin_appBrowser);
  checkCrossOrigin(exampleOrg_appBrowser, exampleCom_appBrowser);
  checkCrossOrigin(exampleOrg_firstPartyDomain, exampleOrg);
  checkCrossOrigin(exampleOrg_userContext, exampleOrg);
  checkCrossOrigin(exampleOrg_userContext, exampleOrg_userContextApp);

  // Check Principal kinds.
  function checkKind(prin, kind) {
    Assert.equal(prin.isNullPrincipal, kind == "nullPrincipal");
    Assert.equal(prin.isCodebasePrincipal, kind == "codebasePrincipal");
    Assert.equal(prin.isExpandedPrincipal, kind == "expandedPrincipal");
    Assert.equal(prin.isSystemPrincipal, kind == "systemPrincipal");
  }
  checkKind(ssm.createNullPrincipal({}), "nullPrincipal");
  checkKind(ssm.createCodebasePrincipal(makeURI("http://www.example.com"), {}), "codebasePrincipal");
  checkKind(Cu.getObjectPrincipal(Cu.Sandbox([ssm.createCodebasePrincipal(makeURI("http://www.example.com"), {})])), "expandedPrincipal");
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
    [ "", {} ],
    [ "^appId=5", {appId: 5} ],
    [ "^userContextId=3", {userContextId: 3} ],
    [ "^inBrowser=1", {inIsolatedMozBrowser: true} ],
    [ "^firstPartyDomain=example.org", {firstPartyDomain: "example.org"} ],
    [ "^appId=3&inBrowser=1&userContextId=6",
      {appId: 3, userContextId: 6, inIsolatedMozBrowser: true} ] ];

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

  // each row in the set_tests array has these values:
  // [0] - the suffix used to create an origin attribute from
  // [1] - the expected result of creating an origin attribute from [0]
  // [2] - the pattern to set on the origin attributes
  // [3] - the expected result of setting [2] values on [1]
  // [4] - the expected result of creating a suffix from [3]
  var set_tests = [
    [ "", {}, {appId: 5}, {appId: 5}, "^appId=5" ],
    [ "^appId=5", {appId: 5}, {appId: 3}, {appId: 3}, "^appId=3" ],
    [ "^appId=5", {appId: 5}, {userContextId: 3}, {appId: 5, userContextId: 3}, "^appId=5&userContextId=3" ],
    [ "^appId=5", {appId: 5}, {appId: 3, userContextId: 7}, {appId: 3, userContextId: 7}, "^appId=3&userContextId=7" ] ];

  // check that we can set origin attributes values properly
  set_tests.forEach(t => {
    let orig = ChromeUtils.createOriginAttributesFromOrigin(uri + t[0]);
    checkValues(orig, t[1]);
    let mod = orig;
    for (var key in t[2]) {
      mod[key] = t[2][key];
    }
    checkValues(mod, t[3]);
    Assert.equal(ChromeUtils.originAttributesToSuffix(mod), t[4]);
  });

  // each row in the dflt_tests array has these values:
  // [0] - the suffix used to create an origin attribute from
  // [1] - the expected result of creating an origin attributes from [0]
  // [2] - the expected result after setting userContextId to the default
  // [3] - the expected result of creating a suffix from [2]
  var dflt_tests = [
    [ "", {}, {}, "" ],
    [ "^userContextId=3", {userContextId: 3}, {}, "" ],
    [ "^appId=5", {appId: 5}, {appId: 5}, "^appId=5" ],
    [ "^appId=5&userContextId=3", {appId: 5, userContextId: 3}, {appId: 5}, "^appId=5" ] ];

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
    [ "", {}, {}, "" ],
    [ "^firstPartyDomain=foo.com", {firstPartyDomain: "foo.com"}, {}, "" ],
    [ "^appId=5", {appId: 5}, {appId: 5}, "^appId=5" ],
    [ "^appId=5&firstPartyDomain=foo.com", {appId: 5, firstPartyDomain: "foo.com"}, {appId: 5}, "^appId=5" ] ];

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
    var filePrin = ssm.createCodebasePrincipal(fileURI, {});
    Assert.equal(filePrin.origin, t[1]);
  });
  Services.prefs.clearUserPref("security.fileuri.strict_origin_policy");

  var aboutBlankURI = makeURI("about:blank");
  var aboutBlankPrin = ssm.createCodebasePrincipal(aboutBlankURI, {});
  Assert.ok(/^moz-nullprincipal:\{([0-9]|[a-z]|\-){36}\}$/.test(aboutBlankPrin.origin));
}
