/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//load('CSPUtils.jsm');
Components.utils.import('resource://gre/modules/CSPUtils.jsm');
Components.utils.import('resource://gre/modules/NetUtil.jsm');

// load the HTTP server
do_load_httpd_js();

var httpServer = new nsHttpServer();

const POLICY_FROM_URI = "allow 'self'; img-src *";
const POLICY_PORT = 9000;
const POLICY_URI = "http://localhost:" + POLICY_PORT + "/policy";
const POLICY_URI_RELATIVE = "/policy";

//converts string to nsIURI
function URI(uriString) {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
  return ioService.newURI(uriString, null, null);
}


// helper to assert that an array has the given value somewhere.
function do_check_in_array(arr, val, stack) {
  if (!stack)
    stack = Components.stack.caller;

  var text = val + " in [" + arr.join(",") + "]";

  for(var i in arr) {
    dump(".......... " + i + "> " + arr[i] + "\n");
    if(arr[i] == val) {
      //succeed
      ++_passedChecks;
      dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
           stack.lineNumber + "] " + text + "\n");
      return;
    }
  }
  do_throw(text, stack);
}

// helper to assert that an object or array must have a given key
function do_check_has_key(foo, key, stack) {
  if (!stack) 
    stack = Components.stack.caller;

  var keys = [];
  for (let k in foo) { keys.push(k); }
  var text = key + " in [" + keys.join(",") + "]";

  for (var x in foo) {
    if (x == key) {
      //succeed
      ++_passedChecks;
      dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
           stack.lineNumber + "] " + text + "\n");
      return;
    }
  }
  do_throw(text, stack);
}

// helper to use .equals on stuff
function do_check_equivalent(foo, bar, stack) {
  if (!stack) 
    stack = Components.stack.caller;

  var text = foo + ".equals(" + bar + ")";

  if(foo.equals && foo.equals(bar)) {
    ++_passedChecks;
      dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
           stack.lineNumber + "] " + text + "\n");
      return;
  }
  do_throw(text, stack);
}

var tests = [];
function test(fcn) {
  tests.push(fcn);
}

test(
  function test_CSPHost_fromstring() {
    var h;

    h = CSPHost.fromString("*");
    do_check_neq(null, h); // "* lone wildcard should work"

    h = CSPHost.fromString("foo.bar");
    do_check_neq(null, h); // "standard tuple failed"

    h = CSPHost.fromString("*.bar");
    do_check_neq(null, h); // "wildcard failed"

    h = CSPHost.fromString("foo.*.bar");
    do_check_eq(null, h); // "wildcard in wrong place worked"

    h = CSPHost.fromString("com");
    do_check_neq(null, h); // "lone symbol should not fail"

    h = CSPHost.fromString("f00b4r.com");
    do_check_neq(null, h); // "Numbers in hosts should work"

    h = CSPHost.fromString("foo-bar.com");
    do_check_neq(null, h); // "dashes in hosts should work"

    h = CSPHost.fromString("foo!bar.com");
    do_check_eq(null, h); // "special chars in hosts should fail"
  });

test(
  function test_CSPHost_clone() {
    h = CSPHost.fromString("*.a.b.c");
    h2 = h.clone();
    for(var i in h._segments) {
      // "cloned segments should match"
      do_check_eq(h._segments[i], h2._segments[i]);
    }
  });

test(
  function test_CSPHost_permits() {
    var h = CSPHost.fromString("*.b.c");
    var h2 = CSPHost.fromString("a.b.c");
    do_check_true( h.permits(h2));       //"CSPHost *.b.c should allow CSPHost a.b.c"
    do_check_true( h.permits("a.b.c"));  //"CSPHost *.b.c should allow string a.b.c"
    do_check_false(h.permits("b.c"));    //"CSPHost *.b.c should not allow string b.c"
    do_check_false(h.permits("a.a.c"));  //"CSPHost *.b.c should not allow string a.a.c"
    do_check_false(h2.permits(h));       //"CSPHost a.b.c should not allow CSPHost *.b.c"
    do_check_false(h2.permits("b.c"));   //"CSPHost a.b.c should not allow string b.c"
    do_check_true( h2.permits("a.b.c")); //"CSPHost a.b.c should allow string a.b.c"
  });

