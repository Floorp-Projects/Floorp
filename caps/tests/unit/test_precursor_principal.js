"use strict";
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);
const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

XPCShellContentUtils.init(this);
ExtensionTestUtils.init(this);

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com", "example.org"],
});

server.registerPathHandler("/static_frames", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  response.write(`
    <iframe name="same_origin" sandbox="allow-scripts allow-same-origin" src="http://example.com/frame"></iframe>
    <iframe name="same_origin_sandbox" sandbox="allow-scripts" src="http://example.com/frame"></iframe>
    <iframe name="cross_origin" sandbox="allow-scripts allow-same-origin" src="http://example.org/frame"></iframe>
    <iframe name="cross_origin_sandbox" sandbox="allow-scripts" src="http://example.org/frame"></iframe>
    <iframe name="data_uri" sandbox="allow-scripts allow-same-origin" src="data:text/html,<h1>Data Subframe</h1>"></iframe>
    <iframe name="data_uri_sandbox" sandbox="allow-scripts" src="data:text/html,<h1>Data Subframe</h1>"></iframe>
    <iframe name="srcdoc" sandbox="allow-scripts allow-same-origin" srcdoc="<h1>Srcdoc Subframe</h1>"></iframe>
    <iframe name="srcdoc_sandbox" sandbox="allow-scripts" srcdoc="<h1>Srcdoc Subframe</h1>"></iframe>
    <iframe name="blank" sandbox="allow-scripts allow-same-origin"></iframe>
    <iframe name="blank_sandbox" sandbox="allow-scripts"></iframe>
    <iframe name="redirect_com" sandbox="allow-scripts allow-same-origin" src="/redirect?com"></iframe>
    <iframe name="redirect_com_sandbox" sandbox="allow-scripts" src="/redirect?com"></iframe>
    <iframe name="redirect_org" sandbox="allow-scripts allow-same-origin" src="/redirect?org"></iframe>
    <iframe name="redirect_org_sandbox" sandbox="allow-scripts" src="/redirect?org"></iframe>
    <iframe name="ext_redirect_com_com" sandbox="allow-scripts allow-same-origin" src="/ext_redirect?com"></iframe>
    <iframe name="ext_redirect_com_com_sandbox" sandbox="allow-scripts" src="/ext_redirect?com"></iframe>
    <iframe name="ext_redirect_org_com" sandbox="allow-scripts allow-same-origin" src="http://example.org/ext_redirect?com"></iframe>
    <iframe name="ext_redirect_org_com_sandbox" sandbox="allow-scripts" src="http://example.org/ext_redirect?com"></iframe>
    <iframe name="ext_redirect_com_org" sandbox="allow-scripts allow-same-origin" src="/ext_redirect?org"></iframe>
    <iframe name="ext_redirect_com_org_sandbox" sandbox="allow-scripts" src="/ext_redirect?org"></iframe>
    <iframe name="ext_redirect_org_org" sandbox="allow-scripts allow-same-origin" src="http://example.org/ext_redirect?org"></iframe>
    <iframe name="ext_redirect_org_org_sandbox" sandbox="allow-scripts" src="http://example.org/ext_redirect?org"></iframe>
    <iframe name="ext_redirect_com_data" sandbox="allow-scripts allow-same-origin" src="/ext_redirect?data"></iframe>
    <iframe name="ext_redirect_com_data_sandbox" sandbox="allow-scripts" src="/ext_redirect?data"></iframe>
    <iframe name="ext_redirect_org_data" sandbox="allow-scripts allow-same-origin" src="http://example.org/ext_redirect?data"></iframe>
    <iframe name="ext_redirect_org_data_sandbox" sandbox="allow-scripts" src="http://example.org/ext_redirect?data"></iframe>

    <!-- XXX(nika): These aren't static as they perform loads dynamically - perhaps consider testing them separately? -->
    <iframe name="client_replace_org_blank" sandbox="allow-scripts allow-same-origin" src="http://example.org/client_replace?blank"></iframe>
    <iframe name="client_replace_org_blank_sandbox" sandbox="allow-scripts" src="http://example.org/client_replace?blank"></iframe>
    <iframe name="client_replace_org_data" sandbox="allow-scripts allow-same-origin" src="http://example.org/client_replace?data"></iframe>
    <iframe name="client_replace_org_data_sandbox" sandbox="allow-scripts" src="http://example.org/client_replace?data"></iframe>
  `);
});

server.registerPathHandler("/frame", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  response.write(`<h1>HTTP Subframe</h1>`);
});

server.registerPathHandler("/redirect", (request, response) => {
  let redirect;
  if (request.queryString == "com") {
    redirect = "http://example.com/frame";
  } else if (request.queryString == "org") {
    redirect = "http://example.org/frame";
  } else {
    response.setStatusLine(request.httpVersion, 404, "Not found");
    return;
  }

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", redirect);
});

server.registerPathHandler("/client_replace", (request, response) => {
  let redirect;
  if (request.queryString == "blank") {
    redirect = "about:blank";
  } else if (request.queryString == "data") {
    redirect = "data:text/html,<h1>Data Subframe</h1>";
  } else {
    response.setStatusLine(request.httpVersion, 404, "Not found");
    return;
  }

  response.setHeader("Content-Type", "text/html");
  response.write(`
    <script>
      window.location.replace(${JSON.stringify(redirect)});
    </script>
  `);
});

