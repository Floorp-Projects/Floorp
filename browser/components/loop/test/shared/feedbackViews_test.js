/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, React */
/* jshint newcap:false */

var expect = chai.expect;
var l10n = navigator.mozL10n || document.mozL10n;
var TestUtils = React.addons.TestUtils;
var sharedActions = loop.shared.actions;
var sharedViews = loop.shared.views;

var FEEDBACK_STATES = loop.store.FEEDBACK_STATES;

describe("loop.shared.views.FeedbackView", function() {
  "use strict";

  var sandbox, comp, dispatcher, fakeFeedbackClient, feedbackStore;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
    fakeFeedbackClient = {send: sandbox.stub()};
    feedbackStore = new loop.store.FeedbackStore(dispatcher, {
      feedbackClient: fakeFeedbackClient
    });
    loop.store.StoreMixin.register({feedbackStore: feedbackStore});
    comp = TestUtils.renderIntoDocument(
      React.createElement(sharedViews.FeedbackView));
  });

  afterEach(function() {
    sandbox.restore();
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
    it("should dispatch a SendFeedback action", function() {
      var dispatch = sandbox.stub(dispatcher, "dispatch");

      clickHappyFace(comp);

      sinon.assert.calledWithMatch(dispatch, new sharedActions.SendFeedback({
        happy: true,
        category: "",
        description: ""
      }));
    });

    it("should thank the user once feedback data is sent", function() {
      feedbackStore.setStoreState({feedbackState: FEEDBACK_STATES.SENT});

      expect(comp.getDOMNode().querySelectorAll(".thank-you")).not.eql(null);
      expect(comp.getDOMNode().querySelector("button.fx-embedded-btn-back"))
        .eql(null);
    });
  });

  describe("Sad feedback", function() {
    it("should bring the user to feedback form when clicking on the sad face",
      function() {
        clickSadFace(comp);

        expect(comp.getDOMNode().querySelectorAll("form")).not.eql(null);
      });

    it("should render a back button", function() {
      feedbackStore.setStoreState({feedbackState: FEEDBACK_STATES.DETAILS});

      expect(comp.getDOMNode().querySelector("button.fx-embedded-btn-back"))
        .not.eql(null);
    });

    it("should reset the view when clicking the back button", function() {
      feedbackStore.setStoreState({feedbackState: FEEDBACK_STATES.DETAILS});

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector("button.fx-embedded-btn-back"));

      expect(comp.getDOMNode().querySelector(".faces")).not.eql(null);
    });

    it("should disable the form submit button when no category is chosen",
      function() {
        clickSadFace(comp);

        expect(comp.getDOMNode().querySelector("form button").disabled).eql(true);
      });

    it("should disable the form submit button when the 'other' category is " +
       "chosen but no description has been entered yet",
      function() {
        clickSadFace(comp);
        fillSadFeedbackForm(comp, "other");

        expect(comp.getDOMNode().querySelector("form button").disabled).eql(true);
      });

    it("should enable the form submit button when the 'other' category is " +
       "chosen and a description is entered",
      function() {
        clickSadFace(comp);
        fillSadFeedbackForm(comp, "other", "fake");

        expect(comp.getDOMNode().querySelector("form button").disabled).eql(false);
      });

    it("should enable the form submit button once a predefined category is " +
       "chosen",
      function() {
        clickSadFace(comp);

        fillSadFeedbackForm(comp, "confusing");

        expect(comp.getDOMNode().querySelector("form button").disabled).eql(false);
      });

    it("should send feedback data when the form is submitted", function() {
      var dispatch = sandbox.stub(dispatcher, "dispatch");
      feedbackStore.setStoreState({feedbackState: FEEDBACK_STATES.DETAILS});
      fillSadFeedbackForm(comp, "confusing");

      submitSadFeedbackForm(comp);

      sinon.assert.calledOnce(dispatch);
      sinon.assert.calledWithMatch(dispatch, new sharedActions.SendFeedback({
        happy: false,
        category: "confusing",
        description: ""
      }));
    });

    it("should send feedback data when user has entered a custom description",
      function() {
        clickSadFace(comp);

        fillSadFeedbackForm(comp, "other", "fake reason");
        submitSadFeedbackForm(comp);

        sinon.assert.calledOnce(fakeFeedbackClient.send);
        sinon.assert.calledWith(fakeFeedbackClient.send, {
          happy: false,
          category: "other",
          description: "fake reason"
        });
      });

    it("should thank the user when feedback data has been sent", function() {
      fakeFeedbackClient.send = function(data, cb) {
        cb();
      };
      clickSadFace(comp);
      fillSadFeedbackForm(comp, "confusing");
      submitSadFeedbackForm(comp);

      expect(comp.getDOMNode().querySelectorAll(".thank-you")).not.eql(null);
    });
  });
});
