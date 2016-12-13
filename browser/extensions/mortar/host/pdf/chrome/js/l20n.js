/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () { 'use strict';

  function emit(listeners, ...args) {
    const type = args.shift();

    if (listeners['*']) {
      listeners['*'].slice().forEach(
        listener => listener.apply(this, args));
    }

    if (listeners[type]) {
      listeners[type].slice().forEach(
        listener => listener.apply(this, args));
    }
  }

  function addEventListener(listeners, type, listener) {
    if (!(type in listeners)) {
      listeners[type] = [];
    }
    listeners[type].push(listener);
  }

  function removeEventListener(listeners, type, listener) {
    const typeListeners = listeners[type];
    const pos = typeListeners.indexOf(listener);
    if (pos === -1) {
      return;
    }

    typeListeners.splice(pos, 1);
  }

  class Client {
    constructor(remote) {
      this.id = this;
      this.remote = remote;

      const listeners = {};
      this.on = (...args) => addEventListener(listeners, ...args);
      this.emit = (...args) => emit(listeners, ...args);
    }

    method(name, ...args) {
      return this.remote[name](...args);
    }
  }

  function broadcast(type, data) {
    Array.from(this.ctxs.keys()).forEach(
      client => client.emit(type, data));
  }

  function L10nError(message, id, lang) {
    this.name = 'L10nError';
    this.message = message;
    this.id = id;
    this.lang = lang;
  }
  L10nError.prototype = Object.create(Error.prototype);
  L10nError.prototype.constructor = L10nError;

  const HTTP_STATUS_CODE_OK = 200;

  function load(type, url) {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();

      if (xhr.overrideMimeType) {
        xhr.overrideMimeType(type);
      }

      xhr.open('GET', url, true);

      if (type === 'application/json') {
        xhr.responseType = 'json';
      }

      xhr.addEventListener('load', e => {
        if (e.target.status === HTTP_STATUS_CODE_OK ||
            e.target.status === 0) {
          resolve(e.target.response);
        } else {
          reject(new L10nError('Not found: ' + url));
        }
      });
      xhr.addEventListener('error', reject);
      xhr.addEventListener('timeout', reject);

      // the app: protocol throws on 404, see https://bugzil.la/827243
      try {
        xhr.send(null);
      } catch (e) {
        if (e.name === 'NS_ERROR_FILE_NOT_FOUND') {
          // the app: protocol throws on 404, see https://bugzil.la/827243
          reject(new L10nError('Not found: ' + url));
        } else {
          throw e;
        }
      }
    });
  }

  const io = {
    extra: function(code, ver, path, type) {
      return navigator.mozApps.getLocalizationResource(
        code, ver, path, type);
    },
    app: function(code, ver, path, type) {
      switch (type) {
        case 'text':
          return load('text/plain', path);
        case 'json':
          return load('application/json', path);
        default:
          throw new L10nError('Unknown file type: ' + type);
      }
    },
  };

  function fetchResource(res, { code, src, ver }) {
    const url = res.replace('{locale}', code);
    const type = res.endsWith('.json') ? 'json' : 'text';
    return io[src](code, ver, url, type);
  }

  const KNOWN_MACROS = ['plural'];
  const MAX_PLACEABLE_LENGTH = 2500;

  // Unicode bidi isolation characters
  const FSI = '\u2068';
  const PDI = '\u2069';

  const resolutionChain = new WeakSet();

  function format(ctx, lang, args, entity) {
    if (typeof entity === 'string') {
      return [{}, entity];
    }

    if (resolutionChain.has(entity)) {
      throw new L10nError('Cyclic reference detected');
    }

    resolutionChain.add(entity);

    let rv;
    // if format fails, we want the exception to bubble up and stop the whole
    // resolving process;  however, we still need to remove the entity from the
    // resolution chain
    try {
      rv = resolveValue(
        {}, ctx, lang, args, entity.value, entity.index);
    } finally {
      resolutionChain.delete(entity);
    }
    return rv;
  }

  function resolveIdentifier(ctx, lang, args, id) {
    if (KNOWN_MACROS.indexOf(id) > -1) {
      return [{}, ctx._getMacro(lang, id)];
    }

    if (args && args.hasOwnProperty(id)) {
      if (typeof args[id] === 'string' || (typeof args[id] === 'number' &&
          !isNaN(args[id]))) {
        return [{}, args[id]];
      } else {
        throw new L10nError('Arg must be a string or a number: ' + id);
      }
    }

    // XXX: special case for Node.js where still:
    // '__proto__' in Object.create(null) => true
    if (id === '__proto__') {
      throw new L10nError('Illegal id: ' + id);
    }

    const entity = ctx._getEntity(lang, id);

    if (entity) {
      return format(ctx, lang, args, entity);
    }

    throw new L10nError('Unknown reference: ' + id);
  }

  function subPlaceable(locals, ctx, lang, args, id) {
    let newLocals, value;

    try {
      [newLocals, value] = resolveIdentifier(ctx, lang, args, id);
    } catch (err) {
      return [{ error: err }, FSI + '{{ ' + id + ' }}' + PDI];
    }

    if (typeof value === 'number') {
      const formatter = ctx._getNumberFormatter(lang);
      return [newLocals, formatter.format(value)];
    }

    if (typeof value === 'string') {
      // prevent Billion Laughs attacks
      if (value.length >= MAX_PLACEABLE_LENGTH) {
        throw new L10nError('Too many characters in placeable (' +
                            value.length + ', max allowed is ' +
                            MAX_PLACEABLE_LENGTH + ')');
      }
      return [newLocals, FSI + value + PDI];
    }

    return [{}, FSI + '{{ ' + id + ' }}' + PDI];
  }

  function interpolate(locals, ctx, lang, args, arr) {
    return arr.reduce(([localsSeq, valueSeq], cur) => {
      if (typeof cur === 'string') {
        return [localsSeq, valueSeq + cur];
      } else {
        const [, value] = subPlaceable(locals, ctx, lang, args, cur.name);
        // wrap the substitution in bidi isolate characters
        return [localsSeq, valueSeq + value];
      }
    }, [locals, '']);
  }

  function resolveSelector(ctx, lang, args, expr, index) {
    //XXX: Dehardcode!!!
    let selectorName;
    if (index[0].type === 'call' && index[0].expr.type === 'prop' &&
        index[0].expr.expr.name === 'cldr') {
      selectorName = 'plural';
    } else {
      selectorName = index[0].name;
    }
    const selector = resolveIdentifier(ctx, lang, args, selectorName)[1];

    if (typeof selector !== 'function') {
      // selector is a simple reference to an entity or args
      return selector;
    }

    const argValue = index[0].args ?
      resolveIdentifier(ctx, lang, args, index[0].args[0].name)[1] : undefined;

    if (selectorName === 'plural') {
      // special cases for zero, one, two if they are defined on the hash
      if (argValue === 0 && 'zero' in expr) {
        return 'zero';
      }
      if (argValue === 1 && 'one' in expr) {
        return 'one';
      }
      if (argValue === 2 && 'two' in expr) {
        return 'two';
      }
    }

    return selector(argValue);
  }

  function resolveValue(locals, ctx, lang, args, expr, index) {
    if (!expr) {
      return [locals, expr];
    }

    if (typeof expr === 'string' ||
        typeof expr === 'boolean' ||
        typeof expr === 'number') {
      return [locals, expr];
    }

    if (Array.isArray(expr)) {
      return interpolate(locals, ctx, lang, args, expr);
    }

    // otherwise, it's a dict
    if (index) {
      // try to use the index in order to select the right dict member
      const selector = resolveSelector(ctx, lang, args, expr, index);
      if (selector in expr) {
        return resolveValue(locals, ctx, lang, args, expr[selector]);
      }
    }

    // if there was no index or no selector was found, try the default
    // XXX 'other' is an artifact from Gaia
    const defaultKey = expr.__default || 'other';
    if (defaultKey in expr) {
      return resolveValue(locals, ctx, lang, args, expr[defaultKey]);
    }

    throw new L10nError('Unresolvable value');
  }

  /*eslint no-magic-numbers: [0]*/

  const locales2rules = {
    'af': 3,
    'ak': 4,
    'am': 4,
    'ar': 1,
    'asa': 3,
    'az': 0,
    'be': 11,
    'bem': 3,
    'bez': 3,
    'bg': 3,
    'bh': 4,
    'bm': 0,
    'bn': 3,
    'bo': 0,
    'br': 20,
    'brx': 3,
    'bs': 11,
    'ca': 3,
    'cgg': 3,
    'chr': 3,
    'cs': 12,
    'cy': 17,
    'da': 3,
    'de': 3,
    'dv': 3,
    'dz': 0,
    'ee': 3,
    'el': 3,
    'en': 3,
    'eo': 3,
    'es': 3,
    'et': 3,
    'eu': 3,
    'fa': 0,
    'ff': 5,
    'fi': 3,
    'fil': 4,
    'fo': 3,
    'fr': 5,
    'fur': 3,
    'fy': 3,
    'ga': 8,
    'gd': 24,
    'gl': 3,
    'gsw': 3,
    'gu': 3,
    'guw': 4,
    'gv': 23,
    'ha': 3,
    'haw': 3,
    'he': 2,
    'hi': 4,
    'hr': 11,
    'hu': 0,
    'id': 0,
    'ig': 0,
    'ii': 0,
    'is': 3,
    'it': 3,
    'iu': 7,
    'ja': 0,
    'jmc': 3,
    'jv': 0,
    'ka': 0,
    'kab': 5,
    'kaj': 3,
    'kcg': 3,
    'kde': 0,
    'kea': 0,
    'kk': 3,
    'kl': 3,
    'km': 0,
    'kn': 0,
    'ko': 0,
    'ksb': 3,
    'ksh': 21,
    'ku': 3,
    'kw': 7,
    'lag': 18,
    'lb': 3,
    'lg': 3,
    'ln': 4,
    'lo': 0,
    'lt': 10,
    'lv': 6,
    'mas': 3,
    'mg': 4,
    'mk': 16,
    'ml': 3,
    'mn': 3,
    'mo': 9,
    'mr': 3,
    'ms': 0,
    'mt': 15,
    'my': 0,
    'nah': 3,
    'naq': 7,
    'nb': 3,
    'nd': 3,
    'ne': 3,
    'nl': 3,
    'nn': 3,
    'no': 3,
    'nr': 3,
    'nso': 4,
    'ny': 3,
    'nyn': 3,
    'om': 3,
    'or': 3,
    'pa': 3,
    'pap': 3,
    'pl': 13,
    'ps': 3,
    'pt': 3,
    'rm': 3,
    'ro': 9,
    'rof': 3,
    'ru': 11,
    'rwk': 3,
    'sah': 0,
    'saq': 3,
    'se': 7,
    'seh': 3,
    'ses': 0,
    'sg': 0,
    'sh': 11,
    'shi': 19,
    'sk': 12,
    'sl': 14,
    'sma': 7,
    'smi': 7,
    'smj': 7,
    'smn': 7,
    'sms': 7,
    'sn': 3,
    'so': 3,
    'sq': 3,
    'sr': 11,
    'ss': 3,
    'ssy': 3,
    'st': 3,
    'sv': 3,
    'sw': 3,
    'syr': 3,
    'ta': 3,
    'te': 3,
    'teo': 3,
    'th': 0,
    'ti': 4,
    'tig': 3,
    'tk': 3,
    'tl': 4,
    'tn': 3,
    'to': 0,
    'tr': 0,
    'ts': 3,
    'tzm': 22,
    'uk': 11,
    'ur': 3,
    've': 3,
    'vi': 0,
    'vun': 3,
    'wa': 4,
    'wae': 3,
    'wo': 0,
    'xh': 3,
    'xog': 3,
    'yo': 0,
    'zh': 0,
    'zu': 3
  };

  // utility functions for plural rules methods
  function isIn(n, list) {
    return list.indexOf(n) !== -1;
  }
  function isBetween(n, start, end) {
    return typeof n === typeof start && start <= n && n <= end;
  }

  // list of all plural rules methods:
  // map an integer to the plural form name to use
  const pluralRules = {
    '0': function() {
      return 'other';
    },
    '1': function(n) {
      if ((isBetween((n % 100), 3, 10))) {
        return 'few';
      }
      if (n === 0) {
        return 'zero';
      }
      if ((isBetween((n % 100), 11, 99))) {
        return 'many';
      }
      if (n === 2) {
        return 'two';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '2': function(n) {
      if (n !== 0 && (n % 10) === 0) {
        return 'many';
      }
      if (n === 2) {
        return 'two';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '3': function(n) {
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '4': function(n) {
      if ((isBetween(n, 0, 1))) {
        return 'one';
      }
      return 'other';
    },
    '5': function(n) {
      if ((isBetween(n, 0, 2)) && n !== 2) {
        return 'one';
      }
      return 'other';
    },
    '6': function(n) {
      if (n === 0) {
        return 'zero';
      }
      if ((n % 10) === 1 && (n % 100) !== 11) {
        return 'one';
      }
      return 'other';
    },
    '7': function(n) {
      if (n === 2) {
        return 'two';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '8': function(n) {
      if ((isBetween(n, 3, 6))) {
        return 'few';
      }
      if ((isBetween(n, 7, 10))) {
        return 'many';
      }
      if (n === 2) {
        return 'two';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '9': function(n) {
      if (n === 0 || n !== 1 && (isBetween((n % 100), 1, 19))) {
        return 'few';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '10': function(n) {
      if ((isBetween((n % 10), 2, 9)) && !(isBetween((n % 100), 11, 19))) {
        return 'few';
      }
      if ((n % 10) === 1 && !(isBetween((n % 100), 11, 19))) {
        return 'one';
      }
      return 'other';
    },
    '11': function(n) {
      if ((isBetween((n % 10), 2, 4)) && !(isBetween((n % 100), 12, 14))) {
        return 'few';
      }
      if ((n % 10) === 0 ||
          (isBetween((n % 10), 5, 9)) ||
          (isBetween((n % 100), 11, 14))) {
        return 'many';
      }
      if ((n % 10) === 1 && (n % 100) !== 11) {
        return 'one';
      }
      return 'other';
    },
    '12': function(n) {
      if ((isBetween(n, 2, 4))) {
        return 'few';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '13': function(n) {
      if ((isBetween((n % 10), 2, 4)) && !(isBetween((n % 100), 12, 14))) {
        return 'few';
      }
      if (n !== 1 && (isBetween((n % 10), 0, 1)) ||
          (isBetween((n % 10), 5, 9)) ||
          (isBetween((n % 100), 12, 14))) {
        return 'many';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '14': function(n) {
      if ((isBetween((n % 100), 3, 4))) {
        return 'few';
      }
      if ((n % 100) === 2) {
        return 'two';
      }
      if ((n % 100) === 1) {
        return 'one';
      }
      return 'other';
    },
    '15': function(n) {
      if (n === 0 || (isBetween((n % 100), 2, 10))) {
        return 'few';
      }
      if ((isBetween((n % 100), 11, 19))) {
        return 'many';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '16': function(n) {
      if ((n % 10) === 1 && n !== 11) {
        return 'one';
      }
      return 'other';
    },
    '17': function(n) {
      if (n === 3) {
        return 'few';
      }
      if (n === 0) {
        return 'zero';
      }
      if (n === 6) {
        return 'many';
      }
      if (n === 2) {
        return 'two';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '18': function(n) {
      if (n === 0) {
        return 'zero';
      }
      if ((isBetween(n, 0, 2)) && n !== 0 && n !== 2) {
        return 'one';
      }
      return 'other';
    },
    '19': function(n) {
      if ((isBetween(n, 2, 10))) {
        return 'few';
      }
      if ((isBetween(n, 0, 1))) {
        return 'one';
      }
      return 'other';
    },
    '20': function(n) {
      if ((isBetween((n % 10), 3, 4) || ((n % 10) === 9)) && !(
          isBetween((n % 100), 10, 19) ||
          isBetween((n % 100), 70, 79) ||
          isBetween((n % 100), 90, 99)
          )) {
        return 'few';
      }
      if ((n % 1000000) === 0 && n !== 0) {
        return 'many';
      }
      if ((n % 10) === 2 && !isIn((n % 100), [12, 72, 92])) {
        return 'two';
      }
      if ((n % 10) === 1 && !isIn((n % 100), [11, 71, 91])) {
        return 'one';
      }
      return 'other';
    },
    '21': function(n) {
      if (n === 0) {
        return 'zero';
      }
      if (n === 1) {
        return 'one';
      }
      return 'other';
    },
    '22': function(n) {
      if ((isBetween(n, 0, 1)) || (isBetween(n, 11, 99))) {
        return 'one';
      }
      return 'other';
    },
    '23': function(n) {
      if ((isBetween((n % 10), 1, 2)) || (n % 20) === 0) {
        return 'one';
      }
      return 'other';
    },
    '24': function(n) {
      if ((isBetween(n, 3, 10) || isBetween(n, 13, 19))) {
        return 'few';
      }
      if (isIn(n, [2, 12])) {
        return 'two';
      }
      if (isIn(n, [1, 11])) {
        return 'one';
      }
      return 'other';
    }
  };

  function getPluralRule(code) {
    // return a function that gives the plural form name for a given integer
    const index = locales2rules[code.replace(/-.*$/, '')];
    if (!(index in pluralRules)) {
      return () => 'other';
    }
    return pluralRules[index];
  }

  // Safari 9 and iOS 9 does not support Intl
  const L20nIntl = typeof Intl !== 'undefined' ?
    Intl : {
      NumberFormat: function() {
        return {
          format: function(v) {
            return v;
          }
        };
      }
    };

  class Context {
    constructor(env, langs, resIds) {
      this.langs = langs;
      this.resIds = resIds;
      this.env = env;
      this.emit = (type, evt) => env.emit(type, evt, this);
    }

    _formatTuple(lang, args, entity, id, key) {
      try {
        return format(this, lang, args, entity);
      } catch (err) {
        err.id = key ? id + '::' + key : id;
        err.lang = lang;
        this.emit('resolveerror', err);
        return [{ error: err }, err.id];
      }
    }

    _formatEntity(lang, args, entity, id) {
      const [, value] = this._formatTuple(lang, args, entity, id);

      const formatted = {
        value,
        attrs: null,
      };

      if (entity.attrs) {
        formatted.attrs = Object.create(null);
        for (let key in entity.attrs) {
          /* jshint -W089 */
          const [, attrValue] = this._formatTuple(
            lang, args, entity.attrs[key], id, key);
          formatted.attrs[key] = attrValue;
        }
      }

      return formatted;
    }

    _formatValue(lang, args, entity, id) {
      return this._formatTuple(lang, args, entity, id)[1];
    }

    fetch(langs = this.langs) {
      if (langs.length === 0) {
        return Promise.resolve(langs);
      }

      return Promise.all(
        this.resIds.map(
          resId => this.env._getResource(langs[0], resId))
      ).then(() => langs);
    }

    _resolve(langs, keys, formatter, prevResolved) {
      const lang = langs[0];

      if (!lang) {
        return reportMissing.call(this, keys, formatter, prevResolved);
      }

      let hasUnresolved = false;

      const resolved = keys.map((key, i) => {
        if (prevResolved && prevResolved[i] !== undefined) {
          return prevResolved[i];
        }
        const [id, args] = Array.isArray(key) ?
          key : [key, undefined];
        const entity = this._getEntity(lang, id);

        if (entity) {
          return formatter.call(this, lang, args, entity, id);
        }

        this.emit('notfounderror',
          new L10nError('"' + id + '" not found in ' + lang.code, id, lang));
        hasUnresolved = true;
      });

      if (!hasUnresolved) {
        return resolved;
      }

      return this.fetch(langs.slice(1)).then(
        nextLangs => this._resolve(nextLangs, keys, formatter, resolved));
    }

    formatEntities(...keys) {
      return this.fetch().then(
        langs => this._resolve(langs, keys, this._formatEntity));
    }

    formatValues(...keys) {
      return this.fetch().then(
        langs => this._resolve(langs, keys, this._formatValue));
    }

    _getEntity(lang, id) {
      const cache = this.env.resCache;

      // Look for `id` in every resource in order.
      for (let i = 0, resId; resId = this.resIds[i]; i++) {
        const resource = cache.get(resId + lang.code + lang.src);
        if (resource instanceof L10nError) {
          continue;
        }
        if (id in resource) {
          return resource[id];
        }
      }
      return undefined;
    }

    _getNumberFormatter(lang) {
      if (!this.env.numberFormatters) {
        this.env.numberFormatters = new Map();
      }
      if (!this.env.numberFormatters.has(lang)) {
        const formatter = L20nIntl.NumberFormat(lang);
        this.env.numberFormatters.set(lang, formatter);
        return formatter;
      }
      return this.env.numberFormatters.get(lang);
    }

    // XXX in the future macros will be stored in localization resources together 
    // with regular entities and this method will not be needed anymore
    _getMacro(lang, id) {
      switch(id) {
        case 'plural':
          return getPluralRule(lang.code);
        default:
          return undefined;
      }
    }

  }

  function reportMissing(keys, formatter, resolved) {
    const missingIds = new Set();

    keys.forEach((key, i) => {
      if (resolved && resolved[i] !== undefined) {
        return;
      }
      const id = Array.isArray(key) ? key[0] : key;
      missingIds.add(id);
      resolved[i] = formatter === this._formatValue ?
        id : {value: id, attrs: null};
    });

    this.emit('notfounderror', new L10nError(
      '"' + Array.from(missingIds).join(', ') + '"' +
      ' not found in any language', missingIds));

    return resolved;
  }

  const MAX_PLACEABLES = 100;

  var PropertiesParser = {
    patterns: null,
    entryIds: null,
    emit: null,

    init: function() {
      this.patterns = {
        comment: /^\s*#|^\s*$/,
        entity: /^([^=\s]+)\s*=\s*(.*)$/,
        multiline: /[^\\]\\$/,
        index: /\{\[\s*(\w+)(?:\(([^\)]*)\))?\s*\]\}/i,
        unicode: /\\u([0-9a-fA-F]{1,4})/g,
        entries: /[^\r\n]+/g,
        controlChars: /\\([\\\n\r\t\b\f\{\}\"\'])/g,
        placeables: /\{\{\s*([^\s]*?)\s*\}\}/,
      };
    },

    parse: function(emit, source) {
      if (!this.patterns) {
        this.init();
      }
      this.emit = emit;

      const entries = {};

      const lines = source.match(this.patterns.entries);
      if (!lines) {
        return entries;
      }
      for (let i = 0; i < lines.length; i++) {
        let line = lines[i];

        if (this.patterns.comment.test(line)) {
          continue;
        }

        while (this.patterns.multiline.test(line) && i < lines.length) {
          line = line.slice(0, -1) + lines[++i].trim();
        }

        const entityMatch = line.match(this.patterns.entity);
        if (entityMatch) {
          try {
            this.parseEntity(entityMatch[1], entityMatch[2], entries);
          } catch (e) {
            if (!this.emit) {
              throw e;
            }
          }
        }
      }
      return entries;
    },

    parseEntity: function(id, value, entries) {
      let name, key;

      const pos = id.indexOf('[');
      if (pos !== -1) {
        name = id.substr(0, pos);
        key = id.substring(pos + 1, id.length - 1);
      } else {
        name = id;
        key = null;
      }

      const nameElements = name.split('.');

      if (nameElements.length > 2) {
        throw this.error('Error in ID: "' + name + '".' +
            ' Nested attributes are not supported.');
      }

      let attr;
      if (nameElements.length > 1) {
        name = nameElements[0];
        attr = nameElements[1];

        if (attr[0] === '$') {
          throw this.error('Attribute can\'t start with "$"');
        }
      } else {
        attr = null;
      }

      this.setEntityValue(name, attr, key, this.unescapeString(value), entries);
    },

    setEntityValue: function(id, attr, key, rawValue, entries) {
      const value = rawValue.indexOf('{{') > -1 ?
        this.parseString(rawValue) : rawValue;

      let isSimpleValue = typeof value === 'string';
      let root = entries;

      let isSimpleNode = typeof entries[id] === 'string';

      if (!entries[id] && (attr || key || !isSimpleValue)) {
        entries[id] = Object.create(null);
        isSimpleNode = false;
      }

      if (attr) {
        if (isSimpleNode) {
          const val = entries[id];
          entries[id] = Object.create(null);
          entries[id].value = val;
        }
        if (!entries[id].attrs) {
          entries[id].attrs = Object.create(null);
        }
        if (!entries[id].attrs && !isSimpleValue) {
          entries[id].attrs[attr] = Object.create(null);
        }
        root = entries[id].attrs;
        id = attr;
      }

      if (key) {
        isSimpleNode = false;
        if (typeof root[id] === 'string') {
          const val = root[id];
          root[id] = Object.create(null);
          root[id].index = this.parseIndex(val);
          root[id].value = Object.create(null);
        }
        root = root[id].value;
        id = key;
        isSimpleValue = true;
      }

      if (isSimpleValue) {
        if (id in root) {
          throw this.error('Duplicated id: ' + id);
        }
        root[id] = value;
      } else {
        if (!root[id]) {
          root[id] = Object.create(null);
        }
        root[id].value = value;
      }
    },

    parseString: function(str) {
      const chunks = str.split(this.patterns.placeables);
      const complexStr = [];

      const len = chunks.length;
      const placeablesCount = (len - 1) / 2;

      if (placeablesCount >= MAX_PLACEABLES) {
        throw this.error('Too many placeables (' + placeablesCount +
                            ', max allowed is ' + MAX_PLACEABLES + ')');
      }

      for (let i = 0; i < chunks.length; i++) {
        if (chunks[i].length === 0) {
          continue;
        }
        if (i % 2 === 1) {
          complexStr.push({type: 'idOrVar', name: chunks[i]});
        } else {
          complexStr.push(chunks[i]);
        }
      }
      return complexStr;
    },

    unescapeString: function(str) {
      if (str.lastIndexOf('\\') !== -1) {
        str = str.replace(this.patterns.controlChars, '$1');
      }
      return str.replace(this.patterns.unicode,
        (match, token) => String.fromCodePoint(parseInt(token, 16))
      );
    },

    parseIndex: function(str) {
      const match = str.match(this.patterns.index);
      if (!match) {
        throw new L10nError('Malformed index');
      }
      if (match[2]) {
        return [{
          type: 'call',
          expr: {
            type: 'prop',
            expr: {
              type: 'glob',
              name: 'cldr'
            },
            prop: 'plural',
            cmpt: false
          }, args: [{
            type: 'idOrVar',
            name: match[2]
          }]
        }];
      } else {
        return [{type: 'idOrVar', name: match[1]}];
      }
    },

    error: function(msg, type = 'parsererror') {
      const err = new L10nError(msg);
      if (this.emit) {
        this.emit(type, err);
      }
      return err;
    }
  };

  const MAX_PLACEABLES$1 = 100;

  var L20nParser = {
    parse: function(emit, string) {
      this._source = string;
      this._index = 0;
      this._length = string.length;
      this.entries = Object.create(null);
      this.emit = emit;

      return this.getResource();
    },

    getResource: function() {
      this.getWS();
      while (this._index < this._length) {
        try {
          this.getEntry();
        } catch (e) {
          if (e instanceof L10nError) {
            // we want to recover, but we don't need it in entries
            this.getJunkEntry();
            if (!this.emit) {
              throw e;
            }
          } else {
            throw e;
          }
        }

        if (this._index < this._length) {
          this.getWS();
        }
      }

      return this.entries;
    },

    getEntry: function() {
      if (this._source[this._index] === '<') {
        ++this._index;
        const id = this.getIdentifier();
        if (this._source[this._index] === '[') {
          ++this._index;
          return this.getEntity(id, this.getItemList(this.getExpression, ']'));
        }
        return this.getEntity(id);
      }

      if (this._source.startsWith('/*', this._index)) {
        return this.getComment();
      }

      throw this.error('Invalid entry');
    },

    getEntity: function(id, index) {
      if (!this.getRequiredWS()) {
        throw this.error('Expected white space');
      }

      const ch = this._source[this._index];
      const hasIndex = index !== undefined;
      const value = this.getValue(ch, hasIndex, hasIndex);
      let attrs;

      if (value === undefined) {
        if (ch === '>') {
          throw this.error('Expected ">"');
        }
        attrs = this.getAttributes();
      } else {
        const ws1 = this.getRequiredWS();
        if (this._source[this._index] !== '>') {
          if (!ws1) {
            throw this.error('Expected ">"');
          }
          attrs = this.getAttributes();
        }
      }

      // skip '>'
      ++this._index;

      if (id in this.entries) {
        throw this.error('Duplicate entry ID "' + id, 'duplicateerror');
      }
      if (!attrs && !index && typeof value === 'string') {
        this.entries[id] = value;
      } else {
        this.entries[id] = {
          value,
          attrs,
          index
        };
      }
    },

    getValue: function(
      ch = this._source[this._index], index = false, required = true) {
      switch (ch) {
        case '\'':
        case '"':
          return this.getString(ch, 1);
        case '{':
          return this.getHash(index);
      }

      if (required) {
        throw this.error('Unknown value type');
      }

      return undefined;
    },

    getWS: function() {
      let cc = this._source.charCodeAt(this._index);
      // space, \n, \t, \r
      while (cc === 32 || cc === 10 || cc === 9 || cc === 13) {
        cc = this._source.charCodeAt(++this._index);
      }
    },

    getRequiredWS: function() {
      const pos = this._index;
      let cc = this._source.charCodeAt(pos);
      // space, \n, \t, \r
      while (cc === 32 || cc === 10 || cc === 9 || cc === 13) {
        cc = this._source.charCodeAt(++this._index);
      }
      return this._index !== pos;
    },

    getIdentifier: function() {
      const start = this._index;
      let cc = this._source.charCodeAt(this._index);

      if ((cc >= 97 && cc <= 122) || // a-z
          (cc >= 65 && cc <= 90) ||  // A-Z
          cc === 95) {               // _
        cc = this._source.charCodeAt(++this._index);
      } else {
        throw this.error('Identifier has to start with [a-zA-Z_]');
      }

      while ((cc >= 97 && cc <= 122) || // a-z
             (cc >= 65 && cc <= 90) ||  // A-Z
             (cc >= 48 && cc <= 57) ||  // 0-9
             cc === 95) {               // _
        cc = this._source.charCodeAt(++this._index);
      }

      return this._source.slice(start, this._index);
    },

    getUnicodeChar: function() {
      for (let i = 0; i < 4; i++) {
        const cc = this._source.charCodeAt(++this._index);
        if ((cc > 96 && cc < 103) || // a-f
            (cc > 64 && cc < 71) ||  // A-F
            (cc > 47 && cc < 58)) {  // 0-9
          continue;
        }
        throw this.error('Illegal unicode escape sequence');
      }
      this._index++;
      return String.fromCharCode(
        parseInt(this._source.slice(this._index - 4, this._index), 16));
    },

    stringRe: /"|'|{{|\\/g,
    getString: function(opchar, opcharLen) {
      const body = [];
      let placeables = 0;

      this._index += opcharLen;
      const start = this._index;

      let bufStart = start;
      let buf = '';

      while (true) {
        this.stringRe.lastIndex = this._index;
        const match = this.stringRe.exec(this._source);

        if (!match) {
          throw this.error('Unclosed string literal');
        }

        if (match[0] === '"' || match[0] === '\'') {
          if (match[0] !== opchar) {
            this._index += opcharLen;
            continue;
          }
          this._index = match.index + opcharLen;
          break;
        }

        if (match[0] === '{{') {
          if (placeables > MAX_PLACEABLES$1 - 1) {
            throw this.error('Too many placeables, maximum allowed is ' +
                MAX_PLACEABLES$1);
          }
          placeables++;
          if (match.index > bufStart || buf.length > 0) {
            body.push(buf + this._source.slice(bufStart, match.index));
            buf = '';
          }
          this._index = match.index + 2;
          this.getWS();
          body.push(this.getExpression());
          this.getWS();
          this._index += 2;
          bufStart = this._index;
          continue;
        }

        if (match[0] === '\\') {
          this._index = match.index + 1;
          const ch2 = this._source[this._index];
          if (ch2 === 'u') {
            buf += this._source.slice(bufStart, match.index) +
              this.getUnicodeChar();
          } else if (ch2 === opchar || ch2 === '\\') {
            buf += this._source.slice(bufStart, match.index) + ch2;
            this._index++;
          } else if (this._source.startsWith('{{', this._index)) {
            buf += this._source.slice(bufStart, match.index) + '{{';
            this._index += 2;
          } else {
            throw this.error('Illegal escape sequence');
          }
          bufStart = this._index;
        }
      }

      if (body.length === 0) {
        return buf + this._source.slice(bufStart, this._index - opcharLen);
      }

      if (this._index - opcharLen > bufStart || buf.length > 0) {
        body.push(buf + this._source.slice(bufStart, this._index - opcharLen));
      }

      return body;
    },

    getAttributes: function() {
      const attrs = Object.create(null);

      while (true) {
        this.getAttribute(attrs);
        const ws1 = this.getRequiredWS();
        const ch = this._source.charAt(this._index);
        if (ch === '>') {
          break;
        } else if (!ws1) {
          throw this.error('Expected ">"');
        }
      }
      return attrs;
    },

    getAttribute: function(attrs) {
      const key = this.getIdentifier();
      let index;

      if (this._source[this._index]=== '[') {
        ++this._index;
        this.getWS();
        index = this.getItemList(this.getExpression, ']');
      }
      this.getWS();
      if (this._source[this._index] !== ':') {
        throw this.error('Expected ":"');
      }
      ++this._index;
      this.getWS();
      const hasIndex = index !== undefined;
      const value = this.getValue(undefined, hasIndex);

      if (key in attrs) {
        throw this.error('Duplicate attribute "' + key, 'duplicateerror');
      }

      if (!index && typeof value === 'string') {
        attrs[key] = value;
      } else {
        attrs[key] = {
          value,
          index
        };
      }
    },

    getHash: function(index) {
      const items = Object.create(null);

      ++this._index;
      this.getWS();

      let defKey;

      while (true) {
        const [key, value, def] = this.getHashItem();
        items[key] = value;

        if (def) {
          if (defKey) {
            throw this.error('Default item redefinition forbidden');
          }
          defKey = key;
        }
        this.getWS();

        const comma = this._source[this._index] === ',';
        if (comma) {
          ++this._index;
          this.getWS();
        }
        if (this._source[this._index] === '}') {
          ++this._index;
          break;
        }
        if (!comma) {
          throw this.error('Expected "}"');
        }
      }

      if (defKey) {
        items.__default = defKey;
      } else if (!index) {
        throw this.error('Unresolvable Hash Value');
      }

      return items;
    },

    getHashItem: function() {
      let defItem = false;
      if (this._source[this._index] === '*') {
        ++this._index;
        defItem = true;
      }

      const key = this.getIdentifier();
      this.getWS();
      if (this._source[this._index] !== ':') {
        throw this.error('Expected ":"');
      }
      ++this._index;
      this.getWS();

      return [key, this.getValue(), defItem];
    },

    getComment: function() {
      this._index += 2;
      const start = this._index;
      const end = this._source.indexOf('*/', start);

      if (end === -1) {
        throw this.error('Comment without a closing tag');
      }

      this._index = end + 2;
    },

    getExpression: function () {
      let exp = this.getPrimaryExpression();

      while (true) {
        const ch = this._source[this._index];
        if (ch === '.' || ch === '[') {
          ++this._index;
          exp = this.getPropertyExpression(exp, ch === '[');
        } else if (ch === '(') {
          ++this._index;
          exp = this.getCallExpression(exp);
        } else {
          break;
        }
      }

      return exp;
    },

    getPropertyExpression: function(idref, computed) {
      let exp;

      if (computed) {
        this.getWS();
        exp = this.getExpression();
        this.getWS();
        if (this._source[this._index] !== ']') {
          throw this.error('Expected "]"');
        }
        ++this._index;
      } else {
        exp = this.getIdentifier();
      }

      return {
        type: 'prop',
        expr: idref,
        prop: exp,
        cmpt: computed
      };
    },

    getCallExpression: function(callee) {
      this.getWS();

      return {
        type: 'call',
        expr: callee,
        args: this.getItemList(this.getExpression, ')')
      };
    },

    getPrimaryExpression: function() {
      const ch = this._source[this._index];

      switch (ch) {
        case '$':
          ++this._index;
          return {
            type: 'var',
            name: this.getIdentifier()
          };
        case '@':
          ++this._index;
          return {
            type: 'glob',
            name: this.getIdentifier()
          };
        default:
          return {
            type: 'id',
            name: this.getIdentifier()
          };
      }
    },

    getItemList: function(callback, closeChar) {
      const items = [];
      let closed = false;

      this.getWS();

      if (this._source[this._index] === closeChar) {
        ++this._index;
        closed = true;
      }

      while (!closed) {
        items.push(callback.call(this));
        this.getWS();
        const ch = this._source.charAt(this._index);
        switch (ch) {
          case ',':
            ++this._index;
            this.getWS();
            break;
          case closeChar:
            ++this._index;
            closed = true;
            break;
          default:
            throw this.error('Expected "," or "' + closeChar + '"');
        }
      }

      return items;
    },


    getJunkEntry: function() {
      const pos = this._index;
      let nextEntity = this._source.indexOf('<', pos);
      let nextComment = this._source.indexOf('/*', pos);

      if (nextEntity === -1) {
        nextEntity = this._length;
      }
      if (nextComment === -1) {
        nextComment = this._length;
      }

      const nextEntry = Math.min(nextEntity, nextComment);

      this._index = nextEntry;
    },

    error: function(message, type = 'parsererror') {
      const pos = this._index;

      let start = this._source.lastIndexOf('<', pos - 1);
      const lastClose = this._source.lastIndexOf('>', pos - 1);
      start = lastClose > start ? lastClose + 1 : start;
      const context = this._source.slice(start, pos + 10);

      const msg = message + ' at pos ' + pos + ': `' + context + '`';
      const err = new L10nError(msg);
      if (this.emit) {
        this.emit(type, err);
      }
      return err;
    },
  };

  // Walk an entry node searching for content leaves
  function walkEntry(entry, fn) {
    if (typeof entry === 'string') {
      return fn(entry);
    }

    const newEntry = Object.create(null);

    if (entry.value) {
      newEntry.value = walkValue(entry.value, fn);
    }

    if (entry.index) {
      newEntry.index = entry.index;
    }

    if (entry.attrs) {
      newEntry.attrs = Object.create(null);
      for (let key in entry.attrs) {
        newEntry.attrs[key] = walkEntry(entry.attrs[key], fn);
      }
    }

    return newEntry;
  }

  function walkValue(value, fn) {
    if (typeof value === 'string') {
      return fn(value);
    }

    // skip expressions in placeables
    if (value.type) {
      return value;
    }

    const newValue = Array.isArray(value) ? [] : Object.create(null);
    const keys = Object.keys(value);

    for (let i = 0, key; (key = keys[i]); i++) {
      newValue[key] = walkValue(value[key], fn);
    }

    return newValue;
  }

  /* Pseudolocalizations
   *
   * pseudo is a dict of strategies to be used to modify the English
   * context in order to create pseudolocalizations.  These can be used by
   * developers to test the localizability of their code without having to
   * actually speak a foreign language.
   *
   * Currently, the following pseudolocales are supported:
   *
   *   fr-x-psaccent - Ȧȧƈƈḗḗƞŧḗḗḓ Ḗḗƞɠŀīīşħ
   *
   *     In Accented English all English letters are replaced by accented
   *     Unicode counterparts which don't impair the readability of the content.
   *     This allows developers to quickly test if any given string is being
   *     correctly displayed in its 'translated' form.  Additionally, simple
   *     heuristics are used to make certain words longer to better simulate the
   *     experience of international users.
   *
   *   ar-x-psbidi - ɥsıʅƃuƎ ıpıԐ
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
   * See https://bugzil.la/900182 for more information.
   *
   */

  function createGetter(id, name) {
    let _pseudo = null;

    return function getPseudo() {
      if (_pseudo) {
        return _pseudo;
      }

      const reAlphas = /[a-zA-Z]/g;
      const reVowels = /[aeiouAEIOU]/g;
      const reWords = /[^\W0-9_]+/g;
      // strftime tokens (%a, %Eb), template {vars}, HTML entities (&#x202a;)
      // and HTML tags.
      const reExcluded = /(%[EO]?\w|\{\s*.+?\s*\}|&[#\w]+;|<\s*.+?\s*>)/;

      const charMaps = {
        'fr-x-psaccent':
          'ȦƁƇḒḖƑƓĦĪĴĶĿḾȠǾƤɊŘŞŦŬṼẆẊẎẐ[\\]^_`ȧƀƈḓḗƒɠħīĵķŀḿƞǿƥɋřşŧŭṽẇẋẏẑ',
        'ar-x-psbidi':
          // XXX Use pɟפ˥ʎ as replacements for ᗡℲ⅁⅂⅄. https://bugzil.la/1007340
          '∀ԐↃpƎɟפHIſӼ˥WNOԀÒᴚS⊥∩ɅＭXʎZ[\\]ᵥ_,ɐqɔpǝɟƃɥıɾʞʅɯuodbɹsʇnʌʍxʎz',
      };

      const mods = {
        'fr-x-psaccent': val =>
          val.replace(reVowels, match => match + match.toLowerCase()),

        // Surround each word with Unicode formatting codes, RLO and PDF:
        //   U+202E:   RIGHT-TO-LEFT OVERRIDE (RLO)
        //   U+202C:   POP DIRECTIONAL FORMATTING (PDF)
        // See http://www.w3.org/International/questions/qa-bidi-controls
        'ar-x-psbidi': val =>
          val.replace(reWords, match => '\u202e' + match + '\u202c'),
      };

      // Replace each Latin letter with a Unicode character from map
      const ASCII_LETTER_A = 65;
      const replaceChars =
        (map, val) => val.replace(
          reAlphas, match => map.charAt(match.charCodeAt(0) - ASCII_LETTER_A));

      const transform =
        val => replaceChars(charMaps[id], mods[id](val));

      // apply fn to translatable parts of val
      const apply = (fn, val) => {
        if (!val) {
          return val;
        }

        const parts = val.split(reExcluded);
        const modified = parts.map((part) => {
          if (reExcluded.test(part)) {
            return part;
          }
          return fn(part);
        });
        return modified.join('');
      };

      return _pseudo = {
        name: transform(name),
        process: str => apply(transform, str)
      };
    };
  }

  const pseudo = Object.defineProperties(Object.create(null), {
    'fr-x-psaccent': {
      enumerable: true,
      get: createGetter('fr-x-psaccent', 'Runtime Accented')
    },
    'ar-x-psbidi': {
      enumerable: true,
      get: createGetter('ar-x-psbidi', 'Runtime Bidi')
    }
  });

  class Env {
    constructor(fetchResource) {
      this.fetchResource = fetchResource;

      this.resCache = new Map();
      this.resRefs = new Map();
      this.numberFormatters = null;
      this.parsers = {
        properties: PropertiesParser,
        l20n: L20nParser,
      };

      const listeners = {};
      this.emit = emit.bind(this, listeners);
      this.addEventListener = addEventListener.bind(this, listeners);
      this.removeEventListener = removeEventListener.bind(this, listeners);
    }

    createContext(langs, resIds) {
      const ctx = new Context(this, langs, resIds);
      resIds.forEach(resId => {
        const usedBy = this.resRefs.get(resId) || 0;
        this.resRefs.set(resId, usedBy + 1);
      });

      return ctx;
    }

    destroyContext(ctx) {
      ctx.resIds.forEach(resId => {
        const usedBy = this.resRefs.get(resId) || 0;

        if (usedBy > 1) {
          return this.resRefs.set(resId, usedBy - 1);
        }

        this.resRefs.delete(resId);
        this.resCache.forEach((val, key) =>
          key.startsWith(resId) ? this.resCache.delete(key) : null);
      });
    }

    _parse(syntax, lang, data) {
      const parser = this.parsers[syntax];
      if (!parser) {
        return data;
      }

      const emitAndAmend = (type, err) => this.emit(type, amendError(lang, err));
      return parser.parse(emitAndAmend, data);
    }

    _create(lang, entries) {
      if (lang.src !== 'pseudo') {
        return entries;
      }

      const pseudoentries = Object.create(null);
      for (let key in entries) {
        pseudoentries[key] = walkEntry(
          entries[key], pseudo[lang.code].process);
      }
      return pseudoentries;
    }

    _getResource(lang, res) {
      const cache = this.resCache;
      const id = res + lang.code + lang.src;

      if (cache.has(id)) {
        return cache.get(id);
      }

      const syntax = res.substr(res.lastIndexOf('.') + 1);

      const saveEntries = data => {
        const entries = this._parse(syntax, lang, data);
        cache.set(id, this._create(lang, entries));
      };

      const recover = err => {
        err.lang = lang;
        this.emit('fetcherror', err);
        cache.set(id, err);
      };

      const langToFetch = lang.src === 'pseudo' ?
        { code: 'en-US', src: 'app', ver: lang.ver } :
        lang;

      const resource = this.fetchResource(res, langToFetch)
        .then(saveEntries, recover);

      cache.set(id, resource);

      return resource;
    }
  }

  function amendError(lang, err) {
    err.lang = lang;
    return err;
  }

  function prioritizeLocales(def, availableLangs, requested) {
    let supportedLocale;
    // Find the first locale in the requested list that is supported.
    for (let i = 0; i < requested.length; i++) {
      const locale = requested[i];
      if (availableLangs.indexOf(locale) !== -1) {
        supportedLocale = locale;
        break;
      }
    }
    if (!supportedLocale ||
        supportedLocale === def) {
      return [def];
    }

    return [supportedLocale, def];
  }

  function negotiateLanguages(
    { appVersion, defaultLang, availableLangs }, additionalLangs, prevLangs,
    requestedLangs) {

    const allAvailableLangs = Object.keys(availableLangs)
      .concat(Object.keys(additionalLangs))
      .concat(Object.keys(pseudo));
    const newLangs = prioritizeLocales(
      defaultLang, allAvailableLangs, requestedLangs);

    const langs = newLangs.map(code => ({
      code: code,
      src: getLangSource(appVersion, availableLangs, additionalLangs, code),
      ver: appVersion,
    }));

    return { langs, haveChanged: !arrEqual(prevLangs, newLangs) };
  }

  function arrEqual(arr1, arr2) {
    return arr1.length === arr2.length &&
      arr1.every((elem, i) => elem === arr2[i]);
  }

  function getMatchingLangpack(appVersion, langpacks) {
    for (let i = 0, langpack; (langpack = langpacks[i]); i++) {
      if (langpack.target === appVersion) {
        return langpack;
      }
    }
    return null;
  }

  function getLangSource(appVersion, availableLangs, additionalLangs, code) {
    if (additionalLangs && additionalLangs[code]) {
      const lp = getMatchingLangpack(appVersion, additionalLangs[code]);
      if (lp &&
          (!(code in availableLangs) ||
           parseInt(lp.revision) > availableLangs[code])) {
        return 'extra';
      }
    }

    if ((code in pseudo) && !(code in availableLangs)) {
      return 'pseudo';
    }

    return 'app';
  }

  class Remote {
    constructor(fetchResource, broadcast) {
      this.broadcast = broadcast;
      this.env = new Env(fetchResource);
      this.ctxs = new Map();
    }

    registerView(view, resources, meta, additionalLangs, requestedLangs) {
      const { langs } = negotiateLanguages(
        meta, additionalLangs, [], requestedLangs);
      this.ctxs.set(view, this.env.createContext(langs, resources));
      return langs;
    }

    unregisterView(view) {
      this.ctxs.delete(view);
      return true;
    }

    formatEntities(view, keys) {
      return this.ctxs.get(view).formatEntities(...keys);
    }

    formatValues(view, keys) {
      return this.ctxs.get(view).formatValues(...keys);
    }

    changeLanguages(view, meta, additionalLangs, requestedLangs) {
      const oldCtx = this.ctxs.get(view);
      const prevLangs = oldCtx.langs;
      const newLangs = negotiateLanguages(
        meta, additionalLangs, prevLangs, requestedLangs);
      this.ctxs.set(view, this.env.createContext(
        newLangs.langs, oldCtx.resIds));
      return newLangs;
    }

    requestLanguages(requestedLangs) {
      this.broadcast('languageschangerequest', requestedLangs);
    }

    getName(code) {
      return pseudo[code].name;
    }

    processString(code, str) {
      return pseudo[code].process(str);
    }
  }

  const observerConfig = {
    attributes: true,
    characterData: false,
    childList: true,
    subtree: true,
    attributeFilter: ['data-l10n-id', 'data-l10n-args']
  };

  const observers = new WeakMap();

  function initMutationObserver(view) {
    observers.set(view, {
      roots: new Set(),
      observer: new MutationObserver(
        mutations => translateMutations(view, mutations)),
    });
  }

  function translateRoots(view) {
    const roots = Array.from(observers.get(view).roots);
    return Promise.all(roots.map(
        root => translateFragment(view, root)));
  }

  function observe(view, root) {
    const obs = observers.get(view);
    if (obs) {
      obs.roots.add(root);
      obs.observer.observe(root, observerConfig);
    }
  }

  function disconnect(view, root, allRoots) {
    const obs = observers.get(view);
    if (obs) {
      obs.observer.disconnect();
      if (allRoots) {
        return;
      }
      obs.roots.delete(root);
      obs.roots.forEach(
        other => obs.observer.observe(other, observerConfig));
    }
  }

  function reconnect(view) {
    const obs = observers.get(view);
    if (obs) {
      obs.roots.forEach(
        root => obs.observer.observe(root, observerConfig));
    }
  }

  // match the opening angle bracket (<) in HTML tags, and HTML entities like
  // &amp;, &#0038;, &#x0026;.
  const reOverlay = /<|&#?\w+;/;

  const allowed = {
    elements: [
      'a', 'em', 'strong', 'small', 's', 'cite', 'q', 'dfn', 'abbr', 'data',
      'time', 'code', 'var', 'samp', 'kbd', 'sub', 'sup', 'i', 'b', 'u',
      'mark', 'ruby', 'rt', 'rp', 'bdi', 'bdo', 'span', 'br', 'wbr'
    ],
    attributes: {
      global: ['title', 'aria-label', 'aria-valuetext', 'aria-moz-hint'],
      a: ['download'],
      area: ['download', 'alt'],
      // value is special-cased in isAttrAllowed
      input: ['alt', 'placeholder'],
      menuitem: ['label'],
      menu: ['label'],
      optgroup: ['label'],
      option: ['label'],
      track: ['label'],
      img: ['alt'],
      textarea: ['placeholder'],
      th: ['abbr']
    }
  };

  function overlayElement(element, translation) {
    const value = translation.value;

    if (typeof value === 'string') {
      if (!reOverlay.test(value)) {
        element.textContent = value;
      } else {
        // start with an inert template element and move its children into
        // `element` but such that `element`'s own children are not replaced
        const tmpl = element.ownerDocument.createElement('template');
        tmpl.innerHTML = value;
        // overlay the node with the DocumentFragment
        overlay(element, tmpl.content);
      }
    }

    for (let key in translation.attrs) {
      const attrName = camelCaseToDashed(key);
      if (isAttrAllowed({ name: attrName }, element)) {
        element.setAttribute(attrName, translation.attrs[key]);
      }
    }
  }

  // The goal of overlay is to move the children of `translationElement`
  // into `sourceElement` such that `sourceElement`'s own children are not
  // replaced, but onle have their text nodes and their attributes modified.
  //
  // We want to make it possible for localizers to apply text-level semantics to
  // the translations and make use of HTML entities. At the same time, we
  // don't trust translations so we need to filter unsafe elements and
  // attribtues out and we don't want to break the Web by replacing elements to
  // which third-party code might have created references (e.g. two-way
  // bindings in MVC frameworks).
  function overlay(sourceElement, translationElement) {
    const result = translationElement.ownerDocument.createDocumentFragment();
    let k, attr;

    // take one node from translationElement at a time and check it against
    // the allowed list or try to match it with a corresponding element
    // in the source
    let childElement;
    while ((childElement = translationElement.childNodes[0])) {
      translationElement.removeChild(childElement);

      if (childElement.nodeType === childElement.TEXT_NODE) {
        result.appendChild(childElement);
        continue;
      }

      const index = getIndexOfType(childElement);
      const sourceChild = getNthElementOfType(sourceElement, childElement, index);
      if (sourceChild) {
        // there is a corresponding element in the source, let's use it
        overlay(sourceChild, childElement);
        result.appendChild(sourceChild);
        continue;
      }

      if (isElementAllowed(childElement)) {
        const sanitizedChild = childElement.ownerDocument.createElement(
          childElement.nodeName);
        overlay(sanitizedChild, childElement);
        result.appendChild(sanitizedChild);
        continue;
      }

      // otherwise just take this child's textContent
      result.appendChild(
        translationElement.ownerDocument.createTextNode(
          childElement.textContent));
    }

    // clear `sourceElement` and append `result` which by this time contains
    // `sourceElement`'s original children, overlayed with translation
    sourceElement.textContent = '';
    sourceElement.appendChild(result);

    // if we're overlaying a nested element, translate the allowed
    // attributes; top-level attributes are handled in `translateElement`
    // XXX attributes previously set here for another language should be
    // cleared if a new language doesn't use them; https://bugzil.la/922577
    if (translationElement.attributes) {
      for (k = 0, attr; (attr = translationElement.attributes[k]); k++) {
        if (isAttrAllowed(attr, sourceElement)) {
          sourceElement.setAttribute(attr.name, attr.value);
        }
      }
    }
  }

  // XXX the allowed list should be amendable; https://bugzil.la/922573
  function isElementAllowed(element) {
    return allowed.elements.indexOf(element.tagName.toLowerCase()) !== -1;
  }

  function isAttrAllowed(attr, element) {
    const attrName = attr.name.toLowerCase();
    const tagName = element.tagName.toLowerCase();
    // is it a globally safe attribute?
    if (allowed.attributes.global.indexOf(attrName) !== -1) {
      return true;
    }
    // are there no allowed attributes for this element?
    if (!allowed.attributes[tagName]) {
      return false;
    }
    // is it allowed on this element?
    // XXX the allowed list should be amendable; https://bugzil.la/922573
    if (allowed.attributes[tagName].indexOf(attrName) !== -1) {
      return true;
    }
    // special case for value on inputs with type button, reset, submit
    if (tagName === 'input' && attrName === 'value') {
      const type = element.type.toLowerCase();
      if (type === 'submit' || type === 'button' || type === 'reset') {
        return true;
      }
    }
    return false;
  }

  // Get n-th immediate child of context that is of the same type as element.
  // XXX Use querySelector(':scope > ELEMENT:nth-of-type(index)'), when:
  // 1) :scope is widely supported in more browsers and 2) it works with
  // DocumentFragments.
  function getNthElementOfType(context, element, index) {
    /* jshint boss:true */
    let nthOfType = 0;
    for (let i = 0, child; child = context.children[i]; i++) {
      if (child.nodeType === child.ELEMENT_NODE &&
          child.tagName === element.tagName) {
        if (nthOfType === index) {
          return child;
        }
        nthOfType++;
      }
    }
    return null;
  }

  // Get the index of the element among siblings of the same type.
  function getIndexOfType(element) {
    let index = 0;
    let child;
    while ((child = element.previousElementSibling)) {
      if (child.tagName === element.tagName) {
        index++;
      }
    }
    return index;
  }

  function camelCaseToDashed(string) {
    // XXX workaround for https://bugzil.la/1141934
    if (string === 'ariaValueText') {
      return 'aria-valuetext';
    }

    return string
      .replace(/[A-Z]/g, match => '-' + match.toLowerCase())
      .replace(/^-/, '');
  }

  const reHtml = /[&<>]/g;
  const htmlEntities = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
  };

  function setAttributes(element, id, args) {
    element.setAttribute('data-l10n-id', id);
    if (args) {
      element.setAttribute('data-l10n-args', JSON.stringify(args));
    }
  }

  function getAttributes(element) {
    return {
      id: element.getAttribute('data-l10n-id'),
      args: JSON.parse(element.getAttribute('data-l10n-args'))
    };
  }

  function getTranslatables(element) {
    const nodes = Array.from(element.querySelectorAll('[data-l10n-id]'));

    if (typeof element.hasAttribute === 'function' &&
        element.hasAttribute('data-l10n-id')) {
      nodes.push(element);
    }

    return nodes;
  }

  function translateMutations(view, mutations) {
    const targets = new Set();

    for (let mutation of mutations) {
      switch (mutation.type) {
        case 'attributes':
          targets.add(mutation.target);
          break;
        case 'childList':
          for (let addedNode of mutation.addedNodes) {
            if (addedNode.nodeType === addedNode.ELEMENT_NODE) {
              if (addedNode.childElementCount) {
                getTranslatables(addedNode).forEach(targets.add.bind(targets));
              } else {
                if (addedNode.hasAttribute('data-l10n-id')) {
                  targets.add(addedNode);
                }
              }
            }
          }
          break;
      }
    }

    if (targets.size === 0) {
      return;
    }

    translateElements(view, Array.from(targets));
  }

  function translateFragment(view, frag) {
    return translateElements(view, getTranslatables(frag));
  }

  function getElementsTranslation(view, elems) {
    const keys = elems.map(elem => {
      const id = elem.getAttribute('data-l10n-id');
      const args = elem.getAttribute('data-l10n-args');
      return args ? [
        id,
        JSON.parse(args.replace(reHtml, match => htmlEntities[match]))
      ] : id;
    });

    return view.formatEntities(...keys);
  }

  function translateElements(view, elements) {
    return getElementsTranslation(view, elements).then(
      translations => applyTranslations(view, elements, translations));
  }

  function applyTranslations(view, elems, translations) {
    disconnect(view, null, true);
    for (let i = 0; i < elems.length; i++) {
      overlayElement(elems[i], translations[i]);
    }
    reconnect(view);
  }

  // Polyfill NodeList.prototype[Symbol.iterator] for Chrome.
  // See https://code.google.com/p/chromium/issues/detail?id=401699
  if (typeof NodeList === 'function' && !NodeList.prototype[Symbol.iterator]) {
    NodeList.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];
  }

  // A document.ready shim
  // https://github.com/whatwg/html/issues/127
  function documentReady() {
    if (document.readyState !== 'loading') {
      return Promise.resolve();
    }

    return new Promise(resolve => {
      document.addEventListener('readystatechange', function onrsc() {
        document.removeEventListener('readystatechange', onrsc);
        resolve();
      });
    });
  }

  // Intl.Locale
  function getDirection(code) {
    const tag = code.split('-')[0];
    return ['ar', 'he', 'fa', 'ps', 'ur'].indexOf(tag) >= 0 ?
      'rtl' : 'ltr';
  }

  // Opera and Safari don't support it yet
  if (navigator.languages === undefined) {
    navigator.languages = [navigator.language];
  }

  function getResourceLinks(head) {
    return Array.prototype.map.call(
      head.querySelectorAll('link[rel="localization"]'),
      el => el.getAttribute('href'));
  }

  function getMeta(head) {
    let availableLangs = Object.create(null);
    let defaultLang = null;
    let appVersion = null;

    // XXX take last found instead of first?
    const metas = Array.from(head.querySelectorAll(
      'meta[name="availableLanguages"],' +
      'meta[name="defaultLanguage"],' +
      'meta[name="appVersion"]'));
    for (let meta of metas) {
      const name = meta.getAttribute('name');
      const content = meta.getAttribute('content').trim();
      switch (name) {
        case 'availableLanguages':
          availableLangs = getLangRevisionMap(
            availableLangs, content);
          break;
        case 'defaultLanguage':
          const [lang, rev] = getLangRevisionTuple(content);
          defaultLang = lang;
          if (!(lang in availableLangs)) {
            availableLangs[lang] = rev;
          }
          break;
        case 'appVersion':
          appVersion = content;
      }
    }

    return {
      defaultLang,
      availableLangs,
      appVersion
    };
  }

  function getLangRevisionMap(seq, str) {
    return str.split(',').reduce((prevSeq, cur) => {
      const [lang, rev] = getLangRevisionTuple(cur);
      prevSeq[lang] = rev;
      return prevSeq;
    }, seq);
  }

  function getLangRevisionTuple(str) {
    const [lang, rev]  = str.trim().split(':');
    // if revision is missing, use NaN
    return [lang, parseInt(rev)];
  }

  const viewProps = new WeakMap();

  class View {
    constructor(client, doc) {
      this.pseudo = {
        'fr-x-psaccent': createPseudo(this, 'fr-x-psaccent'),
        'ar-x-psbidi': createPseudo(this, 'ar-x-psbidi')
      };

      const initialized = documentReady().then(() => init(this, client));
      this._interactive = initialized.then(() => client);
      this.ready = initialized.then(langs => translateView(this, langs));
      initMutationObserver(this);

      viewProps.set(this, {
        doc: doc,
        ready: false
      });

      client.on('languageschangerequest',
        requestedLangs => this.requestLanguages(requestedLangs));
    }

    requestLanguages(requestedLangs, isGlobal) {
      const method = isGlobal ?
        client => client.method('requestLanguages', requestedLangs) :
        client => changeLanguages(this, client, requestedLangs);
      return this._interactive.then(method);
    }

    handleEvent() {
      return this.requestLanguages(navigator.languages);
    }

    formatEntities(...keys) {
      return this._interactive.then(
        client => client.method('formatEntities', client.id, keys));
    }

    formatValue(id, args) {
      return this._interactive.then(
        client => client.method('formatValues', client.id, [[id, args]])).then(
        values => values[0]);
    }

    formatValues(...keys) {
      return this._interactive.then(
        client => client.method('formatValues', client.id, keys));
    }

    translateFragment(frag) {
      return translateFragment(this, frag);
    }

    observeRoot(root) {
      observe(this, root);
    }

    disconnectRoot(root) {
      disconnect(this, root);
    }
  }

  View.prototype.setAttributes = setAttributes;
  View.prototype.getAttributes = getAttributes;

  function createPseudo(view, code) {
    return {
      getName: () => view._interactive.then(
        client => client.method('getName', code)),
      processString: str => view._interactive.then(
        client => client.method('processString', code, str)),
    };
  }

  function init(view, client) {
    const doc = viewProps.get(view).doc;
    const resources = getResourceLinks(doc.head);
    const meta = getMeta(doc.head);
    view.observeRoot(doc.documentElement);
    return getAdditionalLanguages().then(
      additionalLangs => client.method(
        'registerView', client.id, resources, meta, additionalLangs,
        navigator.languages));
  }

  function changeLanguages(view, client, requestedLangs) {
    const doc = viewProps.get(view).doc;
    const meta = getMeta(doc.head);
    return getAdditionalLanguages()
      .then(additionalLangs => client.method(
        'changeLanguages', client.id, meta, additionalLangs, requestedLangs
      ))
      .then(({langs, haveChanged}) => haveChanged ?
        translateView(view, langs) : undefined
      );
  }

  function getAdditionalLanguages() {
    if (navigator.mozApps && navigator.mozApps.getAdditionalLanguages) {
      return navigator.mozApps.getAdditionalLanguages()
        .catch(() => Object.create(null));
    }

    return Promise.resolve(Object.create(null));
  }

  function translateView(view, langs) {
    const props = viewProps.get(view);
    const html = props.doc.documentElement;

    if (props.ready) {
      return translateRoots(view).then(
        () => setAllAndEmit(html, langs));
    }

    const translated =
      // has the document been already pre-translated?
      langs[0].code === html.getAttribute('lang') ?
        Promise.resolve() :
        translateRoots(view).then(
          () => setLangDir(html, langs));

    return translated.then(() => {
      setLangs(html, langs);
      props.ready = true;
    });
  }

  function setLangs(html, langs) {
    const codes = langs.map(lang => lang.code);
    html.setAttribute('langs', codes.join(' '));
  }

  function setLangDir(html, langs) {
    const code = langs[0].code;
    html.setAttribute('lang', code);
    html.setAttribute('dir', getDirection(code));
  }

  function setAllAndEmit(html, langs) {
    setLangDir(html, langs);
    setLangs(html, langs);
    html.parentNode.dispatchEvent(new CustomEvent('DOMRetranslated', {
      bubbles: false,
      cancelable: false,
    }));
  }

  const remote = new Remote(fetchResource, broadcast);
  const client = new Client(remote);
  document.l10n = new View(client, document);

  window.addEventListener('languagechange', document.l10n);
  document.addEventListener('additionallanguageschange', document.l10n);

})();
