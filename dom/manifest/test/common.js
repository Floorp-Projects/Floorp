/**
 * Common infrastructure for manifest tests.
 **/

'use strict';
const bsp = SpecialPowers.Cu.import('resource://gre/modules/ManifestProcessor.jsm'),
  processor = new bsp.ManifestProcessor(),
  manifestURL = new URL(document.location.origin + '/manifest.json'),
  docURL = new URL('', document.location.origin),
  seperators = '\u2028\u2029\u0020\u00A0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000',
  lineTerminators = '\u000D\u000A\u2028\u2029',
  whiteSpace = `${seperators}${lineTerminators}`,
  typeTests = [1, null, {},
    [], false
  ],
  data = {
    jsonText: '{}',
    manifestURL: manifestURL,
    docURL: docURL
  };
