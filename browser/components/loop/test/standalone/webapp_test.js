/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.webapp", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      sandbox,
      notifier;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    notifier = {
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy(),
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("WebappRouter", function() {
    var router, conversation;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel({}, {sdk: {}});
      router = new loop.webapp.WebappRouter({
        conversation: conversation,
        notifier: notifier
      });
      sandbox.stub(router, "loadView");
      sandbox.stub(router, "navigate");
    });

    describe("#startCall", function() {
      it("should navigate back home if session token is missing", function() {
        router.startCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "home");
      });

      it("should notify the user if session token is missing", function() {
        router.startCall();

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "missing_conversation_info");
      });

      it("should navigate to call/ongoing/:token if session token is available",
        function() {
          conversation.set("loopToken", "fake");

          router.startCall();

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "call/ongoing/fake");
        });
    });

    describe("#endCall", function() {
      it("should navigate to home if session token is unset", function() {
        router.endCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "home");
      });

      it("should navigate to call/:token if session token is set", function() {
        conversation.set("loopToken", "fake");

        router.endCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fake");
      });
    });

    describe("Routes", function() {
      describe("#home", function() {
        it("should load the HomeView", function() {
          router.home();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(loop.webapp.HomeView));
        });
      });

      describe("#initiate", function() {
        it("should set the token on the conversation model", function() {
          router.initiate("fakeToken");

          expect(conversation.get("loopToken")).eql("fakeToken");
        });

        it("should load the ConversationFormView", function() {
          router.initiate("fakeToken");

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(loop.webapp.ConversationFormView));
        });

        // https://bugzilla.mozilla.org/show_bug.cgi?id=991118
        it("should terminate any ongoing call session", function() {
          sinon.stub(conversation, "endSession");
          conversation.set("ongoing", true);

          router.initiate("fakeToken");

          sinon.assert.calledOnce(conversation.endSession);
        });
      });

      describe("#loadConversation", function() {
        it("should load the ConversationView if session is set", function() {
          sandbox.stub(sharedViews.ConversationView.prototype, "initialize");
          conversation.set("sessionId", "fakeSessionId");

          router.loadConversation();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(sharedViews.ConversationView));
        });

        it("should navigate to #call/{token} if session isn't ready",
          function() {
            router.loadConversation("fakeToken");

            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
          });
      });
    });

    describe("Events", function() {
      var fakeSessionData;

      beforeEach(function() {
        fakeSessionData = {
          sessionId:    "sessionId",
          sessionToken: "sessionToken",
          apiKey:       "apiKey"
        };
        conversation.set("loopToken", "fakeToken");
      });

      it("should navigate to call/ongoing/:token once call session is ready",
        function() {
          conversation.trigger("session:ready");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ongoing/fakeToken");
        });

      it("should navigate to call/{token} when conversation ended", function() {
        conversation.trigger("session:ended");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
      });

      it("should navigate to call/{token} when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
      });

      it("should navigate to call/{token} when network disconnects",
        function() {
          conversation.trigger("session:network-disconnected");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
        });
    });
  });

  describe("ConversationFormView", function() {
    var conversation;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel({}, {sdk: {}});
    });

    describe("#initialize", function() {
      it("should require a conversation option", function() {
        expect(function() {
          new loop.webapp.WebappRouter();
        }).to.Throw(Error, /missing required conversation/);
      });
    });

    describe("#initiate", function() {
      var conversation, initiate, view, fakeSubmitEvent;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {sdk: {}});
        view = new loop.webapp.ConversationFormView({
          model: conversation,
          notifier: notifier
        });
        fakeSubmitEvent = {preventDefault: sinon.spy()};
        initiate = sinon.stub(conversation, "initiate");
      });

      it("should start the conversation establishment process", function() {
        conversation.set("loopToken", "fake");

        view.initiate(fakeSubmitEvent);

        sinon.assert.calledOnce(fakeSubmitEvent.preventDefault);
        sinon.assert.calledOnce(initiate);
        sinon.assert.calledWith(initiate, {
          baseServerUrl: loop.webapp.baseServerUrl,
          outgoing: true
        });
      });

      it("should disable current form once session is initiated", function() {
        sandbox.stub(view, "disableForm");
        conversation.set("loopToken", "fake");

        view.initiate(fakeSubmitEvent);

        sinon.assert.calledOnce(view.disableForm);
      });
    });

    describe("Events", function() {
      var conversation, view;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({
          loopToken: "fake"
        }, {sdk: {}});
        view = new loop.webapp.ConversationFormView({
          model: conversation,
          notifier: notifier
        });
      });

      it("should trigger a notication when a session:error model event is " +
         " received", function() {
        conversation.trigger("session:error", "tech error");

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "unable_retrieve_call_info");
      });
    });
  });
});
