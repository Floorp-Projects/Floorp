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
      getLoopPref: sandbox.stub()
        .returns(null)
        .withArgs("hawk-session-token")
        .returns(fakeToken),
      hawkRequest: sinon.stub(),
      LOOP_SESSION_TYPE: {
        GUEST: 1,
        FXA: 2
      },
      userProfile: null,
      telemetryAdd: sinon.spy()
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
    describe("#setupOutgoingCall", function() {
      var calleeIds, callType;

      beforeEach(function() {
        calleeIds = [
          "fakeemail", "fake phone"
        ];
        callType = "audio";
      });

      it("should make a POST call to /calls", function() {
        client.setupOutgoingCall(calleeIds, callType);

        sinon.assert.calledOnce(hawkRequestStub);
        sinon.assert.calledWith(hawkRequestStub,
          mozLoop.LOOP_SESSION_TYPE.FXA,
          "/calls",
          "POST",
          { calleeId: calleeIds, callType: callType, channel: "unknown" }
        );
      });

      it("should include the channel when defined", function() {
        mozLoop.appVersionInfo = {
          channel: "beta"
        };

        client.setupOutgoingCall(calleeIds, callType);

        sinon.assert.calledOnce(hawkRequestStub);
        sinon.assert.calledWith(hawkRequestStub,
          mozLoop.LOOP_SESSION_TYPE.FXA,
          "/calls",
          "POST",
          { calleeId: calleeIds, callType: callType, channel: "beta" }
        );
      });

      it("should call the callback if the request is successful", function() {
        var requestData = {
          apiKey: "fake",
          callId: "fakeCall",
          progressURL: "fakeurl",
          sessionId: "12345678",
          sessionToken: "15263748",
          websocketToken: "13572468"
        };

        hawkRequestStub.callsArgWith(4, null, JSON.stringify(requestData));

        client.setupOutgoingCall(calleeIds, callType, callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithExactly(callback, null, requestData);
      });

      it("should send an error when the request fails", function() {
        hawkRequestStub.callsArgWith(4, fakeErrorRes);

        client.setupOutgoingCall(calleeIds, callType, callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithExactly(callback, sinon.match(function(err) {
          return err.code == 400 && err.message == "invalid token";
        }));
      });

      it("should send an error if the data is not valid", function() {
        // Sets up the hawkRequest stub to trigger the callback with
        // an error
        hawkRequestStub.callsArgWith(4, null, "{}");

        client.setupOutgoingCall(calleeIds, callType, callback);

        sinon.assert.calledOnce(callback);
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid data received/.test(err.message);
        }));
      });
    });
  });
});
