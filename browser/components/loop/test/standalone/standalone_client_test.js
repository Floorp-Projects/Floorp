/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, it, beforeEach, afterEach, describe, hawk */

var expect = chai.expect;

describe("loop.StandaloneClient", function() {
  "use strict";

  var sandbox,
      fakeXHR,
      requests = [],
      callback,
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

  describe("loop.StandaloneClient", function() {
    describe("#constructor", function() {
      it("should require a baseServerUrl setting", function() {
        expect(function() {
          new loop.StandaloneClient();
        }).to.Throw(Error, /required/);
      });
    });

    describe("requestCallInfo", function() {
      var client;

      beforeEach(function() {
        client = new loop.StandaloneClient(
          {baseServerUrl: "http://fake.api"}
        );
      });

      it("should prevent launching a conversation when token is missing",
        function() {
          expect(function() {
            client.requestCallInfo();
          }).to.Throw(Error, /missing.*[Tt]oken/);
        });

      it("should post data for the given call", function() {
        client.requestCallInfo("fake", "audio", callback);

        expect(requests).to.have.length.of(1);
        expect(requests[0].url).to.be.equal("http://fake.api/calls/fake");
        expect(requests[0].method).to.be.equal("POST");
        expect(requests[0].requestBody).to.be.equal('{"callType":"audio"}');
      });

      it("should receive call data for the given call", function() {
        client.requestCallInfo("fake", "audio-video", callback);

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
        client.requestCallInfo("fake", "audio", callback);

        requests[0].respond(400, {"Content-Type": "application/json"},
                            fakeErrorRes);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        client.requestCallInfo("fake", "audio", callback);

        requests[0].respond(200, {"Content-Type": "application/json"},
                            '{"bad": "one"}');
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });
  });
});
