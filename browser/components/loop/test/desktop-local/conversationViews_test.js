/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.conversationViews", function () {
  var sandbox, oldTitle, view;

  var CALL_STATES = loop.store.CALL_STATES;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    oldTitle = document.title;
    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });
  });

  afterEach(function() {
    document.title = oldTitle;
    view = undefined;
    sandbox.restore();
  });

  describe("ConversationDetailView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.ConversationDetailView(props));
    }

    it("should set the document title to the calledId", function() {
      mountTestComponent({calleeId: "mrsmith"});

      expect(document.title).eql("mrsmith");
    });

    it("should set display the calledId", function() {
      view = mountTestComponent({calleeId: "mrsmith"});

      expect(TestUtils.findRenderedDOMComponentWithTag(
        view, "h2").props.children).eql("mrsmith");
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
          calleeId: "mrsmith"
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("connecting");
    });

    it("should set display ringing string when the state is alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.ALERTING,
          calleeId: "mrsmith"
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("ringing");
    });
  });

  describe("OutgoingConversationView", function() {
    var store;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        loop.conversationViews.OutgoingConversationView({
          store: store
        }));
    }

    beforeEach(function() {
      store = new loop.store.ConversationStore({}, {
        dispatcher: new loop.Dispatcher(),
        client: {}
      });
    });

    it("should render the CallFailedView when the call state is 'terminated'",
      function() {
        store.set({callState: CALL_STATES.TERMINATED});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });

    it("should render the PendingConversationView when the call state is connecting",
      function() {
        store.set({callState: CALL_STATES.CONNECTING});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);
    });

    it("should update the rendered views when the state is changed.",
      function() {
        store.set({callState: CALL_STATES.CONNECTING});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);

        store.set({callState: CALL_STATES.TERMINATED});

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });
  });
});
