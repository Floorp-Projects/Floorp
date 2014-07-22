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

      describe("#initiate", function() {
        beforeEach(function() {
          sandbox.stub(conversation, "endSession");
        });

        it("call requestCallInfo on the client for outgoing calls",
          function() {
            conversation.initiate({
              client: fakeClient,
              outgoing: true,
              callType: "audio"
            });

            sinon.assert.calledOnce(requestCallInfoStub);
            sinon.assert.calledWith(requestCallInfoStub, "fakeToken", "audio");
          });

        it("should not call requestCallsInfo on the client for outgoing calls",
          function() {
            conversation.initiate({
              client: fakeClient,
              outgoing: true,
              callType: "audio"
            });

            sinon.assert.notCalled(requestCallsInfoStub);
          });

        it("call requestCallsInfo on the client for incoming calls",
          function() {
            conversation.set("loopVersion", 42);
            conversation.initiate({
              client: fakeClient,
              outgoing: false
            });

            sinon.assert.calledOnce(requestCallsInfoStub);
            sinon.assert.calledWith(requestCallsInfoStub, 42);
          });

        it("should not call requestCallInfo on the client for incoming calls",
          function() {
            conversation.initiate({
              client: fakeClient,
              outgoing: false
            });

            sinon.assert.notCalled(requestCallInfoStub);
          });

        it("should update conversation session information from server data",
          function() {
            sandbox.stub(conversation, "setReady");
            requestCallInfoStub.callsArgWith(2, null, fakeSessionData);

            conversation.initiate({
              client: fakeClient,
              outgoing: true
            });

            sinon.assert.calledOnce(conversation.setReady);
            sinon.assert.calledWith(conversation.setReady, fakeSessionData);
          });

        it("should trigger a `session:error` event errno is undefined",
          function(done) {
            var errMsg = "HTTP 500 Server Error; fake";
            var err = new Error(errMsg);
            requestCallInfoStub.callsArgWith(2, err);

            conversation.on("session:error", function(err) {
              expect(err.message).eql(errMsg);
              done();
            }).initiate({ client: fakeClient, outgoing: true });
          });

        it("should trigger a `session:error` event when errno is not 105",
          function(done) {
            var errMsg = "HTTP 400 Bad Request; fake";
            var err = new Error(errMsg);
            err.errno = 101;
            requestCallInfoStub.callsArgWith(2, err);

            conversation.on("session:error", function(err) {
              expect(err.message).eql(errMsg);
              done();
            }).initiate({ client: fakeClient, outgoing: true });
          });

        it("should trigger a `session:expired` event when errno is 105",
          function(done) {
            var err = new Error("HTTP 404 Not Found; fake");
            err.errno = 105;
            requestCallInfoStub.callsArgWith(2, err);

            conversation.on("session:expired", function(err2) {
              expect(err2).eql(err);
              done();
            }).initiate({ client: fakeClient, outgoing: true });
          });

        it("should end the session on outgoing call timeout", function() {
          requestCallInfoStub.callsArgWith(2, null, fakeSessionData);

          conversation.initiate({
            client: fakeClient,
            outgoing: true
          });

          sandbox.clock.tick(1001);

          sinon.assert.calledOnce(conversation.endSession);
        });

        it("should trigger a `timeout` event on outgoing call timeout",
          function(done) {
            requestCallInfoStub.callsArgWith(2, null, fakeSessionData);

            conversation.once("timeout", function() {
              done();
            });

            conversation.initiate({
              client: fakeClient,
              outgoing: true
            });

            sandbox.clock.tick(1001);
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
