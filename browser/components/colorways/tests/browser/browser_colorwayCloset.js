/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_colorwaycloset_smoke_test() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "chrome://browser/content/colorwaycloset.html",
    },
    async browser => {
      const { document } = browser.contentWindow;

      ok(
        document.getElementById("collection-expiry-date"),
        "expiry date exists"
      );

      ok(
        document.getElementById("collection-title"),
        "collection title exists"
      );

      ok(
        document.getElementsByTagName("colorway-selector"),
        "Found colorway selector element"
      );

      ok(document.getElementById("colorway-name"), "colorway name exists");

      ok(
        document.getElementById("colorway-description"),
        "colorway description exists"
      );

      ok(document.getElementById("set-colorway"), "Found Set Colorway button");

      const useFXHomeControls = document.getElementById("use-fx-home-controls");
      ok(useFXHomeControls, "firefox home controls exists");
      useFXHomeControls.toggleAttribute("hidden", false);
      ok(
        document.querySelector("#use-fx-home-controls > .reset-prompt"),
        "firefox home controls reset prompt exists"
      );
      ok(
        document.querySelector("#use-fx-home-controls > .success-prompt"),
        "firefox home controls reset prompt exists"
      );
    }
  );
});
