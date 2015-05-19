/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * ManifestImageObjectProcessor
 * Implementation of Image Object processing algorithms from:
 * http://www.w3.org/2008/webapps/manifest/#image-object-and-its-members
 *
 * This is intended to be used in conjunction with ManifestProcessor.jsm
 *
 * Creates an object to process Image Objects as defined by the
 * W3C specification. This is used to process things like the
 * icon member and the splash_screen member.
 *
 * Usage:
 *
 *   .process(aManifest, aBaseURL, aMemberName, console);
 *
 */
/*exported EXPORTED_SYMBOLS */
/*globals extractValue, Components*/
'use strict';
this.EXPORTED_SYMBOLS = ['ManifestImageObjectProcessor']; // jshint ignore:line
const imports = {};
const {
  utils: Cu,
  classes: Cc,
  interfaces: Ci
} = Components;
const scriptLoader = Cc['@mozilla.org/moz/jssubscript-loader;1']
  .getService(Ci.mozIJSSubScriptLoader);
scriptLoader.loadSubScript(
  'resource://gre/modules/manifestValueExtractor.js',
  this); // jshint ignore:line
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.importGlobalProperties(['URL']);
imports.netutil = Cc['@mozilla.org/network/util;1']
  .getService(Ci.nsINetUtil);
imports.DOMUtils = Cc['@mozilla.org/inspector/dom-utils;1']
  .getService(Ci.inIDOMUtils);

function ManifestImageObjectProcessor() {}

// Static getters
Object.defineProperties(ManifestImageObjectProcessor, {
  'decimals': {
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

ManifestImageObjectProcessor.process = function(
  aManifest, aBaseURL, aMemberName, console
) {
  const spec = {
    objectName: 'manifest',
    object: aManifest,
    property: aMemberName,
    expectedType: 'array',
    trim: false
  };
  const images = [];
  const value = extractValue(spec, console);
  if (Array.isArray(value)) {
    // Filter out images whose "src" is not useful.
    value.filter(item => !!processSrcMember(item, aBaseURL))
      .map(toImageObject)
      .forEach(image => images.push(image));
  }
  return images;

  function toImageObject(aImageSpec) {
    return {
      'src': processSrcMember(aImageSpec, aBaseURL),
      'type': processTypeMember(aImageSpec),
      'sizes': processSizesMember(aImageSpec),
      'density': processDensityMember(aImageSpec),
      'background_color': processBackgroundColorMember(aImageSpec)
    };
  }

  function processTypeMember(aImage) {
    const charset = {};
    const hadCharset = {};
    const spec = {
      objectName: 'image',
      object: aImage,
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

  function processDensityMember(aImage) {
    const value = parseFloat(aImage.density);
    const validNum = Number.isNaN(value) || value === +Infinity || value <=
      0;
    return (validNum) ? 1.0 : value;
  }

  function processSrcMember(aImage, aBaseURL) {
    const spec = {
      objectName: 'image',
      object: aImage,
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

  function processSizesMember(aImage) {
    const sizes = new Set();
    const spec = {
      objectName: 'image',
      object: aImage,
      property: 'sizes',
      expectedType: 'string',
      trim: true
    };
    const value = extractValue(spec, console);
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
      if (ManifestImageObjectProcessor.anyRegEx.test(aSize)) {
        return true;
      }
      if (!size.includes('x') || size.indexOf('x') !== size.lastIndexOf('x')) {
        return false;
      }
      // Split left of x for width, after x for height.
      const widthAndHeight = size.split('x');
      const w = widthAndHeight.shift();
      const h = widthAndHeight.join('x');
      const validStarts = !w.startsWith('0') && !h.startsWith('0');
      const validDecimals = ManifestImageObjectProcessor.decimals.test(w + h);
      return (validStarts && validDecimals);
    }
  }

  function processBackgroundColorMember(aImage) {
    const spec = {
      objectName: 'image',
      object: aImage,
      property: 'background_color',
      expectedType: 'string',
      trim: true
    };
    const value = extractValue(spec, console);
    let color;
    if (imports.DOMUtils.isValidCSSColor(value)) {
      color = value;
    } else {
      const msg = `background_color: ${value} is not a valid CSS color.`;
      console.warn(msg);
    }
    return color;
  }
};
this.ManifestImageObjectProcessor = ManifestImageObjectProcessor; // jshint ignore:line
