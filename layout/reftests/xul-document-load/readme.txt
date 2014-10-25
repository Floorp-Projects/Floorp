This directory contains various XUL document (mozilla/dom/xul/document) testcases for reftest.

test001: Checks that stylesheets referenced from an overlay via an
         xml-stylesheet PI are applied to the master document appropriately.

test002: Same as #1, but there's additional overlay between the master doc and
         the overlay with the stylesheet.

test003: Checks that dynamically removing the stylesheet PI from an inline
         script has expected effect.

test004: Same as test003, but removing the stylesheet PI happens from a "load"
         event handler.

test005: Same as test003, but removing the stylesheet PI happens from a top-level
         script in an external JS file.

test006: Simple <?xml-stylesheet ?> instruction in the prolog has an effect and exists
         in the DOM.

test007: Same as #006 for xul-overlay PI.

test008: Handle stylesheet PIs pointing to nonexistent resources gracefully.

test009: Same as #008 for xul-overlay PIs

test010: PIs in the master document, outside the prolog, don't have any effect but get
         added to the DOM.

test011: (bug 363406) Relative URIs in overlay's <?xml-stylesheet ?> PI are
         resolved against overlay's URI, not the document URI.

test012: Tests that sheets references from <?xml-stylesheet ?> PIs are added to the
         document in the same order as the PIs themselves are in - the simple case.

test013: Tests the same thing as #012, but for the case when the first sheet contains
         an @import statement, which makes it -finish- loading earlier than the
         second sheet.

test014: (bug 363406) Relative URIs in overlay's <?xul-overlay ?> PI are resolved
         against overlay's URI, not the document URI.

test015: Relative URIs in overlay's <xul:script> are resolved against overlay's 
         URI, not the document URI.

test016: Non-XUL elements work in overlays.

test017: (bug 359959) <?xul-overlay ?> used as a direct child of <overlay>
         should be inserted into the DOM, but not cause the overlay to be
         applied.

test018: <?xul-overlay ?> used deep inside another overlay (i.e. as a child of
         a 'hookup' node) should be inserted into the DOM, but not cause the
         overlay to be applied.

test019: Same as #017 for <?xml-stylesheet ?>

test020: Same as #018 for <?xml-stylesheet ?>

test021: (bug 363419) Non-XUL elements directly underneath <overlay> should
         be merged correctly into the base document.

test022: (bug 369828) <html:style> works in XUL documents