test(
    function test_CSPHost_intersectWith() {
      var h = CSPHost.fromString("*.b.c");
      //"*.a.b.c ^ *.b.c should be *.a.b.c"
      do_check_eq("*.a.b.c", h.intersectWith(CSPHost.fromString("*.a.b.c")).toString());

      //"*.b.c ^ *.d.e should not work (null)"
      do_check_eq(null, h.intersectWith(CSPHost.fromString("*.d.e")));
    });

///////////////////// Test the Source object //////////////////////

test(
    function test_CSPSource_fromString() {
    // can't do these tests because "self" is not defined.
      //"basic source should not be null.");
      do_check_neq(null, CSPSource.fromString("a.com"));

      //"ldh characters should all work for host.");
      do_check_neq(null, CSPSource.fromString("a2-c.com"));

      //"wildcard should work in first token for host.");
      do_check_neq(null, CSPSource.fromString("*.a.com"));

      //print(" --- Ignore the following two errors if they print ---");
      //"wildcard should not work in non-first token for host.");
      do_check_eq(null, CSPSource.fromString("x.*.a.com"));

      //"funny characters (#) should not work for host.");
      do_check_eq(null, CSPSource.fromString("a#2-c.com"));

      //print(" --- Stop ignoring errors that print ---\n");

      //"failed to parse host with port.");
      do_check_neq(null, CSPSource.create("a.com:23"));
      //"failed to parse host with scheme.");
      do_check_neq(null, CSPSource.create("https://a.com"));
      //"failed to parse host with scheme and port.");
      do_check_neq(null, CSPSource.create("https://a.com:200"));
    });

test(
    function test_CSPSource_fromString_withSelf() {
      var src;
      src = CSPSource.create("a.com", "https://foobar.com:443");
      //"src should inherit port *
      do_check_true(src.permits("https://a.com:443"));
      //"src should inherit and require https scheme
      do_check_false(src.permits("http://a.com"));
      //"src should inherit scheme 'https'"
      do_check_true(src.permits("https://a.com"));
      
      src = CSPSource.create("http://a.com", "https://foobar.com:443");
      //"src should inherit and require http scheme"
      do_check_false(src.permits("https://a.com"));
      //"src should inherit scheme 'http'"
      do_check_true(src.permits("http://a.com"));
      //"src should inherit port and scheme from parent"
      //"src should inherit default port for 'http'"
      do_check_true(src.permits("http://a.com:80"));
      
      src = CSPSource.create("'self'", "https://foobar.com:443");
      //"src should inherit port *
      do_check_true(src.permits("https://foobar.com:443"));
      //"src should inherit and require https scheme
      do_check_false(src.permits("http://foobar.com"));
      //"src should inherit scheme 'https'"
      do_check_true(src.permits("https://foobar.com"));
      //"src should reject other hosts"
      do_check_false(src.permits("https://a.com"));

      src = CSPSource.create("javascript:", "https://foobar.com:443");
      //"hostless schemes should be parseable."
      var aUri = NetUtil.newURI("javascript:alert('foo');");
      do_check_true(src.permits(aUri));
      //"src should reject other hosts"
      do_check_false(src.permits("https://a.com"));
      //"nothing else should be allowed"
      do_check_false(src.permits("https://foobar.com"));

    });

///////////////////// Test the source list //////////////////////

test(
    function test_CSPSourceList_fromString() {
      var sd = CSPSourceList.fromString("'none'");
      //"'none' -- should parse"
      do_check_neq(null,sd);
      // "'none' should be a zero-length list"
      do_check_eq(0, sd._sources.length);
      do_check_true(sd.isNone());

      sd = CSPSourceList.fromString("*");
      //"'*' should be a zero-length list"
      do_check_eq(0, sd._sources.length);

      //print(" --- Ignore the following three errors if they print ---");
      //"funny char in host"
      do_check_true(CSPSourceList.fromString("f!oo.bar").isNone());
      //"funny char in scheme"
      do_check_true(CSPSourceList.fromString("ht!ps://f-oo.bar").isNone());
      //"funny char in port"
      do_check_true(CSPSourceList.fromString("https://f-oo.bar:3f").isNone());
      //print(" --- Stop ignoring errors that print ---\n");
    });

