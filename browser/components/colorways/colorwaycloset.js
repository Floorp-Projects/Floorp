/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");

const colorwaySelector = document.getElementById("colorway-selector");

document.getElementById("set-colorway").onclick = () => {
  colorwaySelector.selectedTheme.enable();
};

function showUseFXHomeControls(fluentStrings) {
  let homeState;
  const useFXHomeControls = document.getElementById("use-fx-home-controls");
  useFXHomeControls.hidden = HomePage.isDefault;
  if (!HomePage.isDefault) {
    useFXHomeControls
      .querySelector(".reset-prompt > button")
      .addEventListener("click", () => {
        homeState = HomePage.get();
        HomePage.reset();
        useFXHomeControls.classList.add("success");
      });
    useFXHomeControls
      .querySelector(".success-prompt > button")
      .addEventListener("click", () => {
        HomePage.set(homeState);
        useFXHomeControls.classList.remove("success");
      });
  }
}

const collection = BuiltInThemes.findActiveColorwayCollection();
if (collection) {
  const { expiry, l10nId } = collection;
  const collectionTitle = document.getElementById("collection-title");
  document.l10n.setAttributes(collectionTitle, l10nId.title);
  const colorwayExpiryDateSpan = document.querySelector(
    "#collection-expiry-date > span"
  );
  document.l10n.setAttributes(
    colorwayExpiryDateSpan,
    "colorway-collection-expiry-date-span",
    {
      expiryDate: expiry.getTime(),
    }
  );
  showUseFXHomeControls();
}
