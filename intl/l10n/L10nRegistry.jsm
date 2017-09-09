const { Services } = Components.utils.import('resource://gre/modules/Services.jsm', {});
const { MessageContext } = Components.utils.import("resource://gre/modules/MessageContext.jsm", {});
Components.utils.importGlobalProperties(["fetch"]); /* globals fetch */

/**
 * L10nRegistry is a localization resource management system for Gecko.
 *
 * It manages the list of resource sources provided with the app and allows
 * for additional sources to be added and updated.
 *
 * It's primary purpose is to allow for building an iterator over MessageContext objects
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
 *   L10nRegistry.generateContexts(['de', 'en-US'], [
 *     '/browser/menu.ftl',
 *     '/platform/toolkit.ftl'
 *   ]);
 *
 * the generator will return an iterator over the following contexts:
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
 * This allows the localization API to consume the MessageContext and lazily fallback
 * on the next in case of a missing string or error.
 *
 * If during the life-cycle of the app a new source is added, the generator can be called again
 * and will produce a new set of permutations placing the language pack provided resources
 * at the top.
 */

const L10nRegistry = {
  sources: new Map(),
  ctxCache: new Map(),

  /**
   * Based on the list of requested languages and resource Ids,
   * this function returns an lazy iterator over message context permutations.
   *
   * @param {Array} requestedLangs
   * @param {Array} resourceIds
   * @returns {Iterator<MessageContext>}
   */
  * generateContexts(requestedLangs, resourceIds) {
    const sourcesOrder = Array.from(this.sources.keys()).reverse();
    for (const locale of requestedLangs) {
      yield * generateContextsForLocale(locale, sourcesOrder, resourceIds);
    }
  },

  /**
   * Adds a new resource source to the L10nRegistry.
   *
   * @param {FileSource} source
   */
  registerSource(source) {
    if (this.sources.has(source.name)) {
      throw new Error(`Source with name "${source.name}" already registered.`);
    }
    this.sources.set(source.name, source);
    Services.obs.notifyObservers(null, 'l10n:available-locales-changed', null);
  },

  /**
   * Updates an existing source in the L10nRegistry
   *
   * That will usually happen when a new version of a source becomes
   * available (for example, an updated version of a language pack).
   *
   * @param {FileSource} source
   */
  updateSource(source) {
    if (!this.sources.has(source.name)) {
      throw new Error(`Source with name "${source.name}" is not registered.`);
    }
    this.sources.set(source.name, source);
    this.ctxCache.clear();
    Services.obs.notifyObservers(null, 'l10n:available-locales-changed', null);
  },

  /**
   * Removes a source from the L10nRegistry.
   *
   * @param {String} sourceId
   */
  removeSource(sourceName) {
    this.sources.delete(sourceName);
    Services.obs.notifyObservers(null, 'l10n:available-locales-changed', null);
  },

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
  },
};

/**
 * A helper function for generating unique context ID used for caching
 * MessageContexts.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @returns {String}
 */
function generateContextID(locale, sourcesOrder, resourceIds) {
  const sources = sourcesOrder.join(',');
  const ids = resourceIds.join(',');
  return `${locale}|${sources}|${ids}`;
}

/**
 * This function generates an iterator over MessageContexts for a single locale
 * for a given list of resourceIds for all possible combinations of sources.
 *
 * This function is called recursively to generate all possible permutations
 * and uses the last, optional parameter, to pass the already resolved
 * sources order.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @param {Array} [resolvedOrder]
 * @returns {Iterator<MessageContext>}
 */
