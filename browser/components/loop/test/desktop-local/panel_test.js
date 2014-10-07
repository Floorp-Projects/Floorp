/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.panel", function() {
  "use strict";

  var sandbox, notifications, fakeXHR, requests = [];

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };
    notifications = new loop.shared.models.NotificationCollection();

    navigator.mozLoop = {
      doNotDisturb: true,
      fxAEnabled: true,
      getStrings: function() {
        return JSON.stringify({textContent: "fakeText"});
      },
      get locale() {
        return "en-US";
      },
      setLoopCharPref: sandbox.stub(),
      getLoopCharPref: sandbox.stub().returns("unseen"),
      copyString: sandbox.stub(),
      noteCallUrlExpiry: sinon.spy(),
      composeEmail: sinon.spy(),
      contacts: {
        getAll: function(callback) {
          callback(null, []);
        },
        on: sandbox.stub()
      }
    };

    document.mozL10n.initialize(navigator.mozLoop);
  });

  afterEach(function() {
    delete navigator.mozLoop;
    sandbox.restore();
  });

  describe("#init", function() {
    beforeEach(function() {
      sandbox.stub(React, "renderComponent");
      sandbox.stub(document.mozL10n, "initialize");
      sandbox.stub(document.mozL10n, "get").returns("Fake title");
    });

    it("should initalize L10n", function() {
      loop.panel.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);
      sinon.assert.calledWithExactly(document.mozL10n.initialize,
        navigator.mozLoop);
    });

    it("should render the panel view", function() {
      loop.panel.init();

      sinon.assert.calledOnce(React.renderComponent);
      sinon.assert.calledWith(React.renderComponent,
        sinon.match(function(value) {
          return TestUtils.isDescriptorOfType(value,
            loop.panel.PanelView);
      }));
    });

    it("should dispatch an loopPanelInitialized", function(done) {
      function listener() {
        done();
      }

      window.addEventListener("loopPanelInitialized", listener);

      loop.panel.init();

      window.removeEventListener("loopPanelInitialized", listener);
    });
  });

  describe("loop.panel.AvailabilityDropdown", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(loop.panel.AvailabilityDropdown());
    });

    describe("doNotDisturb preference change", function() {
      beforeEach(function() {
        navigator.mozLoop.doNotDisturb = true;
      });

      it("should toggle the value of mozLoop.doNotDisturb", function() {
        var availableMenuOption = view.getDOMNode()
                                    .querySelector(".dnd-make-available");

        TestUtils.Simulate.click(availableMenuOption);

        expect(navigator.mozLoop.doNotDisturb).eql(false);
      });

      it("should toggle the dropdown menu", function() {
        var availableMenuOption = view.getDOMNode()
                                    .querySelector(".dnd-status span");

        TestUtils.Simulate.click(availableMenuOption);

        expect(view.state.showMenu).eql(true);
      });
    });
  });

  describe("loop.panel.PanelView", function() {
    var fakeClient, callUrlData, view, callTab, contactsTab;

    beforeEach(function() {
      callUrlData = {
        callUrl: "http://call.invalid/",
        expiresAt: 1000
      };

      fakeClient = {
        requestCallUrl: function(_, cb) {
          cb(null, callUrlData);
        }
      };

      view = TestUtils.renderIntoDocument(loop.panel.PanelView({
        notifications: notifications,
        client: fakeClient,
        showTabButtons: true,
      }));

      [callTab, contactsTab] =
        TestUtils.scryRenderedDOMComponentsWithClass(view, "tab");
    });

    describe('TabView', function() {
      it("should select contacts tab when clicking tab button", function() {
        TestUtils.Simulate.click(
          view.getDOMNode().querySelector('li[data-tab-name="contacts"]'));

        expect(contactsTab.getDOMNode().classList.contains("selected"))
          .to.be.true;
      });

      it("should select call tab when clicking tab button", function() {
        TestUtils.Simulate.click(
          view.getDOMNode().querySelector('li[data-tab-name="call"]'));

        expect(callTab.getDOMNode().classList.contains("selected"))
          .to.be.true;
      });
    });

    describe("AuthLink", function() {
      it("should trigger the FxA sign in/up process when clicking the link",
        function() {
          navigator.mozLoop.loggedInToFxA = false;
          navigator.mozLoop.logInToFxA = sandbox.stub();

          TestUtils.Simulate.click(
            view.getDOMNode().querySelector(".signin-link a"));

          sinon.assert.calledOnce(navigator.mozLoop.logInToFxA);
        });

      it("should be hidden if FxA is not enabled",
        function() {
          navigator.mozLoop.fxAEnabled = false;
          var view = TestUtils.renderIntoDocument(loop.panel.AuthLink());
          expect(view.getDOMNode()).to.be.null;
      });

      afterEach(function() {
        navigator.mozLoop.fxAEnabled = true;
      });
    });

    describe("SettingsDropdown", function() {
      var view;

      beforeEach(function() {
        navigator.mozLoop.logInToFxA = sandbox.stub();
        navigator.mozLoop.logOutFromFxA = sandbox.stub();
        navigator.mozLoop.openFxASettings = sandbox.stub();
      });

      afterEach(function() {
        navigator.mozLoop.fxAEnabled = true;
      });

      it("should be hidden if FxA is not enabled",
        function() {
          navigator.mozLoop.fxAEnabled = false;
          var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());
          expect(view.getDOMNode()).to.be.null;
      });

      it("should show a signin entry when user is not authenticated",
        function() {
          navigator.mozLoop.loggedInToFxA = false;

          var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

          expect(view.getDOMNode().querySelectorAll(".icon-signout"))
            .to.have.length.of(0);
          expect(view.getDOMNode().querySelectorAll(".icon-signin"))
            .to.have.length.of(1);
        });

      it("should show a signout entry when user is authenticated", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};

        var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

        expect(view.getDOMNode().querySelectorAll(".icon-signout"))
          .to.have.length.of(1);
        expect(view.getDOMNode().querySelectorAll(".icon-signin"))
          .to.have.length.of(0);
      });

      it("should show an account entry when user is authenticated", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};

        var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

        expect(view.getDOMNode().querySelectorAll(".icon-account"))
          .to.have.length.of(1);
      });

      it("should open the FxA settings when the account entry is clicked", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};

        var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".icon-account"));

        sinon.assert.calledOnce(navigator.mozLoop.openFxASettings);
      });

      it("should hide any account entry when user is not authenticated",
        function() {
          navigator.mozLoop.loggedInToFxA = false;

          var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

          expect(view.getDOMNode().querySelectorAll(".icon-account"))
            .to.have.length.of(0);
        });

      it("should sign in the user on click when unauthenticated", function() {
        navigator.mozLoop.loggedInToFxA = false;
        var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".icon-signin"));

        sinon.assert.calledOnce(navigator.mozLoop.logInToFxA);
      });

      it("should sign out the user on click when authenticated", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};
        var view = TestUtils.renderIntoDocument(loop.panel.SettingsDropdown());

        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".icon-signout"));

        sinon.assert.calledOnce(navigator.mozLoop.logOutFromFxA);
      });
    });

    describe("#render", function() {
      it("should render a ToSView", function() {
        TestUtils.findRenderedComponentWithType(view, loop.panel.ToSView);
      });
    });
  });

  describe("loop.panel.CallUrlResult", function() {
    var fakeClient, callUrlData, view;

    beforeEach(function() {
      callUrlData = {
        callUrl: "http://call.invalid/fakeToken",
        expiresAt: 1000
      };

      fakeClient = {
        requestCallUrl: function(_, cb) {
          cb(null, callUrlData);
        }
      };

      sandbox.stub(notifications, "reset");
      view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
        notifications: notifications,
        client: fakeClient
      }));
    });

    describe("Rendering the component should generate a call URL", function() {

      beforeEach(function() {
        document.mozL10n.initialize({
          getStrings: function(key) {
            var text;

            if (key === "share_email_subject3")
              text = "email-subject";
            else if (key === "share_email_body3")
              text = "{{callUrl}}";

            return JSON.stringify({textContent: text});
          }
        });
      });

      it("should make a request to requestCallUrl", function() {
        sandbox.stub(fakeClient, "requestCallUrl");
        var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
          notifications: notifications,
          client: fakeClient
        }));

        sinon.assert.calledOnce(view.props.client.requestCallUrl);
        sinon.assert.calledWithExactly(view.props.client.requestCallUrl,
                                       sinon.match.string, sinon.match.func);
      });

      it("should set the call url form in a pending state", function() {
        // Cancel requestCallUrl effect to keep the state pending
        fakeClient.requestCallUrl = sandbox.stub();
        var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
          notifications: notifications,
          client: fakeClient
        }));

        expect(view.state.pending).eql(true);
      });

      it("should update state with the call url received", function() {
        expect(view.state.pending).eql(false);
        expect(view.state.callUrl).eql(callUrlData.callUrl);
      });

      it("should clear the pending state when a response is received",
        function() {
          expect(view.state.pending).eql(false);
        });

      it("should update CallUrlResult with the call url", function() {
        var urlField = view.getDOMNode().querySelector("input[type='url']");

        expect(urlField.value).eql(callUrlData.callUrl);
      });

      it("should have 0 pending notifications", function() {
        expect(view.props.notifications.length).eql(0);
      });

      it("should display a share button for email", function() {
        fakeClient.requestCallUrl = sandbox.stub();
        var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
          notifications: notifications,
          client: fakeClient
        }));
        view.setState({pending: false, callUrl: "http://example.com"});

        TestUtils.findRenderedDOMComponentWithClass(view, "button-email");
        TestUtils.Simulate.click(view.getDOMNode().querySelector(".button-email"));
        sinon.assert.calledOnce(navigator.mozLoop.composeEmail);
      });

      it("should feature a copy button capable of copying the call url when clicked", function() {
        fakeClient.requestCallUrl = sandbox.stub();
        var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
          notifications: notifications,
          client: fakeClient
        }));
        view.setState({
          pending: false,
          copied: false,
          callUrl: "http://example.com",
          callUrlExpiry: 6000
        });

        TestUtils.Simulate.click(view.getDOMNode().querySelector(".button-copy"));

        sinon.assert.calledOnce(navigator.mozLoop.copyString);
        sinon.assert.calledWithExactly(navigator.mozLoop.copyString,
          view.state.callUrl);
      });

      it("should note the call url expiry when the url is copied via button",
        function() {
          var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
            notifications: notifications,
            client: fakeClient
          }));
          view.setState({
            pending: false,
            copied: false,
            callUrl: "http://example.com",
            callUrlExpiry: 6000
          });

          TestUtils.Simulate.click(view.getDOMNode().querySelector(".button-copy"));

          sinon.assert.calledOnce(navigator.mozLoop.noteCallUrlExpiry);
          sinon.assert.calledWithExactly(navigator.mozLoop.noteCallUrlExpiry,
            6000);
        });

      it("should note the call url expiry when the url is emailed",
        function() {
          var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
            notifications: notifications,
            client: fakeClient
          }));
          view.setState({
            pending: false,
            copied: false,
            callUrl: "http://example.com",
            callUrlExpiry: 6000
          });

          TestUtils.Simulate.click(view.getDOMNode().querySelector(".button-email"));

          sinon.assert.calledOnce(navigator.mozLoop.noteCallUrlExpiry);
          sinon.assert.calledWithExactly(navigator.mozLoop.noteCallUrlExpiry,
            6000);
        });

      it("should note the call url expiry when the url is copied manually",
        function() {
          var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
            notifications: notifications,
            client: fakeClient
          }));
          view.setState({
            pending: false,
            copied: false,
            callUrl: "http://example.com",
            callUrlExpiry: 6000
          });

          var urlField = view.getDOMNode().querySelector("input[type='url']");
          TestUtils.Simulate.copy(urlField);

          sinon.assert.calledOnce(navigator.mozLoop.noteCallUrlExpiry);
          sinon.assert.calledWithExactly(navigator.mozLoop.noteCallUrlExpiry,
            6000);
        });

      it("should notify the user when the operation failed", function() {
        fakeClient.requestCallUrl = function(_, cb) {
          cb("fake error");
        };
        sandbox.stub(notifications, "errorL10n");
        var view = TestUtils.renderIntoDocument(loop.panel.CallUrlResult({
          notifications: notifications,
          client: fakeClient
        }));

        sinon.assert.calledOnce(notifications.errorL10n);
        sinon.assert.calledWithExactly(notifications.errorL10n,
                                       "unable_retrieve_url");
      });
    });
  });

  describe('loop.panel.ToSView', function() {

    it("should render when the value of loop.seenToS is not set", function() {
      var view = TestUtils.renderIntoDocument(loop.panel.ToSView());

      TestUtils.findRenderedDOMComponentWithClass(view, "terms-service");
    });

    it("should not render when the value of loop.seenToS is set to 'seen'",
      function(done) {
        navigator.mozLoop.getLoopCharPref = function() {
          return "seen";
        };

        try {
          TestUtils.findRenderedDOMComponentWithClass(view, "tos");
        } catch (err) {
          done();
        }
    });
  });
});
