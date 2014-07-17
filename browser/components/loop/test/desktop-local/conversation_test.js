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
    notifier = {
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy()
    };

    window.navigator.mozLoop = {
      get serverUrl() {
        return "http://example.com";
      },

      startAlerting: function() {
      },

      stopAlerting: function() {
      }
    };
  });

  afterEach(function() {
    delete window.navigator.mozLoop;
    sandbox.restore();
  });

  describe("#init", function() {
    var conversation, oldTitle;

    beforeEach(function() {
      oldTitle = document.title;

      sandbox.stub(document.mozL10n, "initialize");
      sandbox.stub(document.mozL10n, "get").returns("Fake title");

      sandbox.stub(loop.conversation.ConversationRouter.prototype,
        "initialize");
      sandbox.stub(loop.shared.models.ConversationModel.prototype,
        "initialize");
      sandbox.stub(loop.shared.views.NotificationListView.prototype,
        "initialize");

      sandbox.stub(Backbone.history, "start");
    });

    afterEach(function() {
      document.title = oldTitle;
    });

    it("should initalize L10n", function() {
      loop.conversation.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);
      sinon.assert.calledWithExactly(document.mozL10n.initialize,
        window.navigator.mozLoop);
    });

    it("should set the document title", function() {
      loop.conversation.init();

      expect(document.title).to.be.equal("Fake title");
    });

    it("should create the router", function() {
      loop.conversation.init();

      sinon.assert.calledOnce(
        loop.conversation.ConversationRouter.prototype.initialize);
    });

    it("should start Backbone history", function() {
      loop.conversation.init();

      sinon.assert.calledOnce(Backbone.history.start);
    });
  });

  describe("ConversationRouter", function() {
    var conversation;

    beforeEach(function() {
      conversation = new loop.shared.models.ConversationModel({}, {
        sdk: {},
        pendingCallTimeout: 1000
      });
      sandbox.stub(conversation, "initiate");
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

      describe("#incoming", function() {
        it("should set the loopVersion on the conversation model", function() {
          router.incoming("fakeVersion");

          expect(conversation.get("loopVersion")).to.equal("fakeVersion");
        });

        it("should display the incoming call view", function() {
          router.incoming("fakeVersion");

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWithExactly(router.loadView,
            sinon.match.instanceOf(loop.conversation.IncomingCallView));
        });

        it("should start alerting", function() {
          sandbox.stub(window.navigator.mozLoop, "startAlerting");
          router.incoming("fakeVersion");

          sinon.assert.calledOnce(window.navigator.mozLoop.startAlerting);
        });
      });

      describe("#accept", function() {
        it("should initiate the conversation", function() {
          router.accept();

          sinon.assert.calledOnce(conversation.initiate);
          sinon.assert.calledWithMatch(conversation.initiate, {
            client: {
              mozLoop: navigator.mozLoop,
              settings: {}
            },
            outgoing: false
          });
        });

        it("should stop alerting", function() {
          sandbox.stub(window.navigator.mozLoop, "stopAlerting");
          router.accept();

          sinon.assert.calledOnce(window.navigator.mozLoop.stopAlerting);
        });
      });

      describe("#conversation", function() {
        beforeEach(function() {
          sandbox.stub(router, "loadReactComponent");
        });

        it("should load the ConversationView if session is set", function() {
          conversation.set("sessionId", "fakeSessionId");

          router.conversation();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWith(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isComponentOfType(
                value, loop.shared.views.ConversationView);
            }));
        });

        it("should not load the ConversationView if session is not set",
          function() {
            router.conversation();

            sinon.assert.notCalled(router.loadReactComponent);
        });

        it("should notify the user when session is not set",
          function() {
            router.conversation();

            sinon.assert.calledOnce(router._notifier.errorL10n);
            sinon.assert.calledWithExactly(router._notifier.errorL10n,
              "cannot_start_call_session_not_ready");
        });
      });

      describe("#decline", function() {
        beforeEach(function() {
          sandbox.stub(window, "close");
        });

        it("should close the window", function() {
          router.decline();

          sinon.assert.calledOnce(window.close);
        });

        it("should stop alerting", function() {
          sandbox.stub(window.navigator.mozLoop, "stopAlerting");
          router.decline();

          sinon.assert.calledOnce(window.navigator.mozLoop.stopAlerting);
        });
      });

      describe("#ended", function() {
        // XXX When the call is ended gracefully, we should check that we
        // close connections nicely
        it("should close the window");
      });
    });

    describe("Events", function() {
      var router, fakeSessionData;

      beforeEach(function() {
        fakeSessionData = {
          sessionId:    "sessionId",
          sessionToken: "sessionToken",
          apiKey:       "apiKey"
        };
        sandbox.stub(loop.conversation.ConversationRouter.prototype,
                     "navigate");
        conversation.set("loopToken", "fakeToken");
        router = new loop.conversation.ConversationRouter({
          conversation: conversation,
          notifier: notifier
        });
      });

      it("should navigate to call/ongoing once the call session is ready",
        function() {
          conversation.setReady(fakeSessionData);

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ongoing");
        });

      it("should navigate to call/ended when the call session ends",
        function() {
          conversation.trigger("session:ended");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ended");
        });

      it("should navigate to call/ended when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWith(router.navigate, "call/ended");
      });

      it("should navigate to call/{token} when network disconnects",
        function() {
          conversation.trigger("session:network-disconnected");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ended");
        });
    });
  });

  describe("EndedCallView", function() {
    describe("#closeWindow", function() {
      it("should close the conversation window", function() {
        sandbox.stub(window, "close");
        var view = new loop.conversation.EndedCallView();

        view.closeWindow({preventDefault: sandbox.spy()});

        sinon.assert.calledOnce(window.close);
      });
    });
  });

  describe("IncomingCallView", function() {
    var conversation, view;

    beforeEach(function() {
      conversation = new loop.shared.models.ConversationModel({}, {
        sdk: {},
        pendingCallTimeout: 1000
      });
      view = new loop.conversation.IncomingCallView({model: conversation});
    });

    describe("#handleAccept", function() {
      it("should trigger an 'accept' conversation model event" ,
        function(done) {
          conversation.once("accept", function() {
            done();
          });

          view.handleAccept({preventDefault: sandbox.spy()});
        });
    });

    describe("#handleDecline", function() {
      it("should trigger an 'decline' conversation model event" ,
        function(done) {
          conversation.once("decline", function() {
            done();
          });

          view.handleDecline({preventDefault: sandbox.spy()});
        });
    });
  });
});
