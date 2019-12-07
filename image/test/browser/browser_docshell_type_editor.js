"use strict";

const SIMPLE_HTML = "data:text/html,<html><head></head><body></body></html>";

/**
 * Returns the directory where the chrome.manifest file for the test can be found.
 *
 * @return nsIFile of the manifest directory
 */
function getManifestDir() {
  let path = getTestFilePath("browser_docshell_type_editor");
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);
  return file;
}

// The following URI is *not* accessible to content, hence loading that URI
// from an unprivileged site should be blocked. If docshell is of appType
// APP_TYPE_EDITOR however the load should be allowed.
// >> chrome://test1/skin/privileged.png

add_task(async function() {
  info("docshell of appType APP_TYPE_EDITOR can access privileged images.");

  // Load a temporary manifest adding a route to a privileged image
  let manifestDir = getManifestDir();
  Components.manager.addBootstrappedManifestLocation(manifestDir);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SIMPLE_HTML,
    },
    async function(browser) {
      await ContentTask.spawn(browser, null, async function() {
        let rootDocShell = docShell
          .QueryInterface(Ci.nsIDocShellTreeItem)
          .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDocShell);
        let defaultAppType = rootDocShell.appType;

        rootDocShell.appType = Ci.nsIDocShell.APP_TYPE_EDITOR;

        is(
          rootDocShell.appType,
          Ci.nsIDocShell.APP_TYPE_EDITOR,
          "sanity check: appType after update should be type editor"
        );

        return new Promise(resolve => {
          let doc = content.document;
          let image = doc.createElement("img");
          image.onload = function() {
            ok(true, "APP_TYPE_EDITOR is allowed to load privileged image");
            // restore appType of rootDocShell before moving on to the next test
            rootDocShell.appType = defaultAppType;
            resolve();
          };
          image.onerror = function() {
            ok(false, "APP_TYPE_EDITOR is allowed to load privileged image");
            // restore appType of rootDocShell before moving on to the next test
            rootDocShell.appType = defaultAppType;
            resolve();
          };
          doc.body.appendChild(image);
          image.src = "chrome://test1/skin/privileged.png";
        });
      });
    }
  );

  Components.manager.removeBootstrappedManifestLocation(manifestDir);
});

add_task(async function() {
  info(
    "docshell of appType APP_TYPE_UNKNOWN can *not* access privileged images."
  );

  // Load a temporary manifest adding a route to a privileged image
  let manifestDir = getManifestDir();
  Components.manager.addBootstrappedManifestLocation(manifestDir);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SIMPLE_HTML,
    },
    async function(browser) {
      await ContentTask.spawn(browser, null, async function() {
        let rootDocShell = docShell
          .QueryInterface(Ci.nsIDocShellTreeItem)
          .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDocShell);
        let defaultAppType = rootDocShell.appType;

        rootDocShell.appType = Ci.nsIDocShell.APP_TYPE_UNKNOWN;

        is(
          rootDocShell.appType,
          Ci.nsIDocShell.APP_TYPE_UNKNOWN,
          "sanity check: appType of docshell should be unknown"
        );

        return new Promise(resolve => {
          let doc = content.document;
          let image = doc.createElement("img");
          image.onload = function() {
            ok(
              false,
              "APP_TYPE_UNKNOWN is *not* allowed to access privileged image"
            );
            // restore appType of rootDocShell before moving on to the next test
            rootDocShell.appType = defaultAppType;
            resolve();
          };
          image.onerror = function() {
            ok(
              true,
              "APP_TYPE_UNKNOWN is *not* allowed to access privileged image"
            );
            // restore appType of rootDocShell before moving on to the next test
            rootDocShell.appType = defaultAppType;
            resolve();
          };
          doc.body.appendChild(image);
          // Set the src via wrappedJSObject so the load is triggered with
          // the content page's principal rather than ours.
          image.wrappedJSObject.src = "chrome://test1/skin/privileged.png";
        });
      });
    }
  );

  Components.manager.removeBootstrappedManifestLocation(manifestDir);
});