add_task(async function sandboxed_precursor() {
  // Bug 1725345: Make XPCShellContentUtils.createHttpServer support https
  Services.prefs.setBoolPref("dom.security.https_first", false);

  let extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background() {
      // eslint-disable-next-line no-undef
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          let url = new URL(details.url);
          if (!url.pathname.includes("ext_redirect")) {
            return {};
          }

          let redirectUrl;
          if (url.search == "?com") {
            redirectUrl = "http://example.com/frame";
          } else if (url.search == "?org") {
            redirectUrl = "http://example.org/frame";
          } else if (url.search == "?data") {
            redirectUrl = "data:text/html,<h1>Data Subframe</h1>";
          }
          return { redirectUrl };
        },
        { urls: ["<all_urls>"] },
        ["blocking"]
      );
    },
  });
  await extension.startup();

  registerCleanupFunction(async function() {
    await extension.unload();
  });

  for (let userContextId of [undefined, 1]) {
    let comURI = Services.io.newURI("http://example.com");
    let comPrin = Services.scriptSecurityManager.createContentPrincipal(
      comURI,
      { userContextId }
    );
    let orgURI = Services.io.newURI("http://example.org");
    let orgPrin = Services.scriptSecurityManager.createContentPrincipal(
      orgURI,
      { userContextId }
    );

    let page = await XPCShellContentUtils.loadContentPage(
      "http://example.com/static_frames",
      {
        remote: true,
        remoteSubframes: true,
        userContextId,
      }
    );
    let bc = page.browsingContext;

    ok(
      bc.currentWindowGlobal.documentPrincipal.equals(comPrin),
      "toplevel principal matches"
    );

    // XXX: This is sketchy as heck, but it's also the easiest way to wait for
    // the `window.location.replace` loads to finish.
    await TestUtils.waitForCondition(
      () =>
        bc.children.every(
          child =>
            !child.currentWindowGlobal.documentURI.spec.includes(
              "/client_replace"
            )
        ),
      "wait for every client_replace global to be replaced"
    );

    let principals = {};
    for (let child of bc.children) {
      notEqual(child.name, "", "child frames must have names");
      ok(!(child.name in principals), "duplicate child frame name");
      principals[child.name] = child.currentWindowGlobal.documentPrincipal;
    }

    function principal_is(name, expected) {
      let principal = principals[name];
      info(`${name} = ${principal.origin}`);
      ok(principal.equals(expected), `${name} is correct`);
    }
    function precursor_is(name, precursor) {
      let principal = principals[name];
      info(`${name} = ${principal.origin}`);
      ok(principal.isNullPrincipal, `${name} is null`);
      ok(
        principal.precursorPrincipal.equals(precursor),
        `${name} has the correct precursor`
      );
    }

    // Basic loads should have the principals or precursor principals for the
    // document being loaded.
    principal_is("same_origin", comPrin);
    precursor_is("same_origin_sandbox", comPrin);

    principal_is("cross_origin", orgPrin);
    precursor_is("cross_origin_sandbox", orgPrin);

    // Loads of a data: URI should complete with a sandboxed principal based on
    // the principal which tried to perform the load.
    precursor_is("data_uri", comPrin);
    precursor_is("data_uri_sandbox", comPrin);

    // Loads which inherit principals, such as srcdoc an about:blank loads,
    // should also inherit sandboxed precursor principals.
    principal_is("srcdoc", comPrin);
    precursor_is("srcdoc_sandbox", comPrin);

    principal_is("blank", comPrin);
    precursor_is("blank_sandbox", comPrin);

    // Redirects shouldn't interfere with the final principal, and it should be
    // based only on the final URI.
    principal_is("redirect_com", comPrin);
    precursor_is("redirect_com_sandbox", comPrin);

    principal_is("redirect_org", orgPrin);
    precursor_is("redirect_org_sandbox", orgPrin);

    // Extension redirects should act like normal redirects, and still resolve
    // with the principal or sandboxed principal of the final URI.
    principal_is("ext_redirect_com_com", comPrin);
    precursor_is("ext_redirect_com_com_sandbox", comPrin);

    principal_is("ext_redirect_com_org", orgPrin);
    precursor_is("ext_redirect_com_org_sandbox", orgPrin);

    principal_is("ext_redirect_org_com", comPrin);
    precursor_is("ext_redirect_org_com_sandbox", comPrin);

    principal_is("ext_redirect_org_org", orgPrin);
    precursor_is("ext_redirect_org_org_sandbox", orgPrin);

    // When an extension redirects to a data: URI, we use the last non-data: URI
    // in the chain as the precursor principal.
    // FIXME: This should perhaps use the extension's principal instead?
    precursor_is("ext_redirect_com_data", comPrin);
    precursor_is("ext_redirect_com_data_sandbox", comPrin);

    precursor_is("ext_redirect_org_data", orgPrin);
    precursor_is("ext_redirect_org_data_sandbox", orgPrin);

    // Check that navigations triggred by script within the frames will have the
    // correct behaviour when navigating to blank and data URIs.
    principal_is("client_replace_org_blank", orgPrin);
    precursor_is("client_replace_org_blank_sandbox", orgPrin);

    precursor_is("client_replace_org_data", orgPrin);
    precursor_is("client_replace_org_data_sandbox", orgPrin);

    await page.close();
  }
  Services.prefs.clearUserPref("dom.security.https_first");
});