test(
    function test_CSPSourceList_fromString_twohost() {
      var str = "foo.bar:21 https://ras.bar";
      var parsed = "http://foo.bar:21 https://ras.bar:443";
      var sd = CSPSourceList.fromString(str, URI("http://self.com:80"));
      //"two-host list should parse"
      do_check_neq(null,sd);
      //"two-host list should parse to two hosts"
      do_check_eq(2, sd._sources.length);
      //"two-host list should contain original data"
      do_check_eq(parsed, sd.toString());
    });

test(
    function test_CSPSourceList_permits() {
      var nullSourceList = CSPSourceList.fromString("'none'");
      var simpleSourceList = CSPSourceList.fromString("a.com", URI("http://self.com"));
      var doubleSourceList = CSPSourceList.fromString("https://foo.com http://bar.com:88",
                                                      URI("http://self.com:88"));
      var allSourceList = CSPSourceList.fromString("*");

      //'none' should permit none."
      do_check_false( nullSourceList.permits("http://a.com"));
      //a.com should permit a.com"
      do_check_true( simpleSourceList.permits("http://a.com"));
      //wrong host"
      do_check_false( simpleSourceList.permits("http://b.com"));
      //double list permits http://bar.com:88"
      do_check_true( doubleSourceList.permits("http://bar.com:88"));
      //double list permits https://bar.com:88"
      do_check_false( doubleSourceList.permits("https://bar.com:88"));
      //double list does not permit http://bar.com:443"
      do_check_false( doubleSourceList.permits("http://bar.com:443"));
      //"double list permits https://foo.com:88" (should not inherit port)
      do_check_false( doubleSourceList.permits("https://foo.com:88"));
      //"double list does not permit foo.com on http"
      do_check_false( doubleSourceList.permits("http://foo.com"));

      //"* does not permit specific host"
      do_check_true( allSourceList.permits("http://x.com:23"));
      //"* does not permit a long host with no port"
      do_check_true( allSourceList.permits("http://a.b.c.d.e.f.g.h.i.j.k.l.x.com"));

    });

test(
    function test_CSPSourceList_intersect() {
      // for this test, 'self' values are irrelevant
      // policy a /\ policy b intersects policies, not context (where 'self'
      // values come into play)
      var nullSourceList = CSPSourceList.fromString("'none'");
      var simpleSourceList = CSPSourceList.fromString("a.com");
      var doubleSourceList = CSPSourceList.fromString("https://foo.com http://bar.com:88");
      var singleFooSourceList = CSPSourceList.fromString("https://foo.com");
      var allSourceList = CSPSourceList.fromString("*");

      //"Intersection of one source with 'none' source list should be none.");
      do_check_true(nullSourceList.intersectWith(simpleSourceList).isNone());
      //"Intersection of two sources with 'none' source list should be none.");
      do_check_true(nullSourceList.intersectWith(doubleSourceList).isNone());
      //"Intersection of '*' with 'none' source list should be none.");
      do_check_true(nullSourceList.intersectWith(allSourceList).isNone());

      //"Intersection of one source with '*' source list should be one source.");
      do_check_equivalent(allSourceList.intersectWith(simpleSourceList),
                          simpleSourceList);
      //"Intersection of two sources with '*' source list should be two sources.");
      do_check_equivalent(allSourceList.intersectWith(doubleSourceList),
                          doubleSourceList);

      //"Non-overlapping source lists should intersect to 'none'");
      do_check_true(simpleSourceList.intersectWith(doubleSourceList).isNone());

      //"subset and superset should intersect to subset.");
      do_check_equivalent(singleFooSourceList,
                          doubleSourceList.intersectWith(singleFooSourceList));

      //TODO: write more tests?

    });

///////////////////// Test the Whole CSP rep object //////////////////////

test(
    function test_CSPRep_fromString() {

      // check default init
      //ASSERT(!(new CSPRep())._isInitialized, "Uninitialized rep thinks it is.")

      var cspr;
      var cspr_allowval;
      var SD = CSPRep.SRC_DIRECTIVES;

      // check default policy "allow *"
      cspr = CSPRep.fromString("allow *", URI("http://self.com:80"));
      // "DEFAULT_SRC directive is missing when specified in fromString"
      do_check_has_key(cspr._directives, SD.DEFAULT_SRC);

      // ... and check that the other directives were auto-filled with the
      // DEFAULT_SRC one.
      cspr_allowval = cspr._directives[SD.DEFAULT_SRC];
      for(var d in SD) {
        //"Missing key " + d
        do_check_has_key(cspr._directives, SD[d]);
        //"Implicit directive " + d + " has non-allow value."
        do_check_eq(cspr._directives[SD[d]].toString(), cspr_allowval.toString());
      }
    });


