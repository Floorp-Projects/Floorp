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
 * Depends on ImageObjectProcessor to process things like
 * icons and splash_screens.
 *
 * TODO: The constructor should accept the UA's supported orientations.
 * TODO: The constructor should accept the UA's supported display modes.
 * TODO: hook up developer tools to console. (1086997).
 */
/*globals Components, ValueExtractor, ImageObjectProcessor, ConsoleAPI*/
'use strict';
const {
  utils: Cu
} = Components;
Cu.importGlobalProperties(['URL']);
const displayModes = new Set(['fullscreen', 'standalone', 'minimal-ui',
  'browser'
]);
const orientationTypes = new Set(['any', 'natural', 'landscape', 'portrait',
  'portrait-primary', 'portrait-secondary', 'landscape-primary',
  'landscape-secondary'
]);
Cu.import('resource://gre/modules/devtools/shared/Console.jsm');
// ValueExtractor is used by the various processors to get values
// from the manifest and to report errors.
Cu.import('resource://gre/modules/ValueExtractor.jsm');
// ImageObjectProcessor is used to process things like icons and images
Cu.import('resource://gre/modules/ImageObjectProcessor.jsm');

this.ManifestProcessor = { // jshint ignore:line
  get defaultDisplayMode() {
    return 'browser';
  },
  get displayModes() {
    return displayModes;
  },
  get orientationTypes() {
    return orientationTypes;
  },
  // process() method processes JSON text into a clean manifest
  // that conforms with the W3C specification. Takes an object
  // expecting the following dictionary items:
  //  * jsonText: the JSON string to be processed.
  //  * manifestURL: the URL of the manifest, to resolve URLs.
  //  * docURL: the URL of the owner doc, for security checks
  process({
    jsonText,
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
      rawManifest = JSON.parse(jsonText);
    } catch (e) {}
    if (typeof rawManifest !== 'object' || rawManifest === null) {
      let msg = 'Manifest needs to be an object.';
      console.warn(msg);
      rawManifest = {};
    }
    const extractor = new ValueExtractor(console);
    const imgObjProcessor = new ImageObjectProcessor(console, extractor);
    const processedManifest = {
      'lang': processLangMember(),
      'start_url': processStartURLMember(),
      'display': processDisplayMember.call(this),
      'orientation': processOrientationMember.call(this),
      'name': processNameMember(),
      'icons': imgObjProcessor.process(
        rawManifest, manifestURL, 'icons'
      ),
      'splash_screens': imgObjProcessor.process(
        rawManifest, manifestURL, 'splash_screens'
      ),
      'short_name': processShortNameMember(),
      'theme_color': processThemeColorMember(),
    };
    processedManifest.scope = processScopeMember();
    return processedManifest;

    function processNameMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'name',
        expectedType: 'string',
        trim: true
      };
      return extractor.extractValue(spec);
    }

    function processShortNameMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'short_name',
        expectedType: 'string',
        trim: true
      };
      return extractor.extractValue(spec);
    }

    function processOrientationMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'orientation',
        expectedType: 'string',
        trim: true
      };
      const value = extractor.extractValue(spec);
      if (this.orientationTypes.has(value)) {
        return value;
      }
      // The spec special-cases orientation to return the empty string.
      return '';
    }

    function processDisplayMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'display',
        expectedType: 'string',
        trim: true
      };
      const value = extractor.extractValue(spec);
      if (displayModes.has(value)) {
        return value;
      }
      return this.defaultDisplayMode;
    }

    function processScopeMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'scope',
        expectedType: 'string',
        trim: false
      };
      let scopeURL;
      const startURL = new URL(processedManifest.start_url);
      const value = extractor.extractValue(spec);
      if (value === undefined || value === '') {
        return undefined;
      }
      try {
        scopeURL = new URL(value, manifestURL);
      } catch (e) {
        let msg = 'The URL of scope is invalid.';
        console.warn(msg);
        return undefined;
      }
      if (scopeURL.origin !== docURL.origin) {
        let msg = 'Scope needs to be same-origin as Document.';
        console.warn(msg);
        return undefined;
      }
      // If start URL is not within scope of scope URL:
      let isSameOrigin = startURL && startURL.origin !== scopeURL.origin;
      if (isSameOrigin || !startURL.pathname.startsWith(scopeURL.pathname)) {
        let msg =
          'The start URL is outside the scope, so scope is invalid.';
        console.warn(msg);
        return undefined;
      }
      return scopeURL.href;
    }

    function processStartURLMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'start_url',
        expectedType: 'string',
        trim: false
      };
      let result = new URL(docURL).href;
      const value = extractor.extractValue(spec);
      if (value === undefined || value === '') {
        return result;
      }
      let potentialResult;
      try {
        potentialResult = new URL(value, manifestURL);
      } catch (e) {
        console.warn('Invalid URL.');
        return result;
      }
      if (potentialResult.origin !== docURL.origin) {
        let msg = 'start_url must be same origin as document.';
        console.warn(msg);
      } else {
        result = potentialResult.href;
      }
      return result;
    }

    function processThemeColorMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'theme_color',
        expectedType: 'string',
        trim: true
      };
      return extractor.extractColorValue(spec);
    }

    function processLangMember() {
      const spec = {
        objectName: 'manifest',
        object: rawManifest,
        property: 'lang',
        expectedType: 'string', trim: true
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
this.EXPORTED_SYMBOLS = ['ManifestProcessor']; // jshint ignore:line
