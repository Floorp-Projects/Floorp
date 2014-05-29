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
    var conversation, fakeSessionData, fakeBaseServerUrl;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel();
      fakeSessionData = {
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
      };
      fakeBaseServerUrl = "http://fakeBaseServerUrl";
    });

    describe("#initiate", function() {
      it("should throw an Error if no baseServerUrl argument is passed",
        function () {
          expect(function() {
            conversation.initiate();
          }).to.Throw(Error, /baseServerUrl/);
        });

      it("should prevent launching a conversation when token is missing",
        function() {
          expect(function() {
            conversation.initiate(fakeBaseServerUrl);
          }).to.Throw(Error, /missing required attribute loopToken/);
        });

      it("should make one ajax POST to a correctly constructed URL",
        function() {
          conversation.set("loopToken", "fakeToken");

          conversation.initiate(fakeBaseServerUrl);

          expect(requests).to.have.length.of(1);
          expect(requests[0].method.toLowerCase()).to.equal("post");
          expect(requests[0].url).to.match(
            new RegExp("^" + fakeBaseServerUrl + "/calls/fakeToken"));
        });

      it("should update conversation session information from server data",
        function() {
          conversation.set("loopToken", "fakeToken");
          conversation.initiate(fakeBaseServerUrl);

          requests[0].respond(200, {"Content-Type": "application/json"},
                                   JSON.stringify(fakeSessionData));

          expect(conversation.get("sessionId")).eql("sessionId");
          expect(conversation.get("sessionToken")).eql("sessionToken");
          expect(conversation.get("apiKey")).eql("apiKey");
        });

      it("should trigger session:ready without fetching session data over "+
         "HTTP when already set", function(done) {
          sandbox.stub(jQuery, "ajax");
          conversation.set("loopToken", "fakeToken");
          conversation.set(fakeSessionData);

          conversation.on("session:ready", function() {
            sinon.assert.notCalled($.ajax);
            done();
          }).initiate(fakeBaseServerUrl);
        });

      it("should trigger a `session:error` on failure", function(done) {
        conversation.set("loopToken", "fakeToken");
        conversation.initiate(fakeBaseServerUrl);

        conversation.on("session:error", function(err) {
          expect(err.message).to.match(/failed: HTTP 400 Bad Request; fake/);
          done();
        });

        requests[0].respond(400, {"Content-Type": "application/json"},
                                  JSON.stringify({error: "fake"}));
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
