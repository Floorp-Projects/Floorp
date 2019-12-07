const { L10nRegistry, FileSource } = ChromeUtils.import(
  "resource://gre/modules/L10nRegistry.jsm"
);

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
let mockSource = new FileSource("test", locales, `${uri}localization/`);
L10nRegistry.registerSource(mockSource);

registerCleanupFunction(() => {
  protocol.setSubstitution("l10n-test", null);
  L10nRegistry.removeSource("test");
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    "resource://l10n-test/test.html",
    async browser => {
      await ContentTask.spawn(browser, null, async function() {
        let document = content.document;
        let window = document.defaultView;

        let { customMsg, l10nArgs } = await document.testsReadyPromise;

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
        is(l10nArgs.id, "subtitle");
        is(l10nArgs.args.name, "Firefox");

        // Test for manual value formatting
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
