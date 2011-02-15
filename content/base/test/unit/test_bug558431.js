Components.utils.import('resource://gre/modules/CSPUtils.jsm');
do_load_httpd_js();

var httpserv = null;

const POLICY_FROM_URI = "allow 'self'; img-src *";
const POLICY_PORT = 9000;
const POLICY_URI = "http://localhost:" + POLICY_PORT + "/policy";
const POLICY_URI_RELATIVE = "/policy";
const DOCUMENT_URI = "http://localhost:" + POLICY_PORT + "/document";
const CSP_DOC_BODY = "CSP doc content";
const SD = CSPRep.SRC_DIRECTIVES;
const MAX_TESTS = 2;
var TESTS_COMPLETED = 0;

var cspr, cspr_static;

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

function listener() {
  this.buffer = "";
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
      // "policy-uri failed to load"
      do_check_neq(null, cspr);

      // other directives inherit self
      for (var i in SD) {
        do_check_equivalent(cspr._directives[SD[i]],
                            cspr_static._directives[SD[i]]);
      }

      do_test_finished();
      TESTS_COMPLETED++;
      // final teardown
      if (TESTS_COMPLETED == MAX_TESTS) {
        httpserv.stop(function(){});
      }
    }
  }
};

function run_test() {
  httpserv = new nsHttpServer();
  httpserv.registerPathHandler("/document", csp_doc_response);
  httpserv.registerPathHandler("/policy", csp_policy_response);
  httpserv.start(POLICY_PORT);

  var tests = [ test_CSPRep_fromPolicyURI, test_CSPRep_fromRelativePolicyURI];
  for (var i = 0 ; i < tests.length ; i++) {
    tests[i]();
    do_test_pending();
  }
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
  var csp = Components.classes["@mozilla.org/contentsecuritypolicy;1"]
    .createInstance[Components.interfaces.nsIContentSecurityPolicy];
  // once the policy-uri is returned we will compare our static CSPRep with one
  // we generated from the content we got back from the network to make sure
  // they are equivalent
  cspr_static = CSPRep.fromString(POLICY_FROM_URI, DOCUMENT_URI);
  // simulates the request for the parent document
  var docChan = makeChan(DOCUMENT_URI);
  docChan.asyncOpen(new listener(), null);
  cspr = CSPRep.fromString("policy-uri " + POLICY_URI, DOCUMENT_URI, docChan, csp);
}

function test_CSPRep_fromRelativePolicyURI() {
  var csp = Components.classes["@mozilla.org/contentsecuritypolicy;1"]
    .createInstance[Components.interfaces.nsIContentSecurityPolicy];
  // once the policy-uri is returned we will compare our static CSPRep with one
  // we generated from the content we got back from the network to make sure
  // they are equivalent
  cspr_static = CSPRep.fromString(POLICY_FROM_URI, DOCUMENT_URI);
  // simulates the request for the parent document
  var docChan = makeChan(DOCUMENT_URI);
  docChan.asyncOpen(new listener(), null);
  cspr = CSPRep.fromString("policy-uri " + POLICY_URI_RELATIVE, DOCUMENT_URI, docChan, csp);
}
