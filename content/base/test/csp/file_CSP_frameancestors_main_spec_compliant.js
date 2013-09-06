// Script to populate the test frames in the frame ancestors mochitest.
//
function setupFrames() {

  var $ = function(v) { return document.getElementById(v); }
  var base = {
        self: '/tests/content/base/test/csp/file_CSP_frameancestors_spec_compliant.sjs',
        a: 'http://mochi.test:8888/tests/content/base/test/csp/file_CSP_frameancestors_spec_compliant.sjs',
        b: 'http://example.com/tests/content/base/test/csp/file_CSP_frameancestors_spec_compliant.sjs'
  };

  var host = { a: 'http://mochi.test:8888', b: 'http://example.com:80' };

  var innerframeuri = null;
  var elt = null;

  elt = $('aa_allow_spec_compliant');
  elt.src = base.a + "?testid=aa_allow_spec_compliant&internalframe=aa_a&csp=" +
            escape("default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'");

  elt = $('aa_block_spec_compliant');
  elt.src = base.a + "?testid=aa_block_spec_compliant&internalframe=aa_b&csp=" +
            escape("default-src 'none'; frame-ancestors 'none'; script-src 'self'");

  elt = $('aa2_block_spec_compliant');
  elt.src = "view-source:" + base.a + "?testid=aa2_block_spec_compliant&internalframe=aa_b&csp=" +
            escape("default-src 'none'; frame-ancestors 'none'; script-src 'self'");

  elt = $('ab_allow_spec_compliant');
  elt.src = base.b + "?testid=ab_allow_spec_compliant&internalframe=ab_a&csp=" +
            escape("default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'");

  elt = $('ab_block_spec_compliant');
  elt.src = base.b + "?testid=ab_block_spec_compliant&internalframe=ab_b&csp=" +
            escape("default-src 'none'; frame-ancestors 'none'; script-src 'self'");

   /* .... two-level framing */
  elt = $('aba_allow_spec_compliant');
  innerframeuri = base.a + "?testid=aba_allow_spec_compliant&double=1&internalframe=aba_a&csp=" +
                  escape("default-src 'none'; frame-ancestors " + host.a + " " + host.b + "; script-src 'self'");
  elt.src = base.b + "?externalframe=" + escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $('aba_block_spec_compliant');
  innerframeuri = base.a + "?testid=aba_allow_spec_compliant&double=1&internalframe=aba_b&csp=" +
                  escape("default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'");
  elt.src = base.b + "?externalframe=" + escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $('aba2_block_spec_compliant');
  innerframeuri = base.a + "?testid=aba_allow_spec_compliant&double=1&internalframe=aba2_b&csp=" +
                  escape("default-src 'none'; frame-ancestors " + host.b + "; script-src 'self'");
  elt.src = base.b + "?externalframe=" + escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $('abb_allow_spec_compliant');
  innerframeuri = base.b + "?testid=abb_allow_spec_compliant&double=1&internalframe=abb_a&csp=" +
                  escape("default-src 'none'; frame-ancestors " + host.a + " " + host.b + "; script-src 'self'");
  elt.src = base.b + "?externalframe=" + escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $('abb_block_spec_compliant');
  innerframeuri = base.b + "?testid=abb_allow_spec_compliant&double=1&internalframe=abb_b&csp=" +
                  escape("default-src 'none'; frame-ancestors " + host.a + "; script-src 'self'");
  elt.src = base.b + "?externalframe=" + escape('<iframe src="' + innerframeuri + '"></iframe>');

  elt = $('abb2_block_spec_compliant');
  innerframeuri = base.b + "?testid=abb_allow_spec_compliant&double=1&internalframe=abb2_b&csp=" +
                  escape("default-src 'none'; frame-ancestors " + host.b + "; script-src 'self'");
  elt.src = base.b + "?externalframe=" + escape('<iframe src="' + innerframeuri + '"></iframe>');
}

window.addEventListener('load', setupFrames, false);
