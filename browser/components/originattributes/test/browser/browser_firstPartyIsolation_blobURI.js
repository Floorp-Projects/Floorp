add_setup(async function () {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
    Services.cookies.removeAll();
  });
});

/**
 * First we generate a Blob URI by using URL.createObjectURL(new Blob(..));
 * then we navigate to this Blob URI, hence to make the top-level document URI
 * is Blob URI.
 * Later we create an iframe on this Blob: document, and we test that the iframe
 * has correct firstPartyDomain.
 */
add_task(async function test_blob_uri_inherit_oa_from_content() {
  const BASE_URI =
    "http://mochi.test:8888/browser/browser/components/" +
    "originattributes/test/browser/dummy.html";
  const BASE_DOMAIN = "mochi.test";

  // First we load a normal web page.
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, BASE_URI);
  await BrowserTestUtils.browserLoaded(browser);

  // Then navigate to the blob: URI.
  await SpecialPowers.spawn(
    browser,
    [{ firstPartyDomain: BASE_DOMAIN }],
    async function (attrs) {
      Assert.ok(true, "origin " + content.document.nodePrincipal.origin);
      Assert.equal(
        content.document.nodePrincipal.originAttributes.firstPartyDomain,
        attrs.firstPartyDomain,
        "The document should have firstPartyDomain"
      );

      // Now we use createObjectURL to generate a blob URI and navigate to it.
      let url = content.window.URL.createObjectURL(
        new content.window.Blob(
          [
            `<script src="http://mochi.test:8888/browser/browser/components/originattributes/test/browser/test.js"></script>`,
          ],
          { type: "text/html" }
        )
      );
      content.document.location = url;
    }
  );

  // Wait for the Blob: URI to be loaded.
  await BrowserTestUtils.browserLoaded(browser, false, function (url) {
    info("BrowserTestUtils.browserLoaded url=" + url);
    return url.startsWith("blob:http://mochi.test:8888/");
  });

  // We verify the blob document has correct origin attributes.
  // Then we inject an iframe to it.
  await SpecialPowers.spawn(
    browser,
    [{ firstPartyDomain: BASE_DOMAIN }],
    async function (attrs) {
      Assert.ok(
        content.document.documentURI.startsWith("blob:http://mochi.test:8888/"),
        "the document URI should be a blob URI."
      );
      Assert.ok(true, "origin " + content.document.nodePrincipal.origin);
      Assert.equal(
        content.document.nodePrincipal.originAttributes.firstPartyDomain,
        attrs.firstPartyDomain,
        "The document should have firstPartyDomain"
      );

      let iframe = content.document.createElement("iframe");
      iframe.src = "http://example.com";
      iframe.id = "iframe1";
      content.document.body.appendChild(iframe);

      // Wait for the iframe to be loaded.
      await new content.Promise(done => {
        iframe.addEventListener(
          "load",
          function () {
            done();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  // Finally we verify the iframe has correct origin attributes.
  await SpecialPowers.spawn(
    browser,
    [{ firstPartyDomain: BASE_DOMAIN }],
    async function (attrs) {
      let iframe = content.document.getElementById("iframe1");
      await SpecialPowers.spawn(
        iframe,
        [attrs.firstPartyDomain],
        function (firstPartyDomain) {
          Assert.equal(
            content.document.nodePrincipal.originAttributes.firstPartyDomain,
            firstPartyDomain,
            "iframe should inherit firstPartyDomain from blob: URI"
          );
        }
      );
    }
  );

  win.close();
});
