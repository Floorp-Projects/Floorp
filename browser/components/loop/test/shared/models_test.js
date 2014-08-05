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
    sandbox.useFakeTimers();
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
          new sharedModels.ConversationModel({}, {});
        }).to.Throw(Error, /missing required sdk/);
      });

      it("should accept a pendingCallTimeout option", function() {
        expect(new sharedModels.ConversationModel({}, {
          sdk: {},
          pendingCallTimeout: 1000
        }).pendingCallTimeout).eql(1000);
      });
    });

    describe("constructed", function() {
      var conversation, fakeClient, fakeBaseServerUrl,
          requestCallInfoStub, requestCallsInfoStub;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {
          sdk: fakeSDK,
          pendingCallTimeout: 1000
        });
        conversation.set("loopToken", "fakeToken");
        fakeBaseServerUrl = "http://fakeBaseServerUrl";
        fakeClient = {
          requestCallInfo: sandbox.stub(),
          requestCallsInfo: sandbox.stub()
        };
        requestCallInfoStub = fakeClient.requestCallInfo;
        requestCallsInfoStub = fakeClient.requestCallsInfo;
      });

      describe("#incoming", function() {
        it("should trigger a `call:incoming` event", function(done) {
          conversation.once("call:incoming", function() {
            done();
          });

          conversation.incoming();
        });
      });

      describe("#setupOutgoingCall", function() {
        it("should trigger a `call:outgoing:setup` event", function(done) {
          conversation.once("call:outgoing:setup", function() {
            done();
          });

          conversation.setupOutgoingCall();
        });
      });

      describe("#outgoing", function() {
        beforeEach(function() {
          sandbox.stub(conversation, "endSession");
          sandbox.stub(conversation, "setSessionData");
        });

        it("should save the sessionData", function() {
          conversation.outgoing(fakeSessionData);

          sinon.assert.calledOnce(conversation.setSessionData);
        });

        it("should trigger a `call:outgoing` event", function(done) {
          conversation.once("call:outgoing", function() {
            done();
          });

          conversation.outgoing();
        });

        it("should end the session on outgoing call timeout", function() {
          conversation.outgoing();

          sandbox.clock.tick(1001);

          sinon.assert.calledOnce(conversation.endSession);
        });

        it("should trigger a `timeout` event on outgoing call timeout",
          function(done) {
            conversation.once("timeout", function() {
              done();
            });

            conversation.outgoing();

            sandbox.clock.tick(1001);
          });
      });

      describe("#setSessionData", function() {
        it("should update conversation session information", function() {
          conversation.setSessionData(fakeSessionData);

          expect(conversation.get("sessionId")).eql("sessionId");
          expect(conversation.get("sessionToken")).eql("sessionToken");
          expect(conversation.get("apiKey")).eql("apiKey");
        });
      });

      describe("#startSession", function() {
        var model;

        beforeEach(function() {
          sandbox.stub(sharedModels.ConversationModel.prototype,
                       "_clearPendingCallTimer");
          model = new sharedModels.ConversationModel(fakeSessionData, {
            sdk: fakeSDK,
            pendingCallTimeout: 1000
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

        it("should set connected to true when no error is called back",
            function() {
              fakeSession.connect = function(key, token, cb) {
                cb(null);
              };
              sandbox.stub(model, "set");

              model.startSession();

              sinon.assert.calledWith(model.set, "connected", true);
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
              sandbox.stub(model, "endSession");

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

          it("should set the connected attr to true on connection completed",
            function() {
              fakeSession.connect = function(key, token, cb) {
                cb();
              };

              model.startSession();

              expect(model.get("connected")).eql(true);
            });

          it("should trigger a session:ended event on sessionDisconnected",
            function(done) {
              model.once("session:ended", function(){ done(); });

              fakeSession.trigger("sessionDisconnected", {reason: "ko"});
            });

          it("should set the connected attribute to false on sessionDisconnected",
            function() {
              fakeSession.trigger("sessionDisconnected", {reason: "ko"});

              expect(model.get("connected")).eql(false);
            });

          it("should set the ongoing attribute to false on sessionDisconnected",
            function() {
              fakeSession.trigger("sessionDisconnected", {reason: "ko"});

              expect(model.get("ongoing")).eql(false);
            });

          it("should clear a pending timer on session:ended", function() {
            model.trigger("session:ended");

            sinon.assert.calledOnce(model._clearPendingCallTimer);
          });

          it("should clear a pending timer on session:error", function() {
            model.trigger("session:error");

            sinon.assert.calledOnce(model._clearPendingCallTimer);
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
            sdk: fakeSDK,
            pendingCallTimeout: 1000
          });
          model.startSession();
        });

        it("should disconnect current session", function() {
          model.endSession();

          sinon.assert.calledOnce(fakeSession.disconnect);
        });

        it("should set the connected attribute to false", function() {
          model.endSession();

          expect(model.get("connected")).eql(false);
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
