/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.conversation", function() {
  "use strict";

  var ConversationRouter = loop.conversation.ConversationRouter,
      sandbox,
      notifier;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    notifier = {notify: sandbox.spy()};
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("ConversationRouter", function() {
    var conversation;

    beforeEach(function() {
      conversation = new loop.shared.models.ConversationModel();
    });

    describe("#constructor", function() {
      it("should require a ConversationModel instance", function() {
        expect(function() {
          new ConversationRouter();
        }).to.Throw(Error, /missing required conversation/);
      });

      it("should require a notifier", function() {
        expect(function() {
          new ConversationRouter({conversation: conversation});
        }).to.Throw(Error, /missing required notifier/);
      });
    });

    describe("Routes", function() {
      var router;

      beforeEach(function() {
        router = new ConversationRouter({
          conversation: conversation,
          notifier: notifier
        });
        sandbox.stub(router, "loadView");
      });

      describe("#start", function() {
        it("should set the loopVersion on the conversation model", function() {
          router.start("fakeVersion");

          expect(conversation.get("loopVersion")).to.equal("fakeVersion");
        });

        it("should initiate the conversation", function() {
          sandbox.stub(conversation, "initiate");

          router.start("fakeVersion");

          sinon.assert.calledOnce(conversation.initiate);
          sinon.assert.calledWithExactly(conversation.initiate, {
            baseServerUrl: "http://example.com",
            outgoing: false
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

        it("should not load the ConversationView if session is not set",
          function() {
            router.conversation();

            sinon.assert.notCalled(router.loadView);
        });

        it("should notify the user when session is not set",
          function() {
            router.conversation();

            sinon.assert.calledOnce(router._notifier.notify);
        });
      });

      describe("#ended", function() {
        // XXX When the call is ended gracefully, we should check that we
        // close connections nicely
        it("should close the window", function() {
          sandbox.stub(window, "close");

          router.ended();

          sinon.assert.calledOnce(window.close);
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
      });

      it("should navigate to call/ongoing once the call session is ready",
        function() {
          sandbox.stub(ConversationRouter.prototype, "navigate");
          var router = new ConversationRouter({
            conversation: conversation,
            notifier: notifier
          });

          conversation.setReady(fakeSessionData);

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ongoing");
        });

      it("should navigate to call/ended when the call session ends",
        function() {
          sandbox.stub(ConversationRouter.prototype, "navigate");
          var router = new ConversationRouter({
            conversation: conversation,
            notifier: notifier
          });

          conversation.trigger("session:ended");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ended");
        });
    });

  });
});
