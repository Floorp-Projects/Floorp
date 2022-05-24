/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

const collection = BuiltInThemes.findActiveColorwayCollection();
if (collection) {
  const { expiry, l10nId } = collection;
  const fluentStrings = new Localization(["preview/colorwaycloset.ftl"], true);
  const formatter = new Intl.DateTimeFormat("default", {
    month: "long",
    day: "numeric",
  });
  document.getElementById(
    "collection-title"
  ).innerText = fluentStrings.formatValueSync(l10nId);
  document.querySelector(
    "#collection-expiry-date > span"
  ).innerText = formatter.format(expiry);
}
