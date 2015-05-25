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
 * Depends on ManifestImageObjectProcessor to process things like
 * icons and splash_screens.
 *
 * TODO: The constructor should accept the UA's supported orientations.
 * TODO: The constructor should accept the UA's supported display modes.
 * TODO: hook up developer tools to console. (1086997).
 */
/*exported EXPORTED_SYMBOLS */
/*JSLint options in comment below: */
/*globals Components, XPCOMUtils, Intl*/
'use strict';
this.EXPORTED_SYMBOLS = ['ManifestProcessor']; // jshint ignore:line
const imports = {};
const {
  utils: Cu
} = Components;
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.importGlobalProperties(['URL']);
XPCOMUtils.defineLazyModuleGetter(imports, 'Services',
  'resource://gre/modules/Services.jsm');
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
const {
  ManifestImageObjectProcessor: ImgObjProcessor
} = Cu.import('resource://gre/modules/ManifestImageObjectProcessor.jsm');

const {
  ManifestValueExtractor
} = Cu.import('resource://gre/modules/ManifestValueExtractor.jsm');

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
  // process() method processes JSON text into a clean manifest
  // that conforms with the W3C specification. Takes an object
  // expecting the following dictionary items:
  //  * aJsonText: the JSON string to be processed.
  //  * aManifestURL: the URL of the manifest, to resolve URLs.
  //  * aDocURL: the URL of the owner doc, for security checks
  process({
    jsonText: aJsonText,
    manifestURL: aManifestURL,
    docURL: aDocURL
  }) {
    const console = new ConsoleAPI({
      prefix: 'Web Manifest: '
    });
    const manifestURL = new URL(aManifestURL);
    const docURL = new URL(aDocURL);
    let rawManifest = {};
    try {
      rawManifest = JSON.parse(aJsonText);
    } catch (e) {}
    if (typeof rawManifest !== 'object' || rawManifest === null) {
      let msg = 'Manifest needs to be an object.';
      console.warn(msg);
      rawManifest = {};
    }
    const extractor = new ManifestValueExtractor(console);
    const imgObjProcessor = new ImgObjProcessor(console, extractor);
    const processedManifest = {
      'lang': processLangMember(rawManifest),
      'start_url': processStartURLMember(rawManifest, manifestURL, docURL),
      'display': processDisplayMember(rawManifest),
      'orientation': processOrientationMember(rawManifest),
      'name': processNameMember(rawManifest),
      'icons': imgObjProcessor.process(
        rawManifest, manifestURL, 'icons'
      ),
      'splash_screens': imgObjProcessor.process(
        rawManifest, manifestURL, 'splash_screens'
      ),
      'short_name': processShortNameMember(rawManifest),
      'theme_color': processThemeColorMember(rawManifest),
    };
    processedManifest.scope = processScopeMember(rawManifest, manifestURL,
      docURL, new URL(processedManifest['start_url'])); // jshint ignore:line

    return processedManifest;

    function processNameMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'name',
        expectedType: 'string',
        trim: true
      };
      return extractor.extractValue(spec);
    }

    function processShortNameMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'short_name',
        expectedType: 'string',
        trim: true
      };
      return extractor.extractValue(spec);
    }

    function processOrientationMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'orientation',
        expectedType: 'string',
        trim: true
      };
      const value = extractor.extractValue(spec);
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
      const value = extractor.extractValue(spec);
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
      const value = extractor.extractValue(spec);
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
      const value = extractor.extractValue(spec);
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

    function processThemeColorMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'theme_color',
        expectedType: 'string',
        trim: true
      };
      return extractor.extractColorValue(spec);
    }

    function processLangMember(aManifest) {
      const spec = {
        objectName: 'manifest',
        object: aManifest,
        property: 'lang',
        expectedType: 'string',
        trim: true
      };
      let tag = extractor.extractValue(spec);
      // TODO: Check if tag is structurally valid.
      //       Cannot do this because we don't support Intl API on Android.
      //       https://bugzilla.mozilla.org/show_bug.cgi?id=864843
      //       https://github.com/tc39/ecma402/issues/5
      // TODO: perform canonicalization on the tag.
      //       Can't do this today because there is no direct means to
      //       access canonicalization algorithms through Intl API.
      //       https://github.com/tc39/ecma402/issues/5
      return tag;
    }
  }
};
this.ManifestProcessor = ManifestProcessor; // jshint ignore:line
