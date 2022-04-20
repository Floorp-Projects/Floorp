/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["setupFakeLangpacks"];

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

/**
 * Allows the current app to install fake langpacks. This is useful for testing live
 * language switching where the browser can switch languages without a restart, as well
 * as testing novel message caching schemes.
 *
 * It's generally not recommended to assert against actual localized strings, as changing
 * these will break tests, and lock in the test suite to a single language. However, with
 * fake langpacks, the real strings can be tested against since they are defined in the
 * fake langpack inside of the test.
 *
 * Usage:
 *
 *   add_task(async function test_stringBundleInvalidation() {
 *     const fakeLangpacks = await setupFakeLangpacks(this, SpecialPowers);
 *
 *     await fakeLangpacks.install({
 *       locale: "es-ES",
 *       propertiesFiles: [
 *         {
 *           rootURL: "chrome://branding/locale",
 *           files: {
 *             "brand.properties": "brandFullName=Zorro de Fuego",
 *             "test-only.properties": "testOnly=Mensaje solo para pruebas"
 *           },
 *         },
 *       ],
 *     });
 *
 *     Services.locale.requestedLocales = ["es-ES"];
 *     const bundle1 = Services.strings.createBundle(
 *      "chrome://branding/locale/branding.properties"
 *     );
 *     const bundle2 = Services.strings.createBundle(
 *      "chrome://branding/locale/test-only.properties"
 *     );
 *   });
 *
 * @param {object} testEnv - The `this` object from the test.
 * @param {SpecialPowers} SpecialPowers - Generally a property on `self` in the test.
 *
 * @return {Promise<{
 *  install: (options: FakeLangpackOptions) => Promise<nsIFile>,
 *  create: (options: FakeLangpackOptions) => Promise<nsIFile>,
 * }>}
 */
async function setupFakeLangpacks(testEnv, SpecialPowers) {
  /**
   * Create a test-only langpack, with actual content.
   *
   * @param {FakeLangpackOptions} options
   * @returns {Promise<nsIFile>}
   */
  function create(options) {
    const { locale, propertiesFiles } = options;
    const xpiFiles = {
      "manifest.json": getManifestData(locale, propertiesFiles),
    };
    for (const { rootURL, files } of propertiesFiles || []) {
      const slug = getChromeUrlSlug(rootURL);
      const fakePath = getFakeXPIPath(slug);
      for (const [name, contents] of Object.entries(files)) {
        xpiFiles[OS.Path.join(fakePath, name)] = contents;
      }
    }
    return AddonTestUtils.createTempXPIFile(xpiFiles);
  }

  /**
   * Create and install a test-only langpack, with actual content. This XPI file will
   * be created in a temp directory, and actually installed in the app.
   *
   * @param {FakeLangpackOptions} options
   * @returns {Promise<nsIFile>}
   */
  function install(options) {
    testEnv.info(`Installing the ${options.locale} langpack`);
    return AddonTestUtils.promiseInstallFile(create(options));
  }

  AddonTestUtils.initMochitest(testEnv);

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.langpacks.signatures.required", false]],
  });

  return {
    install,
    create,
  };
}

/**
 * Expect URLs to come in the form "chrome://slug/locale".
 *
 * @returns {string}
 */
function getChromeUrlSlug(url) {
  const result = /^chrome:\/\/(\w+)\/locale\/?$/.exec(url);
  if (!result) {
    throw new Error(
      'Expected the properties file\'s chrome URL to take the form: "chrome://slug/locale":' +
        JSON.stringify(url)
    );
  }
  return result[1];
}

function getManifestData(locale, propertiesFiles) {
  const chrome_resources = {};
  for (const { rootURL } of propertiesFiles) {
    const slug = getChromeUrlSlug(rootURL);
    chrome_resources[slug] = getFakeXPIPath(slug) + "/";
  }
  return {
    langpack_id: locale,
    name: `${locale} Language Pack`,
    description: `${locale} Language pack`,
    languages: {
      [locale]: {
        chrome_resources,
        version: "1",
      },
    },
    applications: {
      gecko: {
        strict_min_version: AppConstants.MOZ_APP_VERSION,
        id: `langpack-${locale}@firefox.mozilla.org`,
        strict_max_version: AppConstants.MOZ_APP_VERSION,
      },
    },
    version: "2.0",
    manifest_version: 2,
    sources: {
      browser: {
        base_path: "browser/",
      },
    },
    author: "Mozilla",
  };
}

/**
 * @typedef {object} FakeProperties
 * @property {string} rootURL - The path that gets used for the chrome URL. It must take
 *   the form "chrome://slug/locale".
 * @property {{[string]: string}} files - The list of files that will be placed in the
 *   XPI. The key is the file name, and the value is the contents of the file.
 */

/**
 * @typedef {object} FakeLangpackOptions
 * @property {string} locale - The BCP 47 identifier
 * @property {FakeProperties[]} propertiesFiles - This list of fake properties files
 *   to be served from chrome URLs. These can either be invented files, or shadow
 *   the underlying translation files.
 */

/**
 * The real paths in XPI files are something like:
 *
 * - browser/chrome/es-ES/locale/branding/brand.properties
 * - browser/chrome/es-ES/locale/browser/browser.properties
 * - browser/chrome/es-ES/locale/es-ES/devtools/client/debugger.properties
 * - browser/features/formautofill@mozilla.org/es-ES/locale/es-ES/formautofill.properties
 *
 * However, in the manifest.json, the chrome URLs can point to arbitrary files via
 * the chrome_resources key. This function generates an arbitrary fake path to store
 * the files in.
 */
function getFakeXPIPath(rootSlug) {
  return `fake-${rootSlug}`;
}
