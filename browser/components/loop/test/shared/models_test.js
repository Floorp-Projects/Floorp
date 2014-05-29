/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.shared.models", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sandbox, fakeXHR, requests = [];

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function(xhr) {
      requests.push(xhr);
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("ConversationModel", function() {
    var conversation, reqCallInfoStub, reqCallsInfoStub,
      fakeSessionData, fakeBaseServerUrl;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel();
      conversation.set("loopToken", "fakeToken");
      fakeSessionData = {
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
      };
      fakeBaseServerUrl = "http://fakeBaseServerUrl";
      reqCallInfoStub = sandbox.stub(loop.shared.Client.prototype,
        "requestCallInfo");
      reqCallsInfoStub = sandbox.stub(loop.shared.Client.prototype,
        "requestCallsInfo");
    });

    describe("#initiate", function() {
      it("call requestCallInfo on the client for outgoing calls",
        function() {
          conversation.initiate({
            baseServerUrl: fakeBaseServerUrl,
            outgoing: true
          });

          sinon.assert.calledOnce(reqCallInfoStub);
          sinon.assert.calledWith(reqCallInfoStub, "fakeToken");
        });

      it("should not call requestCallsInfo on the client for outgoing calls",
        function() {
          conversation.initiate({
            baseServerUrl: fakeBaseServerUrl,
            outgoing: true
          });

          sinon.assert.notCalled(reqCallsInfoStub);
        });

      it("call requestCallsInfo on the client for incoming calls",
        function() {
          conversation.initiate({
            baseServerUrl: fakeBaseServerUrl,
            outgoing: false
          });

          sinon.assert.calledOnce(reqCallsInfoStub);
          sinon.assert.calledWith(reqCallsInfoStub);
        });

      it("should not call requestCallInfo on the client for incoming calls",
        function() {
          conversation.initiate({
            baseServerUrl: fakeBaseServerUrl,
            outgoing: false
          });

          sinon.assert.notCalled(reqCallInfoStub);
        });

      it("should update conversation session information from server data",
        function() {
          reqCallInfoStub.callsArgWith(1, null, fakeSessionData);

          conversation.initiate({
            baseServerUrl: fakeBaseServerUrl,
            outgoing: true
          });

          expect(conversation.get("sessionId")).eql("sessionId");
          expect(conversation.get("sessionToken")).eql("sessionToken");
          expect(conversation.get("apiKey")).eql("apiKey");
        });

      it("should trigger session:ready without fetching session data over "+
         "HTTP when already set", function(done) {
          conversation.set(fakeSessionData);

          conversation.on("session:ready", function() {
            sinon.assert.notCalled(reqCallInfoStub);
            done();
          }).initiate({
            baseServerUrl: fakeBaseServerUrl,
            outgoing: true
          });

        });

      it("should trigger a `session:error` on failure", function(done) {
        reqCallInfoStub.callsArgWith(1,
          new Error("failed: HTTP 400 Bad Request; fake"));

        conversation.on("session:error", function(err) {
          expect(err.message).to.match(/failed: HTTP 400 Bad Request; fake/);
          done();
        }).initiate({
          baseServerUrl: fakeBaseServerUrl,
          outgoing: true
        });
      });
    });

    describe("#setReady", function() {
      it("should update conversation session information", function() {
        conversation.setReady(fakeSessionData);

        expect(conversation.get("sessionId")).eql("sessionId");
        expect(conversation.get("sessionToken")).eql("sessionToken");
        expect(conversation.get("apiKey")).eql("apiKey");
      });

      it("should trigger a `session:ready` event", function(done) {
        conversation.on("session:ready", function() {
          done();
        }).setReady(fakeSessionData);
      });
    });
  });
});
