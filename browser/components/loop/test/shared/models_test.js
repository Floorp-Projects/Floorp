/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.shared.models", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sandbox, fakeXHR, requests = [], fakeSDK, fakeSession, fakeSessionData;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function(xhr) {
      requests.push(xhr);
    };
    fakeSessionData = {
      sessionId:    "sessionId",
      sessionToken: "sessionToken",
      apiKey:       "apiKey"
    };
    fakeSession = _.extend({
      connect: function () {},
      endSession: sandbox.stub(),
      set: sandbox.stub(),
      disconnect: sandbox.spy(),
      unpublish: sandbox.spy()
    }, Backbone.Events);
    fakeSDK = {
      initPublisher: sandbox.spy(),
      initSession: sandbox.stub().returns(fakeSession)
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("ConversationModel", function() {
    describe("#initialize", function() {
      it("should require a sdk option", function() {
        expect(function() {
          new sharedModels.ConversationModel();
        }).to.Throw(Error, /missing required sdk/);
      });
    });

    describe("constructed", function() {
      var conversation, reqCallInfoStub, reqCallsInfoStub, fakeBaseServerUrl;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {sdk: fakeSDK});
        conversation.set("loopToken", "fakeToken");
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
            sandbox.stub(conversation, "setReady");
            reqCallInfoStub.callsArgWith(1, null, fakeSessionData);

            conversation.initiate({
              baseServerUrl: fakeBaseServerUrl,
              outgoing: true
            });

            sinon.assert.calledOnce(conversation.setReady);
            sinon.assert.calledWith(conversation.setReady, fakeSessionData);
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

      describe("#startSession", function() {
        var model;

        beforeEach(function() {
          model = new sharedModels.ConversationModel(fakeSessionData, {
            sdk: fakeSDK
          });
          model.startSession();
        });

        it("should start a session", function() {
          sinon.assert.calledOnce(fakeSDK.initSession);
        });

        it("should call connect", function() {
          fakeSession.connect = sandbox.stub();

          model.startSession();

          sinon.assert.calledOnce(fakeSession.connect);
          sinon.assert.calledWithExactly(fakeSession.connect,
                        sinon.match.string, sinon.match.string,
                        sinon.match.func);
        });

        it("should set ongoing to true when no error is called back",
            function() {
              fakeSession.connect = function(key, token, cb) {
                cb(null);
              };
              sinon.stub(model, "set");

              model.startSession();

              sinon.assert.calledWith(model.set, "ongoing", true);
            });

        it("should trigger session:connected when no error is called back",
            function() {
              fakeSession.connect = function(key, token, cb) {
                cb(null);
              };
              sandbox.stub(model, "trigger");

              model.startSession();

              sinon.assert.calledWithExactly(model.trigger, "session:connected");
            });

        describe("Session events", function() {

          it("should trigger a fail event when an error is called back",
            function() {
              fakeSession.connect = function(key, token, cb) {
                cb({
                  error: true
                });
              };
              sinon.stub(model, "endSession");

              model.startSession();

              sinon.assert.calledOnce(model.endSession);
              sinon.assert.calledWithExactly(model.endSession);
            });

          it("should trigger session:connection-error event when an error is" +
            " called back", function() {
              fakeSession.connect = function(key, token, cb) {
                cb({
                  error: true
                });
              };
              sandbox.stub(model, "trigger");

              model.startSession();

              sinon.assert.calledOnce(model.trigger);
              sinon.assert.calledWithExactly(model.trigger,
                          "session:connection-error", sinon.match.object);
            });

          it("should trigger a session:ended event on sessionDisconnected",
            function(done) {
              model.once("session:ended", function(){ done(); });

              fakeSession.trigger("sessionDisconnected", {reason: "ko"});
            });

          it("should set the ongoing attribute to false on sessionDisconnected",
            function(done) {
              model.once("session:ended", function() {
                expect(model.get("ongoing")).eql(false);
                done();
              });

              fakeSession.trigger("sessionDisconnected", {reason: "ko"});
            });

          describe("connectionDestroyed event received", function() {
            var fakeEvent = {reason: "ko", connection: {connectionId: 42}};

            it("should trigger a session:peer-hungup model event",
              function(done) {
                model.once("session:peer-hungup", function(event) {
                  expect(event.connectionId).eql(42);
                  done();
                });

                fakeSession.trigger("connectionDestroyed", fakeEvent);
              });

            it("should terminate the session", function() {
              sandbox.stub(model, "endSession");

              fakeSession.trigger("connectionDestroyed", fakeEvent);

              sinon.assert.calledOnce(model.endSession);
            });
          });

          describe("networkDisconnected event received", function() {
            it("should trigger a session:network-disconnected event",
              function(done) {
                model.once("session:network-disconnected", function() {
                  done();
                });

                fakeSession.trigger("networkDisconnected");
              });

            it("should terminate the session", function() {
              sandbox.stub(model, "endSession");

              fakeSession.trigger("networkDisconnected", {reason: "ko"});

              sinon.assert.calledOnce(model.endSession);
            });
          });
        });
      });

      describe("#endSession", function() {
        var model;

        beforeEach(function() {
          model = new sharedModels.ConversationModel(fakeSessionData, {
            sdk: fakeSDK
          });
          model.startSession();
        });

        it("should disconnect current session", function() {
          model.endSession();

          sinon.assert.calledOnce(fakeSession.disconnect);
        });

        it("should set the ongoing attribute to false", function() {
          model.endSession();

          expect(model.get("ongoing")).eql(false);
        });

        it("should stop listening to session events once the session is " +
           "actually disconnected", function() {
            sandbox.stub(model, "stopListening");

            model.endSession();
            model.trigger("session:ended");

            sinon.assert.calledOnce(model.stopListening);
          });
      });
    });
  });
});
