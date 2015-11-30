/*
 * Note, please see l10n-gaia-upstream.txt for a list of changes
 * that will need to be reapplied if this file is updated.
 */

(function(window, undefined) {
  'use strict';

  /* jshint validthis:true */
  function L10nError(message, id, loc) {
    this.name = 'L10nError';
    this.message = message;
    this.id = id;
    this.loc = loc;
  }
  L10nError.prototype = Object.create(Error.prototype);
  L10nError.prototype.constructor = L10nError;


  /* jshint browser:true */

  var io = {
    load: function load(url, callback, sync) {
      var xhr = new XMLHttpRequest();

      if (xhr.overrideMimeType) {
        xhr.overrideMimeType('text/plain');
      }

      xhr.open('GET', url, !sync);

      xhr.addEventListener('load', function io_load(e) {
        if (e.target.status === 200 || e.target.status === 0) {
          callback(null, e.target.responseText);
        } else {
          callback(new L10nError('Not found: ' + url));
        }
      });
      xhr.addEventListener('error', callback);
      xhr.addEventListener('timeout', callback);

      // the app: protocol throws on 404, see https://bugzil.la/827243
      try {
        xhr.send(null);
      } catch (e) {
        callback(new L10nError('Not found: ' + url));
      }
    },

    loadJSON: function loadJSON(url, callback) {
      var xhr = new XMLHttpRequest();

      if (xhr.overrideMimeType) {
        xhr.overrideMimeType('application/json');
      }

      xhr.open('GET', url);

      xhr.responseType = 'json';
      xhr.addEventListener('load', function io_loadjson(e) {
        if (e.target.status === 200 || e.target.status === 0) {
          callback(null, e.target.response);
        } else {
          callback(new L10nError('Not found: ' + url));
        }
      });
      xhr.addEventListener('error', callback);
      xhr.addEventListener('timeout', callback);

      // the app: protocol throws on 404, see https://bugzil.la/827243
      try {
        xhr.send(null);
      } catch (e) {
        callback(new L10nError('Not found: ' + url));
      }
    }
  };

  function EventEmitter() {}

  EventEmitter.prototype.emit = function ee_emit() {
    if (!this._listeners) {
      return;
    }

    var args = Array.prototype.slice.call(arguments);
    var type = args.shift();
    if (!this._listeners[type]) {
      return;
    }

    var typeListeners = this._listeners[type].slice();
    for (var i = 0; i < typeListeners.length; i++) {
      typeListeners[i].apply(this, args);
    }
  };

  EventEmitter.prototype.addEventListener = function ee_add(type, listener) {
    if (!this._listeners) {
      this._listeners = {};
    }
    if (!(type in this._listeners)) {
      this._listeners[type] = [];
    }
    this._listeners[type].push(listener);
  };

  EventEmitter.prototype.removeEventListener = function ee_rm(type, listener) {
    if (!this._listeners) {
      return;
    }

    var typeListeners = this._listeners[type];
    var pos = typeListeners.indexOf(listener);
    if (pos === -1) {
      return;
    }

    typeListeners.splice(pos, 1);
  };


  function getPluralRule(lang) {
    var locales2rules = {
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
    var pluralRules = {
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

    // return a function that gives the plural form name for a given integer
    var index = locales2rules[lang.replace(/-.*$/, '')];
    if (!(index in pluralRules)) {
      return function() { return 'other'; };
    }
    return pluralRules[index];
  }




  function PropertiesParser() {
    var parsePatterns = {
      comment: /^\s*#|^\s*$/,
      entity: /^([^=\s]+)\s*=\s*(.+)$/,
      multiline: /[^\\]\\$/,
      index: /\{\[\s*(\w+)(?:\(([^\)]*)\))?\s*\]\}/i,
      unicode: /\\u([0-9a-fA-F]{1,4})/g,
      entries: /[\r\n]+/,
      controlChars: /\\([\\\n\r\t\b\f\{\}\"\'])/g
    };

    this.parse = function (ctx, source) {
      var ast = Object.create(null);

      var entries = source.split(parsePatterns.entries);
      for (var i = 0; i < entries.length; i++) {
        var line = entries[i];

        if (parsePatterns.comment.test(line)) {
          continue;
        }

        while (parsePatterns.multiline.test(line) && i < entries.length) {
          line = line.slice(0, -1) + entries[++i].trim();
        }

        var entityMatch = line.match(parsePatterns.entity);
        if (entityMatch) {
          try {
            parseEntity(entityMatch[1], entityMatch[2], ast);
          } catch (e) {
            if (ctx) {
              ctx._emitter.emit('error', e);
            } else {
              throw e;
            }
          }
        }
      }
      return ast;
    };

    function setEntityValue(id, attr, key, value, ast) {
      var obj = ast;
      var prop = id;

      if (attr) {
        if (!(id in obj)) {
          obj[id] = {};
        }
        if (typeof(obj[id]) === 'string') {
          obj[id] = {'_': obj[id]};
        }
        obj = obj[id];
        prop = attr;
      }

      if (!key) {
        obj[prop] = value;
        return;
      }

      if (!(prop in obj)) {
        obj[prop] = {'_': {}};
      } else if (typeof(obj[prop]) === 'string') {
        obj[prop] = {'_index': parseIndex(obj[prop]), '_': {}};
      }
      obj[prop]._[key] = value;
    }

    function parseEntity(id, value, ast) {
      var name, key;

      var pos = id.indexOf('[');
      if (pos !== -1) {
        name = id.substr(0, pos);
        key = id.substring(pos + 1, id.length - 1);
      } else {
        name = id;
        key = null;
      }

      var nameElements = name.split('.');

      if (nameElements.length > 2) {
        throw new Error('Error in ID: "' + name + '".' +
            ' Nested attributes are not supported.');
      }

      var attr;
      if (nameElements.length > 1) {
        name = nameElements[0];
        attr = nameElements[1];
      } else {
        attr = null;
      }

      setEntityValue(name, attr, key, unescapeString(value), ast);
    }

    function unescapeControlCharacters(str) {
      return str.replace(parsePatterns.controlChars, '$1');
    }

    function unescapeUnicode(str) {
      return str.replace(parsePatterns.unicode, function(match, token) {
        return unescape('%u' + '0000'.slice(token.length) + token);
      });
    }

    function unescapeString(str) {
      if (str.lastIndexOf('\\') !== -1) {
        str = unescapeControlCharacters(str);
      }
      return unescapeUnicode(str);
    }

    function parseIndex(str) {
      var match = str.match(parsePatterns.index);
      if (!match) {
        throw new L10nError('Malformed index');
      }
      var parts = Array.prototype.slice.call(match, 1);
      return parts.filter(function(part) {
        return !!part;
      });
    }
  }



  var KNOWN_MACROS = ['plural'];

  var MAX_PLACEABLE_LENGTH = 2500;
  var MAX_PLACEABLES = 100;
  var rePlaceables = /\{\{\s*(.+?)\s*\}\}/g;

  function Entity(id, node, env) {
    this.id = id;
    this.env = env;
    // the dirty guard prevents cyclic or recursive references from other
    // Entities; see Entity.prototype.resolve
    this.dirty = false;
    if (typeof node === 'string') {
      this.value = node;
    } else {
      // it's either a hash or it has attrs, or both
      var keys = Object.keys(node);

      /* jshint -W084 */
      for (var i = 0, key; key = keys[i]; i++) {
        if (key[0] !== '_') {
          if (!this.attributes) {
            this.attributes = Object.create(null);
          }
          this.attributes[key] = new Entity(this.id + '.' + key, node[key],
                                            env);
        }
      }
      this.value = node._ || null;
      this.index = node._index;
    }
  }

  Entity.prototype.resolve = function E_resolve(ctxdata) {
    if (this.dirty) {
      return undefined;
    }

    this.dirty = true;
    var val;
    // if resolve fails, we want the exception to bubble up and stop the whole
    // resolving process;  however, we still need to clean up the dirty flag
    try {
      val = resolveValue(ctxdata, this.env, this.value, this.index);
    } finally {
      this.dirty = false;
    }
    return val;
  };

  Entity.prototype.toString = function E_toString(ctxdata) {
    try {
      return this.resolve(ctxdata);
    } catch (e) {
      return undefined;
    }
  };

  Entity.prototype.valueOf = function E_valueOf(ctxdata) {
    if (!this.attributes) {
      return this.toString(ctxdata);
    }

    var entity = {
      value: this.toString(ctxdata),
      attributes: Object.create(null)
    };

    for (var key in this.attributes) {
      /* jshint -W089 */
      entity.attributes[key] = this.attributes[key].toString(ctxdata);
    }

    return entity;
  };

  function resolveIdentifier(ctxdata, env, id) {
    if (KNOWN_MACROS.indexOf(id) > -1) {
      return env['__' + id];
    }

    if (ctxdata && ctxdata.hasOwnProperty(id) &&
        (typeof ctxdata[id] === 'string' ||
         (typeof ctxdata[id] === 'number' && !isNaN(ctxdata[id])))) {
      return ctxdata[id];
    }

    // XXX: special case for Node.js where still:
    // '__proto__' in Object.create(null) => true
    if (id in env && id !== '__proto__') {
      if (!(env[id] instanceof Entity)) {
        env[id] = new Entity(id, env[id], env);
      }
      return env[id].resolve(ctxdata);
    }

    return undefined;
  }

  function subPlaceable(ctxdata, env, match, id) {
    var value = resolveIdentifier(ctxdata, env, id);

    if (typeof value === 'number') {
      return value;
    }

    if (typeof value === 'string') {
      // prevent Billion Laughs attacks
      if (value.length >= MAX_PLACEABLE_LENGTH) {
        throw new L10nError('Too many characters in placeable (' +
                            value.length + ', max allowed is ' +
                            MAX_PLACEABLE_LENGTH + ')');
      }
      return value;
    }

    return match;
  }

  function interpolate(ctxdata, env, str) {
    var placeablesCount = 0;
    var value = str.replace(rePlaceables, function(match, id) {
      // prevent Quadratic Blowup attacks
      if (placeablesCount++ >= MAX_PLACEABLES) {
        throw new L10nError('Too many placeables (' + placeablesCount +
                            ', max allowed is ' + MAX_PLACEABLES + ')');
      }
      return subPlaceable(ctxdata, env, match, id);
    });
    placeablesCount = 0;
    return value;
  }

  function resolveSelector(ctxdata, env, expr, index) {
      var selector = resolveIdentifier(ctxdata, env, index[0]);
      if (selector === undefined) {
        throw new L10nError('Unknown selector: ' + index[0]);
      }

      if (typeof selector !== 'function') {
        // selector is a simple reference to an entity or ctxdata
        return selector;
      }

      var argLength = index.length - 1;
      if (selector.length !== argLength) {
        throw new L10nError('Macro ' + index[0] + ' expects ' +
                            selector.length + ' argument(s), yet ' + argLength +
                            ' given');
      }

      var argValue = resolveIdentifier(ctxdata, env, index[1]);

      if (selector === env.__plural) {
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

  function resolveValue(ctxdata, env, expr, index) {
    if (typeof expr === 'string') {
      return interpolate(ctxdata, env, expr);
    }

    if (typeof expr === 'boolean' ||
        typeof expr === 'number' ||
        !expr) {
      return expr;
    }

    // otherwise, it's a dict
    if (index) {
      // try to use the index in order to select the right dict member
      var selector = resolveSelector(ctxdata, env, expr, index);
      if (expr.hasOwnProperty(selector)) {
        return resolveValue(ctxdata, env, expr[selector]);
      }
    }

    // if there was no index or no selector was found, try 'other'
    if ('other' in expr) {
      return resolveValue(ctxdata, env, expr.other);
    }

    return undefined;
  }

  function compile(env, ast) {
    /* jshint -W089 */
    env = env || Object.create(null);
    for (var id in ast) {
      env[id] = new Entity(id, ast[id], env);
    }
    return env;
  }



  /* Utility functions */

  // Recursively walk an AST node searching for content leaves
  function walkContent(node, fn) {
    if (typeof node === 'string') {
      return fn(node);
    }

    var rv = {};
    for (var key in node) {
      if (key === '_index') {
        rv[key] = node[key];
      } else {
        rv[key] = walkContent(node[key], fn);
      }
    }
    return rv;
  }


  /* Pseudolocalizations
   *
   * PSEUDO_STRATEGIES is a dict of strategies to be used to modify the English
   * context in order to create pseudolocalizations.  These can be used by
   * developers to test the localizability of their code without having to
   * actually speak a foreign language.
   *
   * Currently, the following pseudolocales are supported:
   *
   *   qps-ploc - Ȧȧƈƈḗḗƞŧḗḗḓ Ḗḗƞɠŀīīşħ
   *
   *     In Accented English all English letters are replaced by accented
   *     Unicode counterparts which don't impair the readability of the content.
   *     This allows developers to quickly test if any given string is being
   *     correctly displayed in its 'translated' form.  Additionally, simple
   *     heuristics are used to make certain words longer to better simulate the
   *     experience of international users.
   *
   *   qps-plocm - ɥsıʅƃuƎ pǝɹoɹɹıW
   *
   *     Mirrored English is a fake RTL locale.  All words are surrounded by
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

  var reAlphas = /[a-zA-Z]/g;
  var reVowels = /[aeiouAEIOU]/g;

  // ȦƁƇḒḖƑƓĦĪĴĶĿḾȠǾƤɊŘŞŦŬṼẆẊẎẐ + [\\]^_` + ȧƀƈḓḗƒɠħīĵķŀḿƞǿƥɋřşŧŭṽẇẋẏẑ
  var ACCENTED_MAP = '\u0226\u0181\u0187\u1E12\u1E16\u0191\u0193\u0126\u012A' +
                     '\u0134\u0136\u013F\u1E3E\u0220\u01FE\u01A4\u024A\u0158' +
                     '\u015E\u0166\u016C\u1E7C\u1E86\u1E8A\u1E8E\u1E90' +
                     '[\\]^_`' +
                     '\u0227\u0180\u0188\u1E13\u1E17\u0192\u0260\u0127\u012B' +
                     '\u0135\u0137\u0140\u1E3F\u019E\u01FF\u01A5\u024B\u0159' +
                     '\u015F\u0167\u016D\u1E7D\u1E87\u1E8B\u1E8F\u1E91';

  // XXX Until https://bugzil.la/1007340 is fixed, ᗡℲ⅁⅂⅄ don't render correctly
  // on the devices.  For now, use the following replacements: pɟפ˥ʎ
  // ∀ԐↃpƎɟפHIſӼ˥WNOԀÒᴚS⊥∩ɅＭXʎZ + [\\]ᵥ_, + ɐqɔpǝɟƃɥıɾʞʅɯuodbɹsʇnʌʍxʎz
  var FLIPPED_MAP = '\u2200\u0510\u2183p\u018E\u025F\u05E4HI\u017F' +
                    '\u04FC\u02E5WNO\u0500\xD2\u1D1AS\u22A5\u2229\u0245' +
                    '\uFF2DX\u028EZ' +
                    '[\\]\u1D65_,' +
                    '\u0250q\u0254p\u01DD\u025F\u0183\u0265\u0131\u027E' +
                    '\u029E\u0285\u026Fuodb\u0279s\u0287n\u028C\u028Dx\u028Ez';

  function makeLonger(val) {
    return val.replace(reVowels, function(match) {
      return match + match.toLowerCase();
    });
  }

  function makeAccented(map, val) {
    // Replace each Latin letter with a Unicode character from map
    return val.replace(reAlphas, function(match) {
      return map.charAt(match.charCodeAt(0) - 65);
    });
  }

  var reWords = /[^\W0-9_]+/g;

  function makeRTL(val) {
    // Surround each word with Unicode formatting codes, RLO and PDF:
    //   U+202E:   RIGHT-TO-LEFT OVERRIDE (RLO)
    //   U+202C:   POP DIRECTIONAL FORMATTING (PDF)
    // See http://www.w3.org/International/questions/qa-bidi-controls
    return val.replace(reWords, function(match) {
      return '\u202e' + match + '\u202c';
    });
  }

  // strftime tokens (%a, %Eb), {{ placeables }} and template {vars}
  var reExcluded = /(%[EO]?\w|\{\{?\s*.+?\s*\}?\})/;

  function mapContent(fn, val) {
    if (!val) {
      return val;
    }
    var parts = val.split(reExcluded);
    var modified = parts.map(function(part) {
      if (reExcluded.test(part)) {
        return part;
      }
      return fn(part);
    });
    return modified.join('');
  }

  function Pseudo(id, name, charMap, modFn) {
    this.id = id;
    this.translate = mapContent.bind(null, function(val) {
      return makeAccented(charMap, modFn(val));
    });
    this.name = this.translate(name);
  }

  var PSEUDO_STRATEGIES = {
    'qps-ploc': new Pseudo('qps-ploc', 'Accented English',
                           ACCENTED_MAP, makeLonger),
    'qps-plocm': new Pseudo('qps-plocm', 'Mirrored English',
                            FLIPPED_MAP, makeRTL)
  };



  var propertiesParser = null;

  function Locale(id, ctx) {
    this.id = id;
    this.ctx = ctx;
    this.isReady = false;
    this.isPseudo = PSEUDO_STRATEGIES.hasOwnProperty(id);
    this.entries = Object.create(null);
    this.entries.__plural = getPluralRule(this.isPseudo ?
                                          this.ctx.defaultLocale : id);
  }

  Locale.prototype.getEntry = function L_getEntry(id) {
    /* jshint -W093 */

    var entries = this.entries;

    if (!(id in entries)) {
      return undefined;
    }

    if (entries[id] instanceof Entity) {
      return entries[id];
    }

    return entries[id] = new Entity(id, entries[id], entries);
  };

  Locale.prototype.build = function L_build(callback) {
    var sync = !callback;
    var ctx = this.ctx;
    var self = this;

    var l10nLoads = ctx.resLinks.length;

    function onL10nLoaded(err) {
      if (err) {
        ctx._emitter.emit('error', err);
      }
      if (--l10nLoads <= 0) {
        self.isReady = true;
        if (callback) {
          callback();
        }
      }
    }

    if (l10nLoads === 0) {
      onL10nLoaded();
      return;
    }

    function onJSONLoaded(err, json) {
      if (!err && json) {
        self.addAST(json);
      }
      onL10nLoaded(err);
    }

    function onPropLoaded(err, source) {
      if (!err && source) {
        if (!propertiesParser) {
          propertiesParser = new PropertiesParser();
        }
        var ast = propertiesParser.parse(ctx, source);
        self.addAST(ast);
      }
      onL10nLoaded(err);
    }

    var idToFetch = this.isPseudo ? ctx.defaultLocale : this.id;
    for (var i = 0; i < ctx.resLinks.length; i++) {
      var path = ctx.resLinks[i].replace('{locale}', idToFetch);
      var type = path.substr(path.lastIndexOf('.') + 1);

      switch (type) {
        case 'json':
          io.loadJSON(path, onJSONLoaded, sync);
          break;
        case 'properties':
          io.load(path, onPropLoaded, sync);
          break;
      }
    }
  };

  Locale.prototype.addAST = function(ast) {
    /* jshint -W084 */
    var keys = Object.keys(ast);
    var i = 0, key;

    if (this.isPseudo) {
      for (; key = keys[i]; i++) {
        this.entries[key] = walkContent(ast[key],
                                        PSEUDO_STRATEGIES[this.id].translate);
      }
    } else {
      for (; key = keys[i]; i++) {
        this.entries[key] = ast[key];
      }
    }
  };

  Locale.prototype.getEntity = function(id, ctxdata) {
    var entry = this.getEntry(id);

    if (!entry) {
      return null;
    }
    return entry.valueOf(ctxdata);
  };



  function Context(id) {

    this.id = id;
    this.isReady = false;
    this.isLoading = false;

    this.defaultLocale = 'en-US';
    this.availableLocales = [];
    this.supportedLocales = [];

    this.resLinks = [];
    this.locales = {};

    this._emitter = new EventEmitter();


    // Getting translations

    function getWithFallback(id) {
      /* jshint -W084 */

      if (!this.isReady) {
        throw new L10nError('Context not ready');
      }

      var cur = 0;
      var loc;
      var locale;
      while (loc = this.supportedLocales[cur]) {
        locale = this.getLocale(loc);
        if (!locale.isReady) {
          // build without callback, synchronously
          locale.build(null);
        }
        var entry = locale.getEntry(id);
        if (entry === undefined) {
          cur++;
          warning.call(this, new L10nError(id + ' not found in ' + loc, id,
                                           loc));
          continue;
        }
        return entry;
      }

      error.call(this, new L10nError(id + ' not found', id));
      return null;
    }

    this.get = function get(id, ctxdata) {
      var entry = getWithFallback.call(this, id);
      if (entry === null) {
        return '';
      }

      return entry.toString(ctxdata) || '';
    };

    this.getEntity = function getEntity(id, ctxdata) {
      var entry = getWithFallback.call(this, id);
      if (entry === null) {
        return null;
      }

      return entry.valueOf(ctxdata);
    };


    // Helpers

    this.getLocale = function getLocale(code) {
      /* jshint -W093 */

      var locales = this.locales;
      if (locales[code]) {
        return locales[code];
      }

      return locales[code] = new Locale(code, this);
    };


    // Getting ready

    function negotiate(available, requested, defaultLocale) {
      var supportedLocale;
      for (var i = 0; i < requested.length; ++i) {
        var locale = requested[i];
        if (available.indexOf(locale) !== -1) {
          supportedLocale = locale;
          break;
        }
      }
      if (!supportedLocale ||
          supportedLocale === defaultLocale) {
        return [defaultLocale];
      } else {
        return [supportedLocale, defaultLocale];
      }
    }

    function freeze(supported) {
      var locale = this.getLocale(supported[0]);
      if (locale.isReady) {
        setReady.call(this, supported);
      } else {
        locale.build(setReady.bind(this, supported));
      }
    }

    function setReady(supported) {
      this.supportedLocales = supported;
      this.isReady = true;
      this._emitter.emit('ready');
    }

    this.registerLocales = function (defLocale, available) {
      /* jshint boss:true */
      this.availableLocales = [this.defaultLocale = defLocale];

      if (available) {
        for (var i = 0, loc; loc = available[i]; i++) {
          if (this.availableLocales.indexOf(loc) === -1) {
            this.availableLocales.push(loc);
          }
        }
      }
    };

    this.requestLocales = function requestLocales() {
      if (this.isLoading && !this.isReady) {
        throw new L10nError('Context not ready');
      }

      this.isLoading = true;
      var requested = Array.prototype.slice.call(arguments);
      if (requested.length === 0) {
        throw new L10nError('No locales requested');
      }

      var reqPseudo = requested.filter(function(loc) {
        return loc in PSEUDO_STRATEGIES;
      });

      var supported = negotiate(this.availableLocales.concat(reqPseudo),
                                requested,
                                this.defaultLocale);
      freeze.call(this, supported);
    };


    // Events

    this.addEventListener = function addEventListener(type, listener) {
      this._emitter.addEventListener(type, listener);
    };

    this.removeEventListener = function removeEventListener(type, listener) {
      this._emitter.removeEventListener(type, listener);
    };

    this.ready = function ready(callback) {
      if (this.isReady) {
        setTimeout(callback);
      }
      this.addEventListener('ready', callback);
    };

    this.once = function once(callback) {
      /* jshint -W068 */
      if (this.isReady) {
        setTimeout(callback);
        return;
      }

      var callAndRemove = (function() {
        this.removeEventListener('ready', callAndRemove);
        callback();
      }).bind(this);
      this.addEventListener('ready', callAndRemove);
    };


    // Errors

    function warning(e) {
      this._emitter.emit('warning', e);
      return e;
    }

    function error(e) {
      this._emitter.emit('error', e);
      return e;
    }
  }



  var DEBUG = false;
  var isPretranslated = false;
  var rtlList = ['ar', 'he', 'fa', 'ps', 'qps-plocm', 'ur'];
  var nodeObserver = null;
  var pendingElements = null;
  var manifest = {};

  var moConfig = {
    attributes: true,
    characterData: false,
    childList: true,
    subtree: true,
    attributeFilter: ['data-l10n-id', 'data-l10n-args']
  };

  // Public API

  navigator.mozL10n = {
    ctx: new Context(),
    get: function get(id, ctxdata) {
      return navigator.mozL10n.ctx.get(id, ctxdata);
    },
    localize: function localize(element, id, args) {
      return localizeElement.call(navigator.mozL10n, element, id, args);
    },
    translate: function () {
      // XXX: Remove after removing obsolete calls. Bugs 992473 and 1020136
    },
    translateFragment: function (fragment) {
      return translateFragment.call(navigator.mozL10n, fragment);
    },
    setAttributes: setL10nAttributes,
    getAttributes: getL10nAttributes,
    ready: function ready(callback) {
      return navigator.mozL10n.ctx.ready(callback);
    },
    once: function once(callback) {
      return navigator.mozL10n.ctx.once(callback);
    },
    get readyState() {
      return navigator.mozL10n.ctx.isReady ? 'complete' : 'loading';
    },
    language: {
      set code(lang) {
        navigator.mozL10n.ctx.requestLocales(lang);
      },
      get code() {
        return navigator.mozL10n.ctx.supportedLocales[0];
      },
      get direction() {
        return getDirection(navigator.mozL10n.ctx.supportedLocales[0]);
      }
    },
    qps: PSEUDO_STRATEGIES,
    _getInternalAPI: function() {
      return {
        Error: L10nError,
        Context: Context,
        Locale: Locale,
        Entity: Entity,
        getPluralRule: getPluralRule,
        rePlaceables: rePlaceables,
        getTranslatableChildren:  getTranslatableChildren,
        translateDocument: translateDocument,
        onManifestInjected: onManifestInjected,
        onMetaInjected: onMetaInjected,
        fireLocalizedEvent: fireLocalizedEvent,
        PropertiesParser: PropertiesParser,
        compile: compile,
        walkContent: walkContent
      };
    }
  };

  navigator.mozL10n.ctx.ready(onReady.bind(navigator.mozL10n));

  if (DEBUG) {
    navigator.mozL10n.ctx.addEventListener('error', console.error);
    navigator.mozL10n.ctx.addEventListener('warning', console.warn);
  }

  function getDirection(lang) {
    return (rtlList.indexOf(lang) >= 0) ? 'rtl' : 'ltr';
  }

  var readyStates = {
    'loading': 0,
    'interactive': 1,
    'complete': 2
  };

  function waitFor(state, callback) {
    state = readyStates[state];
    if (readyStates[document.readyState] >= state) {
      callback();
      return;
    }

    document.addEventListener('readystatechange', function l10n_onrsc() {
      if (readyStates[document.readyState] >= state) {
        document.removeEventListener('readystatechange', l10n_onrsc);
        callback();
      }
    });
  }

  if (window.document) {
    isPretranslated = !PSEUDO_STRATEGIES.hasOwnProperty(navigator.language) &&
                      (document.documentElement.lang === navigator.language);

    // XXX always pretranslate if data-no-complete-bug is set;  this is
    // a workaround for a netError page not firing some onreadystatechange
    // events;  see https://bugzil.la/444165
    var pretranslate = document.documentElement.getAttribute("data-noCompleteBug") ?
      true : !isPretranslated;
    waitFor('interactive', init.bind(navigator.mozL10n, pretranslate));
  }

  function initObserver() {
    nodeObserver = new MutationObserver(onMutations.bind(navigator.mozL10n));
    nodeObserver.observe(document, moConfig);
  }

  function init(pretranslate) {
    if (pretranslate) {
      inlineLocalization.call(navigator.mozL10n);
      initResources.call(navigator.mozL10n);
    } else {
      // if pretranslate is false, we want to initialize MO
      // early, to collect nodes injected between now and when resources
      // are loaded because we're not going to translate the whole
      // document once l10n resources are ready.
      initObserver();
      window.setTimeout(initResources.bind(navigator.mozL10n));
    }
  }

  function inlineLocalization() {
    var locale = this.ctx.getLocale(navigator.language || navigator.browserLanguage);
    var scriptLoc = locale.isPseudo ? this.ctx.defaultLocale : locale.id;
    var script = document.documentElement
                         .querySelector('script[type="application/l10n"]' +
                         '[lang="' + scriptLoc + '"]');
    if (!script) {
      return;
    }

    // the inline localization is happenning very early, when the ctx is not
    // yet ready and when the resources haven't been downloaded yet;  add the
    // inlined JSON directly to the current locale
    locale.addAST(JSON.parse(script.innerHTML));
    // localize the visible DOM
    var l10n = {
      ctx: locale,
      language: {
        code: locale.id,
        direction: getDirection(locale.id)
      }
    };
    translateDocument.call(l10n);

    // the visible DOM is now pretranslated
    isPretranslated = true;
  }

  function initResources() {
    /* jshint boss:true */
    var manifestFound = false;

    var nodes = document.head
                        .querySelectorAll('link[rel="localization"],' +
                                          'link[rel="manifest"],' +
                                          'meta[name="locales"],' +
                                          'meta[name="default_locale"],' +
                                          'script[type="application/l10n"]');
    for (var i = 0, node; node = nodes[i]; i++) {
      var type = node.getAttribute('rel') || node.nodeName.toLowerCase();
      switch (type) {
        case 'manifest':
          manifestFound = true;
          onManifestInjected.call(this, node.getAttribute('href'), initLocale);
          break;
        case 'localization':
          this.ctx.resLinks.push(node.getAttribute('href'));
          break;
        case 'meta':
          onMetaInjected.call(this, node);
          break;
        case 'script':
          onScriptInjected.call(this, node);
          break;
      }
    }

    // if after scanning the head any locales have been registered in the ctx
    // it's safe to initLocale without waiting for manifest.webapp
    if (this.ctx.availableLocales.length) {
      return initLocale.call(this);
    }

    // if no locales were registered so far and no manifest.webapp link was
    // found we still call initLocale with just the default language available
    if (!manifestFound) {
      this.ctx.registerLocales(this.ctx.defaultLocale);
      return initLocale.call(this);
    }
  }

  function onMetaInjected(node) {
    if (this.ctx.availableLocales.length) {
      return;
    }

    switch (node.getAttribute('name')) {
      case 'locales':
        manifest.locales = node.getAttribute('content').split(',').map(
          Function.prototype.call, String.prototype.trim);
        break;
      case 'default_locale':
        manifest.defaultLocale = node.getAttribute('content');
        break;
    }

    if (Object.keys(manifest).length === 2) {
      this.ctx.registerLocales(manifest.defaultLocale, manifest.locales);
      manifest = {};
    }
  }

  function onScriptInjected(node) {
    var lang = node.getAttribute('lang');
    var locale = this.ctx.getLocale(lang);
    locale.addAST(JSON.parse(node.textContent));
  }

  function onManifestInjected(url, callback) {
    if (this.ctx.availableLocales.length) {
      return;
    }

    io.loadJSON(url, function parseManifest(err, json) {
      if (this.ctx.availableLocales.length) {
        return;
      }

      if (err) {
        this.ctx._emitter.emit('error', err);
        this.ctx.registerLocales(this.ctx.defaultLocale);
        if (callback) {
          callback.call(this);
        }
        return;
      }

      // default_locale and locales might have been already provided by meta
      // elements which take precedence;  check if we already have them
      if (!('defaultLocale' in manifest)) {
        if (json.default_locale) {
          manifest.defaultLocale = json.default_locale;
        } else {
          manifest.defaultLocale = this.ctx.defaultLocale;
          this.ctx._emitter.emit(
            'warning', new L10nError('default_locale missing from manifest'));
        }
      }
      if (!('locales' in manifest)) {
        if (json.locales) {
          manifest.locales = Object.keys(json.locales);
        } else {
          this.ctx._emitter.emit(
            'warning', new L10nError('locales missing from manifest'));
        }
      }

      this.ctx.registerLocales(manifest.defaultLocale, manifest.locales);
      manifest = {};

      if (callback) {
        callback.call(this);
      }
    }.bind(this));
  }

  function initLocale() {
    this.ctx.requestLocales.apply(
      this.ctx, navigator.languages || [navigator.language]);
    window.addEventListener('languagechange', function l10n_langchange() {
      this.ctx.requestLocales.apply(
        this.ctx, navigator.languages || [navigator.language]);
    }.bind(this));
  }

  function localizeMutations(mutations) {
    var mutation;

    for (var i = 0; i < mutations.length; i++) {
      mutation = mutations[i];
      if (mutation.type === 'childList') {
        var addedNode;

        for (var j = 0; j < mutation.addedNodes.length; j++) {
          addedNode = mutation.addedNodes[j];

          if (addedNode.nodeType !== Node.ELEMENT_NODE) {
            continue;
          }

          if (addedNode.childElementCount) {
            translateFragment.call(this, addedNode);
          } else if (addedNode.hasAttribute('data-l10n-id')) {
            translateElement.call(this, addedNode);
          }
        }
      }

      if (mutation.type === 'attributes') {
        translateElement.call(this, mutation.target);
      }
    }
  }

  function onMutations(mutations, self) {
    self.disconnect();
    localizeMutations.call(this, mutations);
    self.observe(document, moConfig);
  }

  function onReady() {
    if (!isPretranslated) {
      translateDocument.call(this);
    }
    isPretranslated = false;

    if (pendingElements) {
      /* jshint boss:true */
      for (var i = 0, element; element = pendingElements[i]; i++) {
        translateElement.call(this, element);
      }
      pendingElements = null;
    }

    if (!nodeObserver) {
      initObserver();
    }
    fireLocalizedEvent.call(this);
  }

  function fireLocalizedEvent() {
    var event = new CustomEvent('localized', {
      'bubbles': false,
      'cancelable': false,
      'detail': {
        'language': this.ctx.supportedLocales[0]
      }
    });
    window.dispatchEvent(event);
  }

  /* jshint -W104 */

  function translateDocument() {
    document.documentElement.lang = this.language.code;
    document.documentElement.dir = this.language.direction;
    translateFragment.call(this, document.documentElement);
  }

  function translateFragment(element) {
    if (element.hasAttribute('data-l10n-id')) {
      translateElement.call(this, element);
    }

    var nodes = getTranslatableChildren(element);
    for (var i = 0; i < nodes.length; i++ ) {
      translateElement.call(this, nodes[i]);
    }
  }

  function setL10nAttributes(element, id, args) {
    element.setAttribute('data-l10n-id', id);
    if (args) {
      element.setAttribute('data-l10n-args', JSON.stringify(args));
    }
  }

  function getL10nAttributes(element) {
    return {
      id: element.getAttribute('data-l10n-id'),
      args: JSON.parse(element.getAttribute('data-l10n-args'))
    };
  }

  function getTranslatableChildren(element) {
    return element ? element.querySelectorAll('*[data-l10n-id]') : [];
  }

  function localizeElement(element, id, args) {
    if (!id) {
      element.removeAttribute('data-l10n-id');
      element.removeAttribute('data-l10n-args');
      setTextContent(element, '');
      return;
    }

    element.setAttribute('data-l10n-id', id);
    if (args && typeof args === 'object') {
      element.setAttribute('data-l10n-args', JSON.stringify(args));
    } else {
      element.removeAttribute('data-l10n-args');
    }
  }

  function translateElement(element) {
    if (!this.ctx.isReady) {
      if (!pendingElements) {
        pendingElements = [];
      }
      pendingElements.push(element);
      return;
    }

    var l10n = getL10nAttributes(element);

    if (!l10n.id) {
      return false;
    }

    var entity = this.ctx.getEntity(l10n.id, l10n.args);

    if (!entity) {
      return false;
    }

    if (typeof entity === 'string') {
      setTextContent(element, entity);
      return true;
    }

    if (entity.value) {
      setTextContent(element, entity.value);
    }

    for (var key in entity.attributes) {
      var attr = entity.attributes[key];
      if (key === 'ariaLabel') {
        element.setAttribute('aria-label', attr);
      } else if (key === 'innerHTML') {
        // XXX: to be removed once bug 994357 lands
        element.innerHTML = attr;
      } else {
        element.setAttribute(key, attr);
      }
    }

    return true;
  }

  function setTextContent(element, text) {
    // standard case: no element children
    if (!element.firstElementChild) {
      element.textContent = text;
      return;
    }

    // this element has element children: replace the content of the first
    // (non-blank) child textNode and clear other child textNodes
    var found = false;
    var reNotBlank = /\S/;
    for (var child = element.firstChild; child; child = child.nextSibling) {
      if (child.nodeType === Node.TEXT_NODE &&
          reNotBlank.test(child.nodeValue)) {
        if (found) {
          child.nodeValue = '';
        } else {
          child.nodeValue = text;
          found = true;
        }
      }
    }
    // if no (non-empty) textNode is found, insert a textNode before the
    // element's first child.
    if (!found) {
      element.insertBefore(document.createTextNode(text), element.firstChild);
    }
  }

})(this);
