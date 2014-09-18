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
      hawkRequest: sinon.stub(),
      telemetryAdd: sinon.spy(),
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

      it("should not update call url expiry when the request succeeds",
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

          sinon.assert.notCalled(mozLoop.noteCallUrlExpiry);
        });

      it("should call mozLoop.telemetryAdd when the request succeeds",
        function(done) {
          var callUrlData = {
            "callUrl": "fakeCallUrl",
            "expiresAt": 60
          };

          // Sets up the hawkRequest stub to trigger the callback with no error
          // and the url.
          hawkRequestStub.callsArgWith(3, null,
            JSON.stringify(callUrlData));

          client.requestCallUrl("foo", function(err) {
            expect(err).to.be.null;

            sinon.assert.calledOnce(mozLoop.telemetryAdd);
            sinon.assert.calledWith(mozLoop.telemetryAdd,
                                    "LOOP_CLIENT_CALL_URL_REQUESTS_SUCCESS",
                                    true);

            done();
          });
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

      it("should call mozLoop.telemetryAdd when the request fails",
        function(done) {
          // Sets up the hawkRequest stub to trigger the callback with
          // an error
          hawkRequestStub.callsArgWith(3, fakeErrorRes);

          client.requestCallUrl("foo", function(err) {
            expect(err).not.to.be.null;

            sinon.assert.calledOnce(mozLoop.telemetryAdd);
            sinon.assert.calledWith(mozLoop.telemetryAdd,
                                    "LOOP_CLIENT_CALL_URL_REQUESTS_SUCCESS",
                                    false);

            done();
          });
        });
    });
  });
});
