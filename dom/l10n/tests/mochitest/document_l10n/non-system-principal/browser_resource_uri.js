let uri =
  "chrome://mochitests/content/browser/dom/l10n/tests/mochitest//document_l10n/non-system-principal/";
let protocol = Services.io
  .getProtocolHandler("resource")
  .QueryInterface(Ci.nsIResProtocolHandler);

protocol.setSubstitution("l10n-test", Services.io.newURI(uri));

// Since we want the mock source to work with all locales, we're going
// to register it for currently used locales, and we'll put the path that
// doesn't use the `{locale}` component to make it work irrelevant of
// what locale the mochitest is running in.
//
// Notice: we're using a `chrome://` protocol here only for convenience reasons.
// Real sources should use `resource://` protocol.
let locales = Services.locale.appLocalesAsBCP47;

// This source is actually using a real `FileSource` instead of a mocked one,
// because we want to test that fetching real I/O out of the `uri` works in non-system-principal.
let source = new L10nFileSource("test", "app", locales, `${uri}localization/`);
L10nRegistry.getInstance().registerSources([source]);

registerCleanupFunction(() => {
  protocol.setSubstitution("l10n-test", null);
  L10nRegistry.getInstance().removeSources(["test"]);
  SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processPrelaunch.enabled", true]],
  });
});

const kChildPage = getRootDirectory(gTestPath) + "test.html";

const kAboutPagesRegistered = Promise.all([
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction,
    "test-about-l10n-child",
    kChildPage,
    Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
      Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
      Ci.nsIAboutModule.ALLOW_SCRIPT
  ),
]);

add_task(async () => {
  // Bug 1640333 - windows fails (sometimes) to ever get document.l10n.ready
  // if e10s process caching is enabled
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.security.skip_about_page_has_csp_assert", true],
    ],
  });
  await kAboutPagesRegistered;
  await BrowserTestUtils.withNewTab(
    "about:test-about-l10n-child",
    async browser => {
      await SpecialPowers.spawn(browser, [], async function () {
        let document = content.document;
        let window = document.defaultView;

        await document.testsReadyPromise;

        let principal = SpecialPowers.wrap(document).nodePrincipal;
        is(
          principal.spec,
          "about:test-about-l10n-child",
          "correct content principal"
        );

        let desc = document.getElementById("main-desc");

        // We can test here for a particular value because we're
        // using a mock file source which is locale independent.
        //
        // If you're writing a test that verifies that a UI
        // widget got real localization, you should not rely on
        // the particular value, but rather on the content not
        // being empty (to keep the test pass in non-en-US locales).
        is(desc.textContent, "This is a mock page title");

        // Test for l10n.getAttributes
        let label = document.getElementById("label1");
        let l10nArgs = document.l10n.getAttributes(label);
        is(l10nArgs.id, "subtitle");
        is(l10nArgs.args.name, "Firefox");

        // Test for manual value formatting
        let customMsg = document.getElementById("customMessage").textContent;
        is(customMsg, "This is a custom message formatted from JS.");

        // Since we applied the `data-l10n-id` attribute
        // on `label` in this microtask, we have to wait for
        // the next paint to verify that the MutationObserver
        // applied the translation.
        await new Promise(resolve => {
          let verifyL10n = () => {
            if (!label.textContent.includes("Firefox")) {
              window.requestAnimationFrame(verifyL10n);
            } else {
              resolve();
            }
          };

          window.requestAnimationFrame(verifyL10n);
        });
      });
    }
  );
});
