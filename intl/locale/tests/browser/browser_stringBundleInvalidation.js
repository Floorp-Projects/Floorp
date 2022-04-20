/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { setupFakeLangpacks } = ChromeUtils.import(
  "resource://testing-common/LangPackTestUtils.jsm"
);

add_task(async function test_stringBundleInvalidation() {
  const fakeLangpacks = await setupFakeLangpacks(this, SpecialPowers);

  await fakeLangpacks.install({
    locale: "es-ES",
    propertiesFiles: [
      {
        rootURL: "chrome://branding/locale",
        files: {
          // Localizers don't really translate the brand name this way, but it
          // makes for a useful test.
          "brand.properties": "brandFullName=Zorro de Fuego",
          "test-only.properties": "testOnly=Mensaje solo para pruebas",
        },
      },
    ],
  });

  await fakeLangpacks.install({
    locale: "fr",
    propertiesFiles: [
      {
        rootURL: "chrome://branding/locale",
        files: {
          // Localizers don't really translate the brand name this way, but it
          // makes for a useful test.
          "brand.properties": "brandFullName=Renard de Feu",
          "test-only.properties": "testOnly=Message de test uniquement",
        },
      },
    ],
  });

  info(`Creating the bundle "chrome://branding/locale/brand.properties"`);
  const sharedStringBundle = Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );

  // Don't write an assertion directly off of the brand name, as it could change
  // depending on the build.
  const brandFullName = sharedStringBundle.GetStringFromName("brandFullName");
  info("Reading the brand name: " + brandFullName);
  Assert.equal(typeof brandFullName, "string");
  Assert.greater(brandFullName.length, 0);

  info("Changing the locale to es-ES.");
  Services.locale.requestedLocales = ["es-ES"];
  await document.l10n.ready;

  info("Creating the test-only.properties bundle in Spanish.");
  const testOnlyBundle = Services.strings.createBundle(
    "chrome://branding/locale/test-only.properties"
  );

  Assert.equal(
    testOnlyBundle.GetStringFromName("testOnly"),
    "Mensaje solo para pruebas",
    "String bundles can load the new locale."
  );

  Assert.equal(
    sharedStringBundle.GetStringFromName("brandFullName"),
    brandFullName,
    "Shared string bundles are not invalidated."
  );

  info("Changing the locale to fr.");
  Services.locale.requestedLocales = ["fr"];
  await document.l10n.ready;

  Assert.equal(
    sharedStringBundle.GetStringFromName("brandFullName"),
    brandFullName,
    "Shared string bundles are not invalidated."
  );
  Assert.equal(
    testOnlyBundle.GetStringFromName("testOnly"),
    "Mensaje solo para pruebas",
    "Existing string bundles are retained."
  );

  Assert.equal(
    Services.strings
      .createBundle("chrome://branding/locale/brand.properties")
      .GetStringFromName("brandFullName"),
    brandFullName,
    "Shared string bundles are not invalidated."
  );
  Assert.equal(
    Services.strings
      .createBundle("chrome://branding/locale/test-only.properties")
      .GetStringFromName("testOnly"),
    "Message de test uniquement",
    "The string bundle can be recreated."
  );
});
