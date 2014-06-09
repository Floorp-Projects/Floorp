/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, it, beforeEach, afterEach, describe, hawk */

var expect = chai.expect;

describe("loop.shared.Client", function() {
  "use strict";

  var sandbox,
      fakeXHR,
      requests = [],
      callback,
      mozLoop,
      fakeToken;

  var fakeErrorRes = JSON.stringify({
      status: "errors",
      errors: [{
        location: "url",
        name: "token",
        description: "invalid token"
      }]
    });

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };
    callback = sinon.spy();
    fakeToken = "fakeTokenText";
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("loop.shared.Client", function() {
    describe("#constructor", function() {
      it("should require a baseServerUrl setting", function() {
        expect(function() {
          new loop.shared.Client();
        }).to.Throw(Error, /required/);
      });
    });

    describe("#requestCallUrl", function() {
      var client;

      beforeEach(function() {
        window.navigator.mozLoop = {
          ensureRegistered: sinon.stub().callsArgWith(0, null),
          noteCallUrlExpiry: sinon.spy(),
          getLoopCharPref: sandbox.stub()
            .returns(null)
            .withArgs("hawk-session-token")
            .returns(fakeToken)
        };
        client = new loop.shared.Client(
          {baseServerUrl: "http://fake.api", mozLoop: window.navigator.mozLoop}
        );
      });

      it("should ensure loop is registered", function() {
        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(navigator.mozLoop.ensureRegistered);
      });

      it("should send an error when registration fails", function() {
        navigator.mozLoop.ensureRegistered.callsArgWith(0, "offline");

        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithExactly(callback, "offline");
      });

      it("should post to /call-url/", function() {
        client.requestCallUrl("foo", callback);

        expect(requests).to.have.length.of(1);
        expect(requests[0].method).to.be.equal("POST");
        expect(requests[0].url).to.be.equal("http://fake.api/call-url/");
        expect(requests[0].requestBody).to.be.equal('callerId=foo');
      });

      it("should set the XHR Authorization header", function() {
        sandbox.stub(hawk.client, "header").returns( {field: fakeToken} );
        client._credentials = {
          // XXX we probably really want to stub out external module calls
          // eg deriveHawkCredentials, rather supplying them with valid arguments
          // like we're doing here:
          key: 'werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn',
          algorithm: 'sha256',
          user: 'Steve'
        };

        client.requestCallUrl("foo", callback);

        expect(requests[0].requestHeaders.Authorization).to.equal(fakeToken);
      });

      it("should request a call url", function() {
        var callUrlData = {
          "call_url": "fakeCallUrl",
          "expiresAt": 60
        };

        client.requestCallUrl("foo", callback);
        requests[0].respond(200, {"Content-Type": "application/json"},
                            JSON.stringify(callUrlData));

        sinon.assert.calledWithExactly(callback, null, callUrlData);
      });

      it("should note the call url expiry", function() {
        var callUrlData = {
          "call_url": "fakeCallUrl",
          "expiresAt": 60
        };

        client.requestCallUrl("foo", callback);
        requests[0].respond(200, {"Content-Type": "application/json"},
                            JSON.stringify(callUrlData));

        // expiresAt is in hours, and noteCallUrlExpiry wants seconds.
        sinon.assert.calledWithExactly(navigator.mozLoop.noteCallUrlExpiry,
          60 * 60 * 60);
      });

      it("should send an error when the request fails", function() {
        client.requestCallUrl("foo", callback);

        expect(requests).to.have.length.of(1);
        requests[0].respond(400, {"Content-Type": "application/json"},
                            fakeErrorRes);

        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        client.requestCallUrl("foo", callback);
        requests[0].respond(200, {"Content-Type": "application/json"},
                            '{"bad": {}}');

        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });

    describe("#requestCallsInfo", function() {
      var client;

      beforeEach(function() {
        mozLoop = {
          getLoopCharPref: sandbox.stub()
            .returns(null)
            .withArgs("hawk-session-token")
            .returns(fakeToken)
        };
        client = new loop.shared.Client(
          {baseServerUrl: "http://fake.api", mozLoop: mozLoop}
        );
      });

      it("should prevent launching a conversation when version is missing",
        function() {
          expect(function() {
            client.requestCallsInfo();
          }).to.Throw(Error, /missing required parameter version/);
        });

      it("should request data for all calls", function() {
        client.requestCallsInfo(42, callback);

        expect(requests).to.have.length.of(1);
        expect(requests[0].url).to.be.equal("http://fake.api/calls?version=42");
        expect(requests[0].method).to.be.equal("GET");

        requests[0].respond(200, {"Content-Type": "application/json"},
                                 '{"calls": [{"apiKey": "fake"}]}');
        sinon.assert.calledWithExactly(callback, null, [{apiKey: "fake"}]);
      });

      it("should set the XHR Authorization header", function() {
        sandbox.stub(hawk.client, "header").returns( {field: fakeToken} );
        // XXX we probably really want to stub out external module calls
        // eg deriveHawkCredentials, rather supplying them with valid arguments
        // like we're doing here:
        client._credentials = {
          key: 'werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn',
          algorithm: 'sha256',
          user: 'Steve'
        };

        client.requestCallsInfo("foo", callback);

        expect(requests[0].requestHeaders.Authorization).to.equal(fakeToken);
      });

      it("should send an error when the request fails", function() {
        client.requestCallsInfo(42, callback);

        requests[0].respond(400, {"Content-Type": "application/json"},
                                 fakeErrorRes);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        client.requestCallsInfo(42, callback);

        requests[0].respond(200, {"Content-Type": "application/json"},
                                 '{"bad": {}}');
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });

    describe("requestCallInfo", function() {
      var client;

      beforeEach(function() {
        client = new loop.shared.Client(
          {baseServerUrl: "http://fake.api", mozLoop: undefined}
        );
      });

      it("should prevent launching a conversation when token is missing",
        function() {
          expect(function() {
            client.requestCallInfo();
          }).to.Throw(Error, /missing.*[Tt]oken/);
        });

      it("should post data for the given call", function() {
        client.requestCallInfo("fake", callback);

        expect(requests).to.have.length.of(1);
        expect(requests[0].url).to.be.equal("http://fake.api/calls/fake");
        expect(requests[0].method).to.be.equal("POST");
      });

      it("should receive call data for the given call", function() {
        client.requestCallInfo("fake", callback);

        var sessionData = {
          sessionId: "one",
          sessionToken: "two",
          apiKey: "three"
        };

        requests[0].respond(200, {"Content-Type": "application/json"},
                            JSON.stringify(sessionData));
        sinon.assert.calledWithExactly(callback, null, sessionData);
      });

      it("should send an error when the request fails", function() {
        client.requestCallInfo("fake", callback);

        requests[0].respond(400, {"Content-Type": "application/json"},
                            fakeErrorRes);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        client.requestCallInfo("fake", callback);

        requests[0].respond(200, {"Content-Type": "application/json"},
                            '{"bad": "one"}');
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });
  });
});
