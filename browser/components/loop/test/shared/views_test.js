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
        connect: sandbox.spy()
      }, Backbone.Events);
      fakeSDK = {
        initPublisher: sandbox.spy(),
        initSession: sandbox.stub().returns(fakeSession)
      };
    });

    describe("initialize", function() {
      it("should require an sdk object", function() {
        expect(function() {
          new sharedViews.ConversationView();
        }).to.Throw(Error, /sdk/);
      });

      it("should initiate a session", function() {
        new sharedViews.ConversationView({
          sdk: fakeSDK,
          model: new sharedModels.ConversationModel()
        });

        sinon.assert.calledOnce(fakeSession.connect);
      });

      it("should trigger a session:ended model event when connectionDestroyed" +
         " event is received/", function(done) {
          // XXX remove when we implement proper notifications
          sandbox.stub(window, "alert");
          var model = new sharedModels.ConversationModel();
          new sharedViews.ConversationView({
            sdk: fakeSDK,
            model: model
          });

          model.once("session:ended", function() {
            done();
          });

          fakeSession.trigger("connectionDestroyed", {reason: "ko"});
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
    describe("Collection events", function() {
      var coll, testNotif, view;

      beforeEach(function() {
        sandbox.stub(sharedViews.NotificationListView.prototype, "render");
        testNotif = new sharedModels.NotificationModel({
          level: "error",
          message: "plop"
        });
        coll = new sharedModels.NotificationCollection();
        view = new sharedViews.NotificationListView({collection: coll});
      });

      it("should render on notification added to the collection", function() {
        coll.add(testNotif);

        sinon.assert.calledOnce(view.render);
      });

      it("should render on notification removed from the collection",
        function() {
          coll.add(testNotif);
          coll.remove(testNotif);

          sinon.assert.calledTwice(view.render);
        });

      it("should render on collection reset",
        function() {
          coll.reset();

          sinon.assert.calledOnce(view.render);
        });
    });
  });
});

