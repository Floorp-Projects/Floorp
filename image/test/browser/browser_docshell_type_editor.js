
"use strict";

const Ci = Components.interfaces;
const SIMPLE_HTML = "data:text/html,<html><head></head><body></body></html>";

// The following URI is *not* accessible to content, hence loading that URI
// from an unprivileged site should be blocked. If docshell is of appType
// APP_TYPE_EDITOR however the load should be allowed.
// >> chrome://devtools/content/framework/dev-edition-promo/dev-edition-logo.png

add_task(function* () {
  info("docshell of appType APP_TYPE_EDITOR can access privileged images.");

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: SIMPLE_HTML
  }, function* (browser) {
    yield ContentTask.spawn(browser, null, function* () {
      let rootDocShell = docShell.QueryInterface(Ci.nsIDocShellTreeItem)
                                 .rootTreeItem
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDocShell);
      let defaultAppType = rootDocShell.appType;

      rootDocShell.appType = Ci.nsIDocShell.APP_TYPE_EDITOR;

      is(rootDocShell.appType, Ci.nsIDocShell.APP_TYPE_EDITOR,
        "sanity check: appType after update should be type editor");

      return new Promise(resolve => {
        let doc = content.document;
        let image = doc.createElement("img");
        image.onload = function() {
          ok(true, "APP_TYPE_EDITOR is allowed to load privileged image");
          // restore appType of rootDocShell before moving on to the next test
          rootDocShell.appType = defaultAppType;
          resolve();
        }
        image.onerror = function() {
          ok(false, "APP_TYPE_EDITOR is allowed to load privileged image");
          // restore appType of rootDocShell before moving on to the next test
          rootDocShell.appType = defaultAppType;
          resolve();
        }
        doc.body.appendChild(image);
        image.src = "chrome://devtools/content/framework/dev-edition-promo/dev-edition-logo.png";
      });
    });
  });
});

add_task(function* () {
  info("docshell of appType APP_TYPE_UNKNOWN can *not* access privileged images.");

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: SIMPLE_HTML
  }, function* (browser) {
    yield ContentTask.spawn(browser, null, function* () {
      let rootDocShell = docShell.QueryInterface(Ci.nsIDocShellTreeItem)
                                 .rootTreeItem
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDocShell);
      let defaultAppType = rootDocShell.appType;

      rootDocShell.appType = Ci.nsIDocShell.APP_TYPE_UNKNOWN;

      is(rootDocShell.appType, Ci.nsIDocShell.APP_TYPE_UNKNOWN,
        "sanity check: appType of docshell should be unknown");

      return new Promise(resolve => {
        let doc = content.document;
        let image = doc.createElement("img");
        image.onload = function() {
          ok(false, "APP_TYPE_UNKNOWN is *not* allowed to acces privileged image");
          // restore appType of rootDocShell before moving on to the next test
          rootDocShell.appType = defaultAppType;
          resolve();
        }
        image.onerror = function() {
          ok(true, "APP_TYPE_UNKNOWN is *not* allowed to acces privileged image");
          // restore appType of rootDocShell before moving on to the next test
          rootDocShell.appType = defaultAppType;
          resolve();
        }
        doc.body.appendChild(image);
        image.src = "chrome://devtools/content/framework/dev-edition-promo/dev-edition-logo.png";
      });
    });
  });
});
