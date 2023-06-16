(function (global) {
  "use strict";

  // rather than create a million different IdP configurations and litter the
  // world with files all containing near-identical code, let's use the hash/URL
  // fragment as a way of generating instructions for the IdP
  var instructions = global.location.hash.replace("#", "").split(":");
  function is(target) {
    return function (instruction) {
      return instruction === target;
    };
  }

  function IDPJS() {
    this.domain = global.location.host;
    var path = global.location.pathname;
    this.protocol =
      path.substring(path.lastIndexOf("/") + 1) + global.location.hash;
    this.id = crypto.getRandomValues(new Uint8Array(10)).join(".");
  }

  IDPJS.prototype = {
    getLogin() {
      return fetch(
        "https://example.com/.well-known/idp-proxy/idp.sjs?" + this.id
      ).then(response => response.status === 200);
    },
    checkLogin(result) {
      return this.getLogin().then(loggedIn => {
        if (loggedIn) {
          return result;
        }
        return Promise.reject({
          name: "IdpLoginError",
          loginUrl:
            "https://example.com/.well-known/idp-proxy/login.html#" + this.id,
        });
      });
    },

    borkResult(result) {
      if (instructions.some(is("throw"))) {
        throw new Error("Throwing!");
      }
      if (instructions.some(is("fail"))) {
        return Promise.reject(new Error("Failing!"));
      }
      if (instructions.some(is("login"))) {
        return this.checkLogin(result);
      }
      if (instructions.some(is("hang"))) {
        return new Promise(r => {});
      }
      dump("idp: result=" + JSON.stringify(result) + "\n");
      return Promise.resolve(result);
    },

    _selectUsername(usernameHint) {
      dump("_selectUsername: usernameHint(" + usernameHint + ")\n");
      var username = "someone@" + this.domain;
      if (usernameHint) {
        var at = usernameHint.indexOf("@");
        if (at < 0) {
          username = usernameHint + "@" + this.domain;
        } else if (usernameHint.substring(at + 1) === this.domain) {
          username = usernameHint;
        }
      }
      return username;
    },

    generateAssertion(payload, origin, options) {
      dump(
        "idp: generateAssertion(" +
          payload +
          ", " +
          origin +
          ", " +
          JSON.stringify(options) +
          ")\n"
      );
      var idpDetails = {
        domain: this.domain,
        protocol: this.protocol,
      };
      if (instructions.some(is("bad-assert"))) {
        idpDetails = {};
      }
      return this.borkResult({
        idp: idpDetails,
        assertion: JSON.stringify({
          username: this._selectUsername(options.usernameHint),
          contents: payload,
        }),
      });
    },

    validateAssertion(assertion, origin) {
      dump("idp: validateAssertion(" + assertion + ")\n");
      var assertion = JSON.parse(assertion);
      if (instructions.some(is("bad-validate"))) {
        assertion.contents = {};
      }
      return this.borkResult({
        identity: assertion.username,
        contents: assertion.contents,
      });
    },
  };

  if (!instructions.some(is("not_ready"))) {
    dump("registering idp.js" + global.location.hash + "\n");
    var idp = new IDPJS();
    global.rtcIdentityProvider.register({
      generateAssertion: idp.generateAssertion.bind(idp),
      validateAssertion: idp.validateAssertion.bind(idp),
    });
  }
})(this);
