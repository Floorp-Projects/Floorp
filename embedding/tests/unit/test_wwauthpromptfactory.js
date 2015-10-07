const Cc = Components.classes;
const Ci = Components.interfaces;

var authPromptRequestReceived;

const tPFCID = Components.ID("{749e62f4-60ae-4569-a8a2-de78b649660f}");
const tPFContract = "@mozilla.org/passwordmanager/authpromptfactory;1";

/*
 * TestPromptFactory
 *
 * Implements nsIPromptFactory
 */
var TestPromptFactory = {
  QueryInterface: function tPF_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsIPromptFactory))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  createInstance: function tPF_ci(outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  lockFactory: function tPF_lockf(lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  getPrompt : function tPF_getPrompt(aWindow, aIID) {
    if (aIID.equals(Ci.nsIAuthPrompt) ||
        aIID.equals(Ci.nsIAuthPrompt2)) {
      authPromptRequestReceived = true;
      return {};
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}; // end of TestPromptFactory implementation

/*
 * The tests
 */
function run_test() {
  Components.manager.nsIComponentRegistrar.registerFactory(tPFCID,
    "TestPromptFactory", tPFContract, TestPromptFactory);

  // Make sure that getting both nsIAuthPrompt and nsIAuthPrompt2 works
  // (these should work independently of whether the application has
  // nsIPromptService2)
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].getService();

  authPromptRequestReceived = false;

  do_check_neq(ww.nsIPromptFactory.getPrompt(null, Ci.nsIAuthPrompt), null);

  do_check_true(authPromptRequestReceived);

  authPromptRequestReceived = false;

  do_check_neq(ww.nsIPromptFactory.getPrompt(null, Ci.nsIAuthPrompt2), null);

  do_check_true(authPromptRequestReceived);
}
