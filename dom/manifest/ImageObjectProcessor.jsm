/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/*
 * ImageObjectProcessor
 * Implementation of Image Object processing algorithms from:
 * http://www.w3.org/TR/appmanifest/#image-object-and-its-members
 *
 * This is intended to be used in conjunction with ManifestProcessor.jsm
 *
 * Creates an object to process Image Objects as defined by the
 * W3C specification. This is used to process things like the
 * icon member and the splash_screen member.
 *
 * Usage:
 *
 *   .process(aManifest, aBaseURL, aMemberName);
 *
 */
/*exported EXPORTED_SYMBOLS*/
/*globals Components */
'use strict';
const {
  utils: Cu,
  interfaces: Ci,
  classes: Cc
} = Components;

Cu.importGlobalProperties(['URL']);
const netutil = Cc['@mozilla.org/network/util;1']
  .getService(Ci.nsINetUtil);

function ImageObjectProcessor(aConsole, aExtractor) {
  this.console = aConsole;
  this.extractor = aExtractor;
}

// Static getters
Object.defineProperties(ImageObjectProcessor, {
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

ImageObjectProcessor.prototype.process = function(
  aManifest, aBaseURL, aMemberName
) {
  const spec = {
    objectName: 'manifest',
    object: aManifest,
    property: aMemberName,
    expectedType: 'array',
    trim: false
  };
  const extractor = this.extractor;
  const images = [];
  const value = extractor.extractValue(spec);
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
    let value = extractor.extractValue(spec);
    if (value) {
      value = netutil.parseRequestContentType(value, charset, hadCharset);
    }
    return value || undefined;
  }

  function processSrcMember(aImage, aBaseURL) {
    const spec = {
      objectName: 'image',
      object: aImage,
      property: 'src',
      expectedType: 'string',
      trim: false
    };
    const value = extractor.extractValue(spec);
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
    const value = extractor.extractValue(spec);
    if (value) {
      // Split on whitespace and filter out invalid values.
      value.split(/\s+/)
        .filter(isValidSizeValue)
        .reduce((collector, size) => collector.add(size), sizes);
    }
    return (sizes.size) ? Array.from(sizes).join(" ") : undefined;
    // Implementation of HTML's link@size attribute checker.
    function isValidSizeValue(aSize) {
      const size = aSize.toLowerCase();
      if (ImageObjectProcessor.anyRegEx.test(aSize)) {
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
      const validDecimals = ImageObjectProcessor.decimals.test(w + h);
      return (validStarts && validDecimals);
    }
  }
};
this.ImageObjectProcessor = ImageObjectProcessor; // jshint ignore:line
this.EXPORTED_SYMBOLS = ['ImageObjectProcessor']; // jshint ignore:line
