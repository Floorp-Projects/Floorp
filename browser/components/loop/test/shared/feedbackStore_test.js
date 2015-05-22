/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.FeedbackStore", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var FEEDBACK_STATES = loop.store.FEEDBACK_STATES;
  var sandbox, dispatcher, store, feedbackClient;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    dispatcher = new loop.Dispatcher();

    feedbackClient = new loop.FeedbackAPIClient("http://invalid", {
      product: "Loop"
    });

    store = new loop.store.FeedbackStore(dispatcher, {
      feedbackClient: feedbackClient
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if feedbackClient is missing", function() {
      expect(function() {
        new loop.store.FeedbackStore(dispatcher);
      }).to.Throw(/feedbackClient/);
    });

    it("should set the store to the INIT feedback state", function() {
      var store = new loop.store.FeedbackStore(dispatcher, {
        feedbackClient: feedbackClient
      });

      expect(store.getStoreState("feedbackState")).eql(FEEDBACK_STATES.INIT);
    });
  });

  describe("#requireFeedbackDetails", function() {
    it("should transition to DETAILS state", function() {
      store.requireFeedbackDetails(new sharedActions.RequireFeedbackDetails());

      expect(store.getStoreState("feedbackState")).eql(FEEDBACK_STATES.DETAILS);
    });
  });

  describe("#sendFeedback", function() {
    var sadFeedbackData = {
      happy: false,
      category: "fakeCategory",
      description: "fakeDescription"
    };

    beforeEach(function() {
      store.requireFeedbackDetails();
    });

    it("should send feedback data over the feedback client", function() {
      sandbox.stub(feedbackClient, "send");

      store.sendFeedback(new sharedActions.SendFeedback(sadFeedbackData));

      sinon.assert.calledOnce(feedbackClient.send);
      sinon.assert.calledWithMatch(feedbackClient.send, sadFeedbackData);
    });

    it("should transition to PENDING state", function() {
      sandbox.stub(feedbackClient, "send");

      store.sendFeedback(new sharedActions.SendFeedback(sadFeedbackData));

      expect(store.getStoreState("feedbackState")).eql(FEEDBACK_STATES.PENDING);
    });

    it("should transition to SENT state on successful submission", function(done) {
      sandbox.stub(feedbackClient, "send", function(data, cb) {
        cb(null);
      });

      store.once("change:feedbackState", function() {
        expect(store.getStoreState("feedbackState")).eql(FEEDBACK_STATES.SENT);
        done();
      });

      store.sendFeedback(new sharedActions.SendFeedback(sadFeedbackData));
    });

    it("should transition to FAILED state on failed submission", function(done) {
      sandbox.stub(feedbackClient, "send", function(data, cb) {
        cb(new Error("failed"));
      });

      store.once("change:feedbackState", function() {
        expect(store.getStoreState("feedbackState")).eql(FEEDBACK_STATES.FAILED);
        done();
      });

      store.sendFeedback(new sharedActions.SendFeedback(sadFeedbackData));
    });
  });

  describe("feedbackComplete", function() {
    it("should reset the store state", function() {
      store.setStoreState({feedbackState: FEEDBACK_STATES.SENT});

      store.feedbackComplete();

      expect(store.getStoreState()).eql({
        feedbackState: FEEDBACK_STATES.INIT
      });
    });
  });
});
