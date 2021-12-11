/*
 * Copyright (c) 2016 Chris O"Hara <cohara87@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * NOTE: This utility is derived from https://github.com/chriso/validator.js but it is
 *       **NOT** the same as the original. We have made the following changes:
 *         - Changed mocha tests to xpcshell based tests.
 *         - Merged the following pull requests:
 *           - [isMobileNumber] Added Lithuanian number pattern #667
 *           - Hongkong mobile number #665
 *           - Added option to validate any phone locale #663
 *           - Added validation for ISRC strings #660
 *         - Added isRFC5646 for rfc 5646 #572
 *         - Added isSemVer for version numbers.
 *         - Added isRGBColor for RGB colors.
 *
 * UPDATING: PLEASE FOLLOW THE INSTRUCTIONS INSIDE UPDATING.md
 */

"use strict";

(function (global, factory) {
      typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory() :
      typeof define === 'function' && define.amd ? define(factory) :
      (global.validator = factory());
}(this, function () { 'use strict';

      function assertString(input) {
        if (typeof input !== 'string') {
          throw new TypeError('This library (validator.js) validates strings only');
        }
      }

      function toDate(date) {
        assertString(date);
        date = Date.parse(date);
        return !isNaN(date) ? new Date(date) : null;
      }

      function toFloat(str) {
        assertString(str);
        return parseFloat(str);
      }

      function toInt(str, radix) {
        assertString(str);
        return parseInt(str, radix || 10);
      }

      function toBoolean(str, strict) {
        assertString(str);
        if (strict) {
          return str === '1' || str === 'true';
        }
        return str !== '0' && str !== 'false' && str !== '';
      }

      function equals(str, comparison) {
        assertString(str);
        return str === comparison;
      }

      var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) {
        return typeof obj;
      } : function (obj) {
        return obj && typeof Symbol === "function" && obj.constructor === Symbol && obj !== Symbol.prototype ? "symbol" : typeof obj;
      };

      var asyncGenerator = function () {
        function AwaitValue(value) {
          this.value = value;
        }

        function AsyncGenerator(gen) {
          var front, back;

          function send(key, arg) {
            return new Promise(function (resolve, reject) {
              var request = {
                key: key,
                arg: arg,
                resolve: resolve,
                reject: reject,
                next: null
              };

              if (back) {
                back = back.next = request;
              } else {
                front = back = request;
                resume(key, arg);
              }
            });
          }

          function resume(key, arg) {
            try {
              var result = gen[key](arg);
              var value = result.value;

              if (value instanceof AwaitValue) {
                Promise.resolve(value.value).then(function (arg) {
                  resume("next", arg);
                }, function (arg) {
                  resume("throw", arg);
                });
              } else {
                settle(result.done ? "return" : "normal", result.value);
              }
            } catch (err) {
              settle("throw", err);
            }
          }

          function settle(type, value) {
            switch (type) {
              case "return":
                front.resolve({
                  value: value,
                  done: true
                });
                break;

              case "throw":
                front.reject(value);
                break;

              default:
                front.resolve({
                  value: value,
                  done: false
                });
                break;
            }

            front = front.next;

            if (front) {
              resume(front.key, front.arg);
            } else {
              back = null;
            }
          }

          this._invoke = send;

          if (typeof gen.return !== "function") {
            this.return = undefined;
          }
        }

        if (typeof Symbol === "function" && Symbol.asyncIterator) {
          AsyncGenerator.prototype[Symbol.asyncIterator] = function () {
            return this;
          };
        }

        AsyncGenerator.prototype.next = function (arg) {
          return this._invoke("next", arg);
        };

        AsyncGenerator.prototype.throw = function (arg) {
          return this._invoke("throw", arg);
        };

        AsyncGenerator.prototype.return = function (arg) {
          return this._invoke("return", arg);
        };

        return {
          wrap: function (fn) {
            return function () {
              return new AsyncGenerator(fn.apply(this, arguments));
            };
          },
          await: function (value) {
            return new AwaitValue(value);
          }
        };
      }();

      function toString(input) {
        if ((typeof input === 'undefined' ? 'undefined' : _typeof(input)) === 'object' && input !== null) {
          if (typeof input.toString === 'function') {
            input = input.toString();
          } else {
            input = '[object Object]';
          }
        } else if (input === null || typeof input === 'undefined' || isNaN(input) && !input.length) {
          input = '';
        }
        return String(input);
      }

      function contains(str, elem) {
        assertString(str);
        return str.indexOf(toString(elem)) >= 0;
      }

      function matches(str, pattern, modifiers) {
        assertString(str);
        if (Object.prototype.toString.call(pattern) !== '[object RegExp]') {
          pattern = new RegExp(pattern, modifiers);
        }
        return pattern.test(str);
      }

      function merge() {
        var obj = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {};
        var defaults = arguments[1];

        for (var key in defaults) {
          if (typeof obj[key] === 'undefined') {
            obj[key] = defaults[key];
          }
        }
        return obj;
      }

      /* eslint-disable prefer-rest-params */
      function isByteLength(str, options) {
        assertString(str);
        var min = void 0;
        var max = void 0;
        if ((typeof options === 'undefined' ? 'undefined' : _typeof(options)) === 'object') {
          min = options.min || 0;
          max = options.max;
        } else {
          // backwards compatibility: isByteLength(str, min [, max])
          min = arguments[1];
          max = arguments[2];
        }
        var len = encodeURI(str).split(/%..|./).length - 1;
        return len >= min && (typeof max === 'undefined' || len <= max);
      }

      var default_fqdn_options = {
        require_tld: true,
        allow_underscores: false,
        allow_trailing_dot: false
      };

      function isFDQN(str, options) {
        assertString(str);
        options = merge(options, default_fqdn_options);

        /* Remove the optional trailing dot before checking validity */
        if (options.allow_trailing_dot && str[str.length - 1] === '.') {
          str = str.substring(0, str.length - 1);
        }
        var parts = str.split('.');
        if (options.require_tld) {
          var tld = parts.pop();
          if (!parts.length || !/^([a-z\u00a1-\uffff]{2,}|xn[a-z0-9-]{2,})$/i.test(tld)) {
            return false;
          }
        }
        for (var part, i = 0; i < parts.length; i++) {
          part = parts[i];
          if (options.allow_underscores) {
            part = part.replace(/_/g, '');
          }
          if (!/^[a-z\u00a1-\uffff0-9-]+$/i.test(part)) {
            return false;
          }
          if (/[\uff01-\uff5e]/.test(part)) {
            // disallow full-width chars
            return false;
          }
          if (part[0] === '-' || part[part.length - 1] === '-') {
            return false;
          }
        }
        return true;
      }

      var default_email_options = {
        allow_display_name: false,
        require_display_name: false,
        allow_utf8_local_part: true,
        require_tld: true
      };

      /* eslint-disable max-len */
      /* eslint-disable no-control-regex */
      var displayName = /^[a-z\d!#\$%&'\*\+\-\/=\?\^_`{\|}~\.\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]+[a-z\d!#\$%&'\*\+\-\/=\?\^_`{\|}~\.\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF\s]*<(.+)>$/i;
      var emailUserPart = /^[a-z\d!#\$%&'\*\+\-\/=\?\^_`{\|}~]+$/i;
      var quotedEmailUser = /^([\s\x01-\x08\x0b\x0c\x0e-\x1f\x7f\x21\x23-\x5b\x5d-\x7e]|(\\[\x01-\x09\x0b\x0c\x0d-\x7f]))*$/i;
      var emailUserUtf8Part = /^[a-z\d!#\$%&'\*\+\-\/=\?\^_`{\|}~\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]+$/i;
      var quotedEmailUserUtf8 = /^([\s\x01-\x08\x0b\x0c\x0e-\x1f\x7f\x21\x23-\x5b\x5d-\x7e\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]|(\\[\x01-\x09\x0b\x0c\x0d-\x7f\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]))*$/i;
      /* eslint-enable max-len */
      /* eslint-enable no-control-regex */

      function isEmail(str, options) {
        assertString(str);
        options = merge(options, default_email_options);

        if (options.require_display_name || options.allow_display_name) {
          var display_email = str.match(displayName);
          if (display_email) {
            str = display_email[1];
          } else if (options.require_display_name) {
            return false;
          }
        }

        var parts = str.split('@');
        var domain = parts.pop();
        var user = parts.join('@');

        var lower_domain = domain.toLowerCase();
        if (lower_domain === 'gmail.com' || lower_domain === 'googlemail.com') {
          user = user.replace(/\./g, '').toLowerCase();
        }

        if (!isByteLength(user, { max: 64 }) || !isByteLength(domain, { max: 256 })) {
          return false;
        }

        if (!isFDQN(domain, { require_tld: options.require_tld })) {
          return false;
        }

        if (user[0] === '"') {
          user = user.slice(1, user.length - 1);
          return options.allow_utf8_local_part ? quotedEmailUserUtf8.test(user) : quotedEmailUser.test(user);
        }

        var pattern = options.allow_utf8_local_part ? emailUserUtf8Part : emailUserPart;

        var user_parts = user.split('.');
        for (var i = 0; i < user_parts.length; i++) {
          if (!pattern.test(user_parts[i])) {
            return false;
          }
        }

        return true;
      }

      var ipv4Maybe = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
      var ipv6Block = /^[0-9A-F]{1,4}$/i;

      function isIP(str) {
        var version = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : '';

        assertString(str);
        version = String(version);
        if (!version) {
          return isIP(str, 4) || isIP(str, 6);
        } else if (version === '4') {
          if (!ipv4Maybe.test(str)) {
            return false;
          }
          var parts = str.split('.').sort(function (a, b) {
            return a - b;
          });
          return parts[3] <= 255;
        } else if (version === '6') {
          var blocks = str.split(':');
          var foundOmissionBlock = false; // marker to indicate ::

          // At least some OS accept the last 32 bits of an IPv6 address
          // (i.e. 2 of the blocks) in IPv4 notation, and RFC 3493 says
          // that '::ffff:a.b.c.d' is valid for IPv4-mapped IPv6 addresses,
          // and '::a.b.c.d' is deprecated, but also valid.
          var foundIPv4TransitionBlock = isIP(blocks[blocks.length - 1], 4);
          var expectedNumberOfBlocks = foundIPv4TransitionBlock ? 7 : 8;

          if (blocks.length > expectedNumberOfBlocks) {
            return false;
          }
          // initial or final ::
          if (str === '::') {
            return true;
          } else if (str.substr(0, 2) === '::') {
            blocks.shift();
            blocks.shift();
            foundOmissionBlock = true;
          } else if (str.substr(str.length - 2) === '::') {
            blocks.pop();
            blocks.pop();
            foundOmissionBlock = true;
          }

          for (var i = 0; i < blocks.length; ++i) {
            // test for a :: which can not be at the string start/end
            // since those cases have been handled above
            if (blocks[i] === '' && i > 0 && i < blocks.length - 1) {
              if (foundOmissionBlock) {
                return false; // multiple :: in address
              }
              foundOmissionBlock = true;
            } else if (foundIPv4TransitionBlock && i === blocks.length - 1) {
              // it has been checked before that the last
              // block is a valid IPv4 address
            } else if (!ipv6Block.test(blocks[i])) {
              return false;
            }
          }
          if (foundOmissionBlock) {
            return blocks.length >= 1;
          }
          return blocks.length === expectedNumberOfBlocks;
        }
        return false;
      }

      var default_url_options = {
        protocols: ['http', 'https', 'ftp'],
        require_tld: true,
        require_protocol: false,
        require_host: true,
        require_valid_protocol: true,
        allow_underscores: false,
        allow_trailing_dot: false,
        allow_protocol_relative_urls: false
      };

      var wrapped_ipv6 = /^\[([^\]]+)\](?::([0-9]+))?$/;

      function isRegExp(obj) {
        return Object.prototype.toString.call(obj) === '[object RegExp]';
      }

      function checkHost(host, matches) {
        for (var i = 0; i < matches.length; i++) {
          var match = matches[i];
          if (host === match || isRegExp(match) && match.test(host)) {
            return true;
          }
        }
        return false;
      }

      function isURL(url, options) {
        assertString(url);
        if (!url || url.length >= 2083 || /[\s<>]/.test(url)) {
          return false;
        }
        if (url.indexOf('mailto:') === 0) {
          return false;
        }
        options = merge(options, default_url_options);
        var protocol = void 0,
            auth = void 0,
            host = void 0,
            hostname = void 0,
            port = void 0,
            port_str = void 0,
            split = void 0,
            ipv6 = void 0;

        split = url.split('#');
        url = split.shift();

        split = url.split('?');
        url = split.shift();

        split = url.split('://');
        if (split.length > 1) {
          protocol = split.shift();
          if (options.require_valid_protocol && options.protocols.indexOf(protocol) === -1) {
            return false;
          }
        } else if (options.require_protocol) {
          return false;
        } else if (options.allow_protocol_relative_urls && url.substr(0, 2) === '//') {
          split[0] = url.substr(2);
        }
        url = split.join('://');

        split = url.split('/');
        url = split.shift();

        if (url === '' && !options.require_host) {
          return true;
        }

        split = url.split('@');
        if (split.length > 1) {
          auth = split.shift();
          if (auth.indexOf(':') >= 0 && auth.split(':').length > 2) {
            return false;
          }
        }
        hostname = split.join('@');

        port_str = ipv6 = null;
        var ipv6_match = hostname.match(wrapped_ipv6);
        if (ipv6_match) {
          host = '';
          ipv6 = ipv6_match[1];
          port_str = ipv6_match[2] || null;
        } else {
          split = hostname.split(':');
          host = split.shift();
          if (split.length) {
            port_str = split.join(':');
          }
        }

        if (port_str !== null) {
          port = parseInt(port_str, 10);
          if (!/^[0-9]+$/.test(port_str) || port <= 0 || port > 65535) {
            return false;
          }
        }

        if (!isIP(host) && !isFDQN(host, options) && (!ipv6 || !isIP(ipv6, 6)) && host !== 'localhost') {
          return false;
        }

        host = host || ipv6;

        if (options.host_whitelist && !checkHost(host, options.host_whitelist)) {
          return false;
        }
        if (options.host_blacklist && checkHost(host, options.host_blacklist)) {
          return false;
        }

        return true;
      }

      var macAddress = /^([0-9a-fA-F][0-9a-fA-F]:){5}([0-9a-fA-F][0-9a-fA-F])$/;

      function isMACAddress(str) {
        assertString(str);
        return macAddress.test(str);
      }

      function isBoolean(str) {
        assertString(str);
        return ['true', 'false', '1', '0'].indexOf(str) >= 0;
      }

      var alpha = {
        'en-US': /^[A-Z]+$/i,
        'cs-CZ': /^[A-ZÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ]+$/i,
        'da-DK': /^[A-ZÆØÅ]+$/i,
        'de-DE': /^[A-ZÄÖÜß]+$/i,
        'es-ES': /^[A-ZÁÉÍÑÓÚÜ]+$/i,
        'fr-FR': /^[A-ZÀÂÆÇÉÈÊËÏÎÔŒÙÛÜŸ]+$/i,
        'nl-NL': /^[A-ZÉËÏÓÖÜ]+$/i,
        'hu-HU': /^[A-ZÁÉÍÓÖŐÚÜŰ]+$/i,
        'pl-PL': /^[A-ZĄĆĘŚŁŃÓŻŹ]+$/i,
        'pt-PT': /^[A-ZÃÁÀÂÇÉÊÍÕÓÔÚÜ]+$/i,
        'ru-RU': /^[А-ЯЁ]+$/i,
        'sr-RS@latin': /^[A-ZČĆŽŠĐ]+$/i,
        'sr-RS': /^[А-ЯЂЈЉЊЋЏ]+$/i,
        'tr-TR': /^[A-ZÇĞİıÖŞÜ]+$/i,
        'uk-UA': /^[А-ЩЬЮЯЄIЇҐ]+$/i,
        ar: /^[ءآأؤإئابةتثجحخدذرزسشصضطظعغفقكلمنهوىيًٌٍَُِّْٰ]+$/
      };

      var alphanumeric = {
        'en-US': /^[0-9A-Z]+$/i,
        'cs-CZ': /^[0-9A-ZÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ]+$/i,
        'da-DK': /^[0-9A-ZÆØÅ]$/i,
        'de-DE': /^[0-9A-ZÄÖÜß]+$/i,
        'es-ES': /^[0-9A-ZÁÉÍÑÓÚÜ]+$/i,
        'fr-FR': /^[0-9A-ZÀÂÆÇÉÈÊËÏÎÔŒÙÛÜŸ]+$/i,
        'hu-HU': /^[0-9A-ZÁÉÍÓÖŐÚÜŰ]+$/i,
        'nl-NL': /^[0-9A-ZÉËÏÓÖÜ]+$/i,
        'pl-PL': /^[0-9A-ZĄĆĘŚŁŃÓŻŹ]+$/i,
        'pt-PT': /^[0-9A-ZÃÁÀÂÇÉÊÍÕÓÔÚÜ]+$/i,
        'ru-RU': /^[0-9А-ЯЁ]+$/i,
        'sr-RS@latin': /^[0-9A-ZČĆŽŠĐ]+$/i,
        'sr-RS': /^[0-9А-ЯЂЈЉЊЋЏ]+$/i,
        'tr-TR': /^[0-9A-ZÇĞİıÖŞÜ]+$/i,
        'uk-UA': /^[0-9А-ЩЬЮЯЄIЇҐ]+$/i,
        ar: /^[٠١٢٣٤٥٦٧٨٩0-9ءآأؤإئابةتثجحخدذرزسشصضطظعغفقكلمنهوىيًٌٍَُِّْٰ]+$/
      };

      var englishLocales = ['AU', 'GB', 'HK', 'IN', 'NZ', 'ZA', 'ZM'];

      for (var locale, i = 0; i < englishLocales.length; i++) {
        locale = 'en-' + englishLocales[i];
        alpha[locale] = alpha['en-US'];
        alphanumeric[locale] = alphanumeric['en-US'];
      }

      alpha['pt-BR'] = alpha['pt-PT'];
      alphanumeric['pt-BR'] = alphanumeric['pt-PT'];

      // Source: http://www.localeplanet.com/java/
      var arabicLocales = ['AE', 'BH', 'DZ', 'EG', 'IQ', 'JO', 'KW', 'LB', 'LY', 'MA', 'QM', 'QA', 'SA', 'SD', 'SY', 'TN', 'YE'];

      for (var _locale, _i = 0; _i < arabicLocales.length; _i++) {
        _locale = 'ar-' + arabicLocales[_i];
        alpha[_locale] = alpha.ar;
        alphanumeric[_locale] = alphanumeric.ar;
      }

      function isAlpha(str) {
        var locale = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'en-US';

        assertString(str);
        if (locale in alpha) {
          return alpha[locale].test(str);
        }
        throw new Error('Invalid locale \'' + locale + '\'');
      }

      function isAlphanumeric(str) {
        var locale = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'en-US';

        assertString(str);
        if (locale in alphanumeric) {
          return alphanumeric[locale].test(str);
        }
        throw new Error('Invalid locale \'' + locale + '\'');
      }

      var numeric = /^[-+]?[0-9]+$/;

      function isNumeric(str) {
        assertString(str);
        return numeric.test(str);
      }

      function isLowercase(str) {
        assertString(str);
        return str === str.toLowerCase();
      }

      function isUppercase(str) {
        assertString(str);
        return str === str.toUpperCase();
      }

      /* eslint-disable no-control-regex */
      var ascii = /^[\x00-\x7F]+$/;
      /* eslint-enable no-control-regex */

      function isAscii(str) {
        assertString(str);
        return ascii.test(str);
      }

      var fullWidth = /[^\u0020-\u007E\uFF61-\uFF9F\uFFA0-\uFFDC\uFFE8-\uFFEE0-9a-zA-Z]/;

      function isFullWidth(str) {
        assertString(str);
        return fullWidth.test(str);
      }

      var halfWidth = /[\u0020-\u007E\uFF61-\uFF9F\uFFA0-\uFFDC\uFFE8-\uFFEE0-9a-zA-Z]/;

      function isHalfWidth(str) {
        assertString(str);
        return halfWidth.test(str);
      }

      function isVariableWidth(str) {
        assertString(str);
        return fullWidth.test(str) && halfWidth.test(str);
      }

      /* eslint-disable no-control-regex */
      var multibyte = /[^\x00-\x7F]/;
      /* eslint-enable no-control-regex */

      function isMultibyte(str) {
        assertString(str);
        return multibyte.test(str);
      }

      var surrogatePair = /[\uD800-\uDBFF][\uDC00-\uDFFF]/;

      function isSurrogatePair(str) {
        assertString(str);
        return surrogatePair.test(str);
      }

      var int = /^(?:[-+]?(?:0|[1-9][0-9]*))$/;
      var intLeadingZeroes = /^[-+]?[0-9]+$/;

      function isInt(str, options) {
        assertString(str);
        options = options || {};

        // Get the regex to use for testing, based on whether
        // leading zeroes are allowed or not.
        var regex = options.hasOwnProperty('allow_leading_zeroes') && !options.allow_leading_zeroes ? int : intLeadingZeroes;

        // Check min/max/lt/gt
        var minCheckPassed = !options.hasOwnProperty('min') || str >= options.min;
        var maxCheckPassed = !options.hasOwnProperty('max') || str <= options.max;
        var ltCheckPassed = !options.hasOwnProperty('lt') || str < options.lt;
        var gtCheckPassed = !options.hasOwnProperty('gt') || str > options.gt;

        return regex.test(str) && minCheckPassed && maxCheckPassed && ltCheckPassed && gtCheckPassed;
      }

      var float = /^(?:[-+])?(?:[0-9]+)?(?:\.[0-9]*)?(?:[eE][\+\-]?(?:[0-9]+))?$/;

      function isFloat(str, options) {
        assertString(str);
        options = options || {};
        if (str === '' || str === '.') {
          return false;
        }
        return float.test(str) && (!options.hasOwnProperty('min') || str >= options.min) && (!options.hasOwnProperty('max') || str <= options.max) && (!options.hasOwnProperty('lt') || str < options.lt) && (!options.hasOwnProperty('gt') || str > options.gt);
      }

      var decimal = /^[-+]?([0-9]+|\.[0-9]+|[0-9]+\.[0-9]+)$/;

      function isDecimal(str) {
        assertString(str);
        return str !== '' && decimal.test(str);
      }

      var hexadecimal = /^[0-9A-F]+$/i;

      function isHexadecimal(str) {
        assertString(str);
        return hexadecimal.test(str);
      }

      function isDivisibleBy(str, num) {
        assertString(str);
        return toFloat(str) % parseInt(num, 10) === 0;
      }

      var hexcolor = /^#?([0-9A-F]{3}|[0-9A-F]{6})$/i;

      function isHexColor(str) {
        assertString(str);
        return hexcolor.test(str);
      }

      var md5 = /^[a-f0-9]{32}$/;

      function isMD5(str) {
        assertString(str);
        return md5.test(str);
      }

      function isJSON(str) {
        assertString(str);
        try {
          var obj = JSON.parse(str);
          return !!obj && (typeof obj === 'undefined' ? 'undefined' : _typeof(obj)) === 'object';
        } catch (e) {/* ignore */}
        return false;
      }

      function isEmpty(str) {
        assertString(str);
        return str.length === 0;
      }

      /* eslint-disable prefer-rest-params */
      function isLength(str, options) {
        assertString(str);
        var min = void 0;
        var max = void 0;
        if ((typeof options === 'undefined' ? 'undefined' : _typeof(options)) === 'object') {
          min = options.min || 0;
          max = options.max;
        } else {
          // backwards compatibility: isLength(str, min [, max])
          min = arguments[1];
          max = arguments[2];
        }
        var surrogatePairs = str.match(/[\uD800-\uDBFF][\uDC00-\uDFFF]/g) || [];
        var len = str.length - surrogatePairs.length;
        return len >= min && (typeof max === 'undefined' || len <= max);
      }

      var uuid = {
        3: /^[0-9A-F]{8}-[0-9A-F]{4}-3[0-9A-F]{3}-[0-9A-F]{4}-[0-9A-F]{12}$/i,
        4: /^[0-9A-F]{8}-[0-9A-F]{4}-4[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
        5: /^[0-9A-F]{8}-[0-9A-F]{4}-5[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
        all: /^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$/i
      };

      function isUUID(str) {
        var version = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'all';

        assertString(str);
        var pattern = uuid[version];
        return pattern && pattern.test(str);
      }

      function isMongoId(str) {
        assertString(str);
        return isHexadecimal(str) && str.length === 24;
      }

      function isAfter(str) {
        var date = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : String(new Date());

        assertString(str);
        var comparison = toDate(date);
        var original = toDate(str);
        return !!(original && comparison && original > comparison);
      }

      function isBefore(str) {
        var date = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : String(new Date());

        assertString(str);
        var comparison = toDate(date);
        var original = toDate(str);
        return !!(original && comparison && original < comparison);
      }

      function isIn(str, options) {
        assertString(str);
        var i = void 0;
        if (Object.prototype.toString.call(options) === '[object Array]') {
          var array = [];
          for (i in options) {
            if ({}.hasOwnProperty.call(options, i)) {
              array[i] = toString(options[i]);
            }
          }
          return array.indexOf(str) >= 0;
        } else if ((typeof options === 'undefined' ? 'undefined' : _typeof(options)) === 'object') {
          return options.hasOwnProperty(str);
        } else if (options && typeof options.indexOf === 'function') {
          return options.indexOf(str) >= 0;
        }
        return false;
      }

      /* eslint-disable max-len */
      var creditCard = /^(?:4[0-9]{12}(?:[0-9]{3})?|5[1-5][0-9]{14}|(222[1-9]|22[3-9][0-9]|2[3-6][0-9]{2}|27[01][0-9]|2720)[0-9]{12}|6(?:011|5[0-9][0-9])[0-9]{12}|3[47][0-9]{13}|3(?:0[0-5]|[68][0-9])[0-9]{11}|(?:2131|1800|35\d{3})\d{11})|62[0-9]{14}$/;
      /* eslint-enable max-len */

      function isCreditCard(str) {
        assertString(str);
        var sanitized = str.replace(/[^0-9]+/g, '');
        if (!creditCard.test(sanitized)) {
          return false;
        }
        var sum = 0;
        var digit = void 0;
        var tmpNum = void 0;
        var shouldDouble = void 0;
        for (var i = sanitized.length - 1; i >= 0; i--) {
          digit = sanitized.substring(i, i + 1);
          tmpNum = parseInt(digit, 10);
          if (shouldDouble) {
            tmpNum *= 2;
            if (tmpNum >= 10) {
              sum += tmpNum % 10 + 1;
            } else {
              sum += tmpNum;
            }
          } else {
            sum += tmpNum;
          }
          shouldDouble = !shouldDouble;
        }
        return !!(sum % 10 === 0 ? sanitized : false);
      }

      var isin = /^[A-Z]{2}[0-9A-Z]{9}[0-9]$/;

      function isISIN(str) {
        assertString(str);
        if (!isin.test(str)) {
          return false;
        }

        var checksumStr = str.replace(/[A-Z]/g, function (character) {
          return parseInt(character, 36);
        });

        var sum = 0;
        var digit = void 0;
        var tmpNum = void 0;
        var shouldDouble = true;
        for (var i = checksumStr.length - 2; i >= 0; i--) {
          digit = checksumStr.substring(i, i + 1);
          tmpNum = parseInt(digit, 10);
          if (shouldDouble) {
            tmpNum *= 2;
            if (tmpNum >= 10) {
              sum += tmpNum + 1;
            } else {
              sum += tmpNum;
            }
          } else {
            sum += tmpNum;
          }
          shouldDouble = !shouldDouble;
        }

        return parseInt(str.substr(str.length - 1), 10) === (10000 - sum) % 10;
      }

      var isbn10Maybe = /^(?:[0-9]{9}X|[0-9]{10})$/;
      var isbn13Maybe = /^(?:[0-9]{13})$/;
      var factor = [1, 3];

      function isISBN(str) {
        var version = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : '';

        assertString(str);
        version = String(version);
        if (!version) {
          return isISBN(str, 10) || isISBN(str, 13);
        }
        var sanitized = str.replace(/[\s-]+/g, '');
        var checksum = 0;
        var i = void 0;
        if (version === '10') {
          if (!isbn10Maybe.test(sanitized)) {
            return false;
          }
          for (i = 0; i < 9; i++) {
            checksum += (i + 1) * sanitized.charAt(i);
          }
          if (sanitized.charAt(9) === 'X') {
            checksum += 10 * 10;
          } else {
            checksum += 10 * sanitized.charAt(9);
          }
          if (checksum % 11 === 0) {
            return !!sanitized;
          }
        } else if (version === '13') {
          if (!isbn13Maybe.test(sanitized)) {
            return false;
          }
          for (i = 0; i < 12; i++) {
            checksum += factor[i % 2] * sanitized.charAt(i);
          }
          if (sanitized.charAt(12) - (10 - checksum % 10) % 10 === 0) {
            return !!sanitized;
          }
        }
        return false;
      }

      var issn = '^\\d{4}-?\\d{3}[\\dX]$';

      function isISSN(str) {
        var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};

        assertString(str);
        var testIssn = issn;
        testIssn = options.require_hyphen ? testIssn.replace('?', '') : testIssn;
        testIssn = options.case_sensitive ? new RegExp(testIssn) : new RegExp(testIssn, 'i');
        if (!testIssn.test(str)) {
          return false;
        }
        var issnDigits = str.replace('-', '');
        var position = 8;
        var checksum = 0;
        var _iteratorNormalCompletion = true;
        var _didIteratorError = false;
        var _iteratorError = undefined;

        try {
          for (var _iterator = issnDigits[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {
            var digit = _step.value;

            var digitValue = digit.toUpperCase() === 'X' ? 10 : +digit;
            checksum += digitValue * position;
            --position;
          }
        } catch (err) {
          _didIteratorError = true;
          _iteratorError = err;
        } finally {
          try {
            if (!_iteratorNormalCompletion && _iterator.return) {
              _iterator.return();
            }
          } finally {
            if (_didIteratorError) {
              throw _iteratorError;
            }
          }
        }

        return checksum % 11 === 0;
      }

      /* eslint-disable max-len */
      var phones = {
        'ar-DZ': /^(\+?213|0)(5|6|7)\d{8}$/,
        'ar-SY': /^(!?(\+?963)|0)?9\d{8}$/,
        'ar-SA': /^(!?(\+?966)|0)?5\d{8}$/,
        'en-US': /^(\+?1)?[2-9]\d{2}[2-9](?!11)\d{6}$/,
        'cs-CZ': /^(\+?420)? ?[1-9][0-9]{2} ?[0-9]{3} ?[0-9]{3}$/,
        'de-DE': /^(\+?49[ \.\-])?([\(]{1}[0-9]{1,6}[\)])?([0-9 \.\-\/]{3,20})((x|ext|extension)[ ]?[0-9]{1,4})?$/,
        'da-DK': /^(\+?45)?(\d{8})$/,
        'el-GR': /^(\+?30)?(69\d{8})$/,
        'en-AU': /^(\+?61|0)4\d{8}$/,
        'en-GB': /^(\+?44|0)7\d{9}$/,
        // According to http://www.ofca.gov.hk/filemanager/ofca/en/content_311/no_plan.pdf
        'en-HK': /^(\+?852-?)?((4(04[01]|06\d|09[3-9]|20\d|2[2-9]\d|3[3-9]\d|[467]\d{2}|5[1-9]\d|81\d|82[1-9]|8[69]\d|92[3-9]|95[2-9]|98\d)|5([1-79]\d{2})|6(0[1-9]\d|[1-9]\d{2})|7(0[1-9]\d|10[4-79]|11[458]|1[24578]\d|13[24-9]|16[0-8]|19[24579]|21[02-79]|2[456]\d|27[13-6]|3[456]\d|37[4578]|39[0146])|8(1[58]\d|2[45]\d|267|27[5-9]|2[89]\d|3[15-9]\d|32[5-8]|[46-9]\d{2}|5[013-9]\d)|9(0[1-9]\d|1[02-9]\d|[2-8]\d{2}))-?\d{4}|7130-?[0124-8]\d{3}|8167-?2\d{3})$/,
        'en-IN': /^(\+?91|0)?[789]\d{9}$/,
        'en-NG': /^(\+?234|0)?[789]\d{9}$/,
        'en-NZ': /^(\+?64|0)2\d{7,9}$/,
        'en-ZA': /^(\+?27|0)\d{9}$/,
        'en-ZM': /^(\+?26)?09[567]\d{7}$/,
        'es-ES': /^(\+?34)?(6\d{1}|7[1234])\d{7}$/,
        'fi-FI': /^(\+?358|0)\s?(4(0|1|2|4|5)?|50)\s?(\d\s?){4,8}\d$/,
        'fr-FR': /^(\+?33|0)[67]\d{8}$/,
        'he-IL': /^(\+972|0)([23489]|5[0248]|77)[1-9]\d{6}/,
        'hu-HU': /^(\+?36)(20|30|70)\d{7}$/,
        'lt-LT': /^(\+370|8)\d{8}$/,
        'id-ID': /^(\+?62|0[1-9])[\s|\d]+$/,
        'it-IT': /^(\+?39)?\s?3\d{2} ?\d{6,7}$/,
        'ko-KR': /^((\+?82)[ \-]?)?0?1([0|1|6|7|8|9]{1})[ \-]?\d{3,4}[ \-]?\d{4}$/,
        'ja-JP': /^(\+?81|0)\d{1,4}[ \-]?\d{1,4}[ \-]?\d{4}$/,
        'ms-MY': /^(\+?6?01){1}(([145]{1}(\-|\s)?\d{7,8})|([236789]{1}(\s|\-)?\d{7}))$/,
        'nb-NO': /^(\+?47)?[49]\d{7}$/,
        'nl-BE': /^(\+?32|0)4?\d{8}$/,
        'nn-NO': /^(\+?47)?[49]\d{7}$/,
        'pl-PL': /^(\+?48)? ?[5-8]\d ?\d{3} ?\d{2} ?\d{2}$/,
        'pt-BR': /^(\+?55|0)\-?[1-9]{2}\-?[2-9]{1}\d{3,4}\-?\d{4}$/,
        'pt-PT': /^(\+?351)?9[1236]\d{7}$/,
        'ro-RO': /^(\+?4?0)\s?7\d{2}(\/|\s|\.|\-)?\d{3}(\s|\.|\-)?\d{3}$/,
        'en-PK': /^((\+92)|(0092))-{0,1}\d{3}-{0,1}\d{7}$|^\d{11}$|^\d{4}-\d{7}$/,
        'ru-RU': /^(\+?7|8)?9\d{9}$/,
        'sr-RS': /^(\+3816|06)[- \d]{5,9}$/,
        'tr-TR': /^(\+?90|0)?5\d{9}$/,
        'vi-VN': /^(\+?84|0)?((1(2([0-9])|6([2-9])|88|99))|(9((?!5)[0-9])))([0-9]{7})$/,
        'zh-CN': /^(\+?0?86\-?)?1[345789]\d{9}$/,
        'zh-TW': /^(\+?886\-?|0)?9\d{8}$/
      };
      /* eslint-enable max-len */

      // aliases
      phones['en-CA'] = phones['en-US'];
      phones['fr-BE'] = phones['nl-BE'];
      phones['zh-HK'] = phones['en-HK'];

      function isMobilePhone(str, locale) {
        assertString(str);
        if (locale in phones) {
          return phones[locale].test(str);
        } else if (locale === 'any') {
          return !!Object.values(phones).find(phone => phone.test(str));
        }
        return false;
      }

      function currencyRegex(options) {
        var symbol = '(\\' + options.symbol.replace(/\./g, '\\.') + ')' + (options.require_symbol ? '' : '?'),
            negative = '-?',
            whole_dollar_amount_without_sep = '[1-9]\\d*',
            whole_dollar_amount_with_sep = '[1-9]\\d{0,2}(\\' + options.thousands_separator + '\\d{3})*',
            valid_whole_dollar_amounts = ['0', whole_dollar_amount_without_sep, whole_dollar_amount_with_sep],
            whole_dollar_amount = '(' + valid_whole_dollar_amounts.join('|') + ')?',
            decimal_amount = '(\\' + options.decimal_separator + '\\d{2})?';
        var pattern = whole_dollar_amount + decimal_amount;

        // default is negative sign before symbol, but there are two other options (besides parens)
        if (options.allow_negatives && !options.parens_for_negatives) {
          if (options.negative_sign_after_digits) {
            pattern += negative;
          } else if (options.negative_sign_before_digits) {
            pattern = negative + pattern;
          }
        }

        // South African Rand, for example, uses R 123 (space) and R-123 (no space)
        if (options.allow_negative_sign_placeholder) {
          pattern = '( (?!\\-))?' + pattern;
        } else if (options.allow_space_after_symbol) {
          pattern = ' ?' + pattern;
        } else if (options.allow_space_after_digits) {
          pattern += '( (?!$))?';
        }

        if (options.symbol_after_digits) {
          pattern += symbol;
        } else {
          pattern = symbol + pattern;
        }

        if (options.allow_negatives) {
          if (options.parens_for_negatives) {
            pattern = '(\\(' + pattern + '\\)|' + pattern + ')';
          } else if (!(options.negative_sign_before_digits || options.negative_sign_after_digits)) {
            pattern = negative + pattern;
          }
        }

        /* eslint-disable prefer-template */
        return new RegExp('^' +
        // ensure there's a dollar and/or decimal amount, and that
        // it doesn't start with a space or a negative sign followed by a space
        '(?!-? )(?=.*\\d)' + pattern + '$');
        /* eslint-enable prefer-template */
      }

      var default_currency_options = {
        symbol: '$',
        require_symbol: false,
        allow_space_after_symbol: false,
        symbol_after_digits: false,
        allow_negatives: true,
        parens_for_negatives: false,
        negative_sign_before_digits: false,
        negative_sign_after_digits: false,
        allow_negative_sign_placeholder: false,
        thousands_separator: ',',
        decimal_separator: '.',
        allow_space_after_digits: false
      };

      function isCurrency(str, options) {
        assertString(str);
        options = merge(options, default_currency_options);
        return currencyRegex(options).test(str);
      }

      /* eslint-disable max-len */
      // from http://goo.gl/0ejHHW
      var iso8601 = /^([\+-]?\d{4}(?!\d{2}\b))((-?)((0[1-9]|1[0-2])(\3([12]\d|0[1-9]|3[01]))?|W([0-4]\d|5[0-2])(-?[1-7])?|(00[1-9]|0[1-9]\d|[12]\d{2}|3([0-5]\d|6[1-6])))([T\s]((([01]\d|2[0-3])((:?)[0-5]\d)?|24:?00)([\.,]\d+(?!:))?)?(\17[0-5]\d([\.,]\d+)?)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?)?)?$/;
      /* eslint-enable max-len */

      function isISO8601 (str) {
        assertString(str);
        return iso8601.test(str);
      }

      function isBase64(str) {
        assertString(str);
        // Value length must be divisible by 4.
        var len = str.length;
        if (!len || len % 4 !== 0) {
          return false;
        }

        try {
          if (atob(str)) {
            return true;
          }
        } catch (e) {
          return false;
        }
      }

      var dataURI = /^\s*data:([a-z]+\/[a-z0-9\-\+]+(;[a-z\-]+=[a-z0-9\-]+)?)?(;base64)?,[a-z0-9!\$&',\(\)\*\+,;=\-\._~:@\/\?%\s]*\s*$/i; // eslint-disable-line max-len

      function isDataURI(str) {
        assertString(str);
        return dataURI.test(str);
      }

      function ltrim(str, chars) {
        assertString(str);
        var pattern = chars ? new RegExp('^[' + chars + ']+', 'g') : /^\s+/g;
        return str.replace(pattern, '');
      }

      function rtrim(str, chars) {
        assertString(str);
        var pattern = chars ? new RegExp('[' + chars + ']') : /\s/;

        var idx = str.length - 1;
        while (idx >= 0 && pattern.test(str[idx])) {
          idx--;
        }

        return idx < str.length ? str.substr(0, idx + 1) : str;
      }

      function trim(str, chars) {
        return rtrim(ltrim(str, chars), chars);
      }

      function escape(str) {
            assertString(str);
            return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#x27;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\//g, '&#x2F;').replace(/\\/g, '&#x5C;').replace(/`/g, '&#96;');
      }

      function unescape(str) {
            assertString(str);
            return str.replace(/&amp;/g, '&').replace(/&quot;/g, '"').replace(/&#x27;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&#x2F;/g, '/').replace(/&#96;/g, '`');
      }

      function blacklist(str, chars) {
        assertString(str);
        return str.replace(new RegExp('[' + chars + ']+', 'g'), '');
      }

      function stripLow(str, keep_new_lines) {
        assertString(str);
        var chars = keep_new_lines ? '\\x00-\\x09\\x0B\\x0C\\x0E-\\x1F\\x7F' : '\\x00-\\x1F\\x7F';
        return blacklist(str, chars);
      }

      function whitelist(str, chars) {
        assertString(str);
        return str.replace(new RegExp('[^' + chars + ']+', 'g'), '');
      }

      function isWhitelisted(str, chars) {
        assertString(str);
        for (var i = str.length - 1; i >= 0; i--) {
          if (chars.indexOf(str[i]) === -1) {
            return false;
          }
        }
        return true;
      }

      var default_normalize_email_options = {
        // The following options apply to all email addresses
        // Lowercases the local part of the email address.
        // Please note this may violate RFC 5321 as per http://stackoverflow.com/a/9808332/192024).
        // The domain is always lowercased, as per RFC 1035
        all_lowercase: true,

        // The following conversions are specific to GMail
        // Lowercases the local part of the GMail address (known to be case-insensitive)
        gmail_lowercase: true,
        // Removes dots from the local part of the email address, as that's ignored by GMail
        gmail_remove_dots: true,
        // Removes the subaddress (e.g. "+foo") from the email address
        gmail_remove_subaddress: true,
        // Conversts the googlemail.com domain to gmail.com
        gmail_convert_googlemaildotcom: true,

        // The following conversions are specific to Outlook.com / Windows Live / Hotmail
        // Lowercases the local part of the Outlook.com address (known to be case-insensitive)
        outlookdotcom_lowercase: true,
        // Removes the subaddress (e.g. "+foo") from the email address
        outlookdotcom_remove_subaddress: true,

        // The following conversions are specific to Yahoo
        // Lowercases the local part of the Yahoo address (known to be case-insensitive)
        yahoo_lowercase: true,
        // Removes the subaddress (e.g. "-foo") from the email address
        yahoo_remove_subaddress: true,

        // The following conversions are specific to iCloud
        // Lowercases the local part of the iCloud address (known to be case-insensitive)
        icloud_lowercase: true,
        // Removes the subaddress (e.g. "+foo") from the email address
        icloud_remove_subaddress: true
      };

      // List of domains used by iCloud
      var icloud_domains = ['icloud.com', 'me.com'];

      // List of domains used by Outlook.com and its predecessors
      // This list is likely incomplete.
      // Partial reference:
      // https://blogs.office.com/2013/04/17/outlook-com-gets-two-step-verification-sign-in-by-alias-and-new-international-domains/
      var outlookdotcom_domains = ['hotmail.at', 'hotmail.be', 'hotmail.ca', 'hotmail.cl', 'hotmail.co.il', 'hotmail.co.nz', 'hotmail.co.th', 'hotmail.co.uk', 'hotmail.com', 'hotmail.com.ar', 'hotmail.com.au', 'hotmail.com.br', 'hotmail.com.gr', 'hotmail.com.mx', 'hotmail.com.pe', 'hotmail.com.tr', 'hotmail.com.vn', 'hotmail.cz', 'hotmail.de', 'hotmail.dk', 'hotmail.es', 'hotmail.fr', 'hotmail.hu', 'hotmail.id', 'hotmail.ie', 'hotmail.in', 'hotmail.it', 'hotmail.jp', 'hotmail.kr', 'hotmail.lv', 'hotmail.my', 'hotmail.ph', 'hotmail.pt', 'hotmail.sa', 'hotmail.sg', 'hotmail.sk', 'live.be', 'live.co.uk', 'live.com', 'live.com.ar', 'live.com.mx', 'live.de', 'live.es', 'live.eu', 'live.fr', 'live.it', 'live.nl', 'msn.com', 'outlook.at', 'outlook.be', 'outlook.cl', 'outlook.co.il', 'outlook.co.nz', 'outlook.co.th', 'outlook.com', 'outlook.com.ar', 'outlook.com.au', 'outlook.com.br', 'outlook.com.gr', 'outlook.com.pe', 'outlook.com.tr', 'outlook.com.vn', 'outlook.cz', 'outlook.de', 'outlook.dk', 'outlook.es', 'outlook.fr', 'outlook.hu', 'outlook.id', 'outlook.ie', 'outlook.in', 'outlook.it', 'outlook.jp', 'outlook.kr', 'outlook.lv', 'outlook.my', 'outlook.ph', 'outlook.pt', 'outlook.sa', 'outlook.sg', 'outlook.sk', 'passport.com'];

      // List of domains used by Yahoo Mail
      // This list is likely incomplete
      var yahoo_domains = ['rocketmail.com', 'yahoo.ca', 'yahoo.co.uk', 'yahoo.com', 'yahoo.de', 'yahoo.fr', 'yahoo.in', 'yahoo.it', 'ymail.com'];

      function normalizeEmail(email, options) {
        options = merge(options, default_normalize_email_options);

        if (!isEmail(email)) {
          return false;
        }

        var raw_parts = email.split('@');
        var domain = raw_parts.pop();
        var user = raw_parts.join('@');
        var parts = [user, domain];

        // The domain is always lowercased, as it's case-insensitive per RFC 1035
        parts[1] = parts[1].toLowerCase();

        if (parts[1] === 'gmail.com' || parts[1] === 'googlemail.com') {
          // Address is GMail
          if (options.gmail_remove_subaddress) {
            parts[0] = parts[0].split('+')[0];
          }
          if (options.gmail_remove_dots) {
            parts[0] = parts[0].replace(/\./g, '');
          }
          if (!parts[0].length) {
            return false;
          }
          if (options.all_lowercase || options.gmail_lowercase) {
            parts[0] = parts[0].toLowerCase();
          }
          parts[1] = options.gmail_convert_googlemaildotcom ? 'gmail.com' : parts[1];
        } else if (~icloud_domains.indexOf(parts[1])) {
          // Address is iCloud
          if (options.icloud_remove_subaddress) {
            parts[0] = parts[0].split('+')[0];
          }
          if (!parts[0].length) {
            return false;
          }
          if (options.all_lowercase || options.icloud_lowercase) {
            parts[0] = parts[0].toLowerCase();
          }
        } else if (~outlookdotcom_domains.indexOf(parts[1])) {
          // Address is Outlook.com
          if (options.outlookdotcom_remove_subaddress) {
            parts[0] = parts[0].split('+')[0];
          }
          if (!parts[0].length) {
            return false;
          }
          if (options.all_lowercase || options.outlookdotcom_lowercase) {
            parts[0] = parts[0].toLowerCase();
          }
        } else if (~yahoo_domains.indexOf(parts[1])) {
          // Address is Yahoo
          if (options.yahoo_remove_subaddress) {
            var components = parts[0].split('-');
            parts[0] = components.length > 1 ? components.slice(0, -1).join('-') : components[0];
          }
          if (!parts[0].length) {
            return false;
          }
          if (options.all_lowercase || options.yahoo_lowercase) {
            parts[0] = parts[0].toLowerCase();
          }
        } else if (options.all_lowercase) {
          // Any other address
          parts[0] = parts[0].toLowerCase();
        }
        return parts.join('@');
      }

      // see http://isrc.ifpi.org/en/isrc-standard/code-syntax
      var isrc = /^[A-Z]{2}[0-9A-Z]{3}\d{2}\d{5}$/;

      function isISRC(str) {
        assertString(str);
        return isrc.test(str);
      }

      var cultureCodes = new Set(["ar", "bg", "ca", "zh-Hans", "cs", "da", "de",
      "el", "en", "es", "fi", "fr", "he", "hu", "is", "it", "ja", "ko", "nl", "no",
      "pl", "pt", "rm", "ro", "ru", "hr", "sk", "sq", "sv", "th", "tr", "ur", "id",
      "uk", "be", "sl", "et", "lv", "lt", "tg", "fa", "vi", "hy", "az", "eu", "hsb",
      "mk", "tn", "xh", "zu", "af", "ka", "fo", "hi", "mt", "se", "ga", "ms", "kk",
      "ky", "sw", "tk", "uz", "tt", "bn", "pa", "gu", "or", "ta", "te", "kn", "ml",
      "as", "mr", "sa", "mn", "bo", "cy", "km", "lo", "gl", "kok", "syr", "si", "iu",
      "am", "tzm", "ne", "fy", "ps", "fil", "dv", "ha", "yo", "quz", "nso", "ba", "lb",
      "kl", "ig", "ii", "arn", "moh", "br", "ug", "mi", "oc", "co", "gsw", "sah",
      "qut", "rw", "wo", "prs", "gd", "ar-SA", "bg-BG", "ca-ES", "zh-TW", "cs-CZ",
      "da-DK", "de-DE", "el-GR", "en-US", "fi-FI", "fr-FR", "he-IL", "hu-HU", "is-IS",
      "it-IT", "ja-JP", "ko-KR", "nl-NL", "nb-NO", "pl-PL", "pt-BR", "rm-CH", "ro-RO",
      "ru-RU", "hr-HR", "sk-SK", "sq-AL", "sv-SE", "th-TH", "tr-TR", "ur-PK", "id-ID",
      "uk-UA", "be-BY", "sl-SI", "et-EE", "lv-LV", "lt-LT", "tg-Cyrl-TJ", "fa-IR",
      "vi-VN", "hy-AM", "az-Latn-AZ", "eu-ES", "hsb-DE", "mk-MK", "tn-ZA", "xh-ZA",
      "zu-ZA", "af-ZA", "ka-GE", "fo-FO", "hi-IN", "mt-MT", "se-NO", "ms-MY", "kk-KZ",
      "ky-KG", "sw-KE", "tk-TM", "uz-Latn-UZ", "tt-RU", "bn-IN", "pa-IN", "gu-IN",
      "or-IN", "ta-IN", "te-IN", "kn-IN", "ml-IN", "as-IN", "mr-IN", "sa-IN", "mn-MN",
      "bo-CN", "cy-GB", "km-KH", "lo-LA", "gl-ES", "kok-IN", "syr-SY", "si-LK",
      "iu-Cans-CA", "am-ET", "ne-NP", "fy-NL", "ps-AF", "fil-PH", "dv-MV",
      "ha-Latn-NG", "yo-NG", "quz-BO", "nso-ZA", "ba-RU", "lb-LU", "kl-GL", "ig-NG",
      "ii-CN", "arn-CL", "moh-CA", "br-FR", "ug-CN", "mi-NZ", "oc-FR", "co-FR",
      "gsw-FR", "sah-RU", "qut-GT", "rw-RW", "wo-SN", "prs-AF", "gd-GB", "ar-IQ",
      "zh-CN", "de-CH", "en-GB", "es-MX", "fr-BE", "it-CH", "nl-BE", "nn-NO", "pt-PT",
      "sr-Latn-CS", "sv-FI", "az-Cyrl-AZ", "dsb-DE", "se-SE", "ga-IE", "ms-BN",
      "uz-Cyrl-UZ", "bn-BD", "mn-Mong-CN", "iu-Latn-CA", "tzm-Latn-DZ", "quz-EC",
      "ar-EG", "zh-HK", "de-AT", "en-AU", "es-ES", "fr-CA", "sr-Cyrl-CS", "se-FI",
      "quz-PE", "ar-LY", "zh-SG", "de-LU", "en-CA", "es-GT", "fr-CH", "hr-BA",
      "smj-NO", "ar-DZ", "zh-MO", "de-LI", "en-NZ", "es-CR", "fr-LU", "bs-Latn-BA",
      "smj-SE", "ar-MA", "en-IE", "es-PA", "fr-MC", "sr-Latn-BA", "sma-NO", "ar-TN",
      "en-ZA", "es-DO", "sr-Cyrl-BA", "sma-SE", "ar-OM", "en-JM", "es-VE",
      "bs-Cyrl-BA", "sms-FI", "ar-YE", "en-029", "es-CO", "sr-Latn-RS", "smn-FI",
      "ar-SY", "en-BZ", "es-PE", "sr-Cyrl-RS", "ar-JO", "en-TT", "es-AR", "sr-Latn-ME",
      "ar-LB", "en-ZW", "es-EC", "sr-Cyrl-ME", "ar-KW", "en-PH", "es-CL", "ar-AE",
      "es-UY", "ar-BH", "es-PY", "ar-QA", "en-IN", "es-BO", "en-MY", "es-SV", "en-SG",
      "es-HN", "es-NI", "es-PR", "es-US", "bs-Cyrl", "bs-Latn", "sr-Cyrl", "sr-Latn",
      "smn", "az-Cyrl", "sms", "zh", "nn", "bs", "az-Latn", "sma", "uz-Cyrl",
      "mn-Cyrl", "iu-Cans", "zh-Hant", "nb", "sr", "tg-Cyrl", "dsb", "smj", "uz-Latn",
      "mn-Mong", "iu-Latn", "tzm-Latn", "ha-Latn", "zh-CHS", "zh-CHT"]);

      function isRFC5646(str) {
        assertString(str);
        // According to the spec these codes are case sensitive so we can check the
        // string directly.
        return cultureCodes.has(str);
      }

      var semver =  /^v?(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(-(0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(\.(0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*)?(\+[0-9a-zA-Z-]+(\.[0-9a-zA-Z-]+)*)?$/i

      function isSemVer(str) {
        assertString(str);
        return semver.test(str);
      }

      var rgbcolor =  /^rgb?\(\s*(0|[1-9]\d?|1\d\d?|2[0-4]\d|25[0-5])\s*,\s*(0|[1-9]\d?|1\d\d?|2[0-4]\d|25[0-5])\s*,\s*(0|[1-9]\d?|1\d\d?|2[0-4]\d|25[0-5])\s*\)$/i

      function isRGBColor(str) {
        assertString(str);
        return rgbcolor.test(str);
      }

      var version = '7.0.0';

      var validator = {
        blacklist: blacklist,
        contains: contains,
        equals: equals,
        escape: escape,
        isAfter: isAfter,
        isAlpha: isAlpha,
        isAlphanumeric: isAlphanumeric,
        isAscii: isAscii,
        isBase64: isBase64,
        isBefore: isBefore,
        isBoolean: isBoolean,
        isByteLength: isByteLength,
        isCreditCard: isCreditCard,
        isCurrency: isCurrency,
        isDataURI: isDataURI,
        isDecimal: isDecimal,
        isDivisibleBy: isDivisibleBy,
        isEmail: isEmail,
        isEmpty: isEmpty,
        isFloat: isFloat,
        isFQDN: isFDQN,
        isFullWidth: isFullWidth,
        isHalfWidth: isHalfWidth,
        isHexadecimal: isHexadecimal,
        isHexColor: isHexColor,
        isIn: isIn,
        isInt: isInt,
        isIP: isIP,
        isRFC5646: isRFC5646,
        isISBN: isISBN,
        isISIN: isISIN,
        isISO8601: isISO8601,
        isISRC: isISRC,
        isRGBColor: isRGBColor,
        isISSN: isISSN,
        isJSON: isJSON,
        isLength: isLength,
        isLowercase: isLowercase,
        isMACAddress: isMACAddress,
        isMD5: isMD5,
        isMobilePhone: isMobilePhone,
        isMongoId: isMongoId,
        isMultibyte: isMultibyte,
        isNumeric: isNumeric,
        isSemVer: isSemVer,
        isSurrogatePair: isSurrogatePair,
        isUppercase: isUppercase,
        isURL: isURL,
        isUUID: isUUID,
        isVariableWidth: isVariableWidth,
        isWhitelisted: isWhitelisted,
        ltrim: ltrim,
        matches: matches,
        normalizeEmail: normalizeEmail,
        rtrim: rtrim,
        stripLow: stripLow,
        toBoolean: toBoolean,
        toDate: toDate,
        toFloat: toFloat,
        toInt: toInt,
        toString: toString,
        trim: trim,
        unescape: unescape,
        version: version,
        whitelist: whitelist
      };

      return validator;
}));
