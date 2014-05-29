/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.webapp", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("WebappRouter", function() {
    var conversation, notifier, fakeSessionData;

    beforeEach(function() {
      notifier = {notify: sandbox.spy()};
      conversation = new sharedModels.ConversationModel();
      fakeSessionData = {
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
      };
    });

    describe("#constructor", function() {
      it("should require a ConversationModel instance", function() {
        expect(function() {
          new loop.webapp.WebappRouter();
        }).to.Throw(Error, /missing required conversation/);
      });

      it("should require a notifier", function() {
        expect(function() {
          new loop.webapp.WebappRouter({conversation: {}});
        }).to.Throw(Error, /missing required notifier/);
      });
    });

    describe("constructed", function() {
      var router;

      beforeEach(function() {
        router = new loop.webapp.WebappRouter({conversation: conversation});
      });

      describe("#loadView", function() {
        // XXX hard to test as hell… functional?
        it("should load the passed view");
      });
    });

    describe("Routes", function() {
      var router;

      beforeEach(function() {
        router = new loop.webapp.WebappRouter({
          conversation: conversation,
          notifier: notifier
        });
        sandbox.stub(router, "loadView");
      });

      describe("#home", function() {
        it("should load the HomeView", function() {
          router.home();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWithMatch(router.loadView,
                                       {$el: {selector: "#home"}});
        });
      });

      describe("#initiate", function() {
        it("should load the ConversationFormView and set the token",
          function() {
            router.initiate("fakeToken");

            sinon.assert.calledOnce(router.loadView);
            sinon.assert.calledWithMatch(router.loadView, {
              $el: {selector: "#conversation-form"},
              model: {attributes: {loopToken: "fakeToken"}}
            });
          });
      });

      describe("#conversation", function() {
        it("should load the ConversationView if session is set", function() {
          sandbox.stub(loop.shared.views.ConversationView.prototype,
            "initialize");
          conversation.set("sessionId", "fakeSessionId");

          router.conversation();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWithMatch(router.loadView, {
            $el: {selector: "#conversation"}
          });
        });

        it("should redirect to #call/{token} if session isn't ready",
          function() {
            conversation.set("loopToken", "fakeToken");
            sandbox.stub(router, "navigate");

            router.conversation();

            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
          });

        it("should redirect to #home if no call token is set", function() {
          sandbox.stub(router, "navigate");

          router.conversation();

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "home");
        });
      });
    });

    describe("Events", function() {
      it("should navigate to call/ongoing once the call session is ready",
        function() {
          sandbox.stub(loop.webapp.WebappRouter.prototype, "navigate");
          var router = new loop.webapp.WebappRouter({
            conversation: conversation,
            notifier: notifier
          });

          conversation.setReady(fakeSessionData);

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ongoing");
        });
    });
  });

  describe("ConversationFormView", function() {
    describe("#initialize", function() {
      it("should require a conversation option", function() {
        expect(function() {
          new loop.webapp.WebappRouter();
        }).to.Throw(Error, /missing required conversation/);
      });

      it("should require a notifier option", function() {
        expect(function() {
          new loop.webapp.WebappRouter({conversation: {}});
        }).to.Throw(Error, /missing required notifier/);
      });
    });

    describe("#initiate", function() {
      var notifier, conversation, initiate, view, fakeSubmitEvent;

      beforeEach(function() {
        notifier = {notify: sandbox.spy()};
        conversation = new sharedModels.ConversationModel();
        view = new loop.webapp.ConversationFormView({
          model: conversation,
          notifier: notifier
        });
        fakeSubmitEvent = {preventDefault: sinon.spy()};
      });

      it("should start the conversation establishment process", function() {
        initiate = sinon.stub(conversation, "initiate");
        conversation.set("loopToken", "fake");

        view.initiate(fakeSubmitEvent);

        sinon.assert.calledOnce(fakeSubmitEvent.preventDefault);
        sinon.assert.calledOnce(initiate);
        sinon.assert.calledWith(initiate, {
          baseServerUrl: loop.webapp.baseServerUrl,
          outgoing: true
        });
      });
    });

    describe("Events", function() {
      var notifier, conversation, view;

      beforeEach(function() {
        notifier = {notify: sandbox.spy()};
        conversation = new sharedModels.ConversationModel({
          loopToken: "fake"
        });
        view = new loop.webapp.ConversationFormView({
          model: conversation,
          notifier: notifier
        });
      });

      it("should trigger a notication when a session:error model event is " +
         " received", function() {
        conversation.trigger("session:error", "tech error");

        sinon.assert.calledOnce(notifier.notify);
        // XXX We should test for the actual message content, but webl10n gets
        //     in the way as translated messages are all empty because matching
        //     DOM elements are missing.
        sinon.assert.calledWithMatch(notifier.notify, {level: "error"});
      });
    });
  });
});
