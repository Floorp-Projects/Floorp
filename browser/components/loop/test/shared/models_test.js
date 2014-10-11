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
      sessionId:      "sessionId",
      sessionToken:   "sessionToken",
      apiKey:         "apiKey",
      callType:       "callType",
      websocketToken: 123,
      callToken:      "callToken",
      callUrl:        "http://invalid/callToken",
      callerId:       "mrssmith"
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
    });

    describe("constructed", function() {
      var conversation;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {
          sdk: fakeSDK
        });
        conversation.set("loopToken", "fakeToken");
      });

      describe("#accepted", function() {
        it("should trigger a `call:accepted` event", function(done) {
          conversation.once("call:accepted", function() {
            done();
          });

          conversation.accepted();
        });
      });

      describe("#setupOutgoingCall", function() {
        it("should set the a custom selected call type", function() {
          conversation.setupOutgoingCall("audio");

          expect(conversation.get("selectedCallType")).eql("audio");
        });

        it("should respect the default selected call type when none is passed",
          function() {
            conversation.setupOutgoingCall();

            expect(conversation.get("selectedCallType")).eql("audio-video");
          });

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
          sandbox.stub(conversation, "setOutgoingSessionData");
          sandbox.stub(conversation, "setIncomingSessionData");
        });

        it("should save the outgoing sessionData", function() {
          conversation.outgoing(fakeSessionData);

          sinon.assert.calledOnce(conversation.setOutgoingSessionData);
        });

        it("should trigger a `call:outgoing` event", function(done) {
          conversation.once("call:outgoing", function() {
            done();
          });

          conversation.outgoing();
        });
      });

      describe("#setSessionData", function() {
        it("should update outgoing conversation session information",
           function() {
             conversation.setOutgoingSessionData(fakeSessionData);

             expect(conversation.get("sessionId")).eql("sessionId");
             expect(conversation.get("sessionToken")).eql("sessionToken");
             expect(conversation.get("apiKey")).eql("apiKey");
           });

        it("should update incoming conversation session information",
           function() {
             conversation.setIncomingSessionData(fakeSessionData);

             expect(conversation.get("sessionId")).eql("sessionId");
             expect(conversation.get("sessionToken")).eql("sessionToken");
             expect(conversation.get("apiKey")).eql("apiKey");
             expect(conversation.get("callType")).eql("callType");
             expect(conversation.get("callToken")).eql("callToken");
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

      describe("#hasVideoStream", function() {
        var model;

        beforeEach(function() {
          model = new sharedModels.ConversationModel(fakeSessionData, {
            sdk: fakeSDK
          });
          model.startSession();
        });

        it("should return true for incoming callType", function() {
          model.set("callType", "audio-video");

          expect(model.hasVideoStream("incoming")).to.eql(true);
        });

        it("should return true for outgoing callType", function() {
          model.set("selectedCallType", "audio-video");

          expect(model.hasVideoStream("outgoing")).to.eql(true);
        });
      });

      describe("#getCallIdentifier", function() {
        var model;

        beforeEach(function() {
          model = new sharedModels.ConversationModel(fakeSessionData, {
            sdk: fakeSDK
          });
          model.startSession();
        });

        it("should return the callerId", function() {
          expect(model.getCallIdentifier()).eql("mrssmith");
        });

        it("should return the shorted callUrl if the callerId does not exist",
          function() {
            model.set({callerId: ""});

            expect(model.getCallIdentifier()).eql("invalid/callToken");
          });

        it("should return an empty string if neither callerId nor callUrl exist",
          function() {
            model.set({
              callerId: undefined,
              callUrl: undefined
            });

            expect(model.getCallIdentifier()).eql("");
          });
      });
    });
  });

  describe("NotificationCollection", function() {
    var collection, notifData, testNotif;

    beforeEach(function() {
      collection = new sharedModels.NotificationCollection();
      sandbox.stub(l10n, "get", function(x, y) {
        return "translated:" + x + (y ? ':' + y : '');
      });
      notifData = {level: "error", message: "plop"};
      testNotif = new sharedModels.NotificationModel(notifData);
    });

    describe("#warn", function() {
      it("should add a warning notification to the stack", function() {
        collection.warn("watch out");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("warning");
        expect(collection.at(0).get("message")).eql("watch out");
      });
    });

    describe("#warnL10n", function() {
      it("should warn using a l10n string id", function() {
        collection.warnL10n("fakeId");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("warning");
        expect(collection.at(0).get("message")).eql("translated:fakeId");
      });
    });

    describe("#error", function() {
      it("should add an error notification to the stack", function() {
        collection.error("wrong");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("error");
        expect(collection.at(0).get("message")).eql("wrong");
      });
    });

    describe("#errorL10n", function() {
      it("should notify an error using a l10n string id", function() {
        collection.errorL10n("fakeId");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("error");
        expect(collection.at(0).get("message")).eql("translated:fakeId");
      });

      it("should notify an error using a l10n string id + l10n properties",
        function() {
          collection.errorL10n("fakeId", "fakeProp");

          expect(collection).to.have.length.of(1);
          expect(collection.at(0).get("level")).eql("error");
          expect(collection.at(0).get("message")).eql("translated:fakeId:fakeProp");
      });
    });

  });
});
