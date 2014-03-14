(function(global) {
  "use strict";

  function IDPJS() {
    this.domain = window.location.host;
    var p = window.location.pathname;
    this.protocol = p.substring(p.lastIndexOf('/') + 1) + window.location.hash;
    this.username = "someone@" + this.domain;
    // so rather than create a million different IdP configurations and litter
    // the world with files all containing near-identical code, let's use the
    // hash/URL fragment as a way of generating instructions for the IdP
    this.instructions = window.location.hash.replace("#", "").split(":");
    this.port = window.rtcwebIdentityPort;
    this.port.onmessage = this.receiveMessage.bind(this);
    this.sendResponse({
      type : "READY"
    });
  }

  IDPJS.prototype.getDelay = function() {
    // instructions in the form "delay123" have that many milliseconds
    // added before sending the response
    var delay = 0;
    function addDelay(instruction) {
      var m = instruction.match(/^delay(\d+)$/);
      if (m) {
        delay += parseInt(m[1], 10);
      }
    }
    this.instructions.forEach(addDelay);
    return delay;
  };

  function is(target) {
    return function(instruction) {
      return instruction === target;
    };
  }

  IDPJS.prototype.sendResponse = function(response) {
    // we don't touch the READY message unless told to
    if (response.type === "READY" && !this.instructions.some(is("ready"))) {
      this.port.postMessage(response);
      return;
    }

    // if any instruction is "error", return an error.
    if (this.instructions.some(is("error"))) {
      response.type = "ERROR";
    }

    window.setTimeout(function() {
      this.port.postMessage(response);
    }.bind(this), this.getDelay());
  };

  IDPJS.prototype.receiveMessage = function(ev) {
    var message = ev.data;
    switch (message.type) {
    case "SIGN":
      if (message.username) {
        var at = message.username.indexOf("@");
        if (at < 0) {
          this.username = message.username + "@" + this.domain;
        } else if (message.username.substring(at + 1) === this.domain) {
          this.username = message.username;
        }
      }
      this.sendResponse({
        type : "SUCCESS",
        id : message.id,
        message : {
          idp : {
            domain : this.domain,
            protocol : this.protocol
          },
          assertion : JSON.stringify({
            username : this.username,
            contents : message.message
          })
        }
      });
      break;

    case "VERIFY":
      var payload = JSON.parse(message.message);
      var contents = payload.contents;
      if (this.instructions.some(is("bad"))) {
        contents = {};
      }
      this.sendResponse({
        type : "SUCCESS",
        id : message.id,
        message : {
          identity : payload.username,
          contents : contents
        }
      });
      break;

    default:
      this.sendResponse({
        type : "ERROR",
        id : message.id,
        error : JSON.stringify(message)
      });
      break;
    }
  };

  global.idp = new IDPJS();
}(this));
