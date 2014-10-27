/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, React */
/* jshint newcap:false */

var expect = chai.expect;
var l10n = navigator.mozL10n || document.mozL10n;
var TestUtils = React.addons.TestUtils;

describe("loop.shared.views", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      getReactElementByClass = TestUtils.findRenderedDOMComponentWithClass,
      sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers(); // exposes sandbox.clock as a fake timer
    sandbox.stub(l10n, "get", function(x) {
      return "translated:" + x;
    });
  });

  afterEach(function() {
    $("#fixtures").empty();
    sandbox.restore();
  });

  describe("MediaControlButton", function() {
    it("should render an enabled local audio button", function() {
      var comp = TestUtils.renderIntoDocument(sharedViews.MediaControlButton({
        scope: "local",
        type: "audio",
        action: function(){},
        enabled: true
      }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local audio button", function() {
      var comp = TestUtils.renderIntoDocument(sharedViews.MediaControlButton({
        scope: "local",
        type: "audio",
        action: function(){},
        enabled: false
      }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });

    it("should render an enabled local video button", function() {
      var comp = TestUtils.renderIntoDocument(sharedViews.MediaControlButton({
        scope: "local",
        type: "video",
        action: function(){},
        enabled: true
      }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local video button", function() {
      var comp = TestUtils.renderIntoDocument(sharedViews.MediaControlButton({
        scope: "local",
        type: "video",
        action: function(){},
        enabled: false
      }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });
  });

  describe("ConversationToolbar", function() {
    var hangup, publishStream;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        sharedViews.ConversationToolbar(props));
    }

    beforeEach(function() {
      hangup = sandbox.stub();
      publishStream = sandbox.stub();
    });

    it("should hangup when hangup button is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: {enabled: true}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-hangup"));

      sinon.assert.calledOnce(hangup);
      sinon.assert.calledWithExactly(hangup);
    });

    it("should unpublish audio when audio mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: {enabled: true}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-audio"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "audio", false);
    });

    it("should publish audio when audio mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: {enabled: false}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-audio"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "audio", true);
    });

    it("should unpublish video when video mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        video: {enabled: true}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-video"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "video", false);
    });

    it("should publish video when video mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        video: {enabled: false}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-video"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "video", true);
    });
  });

  describe("ConversationView", function() {
    var fakeSDK, fakeSessionData, fakeSession, fakePublisher, model;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(sharedViews.ConversationView(props));
    }

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
      fakePublisher = _.extend({
        publishAudio: sandbox.spy(),
        publishVideo: sandbox.spy()
      }, Backbone.Events);
      fakeSDK = {
        initPublisher: sandbox.stub().returns(fakePublisher),
        initSession: sandbox.stub().returns(fakeSession)
      };
      model = new sharedModels.ConversationModel(fakeSessionData, {
        sdk: fakeSDK
      });
    });

    describe("#componentDidMount", function() {
      it("should start a session by default", function() {
        sandbox.stub(model, "startSession");

        mountTestComponent({
          sdk: fakeSDK,
          model: model,
          video: {enabled: true}
        });

        sinon.assert.calledOnce(model.startSession);
      });

      it("shouldn't start a session if initiate is false", function() {
        sandbox.stub(model, "startSession");

        mountTestComponent({
          initiate: false,
          sdk: fakeSDK,
          model: model,
          video: {enabled: true}
        });

        sinon.assert.notCalled(model.startSession);
      });

      it("should set the correct stream publish options", function() {

        var component = mountTestComponent({
          sdk: fakeSDK,
          model: model,
          video: {enabled: false}
        });

        expect(component.publisherConfig.publishVideo).to.eql(false);

      });
    });

    describe("constructed", function() {
      var comp;

      beforeEach(function() {
        comp = mountTestComponent({
          sdk: fakeSDK,
          model: model,
          video: {enabled: false}
        });
      });

      describe("#hangup", function() {
        beforeEach(function() {
          comp.startPublishing();
        });

        it("should disconnect the session", function() {
          sandbox.stub(model, "endSession");

          comp.hangup();

          sinon.assert.calledOnce(model.endSession);
        });

        it("should stop publishing local streams", function() {
          comp.hangup();

          sinon.assert.calledOnce(fakeSession.unpublish);
        });
      });

      describe("#startPublishing", function() {
        beforeEach(function() {
          sandbox.stub(fakePublisher, "on");
        });

        it("should publish local stream", function() {
          comp.startPublishing();

          sinon.assert.calledOnce(fakeSDK.initPublisher);
          sinon.assert.calledOnce(fakeSession.publish);
        });

        it("should start listening to OT publisher accessDialogOpened and " +
          " accessDenied events",
          function() {
            comp.startPublishing();

            sinon.assert.called(fakePublisher.on);
            sinon.assert.calledWith(fakePublisher.on,
                                    "accessDialogOpened accessDenied");
          });
      });

      describe("#stopPublishing", function() {
        beforeEach(function() {
          sandbox.stub(fakePublisher, "off");
          comp.startPublishing();
        });

        it("should stop publish local stream", function() {
          comp.stopPublishing();

          sinon.assert.calledOnce(fakeSession.unpublish);
        });

        it("should unsubscribe from publisher events",
          function() {
            comp.stopPublishing();

            // Note: Backbone.Events#stopListening calls off() on passed object.
            sinon.assert.calledOnce(fakePublisher.off);
          });
      });

      describe("#publishStream", function() {
        var comp;

        beforeEach(function() {
          comp = mountTestComponent({
            sdk: fakeSDK,
            model: model,
            video: {enabled: false}
          });
          comp.startPublishing();
        });

        it("should start streaming local audio", function() {
          comp.publishStream("audio", true);

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, true);
        });

        it("should stop streaming local audio", function() {
          comp.publishStream("audio", false);

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, false);
        });

        it("should start streaming local video", function() {
          comp.publishStream("video", true);

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, true);
        });

        it("should stop streaming local video", function() {
          comp.publishStream("video", false);

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, false);
        });
      });

      describe("Model events", function() {
        it("should start streaming on session:connected", function() {
          model.trigger("session:connected");

          sinon.assert.calledOnce(fakeSDK.initPublisher);
        });

        it("should publish remote stream on session:stream-created",
          function() {
            var s1 = {connection: {connectionId: 42}};

            model.trigger("session:stream-created", {stream: s1});

            sinon.assert.calledOnce(fakeSession.subscribe);
            sinon.assert.calledWith(fakeSession.subscribe, s1);
          });

        it("should unpublish local stream on session:ended", function() {
          comp.startPublishing();

          model.trigger("session:ended");

          sinon.assert.calledOnce(fakeSession.unpublish);
        });

        it("should unpublish local stream on session:peer-hungup", function() {
          comp.startPublishing();

          model.trigger("session:peer-hungup");

          sinon.assert.calledOnce(fakeSession.unpublish);
        });

        it("should unpublish local stream on session:network-disconnected",
          function() {
            comp.startPublishing();

            model.trigger("session:network-disconnected");

            sinon.assert.calledOnce(fakeSession.unpublish);
          });
      });

      describe("Publisher events", function() {
        beforeEach(function() {
          comp.startPublishing();
        });

        it("should set audio state on streamCreated", function() {
          fakePublisher.trigger("streamCreated", {stream: {hasAudio: true}});
          expect(comp.state.audio.enabled).eql(true);

          fakePublisher.trigger("streamCreated", {stream: {hasAudio: false}});
          expect(comp.state.audio.enabled).eql(false);
        });

        it("should set video state on streamCreated", function() {
          fakePublisher.trigger("streamCreated", {stream: {hasVideo: true}});
          expect(comp.state.video.enabled).eql(true);

          fakePublisher.trigger("streamCreated", {stream: {hasVideo: false}});
          expect(comp.state.video.enabled).eql(false);
        });

        it("should set media state on streamDestroyed", function() {
          fakePublisher.trigger("streamDestroyed");

          expect(comp.state.audio.enabled).eql(false);
          expect(comp.state.video.enabled).eql(false);
        });
      });
    });
  });

  describe("FeedbackView", function() {
    var comp, fakeFeedbackApiClient;

    beforeEach(function() {
      fakeFeedbackApiClient = {send: sandbox.stub()};
      comp = TestUtils.renderIntoDocument(sharedViews.FeedbackView({
        feedbackApiClient: fakeFeedbackApiClient
      }));
    });

    // local test helpers
    function clickHappyFace(comp) {
      var happyFace = comp.getDOMNode().querySelector(".face-happy");
      TestUtils.Simulate.click(happyFace);
    }

    function clickSadFace(comp) {
      var sadFace = comp.getDOMNode().querySelector(".face-sad");
      TestUtils.Simulate.click(sadFace);
    }

    function fillSadFeedbackForm(comp, category, text) {
      TestUtils.Simulate.change(
        comp.getDOMNode().querySelector("[value='" + category + "']"));

      if (text) {
        TestUtils.Simulate.change(
          comp.getDOMNode().querySelector("[name='description']"), {
            target: {value: "fake reason"}
          });
      }
    }

    function submitSadFeedbackForm(comp, category, text) {
      TestUtils.Simulate.submit(comp.getDOMNode().querySelector("form"));
    }

    describe("Happy feedback", function() {
      it("should send feedback data when clicking on the happy face",
        function() {
          clickHappyFace(comp);

          sinon.assert.calledOnce(fakeFeedbackApiClient.send);
          sinon.assert.calledWith(fakeFeedbackApiClient.send, {happy: true});
        });

      it("should thank the user once happy feedback data is sent", function() {
        fakeFeedbackApiClient.send = function(data, cb) {
          cb();
        };

        clickHappyFace(comp);

        expect(comp.getDOMNode()
                   .querySelectorAll(".feedback .thank-you").length).eql(1);
        expect(comp.getDOMNode().querySelector("button.back")).to.be.a("null");
      });
    });

    describe("Sad feedback", function() {
      it("should bring the user to feedback form when clicking on the sad face",
        function() {
          clickSadFace(comp);

          expect(comp.getDOMNode().querySelectorAll("form").length).eql(1);
        });

      it("should disable the form submit button when no category is chosen",
        function() {
          clickSadFace(comp);

          expect(comp.getDOMNode()
                     .querySelector("form button").disabled).eql(true);
        });

      it("should disable the form submit button when the 'other' category is " +
         "chosen but no description has been entered yet",
        function() {
          clickSadFace(comp);
          fillSadFeedbackForm(comp, "other");

          expect(comp.getDOMNode()
                     .querySelector("form button").disabled).eql(true);
        });

      it("should enable the form submit button when the 'other' category is " +
         "chosen and a description is entered",
        function() {
          clickSadFace(comp);
          fillSadFeedbackForm(comp, "other", "fake");

          expect(comp.getDOMNode()
                     .querySelector("form button").disabled).eql(false);
        });

      it("should empty the description field when a predefined category is " +
         "chosen",
        function() {
          clickSadFace(comp);

          fillSadFeedbackForm(comp, "confusing");

          expect(comp.getDOMNode()
                     .querySelector(".feedback-description").value).eql("");
        });

      it("should enable the form submit button once a predefined category is " +
         "chosen",
        function() {
          clickSadFace(comp);

          fillSadFeedbackForm(comp, "confusing");

          expect(comp.getDOMNode()
                     .querySelector("form button").disabled).eql(false);
        });

      it("should disable the form submit button once the form is submitted",
        function() {
          clickSadFace(comp);
          fillSadFeedbackForm(comp, "confusing");

          submitSadFeedbackForm(comp);

          expect(comp.getDOMNode()
                     .querySelector("form button").disabled).eql(true);
        });

      it("should send feedback data when the form is submitted", function() {
        clickSadFace(comp);
        fillSadFeedbackForm(comp, "confusing");

        submitSadFeedbackForm(comp);

        sinon.assert.calledOnce(fakeFeedbackApiClient.send);
        sinon.assert.calledWithMatch(fakeFeedbackApiClient.send, {
          happy: false,
          category: "confusing"
        });
      });

      it("should send feedback data when user has entered a custom description",
        function() {
          clickSadFace(comp);

          fillSadFeedbackForm(comp, "other", "fake reason");
          submitSadFeedbackForm(comp);

          sinon.assert.calledOnce(fakeFeedbackApiClient.send);
          sinon.assert.calledWith(fakeFeedbackApiClient.send, {
            happy: false,
            category: "other",
            description: "fake reason"
          });
        });

      it("should thank the user when feedback data has been sent", function() {
        fakeFeedbackApiClient.send = function(data, cb) {
          cb();
        };
        clickSadFace(comp);
        fillSadFeedbackForm(comp, "confusing");
        submitSadFeedbackForm(comp);

        expect(comp.getDOMNode()
                   .querySelectorAll(".feedback .thank-you").length).eql(1);
      });
    });
  });

  describe("NotificationListView", function() {
    var coll, view, testNotif;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(sharedViews.NotificationListView(props));
    }

    beforeEach(function() {
      coll = new sharedModels.NotificationCollection();
      view = mountTestComponent({notifications: coll});
      testNotif = {level: "warning", message: "foo"};
      sinon.spy(view, "render");
    });

    afterEach(function() {
      view.render.restore();
    });

    describe("Collection events", function() {
      it("should render when a notification is added to the collection",
        function() {
          coll.add(testNotif);

          sinon.assert.calledOnce(view.render);
        });

      it("should render when a notification is removed from the collection",
        function() {
          coll.add(testNotif);
          coll.remove(testNotif);

          sinon.assert.calledOnce(view.render);
        });

      it("should render when the collection is reset", function() {
        coll.reset();

        sinon.assert.calledOnce(view.render);
      });
    });
  });
});