test(
    function test_CSPRep_defaultSrc() {
      var cspr, cspr_default_val, cspr_allow;
      var SD = CSPRep.SRC_DIRECTIVES;

      // apply policy of "default-src *" (e.g. "allow *")
      cspr = CSPRep.fromString("default-src *", URI("http://self.com:80"));
      // "DEFAULT_SRC directive is missing when specified in fromString"
      do_check_has_key(cspr._directives, SD.DEFAULT_SRC);

      // check that the other directives were auto-filled with the
      // DEFAULT_SRC one.
      cspr_default_val = cspr._directives[SD.DEFAULT_SRC];
      for (var d in SD) {
        do_check_has_key(cspr._directives, SD[d]);
        // "Implicit directive " + d + " has non-default-src value."
        do_check_eq(cspr._directives[SD[d]].toString(), cspr_default_val.toString());
      }

      // check that |allow *| and |default-src *| are parsed equivalently and
      // result in the same set of explicit policy directives
      cspr = CSPRep.fromString("default-src *", URI("http://self.com:80"));
      cspr_allow = CSPRep.fromString("allow *", URI("http://self.com:80"));

      for (var d in SD) {
        do_check_equivalent(cspr._directives[SD[d]],
                            cspr_allow._directives[SD[d]]);
      }
    });


test(
    function test_CSPRep_fromString_oneDir() {

      var cspr;
      var SD = CSPRep.SRC_DIRECTIVES;
      var DEFAULTS = [SD.STYLE_SRC, SD.MEDIA_SRC, SD.IMG_SRC, SD.FRAME_SRC];

      // check one-directive policies
      cspr = CSPRep.fromString("allow bar.com; script-src https://foo.com", 
                               URI("http://self.com"));

      for(var x in DEFAULTS) {
        //DEFAULTS[x] + " does not use default rule."
        do_check_false(cspr.permits("http://bar.com:22", DEFAULTS[x]));
        //DEFAULTS[x] + " does not use default rule."
        do_check_true(cspr.permits("http://bar.com:80", DEFAULTS[x]));
        //DEFAULTS[x] + " does not use default rule."
        do_check_false(cspr.permits("https://foo.com:400", DEFAULTS[x]));
        //DEFAULTS[x] + " does not use default rule."
        do_check_false(cspr.permits("https://foo.com", DEFAULTS[x]));
      }
      //"script-src false positive in policy.
      do_check_false(cspr.permits("http://bar.com:22", SD.SCRIPT_SRC));
      //"script-src false negative in policy.
      do_check_true(cspr.permits("https://foo.com:443", SD.SCRIPT_SRC));
    });

test(
    function test_CSPRep_fromString_twodir() {
      var cspr;
      var SD = CSPRep.SRC_DIRECTIVES;
      var DEFAULTS = [SD.STYLE_SRC, SD.MEDIA_SRC, SD.FRAME_SRC];

      // check two-directive policies
      var polstr = "allow allow.com; "
                  + "script-src https://foo.com; "
                  + "img-src bar.com:*";
      cspr = CSPRep.fromString(polstr, URI("http://self.com"));

      for(var x in DEFAULTS) {
        do_check_true(cspr.permits("http://allow.com", DEFAULTS[x]));
        //DEFAULTS[x] + " does not use default rule.
        do_check_false(cspr.permits("https://foo.com:400", DEFAULTS[x]));
        //DEFAULTS[x] + " does not use default rule.
        do_check_false(cspr.permits("http://bar.com:400", DEFAULTS[x]));
        //DEFAULTS[x] + " does not use default rule.
      }
      //"img-src does not use default rule.
      do_check_false(cspr.permits("http://allow.com:22", SD.IMG_SRC));
      //"img-src does not use default rule.
      do_check_false(cspr.permits("https://foo.com:400", SD.IMG_SRC));
      //"img-src does not use default rule.
      do_check_true(cspr.permits("http://bar.com:88", SD.IMG_SRC));

      //"script-src does not use default rule.
      do_check_false(cspr.permits("http://allow.com:22", SD.SCRIPT_SRC));
      //"script-src does not use default rule.
      do_check_true(cspr.permits("https://foo.com:443", SD.SCRIPT_SRC));
      //"script-src does not use default rule.
      do_check_false(cspr.permits("http://bar.com:400", SD.SCRIPT_SRC));
    });

