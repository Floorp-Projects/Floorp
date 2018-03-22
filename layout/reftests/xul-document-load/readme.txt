This directory contains various XUL document (mozilla/dom/xul/document) testcases for reftest.

test003: Checks that dynamically removing the stylesheet PI from an inline
         script has expected effect.

test004: Same as test003, but removing the stylesheet PI happens from a "load"
         event handler.

test005: Same as test003, but removing the stylesheet PI happens from a top-level
         script in an external JS file.

test006: Simple <?xml-stylesheet ?> instruction in the prolog has an effect and exists
         in the DOM.

test008: Handle stylesheet PIs pointing to nonexistent resources gracefully.

test010: PIs in the master document, outside the prolog, don't have any effect but get
         added to the DOM.

test012: Tests that sheets references from <?xml-stylesheet ?> PIs are added to the
         document in the same order as the PIs themselves are in - the simple case.

test013: Tests the same thing as #012, but for the case when the first sheet contains
         an @import statement, which makes it -finish- loading earlier than the
         second sheet.

test022: (bug 369828) <html:style> works in XUL documents
