/**
 * Common infrastructure for manifest tests.
 **/
/*globals SpecialPowers, ManifestProcessor*/
'use strict';
const {
  ManifestProcessor
} = SpecialPowers.Cu.import('resource://gre/modules/ManifestProcessor.jsm');
const processor = new ManifestProcessor();
const manifestURL = new URL(document.location.origin + '/manifest.json');
const docURL = document.location;
const seperators = '\u2028\u2029\u0020\u00A0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000';
const lineTerminators = '\u000D\u000A\u2028\u2029';
const whiteSpace = `${seperators}${lineTerminators}`;
const typeTests = [1, null, {},
  [], false
];
const data = {
  jsonText: '{}',
  manifestURL: manifestURL,
  docURL: docURL
};
