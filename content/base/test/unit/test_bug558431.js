
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/CSPUtils.jsm');
Cu.import("resource://testing-common/httpd.js");

var httpserv = null;

const POLICY_FROM_URI = "allow 'self'; img-src *";
const POLICY_PORT = 9000;
const POLICY_URI = "http://localhost:" + POLICY_PORT + "/policy";
const POLICY_URI_RELATIVE = "/policy";
const DOCUMENT_URI = "http://localhost:" + POLICY_PORT + "/document";
const CSP_DOC_BODY = "CSP doc content";
const SD = CSPRep.SRC_DIRECTIVES;

// this will get populated by run_tests()
var TESTS = [];

// helper to make URIs
function mkuri(foo) {
  return Cc["@mozilla.org/network/io-service;1"]
           .getService(Ci.nsIIOService)
           .newURI(foo, null, null);
}

// helper to use .equals on stuff
function do_check_equivalent(foo, bar, stack) {
  if (!stack)
    stack = Components.stack.caller;

  var text = foo + ".equals(" + bar + ")";

  if (foo.equals && foo.equals(bar)) {
    dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
         stack.lineNumber + "] " + text + "\n");
    return;
  }
  do_throw(text, stack);
}

function listener(csp, cspr_static) {
  this.buffer = "";
  this._csp = csp;
  this._cspr_static = cspr_static;
}

listener.prototype = {
  onStartRequest: function (request, ctx) {
  },

  onDataAvailable: function (request, ctx, stream, offset, count) {
    var sInputStream = Cc["@mozilla.org/scriptableinputstream;1"]
      .createInstance(Ci.nsIScriptableInputStream);
    sInputStream.init(stream);
    this.buffer = this.buffer.concat(sInputStream.read(count));
  },

  onStopRequest: function (request, ctx, status) {
    // make sure that we have the full document content, guaranteeing that
    // the document channel has been resumed, before we do the comparisons
    if (this.buffer == CSP_DOC_BODY) {

      // need to re-grab cspr since it may have changed inside the document's
      // nsIContentSecurityPolicy instance.  The problem is, this cspr_str is a
      // string and not a policy due to the way it's exposed from
      // nsIContentSecurityPolicy, so we have to re-parse it.
      let cspr_str = this._csp.policy;
      let cspr = CSPRep.fromString(cspr_str, mkuri(DOCUMENT_URI));

      // and in reparsing it, we lose the 'self' relationships, so need to also
      // reparse the static one (or find a way to resolve 'self' in the parsed
      // policy when doing comparisons).
      let cspr_static_str = this._cspr_static.toString();
      let cspr_static_reparse = CSPRep.fromString(cspr_static_str, mkuri(DOCUMENT_URI));

      // not null, and one policy .equals the other one
      do_check_neq(null, cspr);
      do_check_true(cspr.equals(cspr_static_reparse));

      // final teardown
      if (TESTS.length == 0) {
        httpserv.stop(do_test_finished);
      } else {
        do_test_finished();
        (TESTS.shift())();
      }
    }
  }
};

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/document", csp_doc_response);
  httpserv.registerPathHandler("/policy", csp_policy_response);
  httpserv.start(POLICY_PORT);
  TESTS = [ test_CSPRep_fromPolicyURI, test_CSPRep_fromRelativePolicyURI ];

  // when this triggers the "onStopRequest" callback, it'll
  // go to the next test.
  (TESTS.shift())();
}

function makeChan(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function csp_doc_response(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.bodyOutputStream.write(CSP_DOC_BODY, CSP_DOC_BODY.length);
}

function csp_policy_response(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/csp", false);
  response.bodyOutputStream.write(POLICY_FROM_URI, POLICY_FROM_URI.length);
}

///////////////////// TEST POLICY_URI //////////////////////
function test_CSPRep_fromPolicyURI() {
  do_test_pending();
  let csp = Cc["@mozilla.org/contentsecuritypolicy;1"]
              .createInstance(Ci.nsIContentSecurityPolicy);
  // once the policy-uri is returned we will compare our static CSPRep with one
  // we generated from the content we got back from the network to make sure
  // they are equivalent
  let cspr_static = CSPRep.fromString(POLICY_FROM_URI, mkuri(DOCUMENT_URI));

  // simulates the request for the parent document
  var docChan = makeChan(DOCUMENT_URI);
  docChan.asyncOpen(new listener(csp, cspr_static), null);

  // the resulting policy here can be discarded, since it's going to be
  // "allow *"; when the policy-uri fetching call-back happens, the *real*
  // policy will be in csp.policy
  CSPRep.fromString("policy-uri " + POLICY_URI,
                    mkuri(DOCUMENT_URI), docChan, csp);
}

function test_CSPRep_fromRelativePolicyURI() {
  do_test_pending();
  let csp = Cc["@mozilla.org/contentsecuritypolicy;1"]
              .createInstance(Ci.nsIContentSecurityPolicy);
  // once the policy-uri is returned we will compare our static CSPRep with one
  // we generated from the content we got back from the network to make sure
  // they are equivalent
  let cspr_static = CSPRep.fromString(POLICY_FROM_URI, mkuri(DOCUMENT_URI));

  // simulates the request for the parent document
  var docChan = makeChan(DOCUMENT_URI);
  docChan.asyncOpen(new listener(csp, cspr_static), null);

  // the resulting policy here can be discarded, since it's going to be
  // "allow *"; when the policy-uri fetching call-back happens, the *real*
  // policy will be in csp.policy
  CSPRep.fromString("policy-uri " + POLICY_URI_RELATIVE,
                    mkuri(DOCUMENT_URI), docChan, csp);
}
