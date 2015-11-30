/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.feedbackViews", function() {
  "use strict";

  var FeedbackView = loop.feedbackViews.FeedbackView;
  var l10n = navigator.mozL10n || document.mozL10n;
  var TestUtils = React.addons.TestUtils;
  var sandbox, mozL10nGet;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();
    mozL10nGet = sandbox.stub(l10n, "get", function(x) {
      return "translated:" + x;
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("FeedbackView", function() {
    var openURLStub, getLoopPrefStub, feedbackReceivedStub;
    var fakeURL = "fake.form", mozLoop, view;

    function mountTestComponent(props) {
      props = _.extend({
        onAfterFeedbackReceived: feedbackReceivedStub
      }, props);

      return TestUtils.renderIntoDocument(
        React.createElement(FeedbackView, props));
    }

    beforeEach(function() {
      openURLStub = sandbox.stub();
      getLoopPrefStub = sandbox.stub();
      feedbackReceivedStub = sandbox.stub();

      LoopMochaUtils.stubLoopRequest({
        OpenURL: openURLStub,
        GetLoopPref: getLoopPrefStub
      });
    });

    afterEach(function() {
      view = null;
      LoopMochaUtils.restore();
    });

    it("should render a feedback view", function() {
      view = mountTestComponent();

      TestUtils.findRenderedComponentWithType(view, FeedbackView);
    });

    it("should render a button with correct text", function() {
      view = mountTestComponent();

      sinon.assert.calledWithExactly(mozL10nGet, "feedback_request_button");
    });

    it("should render a header with correct text", function() {
      view = mountTestComponent();

      sinon.assert.calledWithExactly(mozL10nGet, "feedback_window_heading");
    });

    it("should open a new page to the feedback form", function() {
      getLoopPrefStub.withArgs("feedback.formURL").returns(fakeURL);
      view = mountTestComponent();

      TestUtils.Simulate.click(view.refs.feedbackFormBtn.getDOMNode());

      sinon.assert.calledOnce(openURLStub);
      sinon.assert.calledWithExactly(openURLStub, fakeURL);
    });

    it("should fetch the feedback form URL from the prefs", function() {
      getLoopPrefStub.withArgs("feedback.formURL").returns(fakeURL);
      view = mountTestComponent();

      TestUtils.Simulate.click(view.refs.feedbackFormBtn.getDOMNode());

      sinon.assert.calledOnce(getLoopPrefStub);
      sinon.assert.calledWithExactly(getLoopPrefStub, "feedback.formURL");
    });

    it("should close the window after opening the form", function() {
      view = mountTestComponent();

      TestUtils.Simulate.click(view.refs.feedbackFormBtn.getDOMNode());

      sinon.assert.calledOnce(feedbackReceivedStub);
    });
  });
});
