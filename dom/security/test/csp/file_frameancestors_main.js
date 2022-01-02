// Script to populate the test frames in the frame ancestors mochitest.
//
function setupFrames() {
  var $ = function(v) {
    return document.getElementById(v);
  };
  var base = {
    self: "/tests/dom/security/test/csp/file_frameancestors.sjs",
    a:
      "http://mochi.test:8888/tests/dom/security/test/csp/file_frameancestors.sjs",
    b: "http://example.com/tests/dom/security/test/csp/file_frameancestors.sjs",
  };

  // In both cases (base.a, base.b) the path starts with /tests/. Let's make sure this
  // path within the CSP policy is completely ignored when enforcing frame ancestors.
  // To test this behavior we use /foo/ and /bar/ as dummy values for the path.
  var host = {
    a: "http://mochi.test:8888/foo/",
    b: "http://example.com:80/bar/",
  };

  var innerframeuri = null;
  var elt = null;

  elt = $("aa_allow");
  elt.src =
    base.a +
    "?testid=aa_allow&internalframe=aa_a&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'"
    );

  elt = $("aa_block");
  elt.src =
    base.a +
    "?testid=aa_block&internalframe=aa_b&csp=" +
    escape("default-src 'none'; frame-ancestors 'none'; script-src 'self'");

  elt = $("ab_allow");
  elt.src =
    base.b +
    "?testid=ab_allow&internalframe=ab_a&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'"
    );

  elt = $("ab_block");
  elt.src =
    base.b +
    "?testid=ab_block&internalframe=ab_b&csp=" +
    escape("default-src 'none'; frame-ancestors 'none'; script-src 'self'");

  /* .... two-level framing */
  elt = $("aba_allow");
  innerframeuri =
    base.a +
    "?testid=aba_allow&double=1&internalframe=aba_a&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " +
        host.a +
        " " +
        host.b +
        "; script-src 'self'"
    );
  elt.src =
    base.b +
    "?externalframe=" +
    escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $("aba_block");
  innerframeuri =
    base.a +
    "?testid=aba_allow&double=1&internalframe=aba_b&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'"
    );
  elt.src =
    base.b +
    "?externalframe=" +
    escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $("aba2_block");
  innerframeuri =
    base.a +
    "?testid=aba_allow&double=1&internalframe=aba2_b&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " + host.b + "; script-src 'self'"
    );
  elt.src =
    base.b +
    "?externalframe=" +
    escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $("abb_allow");
  innerframeuri =
    base.b +
    "?testid=abb_allow&double=1&internalframe=abb_a&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " +
        host.a +
        " " +
        host.b +
        "; script-src 'self'"
    );
  elt.src =
    base.b +
    "?externalframe=" +
    escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $("abb_block");
  innerframeuri =
    base.b +
    "?testid=abb_allow&double=1&internalframe=abb_b&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'"
    );
  elt.src =
    base.b +
    "?externalframe=" +
    escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $("abb2_block");
  innerframeuri =
    base.b +
    "?testid=abb_allow&double=1&internalframe=abb2_b&csp=" +
    escape(
      "default-src 'none'; frame-ancestors " + host.b + "; script-src 'self'"
    );
  elt.src =
    base.b +
    "?externalframe=" +
    escape('<iframe src="' + innerframeuri + '"></iframe>');
}

window.addEventListener("load", setupFrames);
