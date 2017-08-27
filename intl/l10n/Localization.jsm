/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */

/* Copyright 2017 Mozilla Foundation and others
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* fluent@0.4.1 */

/* eslint no-console: ["error", { allow: ["warn", "error"] }] */
/* global console */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const { L10nRegistry } = Cu.import("resource://gre/modules/L10nRegistry.jsm", {});
const LocaleService = Cc["@mozilla.org/intl/localeservice;1"].getService(Ci.mozILocaleService);
const ObserverService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);

/**
 * CachedIterable caches the elements yielded by an iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class CachedIterable {
  constructor(iterable) {
    if (!(Symbol.iterator in Object(iterable))) {
      throw new TypeError('Argument must implement the iteration protocol.');
    }

    this.iterator = iterable[Symbol.iterator]();
    this.seen = [];
  }

  [Symbol.iterator]() {
    const { seen, iterator } = this;
    let cur = 0;

    return {
      next() {
        if (seen.length <= cur) {
          seen.push(iterator.next());
        }
        return seen[cur++];
      }
    };
  }
}

/**
 * Specialized version of an Error used to indicate errors that are result
 * of a problem during the localization process.
 *
 * We use them to identify the class of errors the require a fallback
 * mechanism to be triggered vs errors that should be reported, but
 * do not prevent the message from being used.
 *
 * An example of an L10nError is a missing entry.
 */
class L10nError extends Error {
  constructor(message) {
    super();
    this.name = 'L10nError';
    this.message = message;
  }
}

/**
 * The default localization strategy for Gecko. It comabines locales
 * available in L10nRegistry, with locales requested by the user to
 * generate the iterator over MessageContexts.
 *
 * In the future, we may want to allow certain modules to override this
 * with a different negotitation strategy to allow for the module to
 * be localized into a different language - for example DevTools.
 */
function defaultGenerateMessages(resourceIds) {
  const availableLocales = L10nRegistry.getAvailableLocales();

  const requestedLocales = LocaleService.getRequestedLocales();
  const defaultLocale = LocaleService.defaultLocale;
  const locales = LocaleService.negotiateLanguages(
    requestedLocales, availableLocales, defaultLocale,
  );
  return L10nRegistry.generateContexts(locales, resourceIds);
}

/**
 * The `Localization` class is a central high-level API for vanilla
 * JavaScript use of Fluent.
 * It combines language negotiation, MessageContext and I/O to
 * provide a scriptable API to format translations.
 */
class Localization {
  /**
   * @param {Array<String>} resourceIds      - List of resource IDs
   * @param {Function}      generateMessages - Function that returns the
   *                                           generator over MessageContexts
   *
   * @returns {Localization}
   */
  constructor(resourceIds, generateMessages = defaultGenerateMessages) {
    this.resourceIds = resourceIds;
    this.generateMessages = generateMessages;
    this.ctxs = new CachedIterable(this.generateMessages(this.resourceIds));
  }

  /**
   * Format translations and handle fallback if needed.
   *
   * Format translations for `keys` from `MessageContext` instances on this
   * DOMLocalization. In case of errors, fetch the next context in the
   * fallback chain.
   *
   * @param   {Array<Array>}          keys    - Translation keys to format.
   * @param   {Function}              method  - Formatting function.
   * @returns {Promise<Array<string|Object>>}
   * @private
   */
  async formatWithFallback(keys, method) {
    const translations = [];
    for (let ctx of this.ctxs) {
      // This can operate on synchronous and asynchronous
      // contexts coming from the iterator.
      if (typeof ctx.then === 'function') {
        ctx = await ctx;
      }
      const errors = keysFromContext(method, ctx, keys, translations);
      if (!errors) {
        break;
      }
    }
    return translations;
  }

  /**
   * Format translations into {value, attrs} objects.
   *
   * The fallback logic is the same as in `formatValues` but the argument type
   * is stricter (an array of arrays) and it returns {value, attrs} objects
   * which are suitable for the translation of DOM elements.
   *
   *     docL10n.formatMessages([
   *       ['hello', { who: 'Mary' }],
   *       ['welcome', undefined]
   *     ]).then(console.log);
   *
   *     // [
   *     //   { value: 'Hello, Mary!', attrs: null },
   *     //   { value: 'Welcome!', attrs: { title: 'Hello' } }
   *     // ]
   *
   * Returns a Promise resolving to an array of the translation strings.
   *
   * @param   {Array<Array>} keys
   * @returns {Promise<Array<{value: string, attrs: Object}>>}
   * @private
   */
  formatMessages(keys) {
    return this.formatWithFallback(keys, messageFromContext);
  }

  /**
   * Retrieve translations corresponding to the passed keys.
   *
   *     docL10n.formatValues([
   *       ['hello', { who: 'Mary' }],
   *       ['hello', { who: 'John' }],
   *       ['welcome']
   *     ]).then(console.log);
   *
   *     // ['Hello, Mary!', 'Hello, John!', 'Welcome!']
   *
   * Returns a Promise resolving to an array of the translation strings.
   *
   * @param   {Array<Array>} keys
   * @returns {Promise<Array<string>>}
   */
  formatValues(keys) {
    return this.formatWithFallback(keys, valueFromContext);
  }

