/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.shared.router", function() {
  "use strict";

  var sandbox, notifier;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    notifier = {
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy()
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("BaseRouter", function() {
    beforeEach(function() {
      $("#fixtures").html('<div id="main"></div>');
    });

    afterEach(function() {
      $("#fixtures").empty();
    });

    describe("#constructor", function() {
      it("should require a notifier", function() {
        expect(function() {
          new loop.shared.router.BaseRouter();
        }).to.Throw(Error, /missing required notifier/);
      });

      describe("inherited", function() {
        var ExtendedRouter = loop.shared.router.BaseRouter.extend({});

        it("should require a notifier", function() {
          expect(function() {
            new ExtendedRouter();
          }).to.Throw(Error, /missing required notifier/);
        });
      });
    });

    describe("constructed", function() {
      var router, view, TestRouter;

      beforeEach(function() {
        TestRouter = loop.shared.router.BaseRouter.extend({});
        var TestView = loop.shared.views.BaseView.extend({
          template: _.template("<p>plop</p>")
        });
        view = new TestView();
        router = new TestRouter({notifier: notifier});
      });

      describe("#loadView", function() {
        it("should set the active view", function() {
          router.loadView(view);

          expect(router._activeView).eql({
            type: "backbone",
            view: view
          });
        });

        it("should load and render the passed view", function() {
          router.loadView(view);

          expect($("#main p").text()).eql("plop");
        });
      });

      describe("#updateView", function() {
        it("should update the main element with provided contents", function() {
          router.updateView($("<p>plip</p>"));

          expect($("#main p").text()).eql("plip");
        });
      });
    });
  });

  describe("BaseConversationRouter", function() {
    var conversation, TestRouter;

    beforeEach(function() {
      TestRouter = loop.shared.router.BaseConversationRouter.extend({
        endCall: sandbox.spy()
      });
      conversation = new loop.shared.models.ConversationModel({
        loopToken: "fakeToken"
      }, {
        sdk: {},
        pendingCallTimeout: 1000
      });
    });

    describe("#constructor", function() {
      it("should require a ConversationModel instance", function() {
        expect(function() {
          new TestRouter({ client: {} });
        }).to.Throw(Error, /missing required conversation/);
      });
      it("should require a Client instance", function() {
        expect(function() {
          new TestRouter({ conversation: {} });
        }).to.Throw(Error, /missing required client/);
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
        router = new TestRouter({
          conversation: conversation,
          notifier: notifier,
          client: {}
        });
      });

      describe("session:connection-error", function() {

        it("should warn the user when .connect() call fails", function() {
          conversation.trigger("session:connection-error");

          sinon.assert.calledOnce(notifier.errorL10n);
          sinon.assert.calledWithExactly(notifier.errorL10n, sinon.match.string);
        });

        it("should invoke endCall()", function() {
          conversation.trigger("session:connection-error");

          sinon.assert.calledOnce(router.endCall);
          sinon.assert.calledWithExactly(router.endCall);
        });

      });

      it("should call endCall() when conversation ended", function() {
        conversation.trigger("session:ended");

        sinon.assert.calledOnce(router.endCall);
      });

      it("should warn the user when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(notifier.warnL10n);
        sinon.assert.calledWithExactly(notifier.warnL10n,
                                       "peer_ended_conversation");

      });

      it("should call endCall() when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(router.endCall);
      });

      it("should warn the user when network disconnects", function() {
        conversation.trigger("session:network-disconnected");

        sinon.assert.calledOnce(notifier.warnL10n);
        sinon.assert.calledWithExactly(notifier.warnL10n,
                                       "network_disconnected");
      });

      it("should call endCall() when network disconnects", function() {
        conversation.trigger("session:network-disconnected");

        sinon.assert.calledOnce(router.endCall);
      });
    });
  });
});
