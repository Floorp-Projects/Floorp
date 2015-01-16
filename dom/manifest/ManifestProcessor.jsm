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
 *   .process(jsonText, manifestURL, docURL);
 *
 * TODO: The constructor should accept the UA's supported orientations.
 * TODO: The constructor should accept the UA's supported display modes.
 * TODO: hook up developer tools to issueDeveloperWarning (1086997).
 */
/*globals Components*/
/*exported EXPORTED_SYMBOLS */
'use strict';

this.EXPORTED_SYMBOLS = ['ManifestProcessor'];
const {
  utils: Cu,
  classes: Cc,
  interfaces: Ci
} = Components;
const imports = {};
Cu.import('resource://gre/modules/Services.jsm', imports);
Cu.importGlobalProperties(['URL']);
const securityManager = imports.Services.scriptSecurityManager;
const netutil = Cc['@mozilla.org/network/util;1'].getService(Ci.nsINetUtil);
const defaultDisplayMode = 'browser';
const displayModes = new Set([
  'fullscreen',
  'standalone',
  'minimal-ui',
  'browser'
]);
const orientationTypes = new Set([
  'any',
  'natural',
  'landscape',
  'portrait',
  'portrait-primary',
  'portrait-secondary',
  'landscape-primary',
  'landscape-secondary'
]);

this.ManifestProcessor = function ManifestProcessor() {};
/**
 * process method: processes json text into a clean manifest
 * that conforms with the W3C specification.
 * @param jsonText - the JSON string to be processd.
 * @param manifestURL - the URL of the manifest, to resolve URLs.
 * @param docURL - the URL of the owner doc, for security checks
 */
