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

    describe("#requestCallUrlInfo", function() {
      var client, fakeServerErrorDescription;

      beforeEach(function() {
        client = new loop.StandaloneClient(
          {baseServerUrl: "http://fake.api"}
        );
      });

      describe("should make the requests to the server", function() {

        it("should throw if loopToken is missing", function() {
          expect(client.requestCallUrlInfo).to
                                .throw(/Missing required parameter loopToken/);
        });

        it("should make a GET request for the call url creation date", function() {
          client.requestCallUrlInfo("fakeCallUrlToken", function() {});

          expect(requests).to.have.length.of(1);
          expect(requests[0].url)
                            .to.eql("http://fake.api/calls/fakeCallUrlToken");
          expect(requests[0].method).to.eql("GET");
        });

        it("should call the callback with (null, serverResponse)", function() {
          var successCallback = sandbox.spy(function() {});
          var serverResponse = {
            calleeFriendlyName: "Andrei",
            urlCreationDate: 0
          };

          client.requestCallUrlInfo("fakeCallUrlToken", successCallback);
          requests[0].respond(200, {"Content-Type": "application/json"},
                              JSON.stringify(serverResponse));

          sinon.assert.calledWithExactly(successCallback,
                                         null,
                                         serverResponse);
        });

        it("should log the error if the requests fails", function() {
          sinon.stub(console, "error");
          var serverResponse = {error: true};
          var error = JSON.stringify(serverResponse);

          client.requestCallUrlInfo("fakeCallUrlToken", sandbox.stub());
          requests[0].respond(404, {"Content-Type": "application/json"},
                              error);

          sinon.assert.calledOnce(console.error);
          sinon.assert.calledWithExactly(console.error, "Server error",
                                        "HTTP 404 Not Found", serverResponse);
        });
     });
    });



    describe("requestCallInfo", function() {
      var client, fakeServerErrorDescription;

      beforeEach(function() {
        client = new loop.StandaloneClient(
          {baseServerUrl: "http://fake.api"}
        );
        fakeServerErrorDescription = {
          code: 401,
          errno: 101,
          error: "error",
          message: "invalid token",
          info: "error info"
        };
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
        expect(requests[0].requestBody).to.be.equal('{"callType":"audio","channel":"standalone"}');
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

        requests[0].respond(401, {"Content-Type": "application/json"},
                            JSON.stringify(fakeServerErrorDescription));
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /HTTP 401 Unauthorized/.test(err.message);
        }));
      });

      it("should attach the server error description object to the error " +
         "passed to the callback",
        function() {
          client.requestCallInfo("fake", "audio", callback);

          requests[0].respond(401, {"Content-Type": "application/json"},
                              JSON.stringify(fakeServerErrorDescription));

          sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
            return err.errno === fakeServerErrorDescription.errno;
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