function* generateContextsForLocale(locale, sourcesOrder, resourceIds, resolvedOrder = []) {
  const resolvedLength = resolvedOrder.length;
  const resourcesLength = resourceIds.length;

  // Inside that loop we have a list of resources and the sources for them, like this:
  //   ['test.ftl', 'menu.ftl', 'foo.ftl']
  //   ['app', 'platform', 'app']
  for (const sourceName of sourcesOrder) {
    const order = resolvedOrder.concat(sourceName);

    // We bail only if the hasFile returns a strict false here,
    // because for FileSource it may also return undefined, which means
    // that we simply don't know if the source contains the file and we'll
    // have to perform the I/O to learn.
    if (L10nRegistry.sources.get(sourceName).hasFile(locale, resourceIds[resolvedOrder.length]) === false) {
      continue;
    }

    // If the number of resolved sources equals the number of resources,
    // create the right context and return it if it loads.
    if (resolvedLength + 1 === resourcesLength) {
      yield generateContext(locale, order, resourceIds);
    } else {
      // otherwise recursively load another generator that walks over the
      // partially resolved list of sources.
      yield * generateContextsForLocale(locale, sourcesOrder, resourceIds, order);
    }
  }
}

/**
 * Generates a single MessageContext by loading all resources
 * from the listed sources for a given locale.
 *
 * @param {String} locale
 * @param {Array} sourcesOrder
 * @param {Array} resourceIds
 * @returns {Promise<MessageContext>}
 */
async function generateContext(locale, sourcesOrder, resourceIds) {
  const ctxId = generateContextID(locale, sourcesOrder, resourceIds);
  if (!L10nRegistry.ctxCache.has(ctxId)) {
    const ctx = new MessageContext(locale);
    for (let i = 0; i < resourceIds.length; i++) {
      const data = await L10nRegistry.sources.get(sourcesOrder[i]).fetchFile(locale, resourceIds[i]);
      if (data === null) {
        return false;
      }
      ctx.addMessages(data);
    }
    L10nRegistry.ctxCache.set(ctxId, ctx);
  }
  return L10nRegistry.ctxCache.get(ctxId);
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
  constructor(name, locales, prePath) {
    this.name = name;
    this.locales = locales;
    this.prePath = prePath;
    this.cache = {};
  }

  getPath(locale, path) {
    return (this.prePath + path).replace(/\{locale\}/g, locale);
  }

  hasFile(locale, path) {
    if (!this.locales.includes(locale)) {
      return false;
    }

    const fullPath = this.getPath(locale, path);
    if (!this.cache.hasOwnProperty(fullPath)) {
      return undefined;
    }

    if (this.cache[fullPath] === null) {
      return false;
    }
    return true;
  }

  async fetchFile(locale, path) {
    if (!this.locales.includes(locale)) {
      return null;
    }

    const fullPath = this.getPath(locale, path);
    if (this.hasFile(locale, path) === undefined) {
      let file = await L10nRegistry.load(fullPath);

      if (file === undefined) {
        this.cache[fullPath] = null;
      } else {
        this.cache[fullPath] = file;
      }
    }
    return this.cache[fullPath];
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
  constructor(name, locales, prePath, paths) {
    super(name, locales, prePath);
    this.paths = paths;
  }

  hasFile(locale, path) {
    if (!this.locales.includes(locale)) {
      return false;
    }
    const fullPath = this.getPath(locale, path);
    return this.paths.includes(fullPath);
  }

  async fetchFile(locale, path) {
    if (!this.locales.includes(locale)) {
      return null;
    }

    const fullPath = this.getPath(locale, path);
    if (this.paths.includes(fullPath)) {
      let file = await L10nRegistry.load(fullPath);

      if (file === undefined) {
        return null;
      } else {
        return file;
      }
    } else {
      return null;
    }
  }
}

/**
 * We keep it as a method to make it easier to override for testing purposes.
 **/
L10nRegistry.load = function(url) {
  return fetch(url).then(data => data.text()).catch(() => undefined);
};

this.L10nRegistry = L10nRegistry;
this.FileSource = FileSource;
this.IndexedFileSource = IndexedFileSource;

this.EXPORTED_SYMBOLS = ['L10nRegistry', 'FileSource', 'IndexedFileSource'];