this.ManifestProcessor.prototype.process = function({
  jsonText: jsonText,
  manifestURL: manifestURL,
  docLocation: docURL
}) {
  /*
   * This helper function is used to extract values from manifest members.
   * It also reports conformance violations.
   */
  function extractValue(obj) {
    let value = obj.object[obj.property];
    //we need to special-case "array", as it's not a JS primitive
    const type = (Array.isArray(value)) ? 'array' : typeof value;

    if (type !== obj.expectedType) {
      if (type !== 'undefined') {
        let msg = `Expected the ${obj.objectName}'s ${obj.property}`;
        msg += `member to a be a ${obj.expectedType}.`;
        issueDeveloperWarning(msg);
      }
      value = undefined;
    }
    return value;
  }

  function issueDeveloperWarning(msg) {
    //https://bugzilla.mozilla.org/show_bug.cgi?id=1086997
  }

  function processNameMember(manifest) {
    const obj = {
      objectName: 'manifest',
      object: manifest,
      property: 'name',
      expectedType: 'string'
    };
    let value = extractValue(obj);
    return (value) ? value.trim() : value;
  }

  function processShortNameMember(manifest) {
    const obj = {
      objectName: 'manifest',
      object: manifest,
      property: 'short_name',
      expectedType: 'string'
    };
    let value = extractValue(obj);
    return (value) ? value.trim() : value;
  }

  function processOrientationMember(manifest) {
    const obj = {
      objectName: 'manifest',
      object: manifest,
      property: 'orientation',
      expectedType: 'string'
    };
    let value = extractValue(obj);
    value = (value) ? value.trim() : undefined;
    //The spec special-cases orientation to return the empty string
    return (orientationTypes.has(value)) ? value : '';
  }

  function processDisplayMember(manifest) {
    const obj = {
      objectName: 'manifest',
      object: manifest,
      property: 'display',
      expectedType: 'string'
    };

    let value = extractValue(obj);
    value = (value) ? value.trim() : value;
    return (displayModes.has(value)) ? value : defaultDisplayMode;
  }

  function processScopeMember(manifest, manifestURL, docURL, startURL) {
    const spec = {
        objectName: 'manifest',
        object: manifest,
        property: 'scope',
        expectedType: 'string',
        dontTrim: true
      },
      value = extractValue(spec);
    let scopeURL;
    try {
      scopeURL = new URL(value, manifestURL);
    } catch (e) {
      let msg = 'The URL of scope is invalid.';
      issueDeveloperWarning(msg);
      return undefined;
    }

    if (scopeURL.origin !== docURL.origin) {
      let msg = 'Scope needs to be same-origin as Document.';
      issueDeveloperWarning(msg);
      return undefined;
    }

    //If start URL is not within scope of scope URL:
    if (startURL && startURL.origin !== scopeURL.origin || !startURL.pathname.startsWith(scopeURL.pathname)) {
      let msg = 'The start URL is outside the scope, so scope is invalid.';
      issueDeveloperWarning(msg);
      return undefined;
    }
    return scopeURL;
  }

  function processStartURLMember(manifest, manifestURL, docURL) {
    const obj = {
      objectName: 'manifest',
      object: manifest,
      property: 'start_url',
      expectedType: 'string'
    };

    let value = extractValue(obj),
      result = new URL(docURL),
      targetURI = makeURI(result),
      sameOrigin = false,
      potentialResult,
      referrerURI;

    if (value === undefined || value === '') {
      return result;
    }

    try {
      potentialResult = new URL(value, manifestURL);
    } catch (e) {
      issueDeveloperWarning('Invalid URL.');
      return result;
    }
    referrerURI = makeURI(potentialResult);
    try {
      securityManager.checkSameOriginURI(referrerURI, targetURI, false);
      sameOrigin = true;
    } catch (e) {}
    if (!sameOrigin) {
      let msg = 'start_url must be same origin as document.';
      issueDeveloperWarning(msg);
    } else {
      result = potentialResult;
    }
    return result;

    //Converts a URL to a Gecko URI
    function makeURI(webURL) {
      return imports.Services.io.newURI(webURL.toString(), null, null);
    }
  }

  //Constants used by IconsProcessor
  const onlyDecimals = /^\d+$/,
    anyRegEx = new RegExp('any', 'i');

  function IconsProcessor() {}
  IconsProcessor.prototype.processIcons = function(manifest, baseURL) {
    const obj = {
        objectName: 'manifest',
        object: manifest,
        property: 'icons',
        expectedType: 'array'
      },
      icons = [];
    let value = extractValue(obj);

    if (Array.isArray(value)) {
      //filter out icons with no "src" or src is empty string
      let processableIcons = value.filter(
        icon => icon && Object.prototype.hasOwnProperty.call(icon, 'src') && icon.src !== ''
      );
      for (let potentialIcon of processableIcons) {
        let src = processSrcMember(potentialIcon, baseURL)
        if(src !== undefined){
          let icon = {
            src: src,
            type: processTypeMember(potentialIcon),
            sizes: processSizesMember(potentialIcon),
            density: processDensityMember(potentialIcon)
          };
          icons.push(icon);
        }
      }
    }
    return icons;

    function processTypeMember(icon) {
      const charset = {},
        hadCharset = {},
        obj = {
          objectName: 'icon',
          object: icon,
          property: 'type',
          expectedType: 'string'
        };
      let value = extractValue(obj),
        isParsable = (typeof value === 'string' && value.length > 0);
      value = (isParsable) ? netutil.parseContentType(value.trim(), charset, hadCharset) : undefined;
      return (value === '') ? undefined : value;
    }

    function processDensityMember(icon) {
      const hasDensity = Object.prototype.hasOwnProperty.call(icon, 'density'),
        rawValue = (hasDensity) ? icon.density : undefined,
        value = parseFloat(rawValue),
        result = (Number.isNaN(value) || value === +Infinity || value <= 0) ? 1.0 : value;
      return result;
    }

    function processSrcMember(icon, baseURL) {
      const obj = {
          objectName: 'icon',
          object: icon,
          property: 'src',
          expectedType: 'string'
        },
        value = extractValue(obj);
      let url;
      if (typeof value === 'string' && value.trim() !== '') {
        try {
          url = new URL(value, baseURL);
        } catch (e) {}
      }
      return url;
    }

    function processSizesMember(icon) {
      const sizes = new Set(),
        obj = {
          objectName: 'icon',
          object: icon,
          property: 'sizes',
          expectedType: 'string'
        };
      let value = extractValue(obj);
      value = (value) ? value.trim() : value;
      if (value) {
        //split on whitespace and filter out invalid values
        let validSizes = value.split(/\s+/).filter(isValidSizeValue);
        validSizes.forEach((size) => sizes.add(size));
      }
      return sizes;

      /*
       * Implementation of HTML's link@size attribute checker
       */
      function isValidSizeValue(size) {
        if (anyRegEx.test(size)) {
          return true;
        }
        size = size.toLowerCase();
        if (!size.contains('x') || size.indexOf('x') !== size.lastIndexOf('x')) {
          return false;
        }

        //split left of x for width, after x for height
        const width = size.substring(0, size.indexOf('x'));
        const height = size.substring(size.indexOf('x') + 1, size.length);
        const isValid = !(height.startsWith('0') || width.startsWith('0') || !onlyDecimals.test(width + height));
        return isValid;
      }
    }
  };

  function processIconsMember(manifest, manifestURL) {
    const iconsProcessor = new IconsProcessor();
    return iconsProcessor.processIcons(manifest, manifestURL);
  }

  //Processing starts here!
  let manifest = {};

  try {
    manifest = JSON.parse(jsonText);
    if (typeof manifest !== 'object' || manifest === null) {
      let msg = 'Manifest needs to be an object.';
      issueDeveloperWarning(msg);
      manifest = {};
    }
  } catch (e) {
    issueDeveloperWarning(e);
  }

  const processedManifest = {
    start_url: processStartURLMember(manifest, manifestURL, docURL),
    display: processDisplayMember(manifest),
    orientation: processOrientationMember(manifest),
    name: processNameMember(manifest),
    icons: processIconsMember(manifest, manifestURL),
    short_name: processShortNameMember(manifest)
  };
  processedManifest.scope = processScopeMember(manifest, manifestURL, docURL, processedManifest.start_url);
  return processedManifest;
};