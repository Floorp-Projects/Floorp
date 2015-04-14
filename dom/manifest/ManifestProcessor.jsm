/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* 
 * ManifestProcessor
 * Implementation of processing algorithms from:
 * http://www.w3.org/2008/webapps/manifest/
 *
 * Creates manifest processor that lets you process a JSON file
 * or individual parts of a manifest object. A manifest is just a
 * standard JS object that has been cleaned up.
 *
 *   .process({jsonText,manifestURL,docURL});
 *
 * TODO: The constructor should accept the UA's supported orientations.
 * TODO: The constructor should accept the UA's supported display modes.
 * TODO: hook up developer tools to console. (1086997).
 */
/*exported EXPORTED_SYMBOLS */
/*JSLint options in comment below: */
/*globals Components, XPCOMUtils*/
'use strict';
this.EXPORTED_SYMBOLS = ['ManifestProcessor'];
const imports = {};
const {
  utils: Cu,
  classes: Cc,
  interfaces: Ci
} = Components;
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.importGlobalProperties(['URL']);
XPCOMUtils.defineLazyModuleGetter(imports, 'Services',
  'resource://gre/modules/Services.jsm');
imports.netutil = Cc['@mozilla.org/network/util;1'].getService(Ci.nsINetUtil);
// Helper function extracts values from manifest members
// and reports conformance violations.
function extractValue({
  objectName,
  object,
  property,
  expectedType,
  trim
}, console) {
  const value = object[property];
  const isArray = Array.isArray(value);
  // We need to special-case "array", as it's not a JS primitive.
  const type = (isArray) ? 'array' : typeof value;
  if (type !== expectedType) {
    if (type !== 'undefined') {
      let msg = `Expected the ${objectName}'s ${property} `;
      msg += `member to a be a ${expectedType}.`;
      console.log(msg);
    }
    return undefined;
  }
  // Trim string and returned undefined if the empty string.
  const shouldTrim = expectedType === 'string' && value && trim;
  if (shouldTrim) {
    return value.trim() || undefined;
  }
  return value;
}
const displayModes = new Set(['fullscreen', 'standalone', 'minimal-ui',
  'browser'
]);
const orientationTypes = new Set(['any', 'natural', 'landscape', 'portrait',
  'portrait-primary', 'portrait-secondary', 'landscape-primary',
  'landscape-secondary'
]);
const {
  ConsoleAPI
} = Cu.import('resource://gre/modules/devtools/Console.jsm');

function ManifestProcessor() {}

// Static getters
Object.defineProperties(ManifestProcessor, {
  'defaultDisplayMode': {
    get: function() {
      return 'browser';
    }
  },
  'displayModes': {
    get: function() {
      return displayModes;
    }
  },
  'orientationTypes': {
    get: function() {
      return orientationTypes;
    }
  }
});

ManifestProcessor.prototype = {

  // process method: processes json text into a clean manifest
  // that conforms with the W3C specification. Takes an object
  // expecting the following dictionary items:
  //  * jsonText: the JSON string to be processd.
  //  * manifestURL: the URL of the manifest, to resolve URLs.
  //  * docURL: the URL of the owner doc, for security checks.
  process({
    jsonText: aJsonText,
    manifestURL: aManifestURL,
    docURL: aDocURL
  }) {
    const manifestURL = new URL(aManifestURL);
    const docURL = new URL(aDocURL);
    const console = new ConsoleAPI({
      prefix: 'Web Manifest: '
    });
    let rawManifest = {};
    try {
      rawManifest = JSON.parse(aJsonText);
    } catch (e) {}
    if (typeof rawManifest !== 'object' || rawManifest === null) {
      let msg = 'Manifest needs to be an object.';
      console.warn(msg);
      rawManifest = {};
    }
    const processedManifest = {
      start_url: processStartURLMember(rawManifest, manifestURL, docURL),
      display: processDisplayMember(rawManifest),
      orientation: processOrientationMember(rawManifest),
      name: processNameMember(rawManifest),
      icons: IconsProcessor.process(rawManifest, manifestURL, console),
      short_name: processShortNameMember(rawManifest),
    };
    processedManifest.scope = processScopeMember(rawManifest, manifestURL,
      docURL, new URL(processedManifest.start_url));

    return processedManifest;

    function processNameMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'name',
        expectedType: 'string',
        trim: true
      };
      return extractValue(spec, console);
    }

    function processShortNameMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'short_name',
        expectedType: 'string',
        trim: true
      };
      return extractValue(spec, console);
    }

    function processOrientationMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'orientation',
        expectedType: 'string',
        trim: true
      };
      const value = extractValue(spec, console);
      if (ManifestProcessor.orientationTypes.has(value)) {
        return value;
      }
      // The spec special-cases orientation to return the empty string.
      return '';
    }

    function processDisplayMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'display',
        expectedType: 'string',
        trim: true
      };
      const value = extractValue(spec, console);
      if (ManifestProcessor.displayModes.has(value)) {
        return value;
      }
      return ManifestProcessor.defaultDisplayMode;
    }

    function processScopeMember(aManifest, aManifestURL, aDocURL, aStartURL) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'scope',
        expectedType: 'string',
        trim: false
      };
      let scopeURL;
      const value = extractValue(spec, console);
      if (value === undefined || value === '') {
        return undefined;
      }
      try {
        scopeURL = new URL(value, aManifestURL);
      } catch (e) {
        let msg = 'The URL of scope is invalid.';
        console.warn(msg);
        return undefined;
      }
      if (scopeURL.origin !== aDocURL.origin) {
        let msg = 'Scope needs to be same-origin as Document.';
        console.warn(msg);
        return undefined;
      }
      // If start URL is not within scope of scope URL:
      let isSameOrigin = aStartURL && aStartURL.origin !== scopeURL.origin;
      if (isSameOrigin || !aStartURL.pathname.startsWith(scopeURL.pathname)) {
        let msg =
          'The start URL is outside the scope, so scope is invalid.';
        console.warn(msg);
        return undefined;
      }
      return scopeURL.href;
    }

    function processStartURLMember(aManifest, aManifestURL, aDocURL) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'start_url',
        expectedType: 'string',
        trim: false
      };
      let result = new URL(aDocURL).href;
      const value = extractValue(spec, console);
      if (value === undefined || value === '') {
        return result;
      }
      let potentialResult;
      try {
        potentialResult = new URL(value, aManifestURL);
      } catch (e) {
        console.warn('Invalid URL.');
        return result;
      }
      if (potentialResult.origin !== aDocURL.origin) {
        let msg = 'start_url must be same origin as document.';
        console.warn(msg);
      } else {
        result = potentialResult.href;
      }
      return result;
    }
  }
};
this.ManifestProcessor = ManifestProcessor;

