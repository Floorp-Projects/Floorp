const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
// eslint-disable-next-line mozilla/use-services
const appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
const { FluentBundle, FluentResource } = ChromeUtils.import("resource://gre/modules/Fluent.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const isParentProcess = appinfo.processType === appinfo.PROCESS_TYPE_DEFAULT;
/**
 * L10nRegistry is a localization resource management system for Gecko.
 *
 * It manages the list of resource sources provided with the app and allows
 * for additional sources to be added and updated.
 *
 * It's primary purpose is to allow for building an iterator over FluentBundle objects
 * that will be utilized by a localization API.
 *
 * The generator creates all possible permutations of locales and sources to allow for
 * complete fallbacking.
 *
 * Example:
 *
 *   FileSource1:
 *     name: 'app'
 *     locales: ['en-US', 'de']
 *     resources: [
 *       '/browser/menu.ftl',
 *       '/platform/toolkit.ftl',
 *     ]
 *   FileSource2:
 *     name: 'platform'
 *     locales: ['en-US', 'de']
 *     resources: [
 *       '/platform/toolkit.ftl',
 *     ]
 *
 * If the user will request:
 *   L10nRegistry.generateBundles(['de', 'en-US'], [
 *     '/browser/menu.ftl',
 *     '/platform/toolkit.ftl'
 *   ]);
 *
 * the generator will return an async iterator over the following contexts:
 *
 *   {
 *     locale: 'de',
 *     resources: [
 *       ['app', '/browser/menu.ftl'],
 *       ['app', '/platform/toolkit.ftl'],
 *     ]
 *   },
 *   {
 *     locale: 'de',
 *     resources: [
 *       ['app', '/browser/menu.ftl'],
 *       ['platform', '/platform/toolkit.ftl'],
 *     ]
 *   },
 *   {
 *     locale: 'en-US',
 *     resources: [
 *       ['app', '/browser/menu.ftl'],
 *       ['app', '/platform/toolkit.ftl'],
 *     ]
 *   },
 *   {
 *     locale: 'en-US',
 *     resources: [
 *       ['app', '/browser/menu.ftl'],
 *       ['platform', '/platform/toolkit.ftl'],
 *     ]
 *   }
 *
 * This allows the localization API to consume the FluentBundle and lazily fallback
 * on the next in case of a missing string or error.
 *
 * If during the life-cycle of the app a new source is added, the generator can be called again
 * and will produce a new set of permutations placing the language pack provided resources
 * at the top.
 *
 * Notice: L10nRegistry is primarily an asynchronous API, but
 * it does provide a synchronous version of it's main method
 * for use by the `LocalizationSync` class.
 * This API should be only used in very specialized cases and
 * the uses should be reviewed by the toolkit owner/peer.
 */
class L10nRegistryService {
  constructor() {
    this.sources = new Map();

    if (!isParentProcess) {
      this._setSourcesFromSharedData();
      Services.cpmm.sharedData.addEventListener("change", this);
    }
  }

  handleEvent(event) {
    if (event.type === "change") {
      if (event.changedKeys.includes("L10nRegistry:Sources")) {
        this._setSourcesFromSharedData();
      }
    }
  }

  /**
   * Based on the list of requested languages and resource Ids,
   * this function returns an lazy iterator over message context permutations.
   *
   * Notice: Any changes to this method should be copied
   * to the `generateBundlesSync` equivalent below.
   *
   * @param {Array} requestedLangs
   * @param {Array} resourceIds
   * @returns {AsyncIterator<FluentBundle>}
   */
  async* generateBundles(requestedLangs, resourceIds) {
    const sourcesOrder = Array.from(this.sources.keys()).reverse();
    const pseudoNameFromPref = Services.prefs.getStringPref("intl.l10n.pseudo", "");
    for (const locale of requestedLangs) {
      for await (const dataSets of generateResourceSetsForLocale(locale, sourcesOrder, resourceIds)) {
        const bundle = new FluentBundle(locale, {
          ...MSG_CONTEXT_OPTIONS,
          transform: PSEUDO_STRATEGIES[pseudoNameFromPref],
        });
        for (const data of dataSets) {
          if (data === null) {
            return;
          }
          bundle.addResource(data);
        }
        yield bundle;
      }
    }
  }

  /**
   * This is a synchronous version of the `generateBundles`
   * method and should stay completely in sync with it at all
   * times except of the async/await changes.
   *
   * Notice: This method should be avoided at all costs
   * You can think of it similarly to a synchronous XMLHttpRequest.
   *
   * @param {Array} requestedLangs
   * @param {Array} resourceIds
   * @returns {Iterator<FluentBundle>}
   */
  * generateBundlesSync(requestedLangs, resourceIds) {
    const sourcesOrder = Array.from(this.sources.keys()).reverse();
    const pseudoNameFromPref = Services.prefs.getStringPref("intl.l10n.pseudo", "");
    for (const locale of requestedLangs) {
      for (const dataSets of generateResourceSetsForLocaleSync(locale, sourcesOrder, resourceIds)) {
        const bundle = new FluentBundle(locale, {
          ...MSG_CONTEXT_OPTIONS,
          transform: PSEUDO_STRATEGIES[pseudoNameFromPref],
        });
        for (const data of dataSets) {
          if (data === null) {
            return;
          }
          bundle.addResource(data);
        }
        yield bundle;
      }
    }
  }

  /**
   * Check whether a source with the given known is already registered.
   *
   * @param {String} sourceName
   * @returns {boolean} whether or not a source by that name is known.
   */
  hasSource(sourceName) {
    return this.sources.has(sourceName);
  }

  /**
   * Adds a new resource source to the L10nRegistry.
   *
   * @param {FileSource} source
   */
  registerSource(source) {
    if (this.hasSource(source.name)) {
      throw new Error(`Source with name "${source.name}" already registered.`);
    }
    this.sources.set(source.name, source);

    if (isParentProcess) {
      this._synchronizeSharedData();
      Services.locale.availableLocales = this.getAvailableLocales();
    }
  }

  /**
   * Updates an existing source in the L10nRegistry
   *
   * That will usually happen when a new version of a source becomes
   * available (for example, an updated version of a language pack).
   *
   * @param {FileSource} source
   */
  updateSource(source) {
    if (!this.hasSource(source.name)) {
      throw new Error(`Source with name "${source.name}" is not registered.`);
    }
    this.sources.set(source.name, source);
    if (isParentProcess) {
      this._synchronizeSharedData();
      Services.locale.availableLocales = this.getAvailableLocales();
    }
  }

  /**
   * Removes a source from the L10nRegistry.
   *
   * @param {String} sourceId
   */
  removeSource(sourceName) {
    this.sources.delete(sourceName);
    if (isParentProcess) {
      this._synchronizeSharedData();
      Services.locale.availableLocales = this.getAvailableLocales();
    }
  }

  _synchronizeSharedData() {
    const sources = new Map();
    for (const [name, source] of this.sources.entries()) {
      if (source.indexed) {
        continue;
      }
      sources.set(name, {
        locales: source.locales,
        prePath: source.prePath,
      });
    }
    Services.ppmm.sharedData.set("L10nRegistry:Sources", sources);
    Services.ppmm.sharedData.flush();
  }

  _setSourcesFromSharedData() {
    let sources = Services.cpmm.sharedData.get("L10nRegistry:Sources");
    for (let [name, data] of sources.entries()) {
      if (!this.hasSource(name)) {
        const source = new FileSource(name, data.locales, data.prePath);
        this.registerSource(source);
      }
    }
    for (let name of this.sources.keys()) {
      if (!sources.has(name)) {
        this.removeSource(name);
      }
    }
  }

  /**
   * Returns a list of locales for which at least one source
   * has resources.
   *
   * @returns {Array<String>}
   */
  getAvailableLocales() {
    const locales = new Set();

    for (const source of this.sources.values()) {
      for (const locale of source.locales) {
        locales.add(locale);
      }
    }
    return Array.from(locales);
  }
}

/**
 * This function generates an iterator over FluentBundles for a single locale
 * for a given list of resourceIds for all possible combinations of sources.
 *
 * This function is called recursively to generate all possible permutations
 * and uses the last, optional parameter, to pass the already resolved
 * sources order.
 *
 * Notice: Any changes to this method should be copied
 * to the `generateResourceSetsForLocaleSync` equivalent below.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @param {Array} [resolvedOrder]
 * @returns {AsyncIterator<FluentBundle>}
 */
async function* generateResourceSetsForLocale(locale, sourcesOrder, resourceIds, resolvedOrder = []) {
  const resolvedLength = resolvedOrder.length;
  const resourcesLength = resourceIds.length;

  // Inside that loop we have a list of resources and the sources for them, like this:
  //   ['test.ftl', 'menu.ftl', 'foo.ftl']
  //   ['app', 'platform', 'app']
  for (const sourceName of sourcesOrder) {
    const order = resolvedOrder.concat(sourceName);

    // We want to bail out early if we know that any of
    // the (res)x(source) combinations in the permutation
    // are unavailable.
    // The combination may have been `undefined` when we
    // stepped into this branch, and now is resolved to
    // `false`.
    //
    // If the combination resolved to `false` is the last
    // in the resolvedOrder, we want to continue in this
    // loop, but if it's somewhere in the middle, we can
    // safely bail from the whole branch.
    for (let [idx, sourceName] of order.entries()) {
      const source = L10nRegistry.sources.get(sourceName);
      if (!source || source.hasFile(locale, resourceIds[idx]) === false) {
        if (idx === order.length - 1) {
          continue;
        } else {
          return;
        }
      }
    }

    // If the number of resolved sources equals the number of resources,
    // create the right context and return it if it loads.
    if (resolvedLength + 1 === resourcesLength) {
      let dataSet = await generateResourceSet(locale, order, resourceIds);
      // Here we check again to see if the newly resolved
      // resources returned `false` on any position.
      if (!dataSet.includes(false)) {
        yield dataSet;
      }
    } else if (resolvedLength < resourcesLength) {
      // otherwise recursively load another generator that walks over the
      // partially resolved list of sources.
      yield * generateResourceSetsForLocale(locale, sourcesOrder, resourceIds, order);
    }
  }
}

/**
 * This is a synchronous version of the `generateResourceSetsForLocale`
 * method and should stay completely in sync with it at all
 * times except of the async/await changes.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @param {Array} [resolvedOrder]
 * @returns {Iterator<FluentBundle>}
 */
function* generateResourceSetsForLocaleSync(locale, sourcesOrder, resourceIds, resolvedOrder = []) {
  const resolvedLength = resolvedOrder.length;
  const resourcesLength = resourceIds.length;

  // Inside that loop we have a list of resources and the sources for them, like this:
  //   ['test.ftl', 'menu.ftl', 'foo.ftl']
  //   ['app', 'platform', 'app']
  for (const sourceName of sourcesOrder) {
    const order = resolvedOrder.concat(sourceName);

    // We want to bail out early if we know that any of
    // the (res)x(source) combinations in the permutation
    // are unavailable.
    // The combination may have been `undefined` when we
    // stepped into this branch, and now is resolved to
    // `false`.
    //
    // If the combination resolved to `false` is the last
    // in the resolvedOrder, we want to continue in this
    // loop, but if it's somewhere in the middle, we can
    // safely bail from the whole branch.
    for (let [idx, sourceName] of order.entries()) {
      const source = L10nRegistry.sources.get(sourceName);
      if (!source || source.hasFile(locale, resourceIds[idx]) === false) {
        if (idx === order.length - 1) {
          continue;
        } else {
          return;
        }
      }
    }

    // If the number of resolved sources equals the number of resources,
    // create the right context and return it if it loads.
    if (resolvedLength + 1 === resourcesLength) {
      let dataSet = generateResourceSetSync(locale, order, resourceIds);
      // Here we check again to see if the newly resolved
      // resources returned `false` on any position.
      if (!dataSet.includes(false)) {
        yield dataSet;
      }
    } else if (resolvedLength < resourcesLength) {
      // otherwise recursively load another generator that walks over the
      // partially resolved list of sources.
      yield * generateResourceSetsForLocaleSync(locale, sourcesOrder, resourceIds, order);
    }
  }
}

const MSG_CONTEXT_OPTIONS = {
  // Temporarily disable bidi isolation due to Microsoft not supporting FSI/PDI.
  // See bug 1439018 for details.
  useIsolating: Services.prefs.getBoolPref("intl.l10n.enable-bidi-marks", false),
  functions: {
    /**
     * PLATFORM is a built-in allowing localizers to differentiate message
     * variants depending on the target platform.
     */
    PLATFORM: () => {
      switch (AppConstants.platform) {
        case "linux":
        case "android":
          return AppConstants.platform;
        case "win":
          return "windows";
        case "macosx":
          return "macos";
        default:
          return "other";
      }
    },
  },
};

/**
 * Pseudolocalizations
 *
 * PSEUDO_STRATEGIES is a dict of strategies to be used to modify a
 * context in order to create pseudolocalizations.  These can be used by
 * developers to test the localizability of their code without having to
 * actually speak a foreign language.
 *
 * Currently, the following pseudolocales are supported:
 *
 *   accented - Ȧȧƈƈḗḗƞŧḗḗḓ Ḗḗƞɠŀīīşħ
 *
 *     In Accented English all Latin letters are replaced by accented
 *     Unicode counterparts which don't impair the readability of the content.
 *     This allows developers to quickly test if any given string is being
 *     correctly displayed in its 'translated' form.  Additionally, simple
 *     heuristics are used to make certain words longer to better simulate the
 *     experience of international users.
 *
 *   bidi - ɥsıʅƃuƎ ıpıԐ
 *
 *     Bidi English is a fake RTL locale.  All words are surrounded by
 *     Unicode formatting marks forcing the RTL directionality of characters.
 *     In addition, to make the reversed text easier to read, individual
 *     letters are flipped.
 *
 *     Note: The name above is hardcoded to be RTL in case code editors have
 *     trouble with the RLO and PDF Unicode marks.  In reality, it should be
 *     surrounded by those marks as well.
 *
 * See https://bugzil.la/1450781 for more information.
 *
 * In this implementation we use code points instead of inline unicode characters
 * because the encoding of JSM files mangles them otherwise.
 */

const ACCENTED_MAP = {
      // ȦƁƇḒḖƑƓĦĪĴĶĿḾȠǾƤɊŘŞŦŬṼẆẊẎẐ
      "caps": [550, 385, 391, 7698, 7702, 401, 403, 294, 298, 308, 310, 319, 7742, 544, 510, 420, 586, 344, 350, 358, 364, 7804, 7814, 7818, 7822, 7824],
      // ȧƀƈḓḗƒɠħīĵķŀḿƞǿƥɋřşŧŭṽẇẋẏẑ
      "small": [551, 384, 392, 7699, 7703, 402, 608, 295, 299, 309, 311, 320, 7743, 414, 511, 421, 587, 345, 351, 359, 365, 7805, 7815, 7819, 7823, 7825],
};

const FLIPPED_MAP = {
      // ∀ԐↃᗡƎℲ⅁HIſӼ⅂WNOԀÒᴚS⊥∩ɅMX⅄Z
      "caps": [8704, 1296, 8579, 5601, 398, 8498, 8513, 72, 73, 383, 1276, 8514, 87, 78, 79, 1280, 210, 7450, 83, 8869, 8745, 581, 77, 88, 8516, 90],
      // ɐqɔpǝɟƃɥıɾʞʅɯuodbɹsʇnʌʍxʎz
      "small": [592, 113, 596, 112, 477, 607, 387, 613, 305, 638, 670, 645, 623, 117, 111, 100, 98, 633, 115, 647, 110, 652, 653, 120, 654, 122],
};

function transformString(map, elongate = false, prefix = "", postfix = "", msg) {
  // Exclude access-keys and other single-char messages
  if (msg.length === 1) {
    return msg;
  }
  // XML entities (&#x202a;) and XML tags.
  const reExcluded = /(&[#\w]+;|<\s*.+?\s*>)/;

  const parts = msg.split(reExcluded);
  const modified = parts.map((part) => {
    if (reExcluded.test(part)) {
      return part;
    }
    return prefix + part.replace(/[a-z]/ig, (ch) => {
      let cc = ch.charCodeAt(0);
      if (cc >= 97 && cc <= 122) {
        const newChar = String.fromCodePoint(map.small[cc - 97]);
        // duplicate "a", "e", "o" and "u" to emulate ~30% longer text
        if (elongate && (cc === 97 || cc === 101 || cc === 111 || cc === 117)) {
          return newChar + newChar;
        }
        return newChar;
      }
      if (cc >= 65 && cc <= 90) {
        return String.fromCodePoint(map.caps[cc - 65]);
      }
      return ch;
    }) + postfix;
  });
  return modified.join("");
}

const PSEUDO_STRATEGIES = {
  "accented": transformString.bind(null, ACCENTED_MAP, true, "", ""),
  "bidi": transformString.bind(null, FLIPPED_MAP, false, "\u202e", "\u202c"),
};

/**
 * Generates a single FluentBundle by loading all resources
 * from the listed sources for a given locale.
 *
 * The function casts all error cases into a Promise that resolves with
 * value `null`.
 * This allows the caller to be an async generator without using
 * try/catch clauses.
 *
 * Notice: Any changes to this method should be copied
 * to the `generateResourceSetSync` equivalent below.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @returns {Promise<FluentBundle>}
 */
function generateResourceSet(locale, sourcesOrder, resourceIds) {
  return Promise.all(resourceIds.map((resourceId, i) => {
    const source = L10nRegistry.sources.get(sourcesOrder[i]);
    if (!source) {
      return false;
    }
    return source.fetchFile(locale, resourceId);
  }));
}

/**
 * This is a synchronous version of the `generateResourceSet`
 * method and should stay completely in sync with it at all
 * times except of the async/await changes.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @returns {FluentBundle}
 */
function generateResourceSetSync(locale, sourcesOrder, resourceIds) {
  return resourceIds.map((resourceId, i) => {
    const source = L10nRegistry.sources.get(sourcesOrder[i]);
    if (!source) {
      return false;
    }
    return source.fetchFile(locale, resourceId, {sync: true});
  });
}

/**
 * This is a basic Source for L10nRegistry.
 * It registers its own locales and a pre-path, and when asked for a file
 * it attempts to download and cache it.
 *
 * The Source caches the downloaded files so any consecutive loads will
 * come from the cache.
 **/
class FileSource {
  /**
   * @param {string}         name
   * @param {Array<string>}  locales
   * @param {string}         prePath
   *
   * @returns {FileSource}
   */
  constructor(name, locales, prePath) {
    this.name = name;
    this.locales = locales;
    this.prePath = prePath;
    this.indexed = false;

    // The cache object stores information about the resources available
    // in the Source.
    //
    // It can take one of three states:
    //   * true - the resource is available but not fetched yet
    //   * false - the resource is not available
    //   * Promise - the resource has been fetched
    //
    // If the cache has no entry for a given path, that means that there
    // is no information available about whether the resource is available.
    //
    // If the `indexed` property is set to `true` it will be treated as the
    // resource not being available. Otherwise, the resource may be
    // available and we do not have any information about it yet.
    this.cache = {};
  }

  getPath(locale, path) {
    // This is a special case for the only not BCP47-conformant locale
    // code we have resources for.
    if (locale === "ja-JP-macos") {
      locale = "ja-JP-mac";
    }
    return (this.prePath + path).replace(/\{locale\}/g, locale);
  }

  hasFile(locale, path) {
    if (!this.locales.includes(locale)) {
      return false;
    }

    const fullPath = this.getPath(locale, path);
    if (!this.cache.hasOwnProperty(fullPath)) {
      return this.indexed ? false : undefined;
    }
    if (this.cache[fullPath] === false) {
      return false;
    }
    if (this.cache[fullPath].then) {
      return undefined;
    }
    return true;
  }

  fetchFile(locale, path, options = {sync: false}) {
    if (!this.locales.includes(locale)) {
      return false;
    }

    const fullPath = this.getPath(locale, path);

    if (this.cache.hasOwnProperty(fullPath)) {
      if (this.cache[fullPath] === false) {
        return false;
      }
      // `true` means that the file is indexed, but hasn't
      // been fetched yet.
      if (this.cache[fullPath] !== true) {
        return this.cache[fullPath];
      }
    } else if (this.indexed) {
      return false;
    }
    if (options.sync) {
      let data = L10nRegistry.loadSync(fullPath);

      if (data === false) {
        this.cache[fullPath] = false;
      } else {
        this.cache[fullPath] = FluentResource.fromString(data);
      }

      return this.cache[fullPath];
    }

    // async
    return this.cache[fullPath] = L10nRegistry.load(fullPath).then(
      data => {
        return this.cache[fullPath] = FluentResource.fromString(data);
      },
      err => {
        this.cache[fullPath] = false;
        return false;
      }
    );
  }
}

/**
 * This is an extension of the FileSource which should be used
 * for sources that can provide the list of files available in the source.
 *
 * This allows for a faster lookup in cases where the source does not
 * contain most of the files that the app will request for (e.g. an addon).
 **/
class IndexedFileSource extends FileSource {
  /**
   * @param {string}         name
   * @param {Array<string>}  locales
   * @param {string}         prePath
   * @param {Array<string>}  paths
   *
   * @returns {IndexedFileSource}
   */
  constructor(name, locales, prePath, paths) {
    super(name, locales, prePath);
    this.indexed = true;
    for (const path of paths) {
      this.cache[path] = true;
    }
  }
}

this.L10nRegistry = new L10nRegistryService();

/**
 * The low level wrapper around Fetch API. It unifies the error scenarios to
 * always produce a promise rejection.
 *
 * We keep it as a method to make it easier to override for testing purposes.
 *
 * @param {string} url
 *
 * @returns {Promise<string>}
 */
this.L10nRegistry.load = function(url) {
  return fetch(url).then(response => {
    if (!response.ok) {
      return Promise.reject(response.statusText);
    }
    return response.text();
  });
};

/**
 * This is a synchronous version of the `load`
 * function and should stay completely in sync with it at all
 * times except of the async/await changes.
 *
 * Notice: Any changes to this method should be copied
 * to the `generateResourceSetSync` equivalent below.
 *
 * @param {string} url
 *
 * @returns {string}
 */
this.L10nRegistry.loadSync = function(uri) {
  try {
    let url = Services.io.newURI(uri);
    let data = Cu.readUTF8URI(url);
    return data;
  } catch (e) {
    return false;
  }
};

this.FileSource = FileSource;
this.IndexedFileSource = IndexedFileSource;

var EXPORTED_SYMBOLS = ["L10nRegistry", "FileSource", "IndexedFileSource"];
