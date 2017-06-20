/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for tabbrowser._isLocalAboutURI to make sure it returns the
 * appropriate values for various URIs as well as optional resolved URI.
 */

add_task(function test_URI() {
  const check = (spec, expect, description) => {
    const URI = Services.io.newURI(spec);
    is(gBrowser._isLocalAboutURI(URI), expect, description);
  };
  check("https://www.mozilla.org/", false, "https is not about");
  check("http://www.mozilla.org/", false, "http is not about");
  check("about:blank", true, "about:blank is local");
  check("about:about", true, "about:about is local");
  check("about:newtab", true, "about:newtab is local");
});

add_task(function test_URI_with_resolved() {
  const check = (spec, resolvedSpec, expect, description) => {
    const URI = Services.io.newURI(spec);
    const resolvedURI = Services.io.newURI(resolvedSpec);
    is(gBrowser._isLocalAboutURI(URI, resolvedURI), expect, description);
  };
  check("about:newtab",
    "jar:file:///Applications/Firefox.app/Contents/Resources/browser/features/activity-stream@mozilla.org.xpi!/chrome/content/data/content/activity-stream.html",
    true,
    "about:newtab with jar is local");
  check("about:newtab",
    "file:///mozilla-central/browser/base/content/newtab/newTab.xhtml",
    true,
    "about:newtab with file is local");
  check("about:newtab",
    "https://www.mozilla.org/newtab",
    false,
    "about:newtab with https is not local");
});