test(function test_CSPRep_fromString_withself() {
      var cspr;
      var SD = CSPRep.SRC_DIRECTIVES;
      var self = "https://self.com:34";

      // check one-directive policies
      cspr = CSPRep.fromString("allow 'self'; script-src 'self' https://*:*",
                              URI(self));
      //"img-src does not enforce default rule, 'self'.
      do_check_false(cspr.permits("https://foo.com:400", SD.IMG_SRC));
      //"img-src does not allow self
      do_check_true(cspr.permits(self, SD.IMG_SRC));
      //"script-src is too relaxed
      do_check_false(cspr.permits("http://evil.com", SD.SCRIPT_SRC));
      //"script-src should allow self
      do_check_true(cspr.permits(self, SD.SCRIPT_SRC));
      //"script-src is too strict on host/port
      do_check_true(cspr.permits("https://evil.com:100", SD.SCRIPT_SRC));
     });

//////////////// TEST FRAME ANCESTOR DEFAULTS /////////////////
// (see bug 555068)
test(function test_FrameAncestor_defaults() {
      var cspr;
      var SD = CSPRep.SRC_DIRECTIVES;
      var self = "http://self.com:34";

      cspr = CSPRep.fromString("allow 'none'", URI(self));

      //"frame-ancestors should default to * not 'allow' value"
      do_check_true(cspr.permits("https://foo.com:400", SD.FRAME_ANCESTORS));
      do_check_true(cspr.permits("http://self.com:34", SD.FRAME_ANCESTORS));
      do_check_true(cspr.permits("https://self.com:34", SD.FRAME_ANCESTORS));
      do_check_true(cspr.permits("http://self.com", SD.FRAME_ANCESTORS));
      do_check_true(cspr.permits("http://subd.self.com:34", SD.FRAME_ANCESTORS));

      cspr = CSPRep.fromString("allow 'none'; frame-ancestors 'self'", URI(self));

      //"frame-ancestors should only allow self"
      do_check_true(cspr.permits("http://self.com:34", SD.FRAME_ANCESTORS));
      do_check_false(cspr.permits("https://foo.com:400", SD.FRAME_ANCESTORS));
      do_check_false(cspr.permits("https://self.com:34", SD.FRAME_ANCESTORS));
      do_check_false(cspr.permits("http://self.com", SD.FRAME_ANCESTORS));
      do_check_false(cspr.permits("http://subd.self.com:34", SD.FRAME_ANCESTORS));
     });

