/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.shared.router", function() {
  "use strict";

  var sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("BaseRouter", function() {
    var router, view;

    beforeEach(function() {
      router = new loop.shared.router.BaseRouter();
      $("#fixtures").html('<div id="main"></div>');
      var TestView = loop.shared.views.BaseView.extend({
        template: _.template("<p>plop</p>")
      });
      view = new TestView();
    });

    afterEach(function() {
      $("#fixtures").empty();
    });

    describe("#loadView", function() {
      it("should set the active view", function() {
        router.loadView(view);

        expect(router.activeView).eql(view);
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

  describe("BaseConversationRouter", function() {
    var conversation, notifier, TestRouter;

    beforeEach(function() {
      TestRouter = loop.shared.router.BaseConversationRouter.extend({
        startCall: sandbox.spy(),
        endCall: sandbox.spy()
      });
      conversation = new loop.shared.models.ConversationModel({
        loopToken: "fakeToken"
      }, {sdk: {}});
      notifier = {
        notify: sandbox.spy(),
        warn: sandbox.spy(),
        error: sandbox.spy()
      };
    });

    describe("#constructor", function() {
      it("should require a ConversationModel instance", function() {
        expect(function() {
          new TestRouter();
        }).to.Throw(Error, /missing required conversation/);
      });

      it("should require a notifier", function() {
        expect(function() {
          new TestRouter({conversation: {}});
        }).to.Throw(Error, /missing required notifier/);
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
          notifier: notifier
        });
      });

      it("should call startCall() once the call session is ready", function() {
        conversation.trigger("session:ready");

        sinon.assert.calledOnce(router.startCall);
      });

      it("should call endCall() when conversation ended", function() {
        conversation.trigger("session:ended");

        sinon.assert.calledOnce(router.endCall);
      });

      it("should warn the user when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(notifier.warn);
      });

      it("should call endCall() when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(router.endCall);
      });

      it("should warn the user when network disconnects", function() {
        conversation.trigger("session:network-disconnected");

        sinon.assert.calledOnce(notifier.warn);
      });

      it("should call endCall() when network disconnects", function() {
        conversation.trigger("session:network-disconnected");

        sinon.assert.calledOnce(router.endCall);
      });
    });
  });
});