  /**
   * Retrieve the translation corresponding to the `id` identifier.
   *
   * If passed, `args` is a simple hash object with a list of variables that
   * will be interpolated in the value of the translation.
   *
   *     docL10n.formatValue(
   *       'hello', { who: 'world' }
   *     ).then(console.log);
   *
   *     // 'Hello, world!'
   *
   * Returns a Promise resolving to the translation string.
   *
   * Use this sparingly for one-off messages which don't need to be
   * retranslated when the user changes their language preferences, e.g. in
   * notifications.
   *
   * @param   {string}  id     - Identifier of the translation to format
   * @param   {Object}  [args] - Optional external arguments
   * @returns {Promise<string>}
   */
  async formatValue(id, args) {
    const [val] = await this.formatValues([[id, args]]);
    return val;
  }

  /**
   * Register observers on events that will trigger cache invalidation
   */
  registerObservers() {
    ObserverService.addObserver(this, 'l10n:available-locales-changed', false);
    ObserverService.addObserver(this, 'intl:requested-locales-changed', false);
  }

  /**
   * Unregister observers on events that will trigger cache invalidation
   */
  unregisterObservers() {
    ObserverService.removeObserver(this, 'l10n:available-locales-changed');
    ObserverService.removeObserver(this, 'intl:requested-locales-changed');
  }

  /**
   * Default observer handler method.
   *
   * @param {String} subject
   * @param {String} topic
   * @param {Object} data
   */
  observe(subject, topic, data) {
    switch (topic) {
      case 'l10n:available-locales-changed':
      case 'intl:requested-locales-changed':
        this.onLanguageChange();
        break;
      default:
        break;
    }
  }

  /**
   * This method should be called when there's a reason to believe
   * that language negotiation or available resources changed.
   */
  onLanguageChange() {
    this.ctxs = new CachedIterable(this.generateMessages(this.resourceIds));
  }
}

/**
 * Format the value of a message into a string.
 *
 * This function is passed as a method to `keysFromContext` and resolve
 * a value of a single L10n Entity using provided `MessageContext`.
 *
 * If the function fails to retrieve the entity, it will return an ID of it.
 * If formatting fails, it will return a partially resolved entity.
 *
 * In both cases, an error is being added to the errors array.
 *
 * @param   {MessageContext} ctx
 * @param   {Array<Error>}   errors
 * @param   {string}         id
 * @param   {Object}         args
 * @returns {string}
 * @private
 */
function valueFromContext(ctx, errors, id, args) {
  const msg = ctx.getMessage(id);

  if (msg === undefined) {
    errors.push(new L10nError(`Unknown entity: ${id}`));
    return id;
  }

  return ctx.format(msg, args, errors);
}

/**
 * Format all public values of a message into a { value, attrs } object.
 *
 * This function is passed as a method to `keysFromContext` and resolve
 * a single L10n Entity using provided `MessageContext`.
 *
 * The function will return an object with a value and attributes of the
 * entity.
 *
 * If the function fails to retrieve the entity, the value is set to the ID of
 * an entity, and attrs to `null`. If formatting fails, it will return
 * a partially resolved value and attributes.
 *
 * In both cases, an error is being added to the errors array.
 *
 * @param   {MessageContext} ctx
 * @param   {Array<Error>}   errors
 * @param   {String}         id
 * @param   {Object}         args
 * @returns {Object}
 * @private
 */
function messageFromContext(ctx, errors, id, args) {
  const msg = ctx.getMessage(id);

  if (msg === undefined) {
    errors.push(new L10nError(`Unknown message: ${id}`));
    return { value: id, attrs: null };
  }

  const formatted = {
    value: ctx.format(msg, args, errors),
    attrs: null,
  };

  if (msg.attrs) {
    formatted.attrs = [];
    for (const attrName in msg.attrs) {
      const formattedAttr = ctx.format(msg.attrs[attrName], args, errors);
      if (formattedAttr !== null) {
        formatted.attrs.push([
          attrName,
          formattedAttr
        ]);
      }
    }
  }

  return formatted;
}

/**
 * This function is an inner function for `Localization.formatWithFallback`.
 *
 * It takes a `MessageContext`, list of l10n-ids and a method to be used for
 * key resolution (either `valueFromContext` or `entityFromContext`) and
 * optionally a value returned from `keysFromContext` executed against
 * another `MessageContext`.
 *
 * The idea here is that if the previous `MessageContext` did not resolve
 * all keys, we're calling this function with the next context to resolve
 * the remaining ones.
 *
 * In the function, we loop over `keys` and check if we have the `prev`
 * passed and if it has an error entry for the position we're in.
 *
 * If it doesn't, it means that we have a good translation for this key and
 * we return it. If it does, we'll try to resolve the key using the passed
 * `MessageContext`.
 *
 * In the end, we fill the translations array, and return if we
 * encountered at least one error.
 *
 * See `Localization.formatWithFallback` for more info on how this is used.
 *
 * @param {Function}       method
 * @param {MessageContext} ctx
 * @param {Array<string>}  keys
 * @param {{Array<{value: string, attrs: Object}>}} translations
 *
 * @returns {Boolean}
 * @private
 */
function keysFromContext(method, ctx, keys, translations) {
  const messageErrors = [];
  let hasErrors = false;

  keys.forEach((key, i) => {
    if (translations[i] !== undefined) {
      return;
    }

    messageErrors.length = 0;
    const translation = method(ctx, messageErrors, key[0], key[1]);

    if (messageErrors.length === 0 ||
        !messageErrors.some(e => e instanceof L10nError)) {
      translations[i] = translation;
    } else {
      hasErrors = true;
    }

    if (messageErrors.length) {
      const { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      messageErrors.forEach(error => console.warn(error));
    }
  });

  return hasErrors;
}

this.Localization = Localization;
this.EXPORTED_SYMBOLS = ['Localization'];
