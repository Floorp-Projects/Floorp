/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

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

  describe("ConversationView", function() {
    var fakeSDK, fakeSession;

    beforeEach(function() {
      fakeSession = _.extend({
        connect: sandbox.spy(),
        disconnect: sandbox.spy(),
        unpublish: sandbox.spy()
      }, Backbone.Events);
      fakeSDK = {
        initPublisher: sandbox.spy(),
        initSession: sandbox.stub().returns(fakeSession)
      };
    });

    describe("#initialize", function() {
      it("should require an sdk object", function() {
        expect(function() {
          new sharedViews.ConversationView();
        }).to.Throw(Error, /sdk/);
      });

      describe("constructed", function() {
        var model, view;

        beforeEach(function() {
          model = new sharedModels.ConversationModel();
          view = new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });
        });

        it("should initiate a session", function() {
          sinon.assert.calledOnce(fakeSession.connect);
        });

        describe("sessionDisconnected event received", function() {
          it("should trigger a session:ended event", function(done) {
            model.once("session:ended", function() {
              done();
            });

            fakeSession.trigger("sessionDisconnected", {reason: "ko"});
          });

          it("should unpublish current stream publisher", function() {
            fakeSession.trigger("sessionDisconnected", {reason: "ko"});

            sinon.assert.calledOnce(fakeSession.unpublish);
          });
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

          it("should disconnect the session", function() {
            fakeSession.trigger("connectionDestroyed", fakeEvent);

            sinon.assert.calledOnce(fakeSession.unpublish);
          });

          it("should unpublish current stream publisher", function() {
            fakeSession.trigger("connectionDestroyed", fakeEvent);

            sinon.assert.calledOnce(fakeSession.unpublish);
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

          it("should disconnect the session", function() {
            fakeSession.trigger("networkDisconnected", {reason: "ko"});

            sinon.assert.calledOnce(fakeSession.unpublish);
          });

          it("should unpublish current stream publisher", function() {
            fakeSession.trigger("networkDisconnected", {reason: "ko"});

            sinon.assert.calledOnce(fakeSession.unpublish);
          });
        });
      });
    });

    describe("#hangup", function() {
      it("should disconnect the session", function() {
        var model = new sharedModels.ConversationModel();
        var view = new sharedViews.ConversationView({
          sdk: fakeSDK,
          model: model
        });
        view.hangup({preventDefault: function() {}});
        sinon.assert.calledOnce(fakeSession.disconnect);
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

    describe("#warn", function() {
      it("should add a warning notification to the stack", function() {
        var view = new sharedViews.NotificationListView({collection: coll});

        view.warn("watch out");

        expect(coll).to.have.length.of(1);
        expect(coll.at(0).get("level")).eql("warning");
        expect(coll.at(0).get("message")).eql("watch out");
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

