/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.conversationViews", function () {
  "use strict";

  var sharedUtils = loop.shared.utils;
  var sandbox, oldTitle, view, dispatcher, contact;

  var CALL_STATES = loop.store.CALL_STATES;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    oldTitle = document.title;
    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    contact = {
      name: [ "mrsmith" ],
      email: [{
        type: "home",
        value: "fakeEmail",
        pref: true
      }]
    };
  });

  afterEach(function() {
    document.title = oldTitle;
    view = undefined;
    sandbox.restore();
  });

  describe("CallIdentifierView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.CallIdentifierView(props));
    }

    it("should set display the peer identifer", function() {
      view = mountTestComponent({
        showIcons: false,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-identifier-text").props.children).eql("mrssmith");
    });

    it("should not display the icons if showIcons is false", function() {
      view = mountTestComponent({
        showIcons: false,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-detail").props.className).to.contain("hide");
    });

    it("should display the icons if showIcons is true", function() {
      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-detail").props.className).to.not.contain("hide");
    });

    it("should display the url timestamp", function() {
      sandbox.stub(loop.shared.utils, "formatDate").returns(("October 9, 2014"));

      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith",
        urlCreationDate: (new Date() / 1000).toString()
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-conversation-timestamp").props.children).eql("(October 9, 2014)");
    });

    it("should show video as muted if video is false", function() {
      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith",
        video: false
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-tiny-video-icon").props.className).to.contain("muted");
    });
  });

  describe("ConversationDetailView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.ConversationDetailView(props));
    }

    it("should set the document title to the calledId", function() {
      mountTestComponent({contact: contact});

      expect(document.title).eql("mrsmith");
    });

    it("should fallback to the email if the contact name is not defined",
      function() {
        delete contact.name;

        mountTestComponent({contact: contact});

        expect(document.title).eql("fakeEmail");
      });
  });

  describe("PendingConversationView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.PendingConversationView(props));
    }

    it("should set display connecting string when the state is not alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("connecting");
    });

    it("should set display ringing string when the state is alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.ALERTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("ringing");
    });

    it("should disable the cancel button if enableCancelButton is false",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher,
          enableCancelButton: false
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        expect(cancelBtn.classList.contains("disabled")).eql(true);
      });

    it("should enable the cancel button if enableCancelButton is false",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher,
          enableCancelButton: true
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        expect(cancelBtn.classList.contains("disabled")).eql(false);
      });

    it("should dispatch a cancelCall action when the cancel button is pressed",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        React.addons.TestUtils.Simulate.click(cancelBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "cancelCall"));
      });
  });

  describe("CallFailedView", function() {
    var store;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.CallFailedView({
          dispatcher: dispatcher,
          store: store,
          contact: {email: [{value: "test@test.tld"}]}
        }));
    }

    beforeEach(function() {
      store = new loop.store.ConversationStore({}, {
        dispatcher: dispatcher,
        client: {},
        sdkDriver: {}
      });
    });

    it("should dispatch a retryCall action when the retry button is pressed",
      function() {
        view = mountTestComponent();

        var retryBtn = view.getDOMNode().querySelector('.btn-retry');

        React.addons.TestUtils.Simulate.click(retryBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "retryCall"));
      });

    it("should dispatch a cancelCall action when the cancel button is pressed",
      function() {
        view = mountTestComponent();

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        React.addons.TestUtils.Simulate.click(cancelBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "cancelCall"));
      });

    it("should dispatch a fetchEmailLink action when the cancel button is pressed",
      function() {
        view = mountTestComponent();

        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');

        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "fetchEmailLink"));
      });

    it("should disable the email link button once the action is dispatched",
      function() {
        view = mountTestComponent();
        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');
        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        expect(view.getDOMNode().querySelector(".btn-email").disabled).eql(true);
      });

    it("should compose an email once the email link is received", function() {
      var composeCallUrlEmail = sandbox.stub(sharedUtils, "composeCallUrlEmail");
      view = mountTestComponent();
      store.set("emailLink", "http://fake.invalid/");

      sinon.assert.calledOnce(composeCallUrlEmail);
      sinon.assert.calledWithExactly(composeCallUrlEmail,
        "http://fake.invalid/", "test@test.tld");
    });

    it("should close the conversation window once the email link is received",
      function() {
        sandbox.stub(window, "close");
        view = mountTestComponent();

        store.set("emailLink", "http://fake.invalid/");

        sinon.assert.calledOnce(window.close);
      });

    it("should display an error message in case email link retrieval failed",
      function() {
        view = mountTestComponent();

        store.trigger("error:emailLink");

        expect(view.getDOMNode().querySelector(".error")).not.eql(null);
      });

    it("should allow retrying to get a call url if it failed previously",
      function() {
        view = mountTestComponent();

        store.trigger("error:emailLink");

        expect(view.getDOMNode().querySelector(".btn-email").disabled).eql(false);
      });
  });

  describe("OngoingConversationView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.OngoingConversationView(props));
    }

    it("should dispatch a setupStreamElements action when the view is created",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setupStreamElements"));
      });

    it("should dispatch a hangupCall action when the hangup button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher
        });

        var hangupBtn = view.getDOMNode().querySelector('.btn-hangup');

        React.addons.TestUtils.Simulate.click(hangupBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "hangupCall"));
      });

    it("should dispatch a setMute action when the audio mute button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          audio: {enabled: false}
        });

        var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", true));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "audio"));
      });

    it("should dispatch a setMute action when the video mute button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          video: {enabled: true}
        });

        var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", false));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "video"));
      });

    it("should set the mute button as mute off", function() {
      view = mountTestComponent({
        dispatcher: dispatcher,
        video: {enabled: true}
      });

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

      expect(muteBtn.classList.contains("muted")).eql(false);
    });

    it("should set the mute button as mute on", function() {
      view = mountTestComponent({
        dispatcher: dispatcher,
        audio: {enabled: false}
      });

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

      expect(muteBtn.classList.contains("muted")).eql(true);
    });
  });

  describe("OutgoingConversationView", function() {
    var store;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.OutgoingConversationView({
          dispatcher: dispatcher,
          store: store
        }));
    }

    beforeEach(function() {
      navigator.mozLoop = {
        getLoopCharPref: function() { return "fake"; },
        appVersionInfo: sinon.spy()
      };

      store = new loop.store.ConversationStore({}, {
        dispatcher: dispatcher,
        client: {},
        sdkDriver: {}
      });
    });

    afterEach(function() {
      delete navigator.mozLoop;
    });

    it("should render the CallFailedView when the call state is 'terminated'",
      function() {
        store.set({callState: CALL_STATES.TERMINATED});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });

    it("should render the PendingConversationView when the call state is 'init'",
      function() {
        store.set({
          callState: CALL_STATES.INIT,
          contact: contact
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);
    });

    it("should render the OngoingConversationView when the call state is 'ongoing'",
      function() {
        store.set({callState: CALL_STATES.ONGOING});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.OngoingConversationView);
    });

    it("should render the FeedbackView when the call state is 'finished'",
      function() {
        store.set({callState: CALL_STATES.FINISHED});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.shared.views.FeedbackView);
    });

    it("should update the rendered views when the state is changed.",
      function() {
        store.set({
          callState: CALL_STATES.INIT,
          contact: contact
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);

        store.set({callState: CALL_STATES.TERMINATED});

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });
  });
});
