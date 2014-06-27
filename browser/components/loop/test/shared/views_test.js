/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;
var l10n = document.webL10n || document.mozL10n;

describe("loop.shared.views", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers(); // exposes sandbox.clock as a fake timer
  });

  afterEach(function() {
    $("#fixtures").empty();
    sandbox.restore();
  });

  describe("L10nView", function() {
    beforeEach(function() {
      sandbox.stub(l10n, "translate");
    });

    it("should translate generated contents on render()", function() {
      var TestView = loop.shared.views.L10nView.extend();

      var view = new TestView();
      view.render();

      sinon.assert.calledOnce(l10n.translate);
      sinon.assert.calledWithExactly(l10n.translate, view.el);
    });
  });

  describe("ConversationView", function() {
    var fakeSDK, fakeSessionData, fakeSession, fakePublisher, model;

    beforeEach(function() {
      fakeSessionData = {
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
      };
      fakeSession = _.extend({
        connection: {connectionId: 42},
        connect: sandbox.spy(),
        disconnect: sandbox.spy(),
        publish: sandbox.spy(),
        unpublish: sandbox.spy(),
        subscribe: sandbox.spy()
      }, Backbone.Events);
      fakePublisher = {
        on: sandbox.spy(),
        off: sandbox.spy(),
        publishAudio: sandbox.spy(),
        publishVideo: sandbox.spy()
      };
      fakeSDK = {
        initPublisher: sandbox.stub().returns(fakePublisher),
        initSession: sandbox.stub().returns(fakeSession)
      };
      model = new sharedModels.ConversationModel(fakeSessionData, {
        sdk: fakeSDK
      });
    });

    describe("#initialize", function() {
      it("should require a sdk object", function() {
        expect(function() {
          new sharedViews.ConversationView();
        }).to.Throw(Error, /sdk/);
      });

      it("should start a session", function() {
        sandbox.stub(model, "startSession");

        new sharedViews.ConversationView({sdk: fakeSDK, model: model});

        sinon.assert.calledOnce(model.startSession);
      });
    });

    describe("constructed", function() {
      describe("#hangup", function() {
        it("should disconnect the session", function() {
          var view = new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });
          sandbox.stub(model, "endSession");
          view.publish();

          view.hangup({preventDefault: function() {}});

          sinon.assert.calledOnce(model.endSession);
        });
      });

      describe("#publish", function() {
        var view;

        beforeEach(function() {
          view = new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });
        });

        it("should publish local stream", function() {
          view.publish();

          sinon.assert.calledOnce(fakeSDK.initPublisher);
          sinon.assert.calledOnce(fakeSession.publish);
        });

        it("should start listening to OT publisher accessDialogOpened and " +
          " accessDenied events",
          function() {
            view.publish();

            sinon.assert.called(fakePublisher.on);
            sinon.assert.calledWith(fakePublisher.on, "accessDialogOpened");
            sinon.assert.calledWith(fakePublisher.on, "accessDenied");
          });
      });

      describe("#unpublish", function() {
        var view;

        beforeEach(function() {
          view = new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });
          view.publish();
        });

        it("should unpublish local stream", function() {
          view.unpublish();

          sinon.assert.calledOnce(fakeSession.unpublish);
        });

        it("should unsubscribe from accessDialogOpened and accessDenied events",
          function() {
            view.unpublish();

            sinon.assert.calledTwice(fakePublisher.off);
            sinon.assert.calledWith(fakePublisher.off, "accessDialogOpened");
            sinon.assert.calledWith(fakePublisher.off, "accessDenied");
          });
      });

      describe("#toggleMuteAudio", function() {
        var view;

        beforeEach(function() {
          view = new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });
          view.publish();
        });

        it("should unpublish local audio when enabled", function() {
          view.localStream = {hasAudio: true};

          view.toggleMuteAudio({preventDefault: sandbox.spy()});

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, false);
        });

        it("should publish local audio when disabled", function() {
          view.localStream = {hasAudio: false};

          view.toggleMuteAudio({preventDefault: sandbox.spy()});

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, true);
        });
      });

      describe("#toggleMuteVideo", function() {
        var view;

        beforeEach(function() {
          view = new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });
          view.publish();
        });

        it("should unpublish local video when enabled", function() {
          view.localStream = {hasVideo: true};

          view.toggleMuteVideo({preventDefault: sandbox.spy()});

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, false);
        });

        it("should publish local video when disabled", function() {
          view.localStream = {hasVideo: false};

          view.toggleMuteVideo({preventDefault: sandbox.spy()});

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, true);
        });
      });

      describe("Model events", function() {
        var view;

        beforeEach(function() {
          sandbox.stub(sharedViews.ConversationView.prototype, "publish");
          sandbox.stub(sharedViews.ConversationView.prototype, "unpublish");
          view = new sharedViews.ConversationView({sdk: fakeSDK, model: model});
        });

        it("should publish local stream on session:connected", function() {
          model.trigger("session:connected");

          sinon.assert.calledOnce(view.publish);
        });

        it("should publish remote streams on session:stream-created",
          function() {
            var s1 = {connection: {connectionId: 42}};
            var s2 = {connection: {connectionId: 43}};

            model.trigger("session:stream-created", {streams: [s1, s2]});

            sinon.assert.calledOnce(fakeSession.subscribe);
            sinon.assert.calledWith(fakeSession.subscribe, s2);
          });

        it("should unpublish local stream on session:ended", function() {
          model.trigger("session:ended");

          sinon.assert.calledOnce(view.unpublish);
        });

        it("should unpublish local stream on session:peer-hungup", function() {
          model.trigger("session:peer-hungup");

          sinon.assert.calledOnce(view.unpublish);
        });

        it("should unpublish local stream on session:network-disconnected",
          function() {
            model.trigger("session:network-disconnected");

            sinon.assert.calledOnce(view.unpublish);
          });
      });
    });
  });

  describe("NotificationView", function() {
    var collection, model, view;

    beforeEach(function() {
      $("#fixtures").append('<div id="test-notif"></div>');
      model = new sharedModels.NotificationModel({
        level: "error",
        message: "plop"
      });
      collection = new sharedModels.NotificationCollection([model]);
      view = new sharedViews.NotificationView({
        el: $("#test-notif"),
        collection: collection,
        model: model
      });
    });

    describe("#dismiss", function() {
      it("should automatically dismiss notification after 500ms", function() {
        view.render().dismiss({preventDefault: sandbox.spy()});

        expect(view.$(".message").text()).eql("plop");

        sandbox.clock.tick(500);

        expect(collection).to.have.length.of(0);
        expect($("#test-notif").html()).eql(undefined);
      });
    });

    describe("#render", function() {
      it("should render template with model attribute values", function() {
        view.render();

        expect(view.$(".message").text()).eql("plop");
      });
    });
  });

  describe("NotificationListView", function() {
    var coll, notifData, testNotif;

    beforeEach(function() {
      sandbox.stub(l10n, "get", function(x) {
        return "translated:" + x;
      });
      notifData = {level: "error", message: "plop"};
      testNotif = new sharedModels.NotificationModel(notifData);
      coll = new sharedModels.NotificationCollection();
    });

    describe("#initialize", function() {
      it("should accept a collection option", function() {
        var view = new sharedViews.NotificationListView({collection: coll});

        expect(view.collection).to.be.an.instanceOf(
          sharedModels.NotificationCollection);
      });

      it("should set a default collection when none is passed", function() {
        var view = new sharedViews.NotificationListView();

        expect(view.collection).to.be.an.instanceOf(
          sharedModels.NotificationCollection);
      });
    });

    describe("#clear", function() {
      it("should clear all notifications from the collection", function() {
        var view = new sharedViews.NotificationListView();
        view.notify(testNotif);

        view.clear();

        expect(coll).to.have.length.of(0);
      });
    });

    describe("#notify", function() {
      var view;

      beforeEach(function() {
        view = new sharedViews.NotificationListView({collection: coll});
      });

      describe("adds a new notification to the stack", function() {
        it("using a plain object", function() {
          view.notify(notifData);

          expect(coll).to.have.length.of(1);
        });

        it("using a NotificationModel instance", function() {
          view.notify(testNotif);

          expect(coll).to.have.length.of(1);
        });
      });
    });

    describe("#notifyL10n", function() {
      var view;

      beforeEach(function() {
        view = new sharedViews.NotificationListView({collection: coll});
      });

      it("should translate a message string identifier", function() {
        view.notifyL10n("fakeId", "warning");

        sinon.assert.calledOnce(l10n.get);
        sinon.assert.calledWithExactly(l10n.get, "fakeId");
      });

      it("should notify end user with the provided message", function() {
        sandbox.stub(view, "notify");

        view.notifyL10n("fakeId", "warning");

        sinon.assert.calledOnce(view.notify);
        sinon.assert.calledWithExactly(view.notify, {
          message: "translated:fakeId",
          level: "warning"
        });
      });
    });

    describe("#warn", function() {
      it("should add a warning notification to the stack", function() {
        var view = new sharedViews.NotificationListView({collection: coll});

        view.warn("watch out");

        expect(coll).to.have.length.of(1);
        expect(coll.at(0).get("level")).eql("warning");
        expect(coll.at(0).get("message")).eql("watch out");
      });
    });

    describe("#warnL10n", function() {
      it("should warn using a l10n string id", function() {
        var view = new sharedViews.NotificationListView({collection: coll});
        sandbox.stub(view, "notify");

        view.warnL10n("fakeId");

        sinon.assert.called(view.notify);
        sinon.assert.calledWithExactly(view.notify, {
          message: "translated:fakeId",
          level: "warning"
        });
      });
    });

    describe("#error", function() {
      it("should add an error notification to the stack", function() {
        var view = new sharedViews.NotificationListView({collection: coll});

        view.error("wrong");

        expect(coll).to.have.length.of(1);
        expect(coll.at(0).get("level")).eql("error");
        expect(coll.at(0).get("message")).eql("wrong");
      });
    });

    describe("#errorL10n", function() {
      it("should notify an error using a l10n string id", function() {
        var view = new sharedViews.NotificationListView({collection: coll});
        sandbox.stub(view, "notify");

        view.errorL10n("fakeId");

        sinon.assert.called(view.notify);
        sinon.assert.calledWithExactly(view.notify, {
          message: "translated:fakeId",
          level: "error"
        });
      });
    });

    describe("Collection events", function() {
      var view;

      beforeEach(function() {
        sandbox.stub(sharedViews.NotificationListView.prototype, "render");
        view = new sharedViews.NotificationListView({collection: coll});
      });

      it("should render when a notification is added to the collection",
        function() {
          coll.add(testNotif);

          sinon.assert.calledOnce(view.render);
        });

      it("should render when a notification is removed from the collection",
        function() {
          coll.add(testNotif);
          coll.remove(testNotif);

          sinon.assert.calledTwice(view.render);
        });

      it("should render when the collection is reset", function() {
        coll.reset();

        sinon.assert.calledOnce(view.render);
      });
    });
  });
});

