/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, it, beforeEach, afterEach, describe */

var expect = chai.expect;

describe("loop.Client", function() {
  "use strict";

  var sandbox,
      callback,
      client,
      mozLoop,
      fakeToken,
      hawkRequestStub;

  var fakeErrorRes = {
      code: 400,
      errno: 400,
      error: "Request Failed",
      message: "invalid token"
    };

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    callback = sinon.spy();
    fakeToken = "fakeTokenText";
    mozLoop = {
      getLoopCharPref: sandbox.stub()
        .returns(null)
        .withArgs("hawk-session-token")
        .returns(fakeToken),
      ensureRegistered: sinon.stub().callsArgWith(0, null),
      noteCallUrlExpiry: sinon.spy(),
      hawkRequest: sinon.stub()
    };
    // Alias for clearer tests.
    hawkRequestStub = mozLoop.hawkRequest;
    client = new loop.Client({
      mozLoop: mozLoop
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("loop.Client", function() {
    describe("#deleteCallUrl", function() {
      it("should ensure loop is registered", function() {
        client.deleteCallUrl("fakeToken", callback);

        sinon.assert.calledOnce(mozLoop.ensureRegistered);
      });

      it("should send an error when registration fails", function() {
        mozLoop.ensureRegistered.callsArgWith(0, "offline");

        client.deleteCallUrl("fakeToken", callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithExactly(callback, "offline");
      });

      it("should make a delete call to /call-url/{fakeToken}", function() {
        client.deleteCallUrl(fakeToken, callback);

        sinon.assert.calledOnce(hawkRequestStub);
        sinon.assert.calledWith(hawkRequestStub,
                                "/call-url/" + fakeToken, "DELETE");
      });

      it("should call the callback with null when the request succeeds",
         function() {

           // Sets up the hawkRequest stub to trigger the callback with no error
           // and the url.
           hawkRequestStub.callsArgWith(3, null);

           client.deleteCallUrl(fakeToken, callback);

           sinon.assert.calledWithExactly(callback, null);
         });

      it("should reset all url expiry when the request succeeds", function() {
        // Sets up the hawkRequest stub to trigger the callback with no error
        // and the url.
        var dateInMilliseconds = new Date(2014,7,20).getTime();
        hawkRequestStub.callsArgWith(3, null);
        sandbox.useFakeTimers(dateInMilliseconds);

        client.deleteCallUrl(fakeToken, callback);

        sinon.assert.calledOnce(mozLoop.noteCallUrlExpiry);
        sinon.assert.calledWithExactly(mozLoop.noteCallUrlExpiry,
                                       dateInMilliseconds / 1000);
      });

      it("should send an error when the request fails", function() {
        // Sets up the hawkRequest stub to trigger the callback with
        // an error
        hawkRequestStub.callsArgWith(3, fakeErrorRes);

        client.deleteCallUrl(fakeToken, callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });
    });

    describe("#requestCallUrl", function() {
      it("should ensure loop is registered", function() {
        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(mozLoop.ensureRegistered);
      });

      it("should send an error when registration fails", function() {
        mozLoop.ensureRegistered.callsArgWith(0, "offline");

        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithExactly(callback, "offline");
      });

      it("should post to /call-url/", function() {
        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(hawkRequestStub);
        sinon.assert.calledWith(hawkRequestStub,
                                "/call-url/", "POST", {callerId: "foo"});
      });

      it("should call the callback with the url when the request succeeds",
        function() {
          var callUrlData = {
            "callUrl": "fakeCallUrl",
            "expiresAt": 60
          };

          // Sets up the hawkRequest stub to trigger the callback with no error
          // and the url.
          hawkRequestStub.callsArgWith(3, null,
            JSON.stringify(callUrlData));

          client.requestCallUrl("foo", callback);

          sinon.assert.calledWithExactly(callback, null, callUrlData);
        });

      it("should note the call url expiry when the request succeeds",
        function() {
          var callUrlData = {
            "callUrl": "fakeCallUrl",
            "expiresAt": 6000
          };

          // Sets up the hawkRequest stub to trigger the callback with no error
          // and the url.
          hawkRequestStub.callsArgWith(3, null,
            JSON.stringify(callUrlData));

          client.requestCallUrl("foo", callback);

          sinon.assert.calledOnce(mozLoop.noteCallUrlExpiry);
          sinon.assert.calledWithExactly(mozLoop.noteCallUrlExpiry,
            6000);
        });

      it("should send an error when the request fails", function() {
        // Sets up the hawkRequest stub to trigger the callback with
        // an error
        hawkRequestStub.callsArgWith(3, fakeErrorRes);

        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        // Sets up the hawkRequest stub to trigger the callback with
        // an error
        hawkRequestStub.callsArgWith(3, null, "{}");

        client.requestCallUrl("foo", callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });

    describe("#requestCallsInfo", function() {
      it("should prevent launching a conversation when version is missing",
        function() {
          expect(function() {
            client.requestCallsInfo();
          }).to.Throw(Error, /missing required parameter version/);
        });

      it("should perform a get on /calls", function() {
        client.requestCallsInfo(42, callback);

        sinon.assert.calledOnce(hawkRequestStub);
        sinon.assert.calledWith(hawkRequestStub,
                                "/calls?version=42", "GET", null);

      });

      it("should request data for all calls", function() {
        hawkRequestStub.callsArgWith(3, null,
                                     '{"calls": [{"apiKey": "fake"}]}');

        client.requestCallsInfo(42, callback);

        sinon.assert.calledWithExactly(callback, null, [{apiKey: "fake"}]);
      });

      it("should send an error when the request fails", function() {
        hawkRequestStub.callsArgWith(3, fakeErrorRes);

        client.requestCallsInfo(42, callback);

        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /400.*invalid token/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        hawkRequestStub.callsArgWith(3, null, "{}");

        client.requestCallsInfo(42, callback);

        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });
  });
});