test(function test_CSP_ReportURI_parsing() {
      var cspr;
      var SD = CSPRep.SRC_DIRECTIVES;
      var self = "http://self.com:34";
      var parsedURIs = [];

      var uri_valid_absolute = self + "/report.py";
      var uri_invalid_host_absolute = "http://foo.org:34/report.py";
      var uri_valid_relative = "/report.py";
      var uri_valid_relative_expanded = self + uri_valid_relative;
      var uri_valid_relative2 = "foo/bar/report.py";
      var uri_valid_relative2_expanded = self + "/" + uri_valid_relative2;
      var uri_invalid_relative = "javascript:alert(1)";

      cspr = CSPRep.fromString("allow *; report-uri " + uri_valid_absolute, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      do_check_in_array(parsedURIs, uri_valid_absolute);
      do_check_eq(parsedURIs.length, 1);

      cspr = CSPRep.fromString("allow *; report-uri " + uri_invalid_host_absolute, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      do_check_in_array(parsedURIs, "");
      do_check_eq(parsedURIs.length, 1); // the empty string is in there.

      cspr = CSPRep.fromString("allow *; report-uri " + uri_invalid_relative, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      do_check_in_array(parsedURIs, "");
      do_check_eq(parsedURIs.length, 1);

      cspr = CSPRep.fromString("allow *; report-uri " + uri_valid_relative, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      do_check_in_array(parsedURIs, uri_valid_relative_expanded);
      do_check_eq(parsedURIs.length, 1);

      cspr = CSPRep.fromString("allow *; report-uri " + uri_valid_relative2, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      dump(parsedURIs.length);
      do_check_in_array(parsedURIs, uri_valid_relative2_expanded);
      do_check_eq(parsedURIs.length, 1);

      // combination!
      cspr = CSPRep.fromString("allow *; report-uri " +
                               uri_valid_relative2 + " " +
                               uri_valid_absolute, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      do_check_in_array(parsedURIs, uri_valid_relative2_expanded);
      do_check_in_array(parsedURIs, uri_valid_absolute);
      do_check_eq(parsedURIs.length, 2);

      cspr = CSPRep.fromString("allow *; report-uri " +
                               uri_valid_relative2 + " " +
                               uri_invalid_host_absolute + " " +
                               uri_valid_absolute, URI(self));
      parsedURIs = cspr.getReportURIs().split(/\s+/);
      do_check_in_array(parsedURIs, uri_valid_relative2_expanded);
      do_check_in_array(parsedURIs, uri_valid_absolute);
      do_check_eq(parsedURIs.length, 2);
    });

test(
    function test_bug672961_withNonstandardSelfPort() {
      /**
       * When a protected document has a non-standard port, other host names
       * listed as sources should inherit the scheme of the protected document
       * but NOT the port.  Other hosts should use the default port for the
       * inherited scheme.  For example, since 443 is default for HTTPS:
       *
       *   Document with CSP: https://foobar.com:4443
       *   Transmitted policy:
       *       "allow 'self' a.com"
       *   Explicit policy:
       *       "allow https://foobar.com:4443 https://a.com:443"
       *
       * This test examines scheme and nonstandard port inheritance.
       */

      var src;
      src = CSPSource.create("a.com", "https://foobar.com:4443");
      //"src should inherit and require https scheme
      do_check_false(src.permits("http://a.com"));
      //"src should inherit scheme 'https'"
      do_check_true(src.permits("https://a.com"));
      //"src should get default port 
      do_check_true(src.permits("https://a.com:443"));
      
      src = CSPSource.create("http://a.com", "https://foobar.com:4443");
      //"src should require http scheme"
      do_check_false(src.permits("https://a.com"));
      //"src should keep scheme 'http'"
      do_check_true(src.permits("http://a.com"));
      //"src should inherit default port for 'http'"
      do_check_true(src.permits("http://a.com:80"));
      
      src = CSPSource.create("'self'", "https://foobar.com:4443");
      //"src should inherit nonstandard port from self
      do_check_true(src.permits("https://foobar.com:4443"));
      do_check_false(src.permits("https://foobar.com"));
      do_check_false(src.permits("https://foobar.com:443"));

      //"src should inherit and require https scheme from self
      do_check_false(src.permits("http://foobar.com:4443"));
      do_check_false(src.permits("http://foobar.com"));

    });

/*

test(function test_CSPRep_fromPolicyURI_failswhenmixed() {
        var cspr;
        var self = "http://localhost:" + POLICY_PORT;
        var closed_policy = CSPRep.fromString("allow 'none'");
        var my_uri_policy = "policy-uri " + POLICY_URI;

        //print(" --- Ignore the following two errors if they print ---");
        cspr = CSPRep.fromString("allow *; " + my_uri_policy, URI(self));

        //"Parsing should fail when 'policy-uri' is mixed with allow directive"
        do_check_equivalent(cspr, closed_policy);
        cspr = CSPRep.fromString("img-src 'self'; " + my_uri_policy, URI(self));

        //"Parsing should fail when 'policy-uri' is mixed with other directives"
        do_check_equivalent(cspr, closed_policy);
        //print(" --- Stop ignoring errors that print ---\n");

    });
*/

// TODO: test reporting
// TODO: test refinements (?)
// TODO: test 'eval' and 'inline' keywords

function run_test() {
  function policyresponder(request,response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/csp", false);
    response.bodyOutputStream.write(POLICY_FROM_URI, POLICY_FROM_URI.length);
  }
  //server.registerDirectory("/", nsILocalFileForBasePath);
  httpServer.registerPathHandler("/policy", policyresponder);
  httpServer.start(POLICY_PORT);

  for(let i in tests) {
    tests[i]();
  }

  //teardown
  httpServer.stop(function() { });
  do_test_finished();
}



