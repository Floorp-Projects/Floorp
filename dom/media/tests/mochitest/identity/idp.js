(function(global) {
  'use strict';

  // rather than create a million different IdP configurations and litter the
  // world with files all containing near-identical code, let's use the hash/URL
  // fragment as a way of generating instructions for the IdP
  var instructions = global.location.hash.replace('#', '').split(':');
  function is(target) {
    return function(instruction) {
      return instruction === target;
    };
  }

  function IDPJS() {
    this.domain = global.location.host;
    var path = global.location.pathname;
    this.protocol =
      path.substring(path.lastIndexOf('/') + 1) + global.location.hash;
    this.id = crypto.getRandomValues(new Uint8Array(10)).join('.');
  }

  IDPJS.prototype = {
    getLogin: function() {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', 'https://example.com/.well-known/idp-proxy/idp.sjs?' + this.id);
      return new Promise(resolve => {
        xhr.onload = e => resolve(xhr.status === 200);
        xhr.send();
      });
    },
    checkLogin: function(result) {
      return this.getLogin()
        .then(loggedIn => {
          if (loggedIn) {
            return result;
          }
          return Promise.reject({
            name: 'IdpLoginError',
            loginUrl: 'https://example.com/.well-known/idp-proxy/login.html#' +
              this.id
          });
        });
    },

    borkResult: function(result) {
      if (instructions.some(is('throw'))) {
        throw new Error('Throwing!');
      }
      if (instructions.some(is('fail'))) {
        return Promise.reject(new Error('Failing!'));
      }
      if (instructions.some(is('login'))) {
        return this.checkLogin(result);
      }
      if (instructions.some(is('hang'))) {
        return new Promise(r => {});
      }
      dump('idp: result=' + JSON.stringify(result) + '\n');
      return Promise.resolve(result);
    },

    _selectUsername: function(usernameHint) {
      var username = 'someone@' + this.domain;
      if (usernameHint) {
        var at = usernameHint.indexOf('@');
        if (at < 0) {
          username = usernameHint + '@' + this.domain;
        } else if (usernameHint.substring(at + 1) === this.domain) {
          username = usernameHint;
        }
      }
      return username;
    },

    generateAssertion: function(payload, origin, usernameHint) {
      dump('idp: generateAssertion(' + payload + ')\n');
      var idpDetails = {
        domain: this.domain,
        protocol: this.protocol
      };
      if (instructions.some(is('bad-assert'))) {
        idpDetails = {};
      }
      return this.borkResult({
        idp: idpDetails,
        assertion: JSON.stringify({
          username: this._selectUsername(usernameHint),
          contents: payload
        })
      });
    },

    validateAssertion: function(assertion, origin) {
      dump('idp: validateAssertion(' + assertion + ')\n');
      var assertion = JSON.parse(assertion);
      if (instructions.some(is('bad-validate'))) {
        assertion.contents = {};
      }
      return this.borkResult({
          identity: assertion.username,
          contents: assertion.contents
        });
    }
  };

  if (!instructions.some(is('not_ready'))) {
    dump('registering idp.js' + global.location.hash + '\n');
    global.rtcIdentityProvider.register(new IDPJS());
  }
}(this));
