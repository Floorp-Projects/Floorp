/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { addDebuggerToGlobal } = ChromeUtils.importESModule(
  "resource://gre/modules/jsdebugger.sys.mjs"
);
addDebuggerToGlobal(this);

const ESM_URL = "resource://test/es6module_devtoolsLoader.sys.mjs";

// Toggle the following pref to enable Cu.getModuleImportStack()
if (AppConstants.NIGHTLY_BUILD) {
  Services.prefs.setBoolPref("browser.startup.record", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.startup.record");
  });
}

add_task(async function testDevToolsModuleLoader() {
  const dbg = new Debugger();

  const sharedGlobal = Cu.getGlobalForObject(Services);
  const sharedPrincipal = Cu.getObjectPrincipal(sharedGlobal);

  info("Test importing in the regular shared loader");
  const ns = ChromeUtils.importESModule(ESM_URL);
  Assert.equal(ns.x, 0);
  ns.increment();
  Assert.equal(ns.x, 1);
  const nsGlobal = Cu.getGlobalForObject(ns);
  const nsPrincipal = Cu.getObjectPrincipal(nsGlobal);
  Assert.equal(nsGlobal, sharedGlobal, "Without any parameter, importESModule load in the shared JSM global");
  Assert.equal(nsPrincipal, sharedPrincipal);
  Assert.ok(nsPrincipal.isSystemPrincipal);
  info("Global of ESM loaded in the shared loader can be inspected by the Debugger");
  dbg.addDebuggee(nsGlobal);
  Assert.ok(true, "The global is accepted by the Debugger API");

  const ns1 = ChromeUtils.importESModule(ESM_URL, { global: "shared" });
  Assert.equal(ns1, ns, "Passing global: 'shared' from the shared JSM global is equivalent to regular importESModule");

  info("Test importing in the devtools loader");
  const ns2 = ChromeUtils.importESModule(ESM_URL, { global: "devtools" });
  Assert.equal(ns2.x, 0, "We get a new module instance with a new incremented number");
  Assert.notEqual(ns2, ns, "We imported a new instance of the module");
  Assert.notEqual(ns2.importedObject, ns.importedObject, "The two module instances expose distinct objects");
  Assert.equal(ns2.importESModuleDevTools, ns2.importedObject, "When using global: 'devtools' from a devtools global, we keep loading in the same loader");
  Assert.equal(ns2.importESModuleCurrent, ns2.importedObject, "When using global: 'current' from a devtools global, we keep loading in the same loader");
  Assert.equal(ns2.importESModuleContextual, ns2.importedObject, "When using global: 'contextual' from a devtools global, we keep loading in the same loader");
  Assert.ok(ns2.importESModuleNoOptionFailed1, "global option is required in DevTools global");
  Assert.ok(ns2.importESModuleNoOptionFailed2, "global option is required in DevTools global");
  Assert.equal(ns2.importESModuleShared, ns.importedObject, "When passing global: 'shared', we load in the shared global, even from a devtools global");

  Assert.equal(ns2.importLazyDevTools(), ns2.importedObject, "When using global: 'devtools' from a devtools global, we keep loading in the same loader");
  Assert.equal(ns2.importLazyCurrent(), ns2.importedObject, "When using global: 'current' from a devtools global, we keep loading in the same loader");
  Assert.equal(ns2.importLazyContextual(), ns2.importedObject, "When using global: 'contextual' from a devtools global, we keep loading in the same loader");
  Assert.ok(ns2.importLazyNoOptionFailed1, "global option is required in DevTools global");
  Assert.ok(ns2.importLazyNoOptionFailed2, "global option is required in DevTools global");
  Assert.equal(ns2.importLazyShared(), ns.importedObject, "When passing global: 'shared', we load in the shared global, even from a devtools global");

  info("When using the devtools loader, we load in a distinct global, but the same compartment");
  const ns2Global = Cu.getGlobalForObject(ns2);
  const ns2Principal = Cu.getObjectPrincipal(ns2Global);
  Assert.notEqual(ns2Global, sharedGlobal, "The module is loaded in a distinct global");
  Assert.equal(ns2Principal, sharedPrincipal, "The principal is still the shared system principal");
  Assert.equal(Cu.getGlobalForObject(ns2.importedObject), ns2Global, "Nested dependencies are also loaded in the same devtools global");
  Assert.throws(() => dbg.addDebuggee(ns2Global), /TypeError: passing non-debuggable global to addDebuggee/,
    "Global os ESM loaded in the devtools loader can't be inspected by the Debugee");

  info("Re-import the same module in the devtools loader");
  const ns3 = ChromeUtils.importESModule(ESM_URL, { global: "devtools" });
  Assert.equal(ns3, ns2, "We import the exact same module");
  Assert.equal(ns3.importedObject, ns2.importedObject, "The two module expose the same objects");

  info("Import a module only from the devtools loader");
  const ns4 = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader_only.js", { global: "devtools" });
  const ns4Global = Cu.getGlobalForObject(ns4);
  Assert.equal(ns4Global, ns2Global, "The module is loaded in the same devtools global");

  // getModuleImportStack only works on nightly builds
  if (AppConstants.NIGHTLY_BUILD) {
    info("Assert the behavior of getModuleImportStack on modules loaded in the devtools loader");
    Assert.ok(Cu.getModuleImportStack(ESM_URL).includes("testDevToolsModuleLoader"));
    Assert.ok(Cu.getModuleImportStack("resource://test/es6module_devtoolsLoader.js").includes("testDevToolsModuleLoader"));
    Assert.ok(Cu.getModuleImportStack("resource://test/es6module_devtoolsLoader.js").includes(ESM_URL));
    // Previous import stack were for module loaded via the shared jsm loader.
    // Let's also assert that we get stack traces for modules loaded via the devtools loader.
    Assert.ok(Cu.getModuleImportStack("resource://test/es6module_devtoolsLoader_only.js").includes("testDevToolsModuleLoader"));
  }
});
