(function(global) {
  'use strict';
  // A minimal implementation of the interface.
  // Though this isn't particularly functional.
  // This is needed so that we can have a "working" IdP served
  // from two different locations in the tree.
  global.rtcIdentityProvider.register({
    generateAssertion: function(payload, origin, usernameHint) {
      dump('idp: generateAssertion(' + payload + ')\n');
      return Promise.resolve({
        idp: { domain: 'example.com', protocol: 'idp.js' },
        assertion: 'bogus'
      });
    },

    validateAssertion: function(assertion, origin) {
      dump('idp: validateAssertion(' + assertion + ')\n');
      return Promise.resolve({
        identity: 'user@example.com',
        contents: 'bogus'
      });
    }
  });
}(this));