function IconsProcessor() {}

// Static getters
Object.defineProperties(IconsProcessor, {
  'onlyDecimals': {
    get: function() {
      return /^\d+$/;
    }
  },
  'anyRegEx': {
    get: function() {
      return new RegExp('any', 'i');
    }
  }
});

IconsProcessor.process = function(aManifest, aBaseURL, console) {
  const spec = {
    objectName: 'manifest',
    object: aManifest,
    property: 'icons',
    expectedType: 'array',
    trim: false
  };
  const icons = [];
  const value = extractValue(spec, console);
  if (Array.isArray(value)) {
    // Filter out icons whose "src" is not useful.
    value.filter(item => !!processSrcMember(item, aBaseURL))
      .map(toIconObject)
      .forEach(icon => icons.push(icon));
  }
  return icons;

  function toIconObject(aIconData) {
    return {
      src: processSrcMember(aIconData, aBaseURL),
      type: processTypeMember(aIconData),
      sizes: processSizesMember(aIconData),
      density: processDensityMember(aIconData)
    };
  }

  function processTypeMember(aIcon) {
    const charset = {};
    const hadCharset = {};
    const spec = {
      objectName: 'icon',
      object: aIcon,
      property: 'type',
      expectedType: 'string',
      trim: true
    };
    let value = extractValue(spec, console);
    if (value) {
      value = imports.netutil.parseContentType(value, charset, hadCharset);
    }
    return value || undefined;
  }

  function processDensityMember(aIcon) {
    const value = parseFloat(aIcon.density);
    const validNum = Number.isNaN(value) || value === +Infinity || value <=
      0;
    return (validNum) ? 1.0 : value;
  }

  function processSrcMember(aIcon, aBaseURL) {
    const spec = {
      objectName: 'icon',
      object: aIcon,
      property: 'src',
      expectedType: 'string',
      trim: false
    };
    const value = extractValue(spec, console);
    let url;
    if (value && value.length) {
      try {
        url = new URL(value, aBaseURL).href;
      } catch (e) {}
    }
    return url;
  }

  function processSizesMember(aIcon) {
    const sizes = new Set(),
      spec = {
        objectName: 'icon',
        object: aIcon,
        property: 'sizes',
        expectedType: 'string',
        trim: true
      },
      value = extractValue(spec, console);
    if (value) {
      // Split on whitespace and filter out invalid values.
      value.split(/\s+/)
        .filter(isValidSizeValue)
        .forEach(size => sizes.add(size));
    }
    return sizes;
    // Implementation of HTML's link@size attribute checker.
    function isValidSizeValue(aSize) {
      const size = aSize.toLowerCase();
      if (IconsProcessor.anyRegEx.test(aSize)) {
        return true;
      }
      if (!size.contains('x') || size.indexOf('x') !== size.lastIndexOf('x')) {
        return false;
      }
      // Split left of x for width, after x for height.
      const widthAndHeight = size.split('x');
      const w = widthAndHeight.shift();
      const h = widthAndHeight.join('x');
      const validStarts = !w.startsWith('0') && !h.startsWith('0');
      const validDecimals = IconsProcessor.onlyDecimals.test(w + h);
      return (validStarts && validDecimals);
    }
  }
};
