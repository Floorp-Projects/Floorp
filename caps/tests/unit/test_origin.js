var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
var ssm = Services.scriptSecurityManager;
function makeURI(uri) { return Services.io.newURI(uri, null, null); }

function checkThrows(f) {
  var threw = false;
  try { f(); } catch (e) { threw = true }
  do_check_true(threw);
}

function checkCrossOrigin(a, b) {
  do_check_false(a.equals(b));
  do_check_false(a.equalsConsideringDomain(b));
  do_check_false(a.subsumes(b));
  do_check_false(a.subsumesConsideringDomain(b));
  do_check_false(b.subsumes(a));
  do_check_false(b.subsumesConsideringDomain(a));
}

function checkOriginAttributes(prin, attrs, suffix) {
  attrs = attrs || {};
  do_check_eq(prin.originAttributes.appId, attrs.appId || 0);
  do_check_eq(prin.originAttributes.inBrowser, attrs.inBrowser || false);
  do_check_eq(prin.originSuffix, suffix || '');
  if (!prin.isNullPrincipal && !prin.origin.startsWith('[')) {
    do_check_true(ssm.createCodebasePrincipalFromOrigin(prin.origin).equals(prin));
  } else {
    checkThrows(() => ssm.createCodebasePrincipalFromOrigin(prin.origin));
  }
}

function run_test() {
  // Attributeless origins.
  do_check_eq(ssm.getSystemPrincipal().origin, '[System Principal]');
  checkOriginAttributes(ssm.getSystemPrincipal());
  var exampleOrg = ssm.createCodebasePrincipal(makeURI('http://example.org'), {});
  do_check_eq(exampleOrg.origin, 'http://example.org');
  checkOriginAttributes(exampleOrg);
  var exampleCom = ssm.createCodebasePrincipal(makeURI('https://www.example.com:123'), {});
  do_check_eq(exampleCom.origin, 'https://www.example.com:123');
  checkOriginAttributes(exampleCom);
  var nullPrin = Cu.getObjectPrincipal(new Cu.Sandbox(null));
  do_check_true(/^moz-nullprincipal:\{([0-9]|[a-z]|\-){36}\}$/.test(nullPrin.origin));
  checkOriginAttributes(nullPrin);
  var ipv6Prin = ssm.createCodebasePrincipal(makeURI('https://[2001:db8::ff00:42:8329]:123'), {});
  do_check_eq(ipv6Prin.origin, 'https://[2001:db8::ff00:42:8329]:123');
  checkOriginAttributes(ipv6Prin);
  var ipv6NPPrin = ssm.createCodebasePrincipal(makeURI('https://[2001:db8::ff00:42:8329]'), {});
  do_check_eq(ipv6NPPrin.origin, 'https://[2001:db8::ff00:42:8329]');
  checkOriginAttributes(ipv6NPPrin);
  var ep = ssm.createExpandedPrincipal([exampleCom, nullPrin, exampleOrg]);
  checkOriginAttributes(ep);
  checkCrossOrigin(exampleCom, exampleOrg);
  checkCrossOrigin(exampleOrg, nullPrin);

  // nsEP origins should be in lexical order.
  do_check_eq(ep.origin, `[Expanded Principal [${exampleOrg.origin}, ${exampleCom.origin}, ${nullPrin.origin}]]`);

  // Make sure createCodebasePrincipal does what the rest of gecko does.
  do_check_true(exampleOrg.equals(Cu.getObjectPrincipal(new Cu.Sandbox('http://example.org'))));

  //
  // Test origin attributes.
  //

  // Just app.
  var exampleOrg_app = ssm.createCodebasePrincipal(makeURI('http://example.org'), {appId: 42});
  var nullPrin_app = ssm.createNullPrincipal({appId: 42});
  checkOriginAttributes(exampleOrg_app, {appId: 42}, '^appId=42');
  checkOriginAttributes(nullPrin_app, {appId: 42}, '^appId=42');
  do_check_eq(exampleOrg_app.origin, 'http://example.org^appId=42');

  // Just browser.
  var exampleOrg_browser = ssm.createCodebasePrincipal(makeURI('http://example.org'), {inBrowser: true});
  var nullPrin_browser = ssm.createNullPrincipal({inBrowser: true});
  checkOriginAttributes(exampleOrg_browser, {inBrowser: true}, '^inBrowser=1');
  checkOriginAttributes(nullPrin_browser, {inBrowser: true}, '^inBrowser=1');
  do_check_eq(exampleOrg_browser.origin, 'http://example.org^inBrowser=1');

  // App and browser.
  var exampleOrg_appBrowser = ssm.createCodebasePrincipal(makeURI('http://example.org'), {inBrowser: true, appId: 42});
  var nullPrin_appBrowser = ssm.createNullPrincipal({inBrowser: true, appId: 42});
  checkOriginAttributes(exampleOrg_appBrowser, {appId: 42, inBrowser: true}, '^appId=42&inBrowser=1');
  checkOriginAttributes(nullPrin_appBrowser, {appId: 42, inBrowser: true}, '^appId=42&inBrowser=1');
  do_check_eq(exampleOrg_appBrowser.origin, 'http://example.org^appId=42&inBrowser=1');

  // App and browser, different domain.
  var exampleCom_appBrowser = ssm.createCodebasePrincipal(makeURI('https://www.example.com:123'), {appId: 42, inBrowser: true});
  checkOriginAttributes(exampleCom_appBrowser, {appId: 42, inBrowser: true}, '^appId=42&inBrowser=1');
  do_check_eq(exampleCom_appBrowser.origin, 'https://www.example.com:123^appId=42&inBrowser=1');

  // Addon.
  var exampleOrg_addon = ssm.createCodebasePrincipal(makeURI('http://example.org'), {addonId: 'dummy'});
  checkOriginAttributes(exampleOrg_addon, { addonId: "dummy" }, '^addonId=dummy');
  do_check_eq(exampleOrg_addon.origin, 'http://example.org^addonId=dummy');

  // Make sure we don't crash when serializing principals with UNKNOWN_APP_ID.
  try {
    let binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].
                       createInstance(Ci.nsIObjectOutputStream);
    let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
    pipe.init(false, false, 0, 0xffffffff, null);
    binaryStream.setOutputStream(pipe.outputStream);
    binaryStream.writeCompoundObject(simplePrin, Ci.nsISupports, true);
    binaryStream.close();
  } catch (e) {
    do_check_true(true);
  }


  // Just userContext.
  var exampleOrg_userContext = ssm.createCodebasePrincipal(makeURI('http://example.org'), {userContextId: 42});
  checkOriginAttributes(exampleOrg_userContext, { userContextId: 42 }, '^userContextId=42');
  do_check_eq(exampleOrg_userContext.origin, 'http://example.org^userContextId=42');

  // UserContext and Addon.
  var exampleOrg_userContextAddon = ssm.createCodebasePrincipal(makeURI('http://example.org'), {addonId: 'dummy', userContextId: 42});
  var nullPrin_userContextAddon = ssm.createNullPrincipal({addonId: 'dummy', userContextId: 42});
  checkOriginAttributes(exampleOrg_userContextAddon, {addonId: 'dummy', userContextId: 42}, '^addonId=dummy&userContextId=42');
  checkOriginAttributes(nullPrin_userContextAddon, {addonId: 'dummy', userContextId: 42}, '^addonId=dummy&userContextId=42');
  do_check_eq(exampleOrg_userContextAddon.origin, 'http://example.org^addonId=dummy&userContextId=42');

  // UserContext and App.
  var exampleOrg_userContextApp = ssm.createCodebasePrincipal(makeURI('http://example.org'), {appId: 24, userContextId: 42});
  var nullPrin_userContextApp = ssm.createNullPrincipal({appId: 24, userContextId: 42});
  checkOriginAttributes(exampleOrg_userContextApp, {appId: 24, userContextId: 42}, '^appId=24&userContextId=42');
  checkOriginAttributes(nullPrin_userContextApp, {appId: 24, userContextId: 42}, '^appId=24&userContextId=42');
  do_check_eq(exampleOrg_userContextApp.origin, 'http://example.org^appId=24&userContextId=42');

  // Just signedPkg
  var exampleOrg_signedPkg = ssm.createCodebasePrincipal(makeURI('http://example.org'), {signedPkg: 'whatever'});
  checkOriginAttributes(exampleOrg_signedPkg, { signedPkg: 'id' }, '^signedPkg=whatever');
  do_check_eq(exampleOrg_signedPkg.origin, 'http://example.org^signedPkg=whatever');

  // signedPkg and browser
  var exampleOrg_signedPkg_browser = ssm.createCodebasePrincipal(makeURI('http://example.org'), {signedPkg: 'whatever', inBrowser: true});
  checkOriginAttributes(exampleOrg_signedPkg_browser, { signedPkg: 'whatever', inBrowser: true }, '^inBrowser=1&signedPkg=whatever');
  do_check_eq(exampleOrg_signedPkg_browser.origin, 'http://example.org^inBrowser=1&signedPkg=whatever');

  // Just signedPkg (but different value from 'exampleOrg_signedPkg_app')
  var exampleOrg_signedPkg_another = ssm.createCodebasePrincipal(makeURI('http://example.org'), {signedPkg: 'whatup'});

  // Check that all of the above are cross-origin.
  checkCrossOrigin(exampleOrg_app, exampleOrg);
  checkCrossOrigin(exampleOrg_app, nullPrin_app);
  checkCrossOrigin(exampleOrg_browser, exampleOrg_app);
  checkCrossOrigin(exampleOrg_browser, nullPrin_browser);
  checkCrossOrigin(exampleOrg_appBrowser, exampleOrg_app);
  checkCrossOrigin(exampleOrg_appBrowser, nullPrin_appBrowser);
  checkCrossOrigin(exampleOrg_appBrowser, exampleCom_appBrowser);
  checkCrossOrigin(exampleOrg_addon, exampleOrg);
  checkCrossOrigin(exampleOrg_userContext, exampleOrg);
  checkCrossOrigin(exampleOrg_userContextAddon, exampleOrg);
  checkCrossOrigin(exampleOrg_userContext, exampleOrg_userContextAddon);
  checkCrossOrigin(exampleOrg_userContext, exampleOrg_userContextApp);
  checkCrossOrigin(exampleOrg_signedPkg, exampleOrg);
  checkCrossOrigin(exampleOrg_signedPkg, exampleOrg_signedPkg_browser);
  checkCrossOrigin(exampleOrg_signedPkg, exampleOrg_signedPkg_another);
}
