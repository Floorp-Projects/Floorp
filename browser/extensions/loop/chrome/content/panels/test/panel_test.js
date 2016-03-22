/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.panel", function() {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;

  var sandbox, notifications, requestStubs;
  var fakeXHR, fakeWindow, fakeEvent;
  var requests = [];
  var roomData, roomData2, roomData3, roomData4, roomData5, roomData6;
  var roomList, roomName;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function(xhr) {
      requests.push(xhr);
    };

    fakeEvent = {
      preventDefault: sandbox.stub(),
      stopPropagation: sandbox.stub(),
      pageY: 42
    };

    fakeWindow = {
      close: sandbox.stub(),
      addEventListener: function() {},
      removeEventListener: function() {},
      document: { addEventListener: function() {} },
      setTimeout: function(callback) { callback(); }
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    notifications = new loop.shared.models.NotificationCollection();

    LoopMochaUtils.stubLoopRequest(requestStubs = {
      GetDoNotDisturb: function() { return true; },
      SetDoNotDisturb: sinon.stub(),
      GetErrors: function() { return null; },
      GetAllStrings: function() {
        return JSON.stringify({ textContent: "fakeText" });
      },
      GetAllConstants: function() { return {}; },
      GetLocale: function() {
        return "en-US";
      },
      GetPluralRule: function() {
        return 1;
      },
      SetLoopPref: sinon.stub(),
      GetLoopPref: function(prefName) {
        if (prefName === "debug.dispatcher") {
          return false;
        } else if (prefName === "facebook.enabled") {
          return true;
        }

        return 1;
      },
      SetPanelHeight: function() { return null; },
      GetPluralForm: function() {
        return "fakeText";
      },
      "Rooms:GetAll": function() {
        return [];
      },
      "Rooms:PushSubscription": sinon.stub(),
      Confirm: sinon.stub(),
      GetHasEncryptionKey: function() { return true; },
      HangupAllChatWindows: function() {},
      IsMultiProcessActive: sinon.stub(),
      LoginToFxA: sinon.stub(),
      LogoutFromFxA: sinon.stub(),
      NotifyUITour: sinon.stub(),
      OpenURL: sinon.stub(),
      GettingStartedURL: sinon.stub().returns("http://fakeFTUUrl.com"),
      OpenGettingStartedTour: sinon.stub(),
      GetSelectedTabMetadata: sinon.stub().returns({}),
      GetUserProfile: function() { return null; }
    });

    loop.storedRequests = {
      GetHasEncryptionKey: true,
      GetUserProfile: null,
      GetDoNotDisturb: false,
      "GetLoopPref|gettingStarted.latestFTUVersion": 2,
      "GetLoopPref|legal.ToS_url": "",
      "GetLoopPref|legal.privacy_url": "",
      "GetLoopPref|remote.autostart": false,
      IsMultiProcessActive: false
    };

    roomName = "First Room Name";
    // XXX Multiple rooms needed for some tests, could clean up this code to
    // utilize a function that generates a given number of rooms for use in
    // those tests
    roomData = {
      roomToken: "QzBbvGmIZWU",
      roomUrl: "http://sample/QzBbvGmIZWU",
      decryptedContext: {
        roomName: roomName,
        urls: [{
          location: "http://testurl.com"
        }]
      },
      maxSize: 2,
        participants: [{
        displayName: "Alexis",
          account: "alexis@example.com",
          roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
      }, {
        displayName: "Adam",
        roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
    }],
      ctime: 1405517418
    };

    roomData2 = {
      roomToken: "QzBbvlmIZWU",
      roomUrl: "http://sample/QzBbvlmIZWU",
      decryptedContext: {
        roomName: "Second Room Name"
      },
      maxSize: 2,
        participants: [{
        displayName: "Bill",
          account: "bill@example.com",
          roomConnectionId: "2a1737a6-4a73-43b5-ae3e-906ec1e763cb"
      }, {
          displayName: "Bob",
            roomConnectionId: "781f212b-f1ea-4ce1-9105-7cfc36fb4ec7"
        }],
      ctime: 1405517417
    };

    roomData3 = {
      roomToken: "QzBbvlmIZWV",
      roomUrl: "http://sample/QzBbvlmIZWV",
      decryptedContext: {
        roomName: "Second Room Name"
      },
      maxSize: 2,
      participants: [{
        displayName: "Bill",
        account: "bill@example.com",
        roomConnectionId: "2a1737a6-4a73-43b5-ae3e-906ec1e763cc"
      }, {
        displayName: "Bob",
        roomConnectionId: "781f212b-f1ea-4ce1-9105-7cfc36fb4ec7"
      }],
      ctime: 1405517417
    };

    roomData4 = {
      roomToken: "QzBbvlmIZWW",
      roomUrl: "http://sample/QzBbvlmIZWW",
      decryptedContext: {
        roomName: "Second Room Name"
      },
      maxSize: 2,
      participants: [{
        displayName: "Bill",
        account: "bill@example.com",
        roomConnectionId: "2a1737a6-4a73-43b5-ae3e-906ec1e763cc"
      }, {
        displayName: "Bob",
        roomConnectionId: "781f212b-f1ea-4ce1-9105-7cfc36fb4ec7"
      }],
      ctime: 1405517417
    };

    roomData5 = {
      roomToken: "QzBbvlmIZWX",
      roomUrl: "http://sample/QzBbvlmIZWX",
      decryptedContext: {
        roomName: "Second Room Name"
      },
      maxSize: 2,
      participants: [{
        displayName: "Bill",
        account: "bill@example.com",
        roomConnectionId: "2a1737a6-4a73-43b5-ae3e-906ec1e763cc"
      }, {
        displayName: "Bob",
        roomConnectionId: "781f212b-f1ea-4ce1-9105-7cfc36fb4ec7"
      }],
      ctime: 1405517417
    };

    roomData6 = {
      roomToken: "QzBbvlmIZWY",
      roomUrl: "http://sample/QzBbvlmIZWY",
      decryptedContext: {
        roomName: "Second Room Name"
      },
      maxSize: 2,
      participants: [{
        displayName: "Bill",
        account: "bill@example.com",
        roomConnectionId: "2a1737a6-4a73-43b5-ae3e-906ec1e763cc"
      }, {
        displayName: "Bob",
        roomConnectionId: "781f212b-f1ea-4ce1-9105-7cfc36fb4ec7"
      }],
      ctime: 1405517417
    };

    roomList = [new loop.store.Room(roomData), new loop.store.Room(roomData2)];

    document.mozL10n.initialize({
      getStrings: function() {
        return JSON.stringify({ textContent: "fakeText" });
      },
      locale: "en-US"
    });
    sandbox.stub(document.mozL10n, "get").returns("Fake title");
  });

  afterEach(function() {
    loop.shared.mixins.setRootObject(window);
    sandbox.restore();
    LoopMochaUtils.restore();
  });

  describe("#init", function() {
    beforeEach(function() {
      sandbox.stub(React, "render");
      sandbox.stub(document.mozL10n, "initialize");
    });

    it("should initalize L10n", function() {
      loop.panel.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);
      sinon.assert.calledWith(document.mozL10n.initialize, sinon.match({ locale: "en-US" }));
    });

    it("should render the panel view", function() {
      loop.panel.init();

      sinon.assert.calledOnce(React.render);
      sinon.assert.calledWith(React.render,
        sinon.match(function(value) {
          return TestUtils.isCompositeComponentElement(value,
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

  describe("loop.panel.PanelView", function() {
    var dispatcher, roomStore;

    beforeEach(function() {
      dispatcher = new loop.Dispatcher();
      roomStore = new loop.store.RoomStore(dispatcher, {
        constants: {}
      });
    });

    function createTestPanelView() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.PanelView, {
          notifications: notifications,
          dispatcher: dispatcher,
          roomStore: roomStore
        }));
    }

    it("should hide the account entry when FxA is not enabled", function() {
      LoopMochaUtils.stubLoopRequest({
        GetUserProfile: function() { return { email: "test@example.com" }; }
      });

      var view = TestUtils.renderIntoDocument(
        React.createElement(loop.panel.SettingsDropdown));

      expect(view.getDOMNode().querySelectorAll(".entry-settings-account"))
        .to.have.length.of(0);
    });

    describe("AccountLink", function() {
      it("should trigger the FxA sign in/up process when clicking the link",
        function() {
          var stub = sandbox.stub();
          LoopMochaUtils.stubLoopRequest({
            LoginToFxA: stub
          });

          var view = createTestPanelView();
          TestUtils.Simulate.click(view.getDOMNode().querySelector(".signin-link > a"));

          sinon.assert.calledOnce(stub);
        });

      it("should close the panel after clicking the link", function() {
        var view = createTestPanelView();

        TestUtils.Simulate.click(view.getDOMNode().querySelector(".signin-link > a"));

        sinon.assert.calledOnce(fakeWindow.close);
      });

      it("should NOT show the context menu on right click", function() {
        var prevent = sandbox.stub();
        var view = createTestPanelView();
        TestUtils.Simulate.contextMenu(
          view.getDOMNode(),
          { preventDefault: prevent }
        );
        sinon.assert.calledOnce(prevent);
      });

      it("should warn when user profile is different from {} or null",
         function() {
          var warnstub = sandbox.stub(console, "warn");

          TestUtils.renderIntoDocument(React.createElement(
            loop.panel.AccountLink, {
              userProfile: []
            }
          ));

          sinon.assert.calledOnce(warnstub);
          sinon.assert.calledWithMatch(warnstub, "Required prop `userProfile` "
            + "was not correctly specified in `AccountLink`.");
      });

      it("should not warn when user profile is an object",
         function() {
          var warnstub = sandbox.stub(console, "warn");

          TestUtils.renderIntoDocument(React.createElement(
            loop.panel.AccountLink, {
              userProfile: {}
            }
          ));

          sinon.assert.notCalled(warnstub);
      });
    });

    describe("SettingsDropdown", function() {
      function mountTestComponent() {
        return TestUtils.renderIntoDocument(
          React.createElement(loop.panel.SettingsDropdown));
      }

      var openFxASettingsStub;

      beforeEach(function() {
        openFxASettingsStub = sandbox.stub();

        LoopMochaUtils.stubLoopRequest({
          OpenFxASettings: openFxASettingsStub
        });
      });

      describe("UserLoggedOut", function() {
        beforeEach(function() {
          LoopMochaUtils.stubLoopRequest({
            GetUserProfile: function() { return null; }
          });
        });

        it("should show a signin entry when user is not authenticated",
           function() {
             var view = mountTestComponent();

             expect(view.getDOMNode().querySelectorAll(".entry-settings-signout"))
               .to.have.length.of(0);
             expect(view.getDOMNode().querySelectorAll(".entry-settings-signin"))
               .to.have.length.of(1);
           });

        it("should hide any account entry when user is not authenticated",
           function() {
             var view = mountTestComponent();

             expect(view.getDOMNode().querySelectorAll(".entry-settings-account"))
               .to.have.length.of(0);
           });

        it("should sign in the user on click when unauthenticated", function() {
          var view = mountTestComponent();

          TestUtils.Simulate.click(view.getDOMNode()
                                     .querySelector(".entry-settings-signin"));

          sinon.assert.calledOnce(requestStubs.LoginToFxA);
        });

        it("should close the menu on clicking sign in", function() {
          var view = mountTestComponent();

          TestUtils.Simulate.click(view.getDOMNode()
                                     .querySelector(".entry-settings-signin"));

          expect(view.state.showMenu).eql(false);
        });

        it("should close the panel on clicking sign in", function() {
          var view = mountTestComponent();

          TestUtils.Simulate.click(view.getDOMNode()
                                     .querySelector(".entry-settings-signin"));

          sinon.assert.calledOnce(fakeWindow.close);
        });
      });

      describe("UserLoggedIn", function() {
        var view;

        beforeEach(function() {
          loop.storedRequests.GetUserProfile = { email: "test@example.com" };
          view = mountTestComponent();
        });

        it("should show a signout entry when user is authenticated", function() {
          expect(view.getDOMNode().querySelectorAll(".entry-settings-signout"))
              .to.have.length.of(1);
          expect(view.getDOMNode().querySelectorAll(".entry-settings-signin"))
              .to.have.length.of(0);
        });

        it("should show an account entry when user is authenticated", function() {
          expect(view.getDOMNode().querySelectorAll(".entry-settings-account"))
              .to.have.length.of(1);
        });

        it("should open the FxA settings when the account entry is clicked",
            function() {
              TestUtils.Simulate.click(view.getDOMNode()
                                         .querySelector(".entry-settings-account"));

              sinon.assert.calledOnce(openFxASettingsStub);
            });

        it("should sign out the user on click when authenticated", function() {
          TestUtils.Simulate.click(view.getDOMNode()
                                     .querySelector(".entry-settings-signout"));

          sinon.assert.calledOnce(requestStubs.LogoutFromFxA);
        });

        it("should close the dropdown menu on clicking sign out", function() {
          LoopMochaUtils.stubLoopRequest({
            GetUserProfile: function() { return { email: "test@example.com" }; }
          });

          view.setState({ showMenu: true });

          TestUtils.Simulate.click(view.getDOMNode()
                                     .querySelector(".entry-settings-signout"));

          expect(view.state.showMenu).eql(false);
        });

        it("should not close the panel on clicking sign out", function() {
          TestUtils.Simulate.click(view.getDOMNode()
                                     .querySelector(".entry-settings-signout"));

          sinon.assert.notCalled(fakeWindow.close);
        });
      });

      describe("Toggle Notifications", function() {
        var view;

        beforeEach(function() {
          view = mountTestComponent();
        });

        it("should toggle mozLoop.doNotDisturb to false", function() {
          var stub = sinon.stub();
          LoopMochaUtils.stubLoopRequest({
            SetDoNotDisturb: stub
          });
          var toggleNotificationsMenuOption = view.getDOMNode()
                                                .querySelector(".entry-settings-notifications");

          TestUtils.Simulate.click(toggleNotificationsMenuOption);

          sinon.assert.calledOnce(stub);
          sinon.assert.calledWithExactly(stub, false);
        });

        it("should toggle mozLoop.doNotDisturb to true", function() {
          var stub = sinon.stub();
          LoopMochaUtils.stubLoopRequest({
            GetDoNotDisturb: function() { return false; },
            SetDoNotDisturb: stub
          });
          var toggleNotificationsMenuOption = view.getDOMNode()
                                                .querySelector(".entry-settings-notifications");

          TestUtils.Simulate.click(toggleNotificationsMenuOption);

          sinon.assert.calledOnce(stub);
          sinon.assert.calledWithExactly(stub, true);
        });

        it("should close dropdown menu", function() {
          var toggleNotificationsMenuOption = view.getDOMNode()
                                                .querySelector(".entry-settings-notifications");

          view.setState({ showMenu: true });

          TestUtils.Simulate.click(toggleNotificationsMenuOption);

          expect(view.state.showMenu).eql(false);
        });
      });
    });

    describe("Help", function() {
      var view, supportUrl;

      function mountTestComponent() {
        return TestUtils.renderIntoDocument(
          React.createElement(loop.panel.SettingsDropdown));
      }

      beforeEach(function() {
        supportUrl = "https://example.com";
        LoopMochaUtils.stubLoopRequest({
          GetLoopPref: function(pref) {
            if (pref === "support_url") {
              return supportUrl;
            }

            return 1;
          }
        });
      });

      it("should open a tab to the support page", function() {
        view = mountTestComponent();

        TestUtils.Simulate
          .click(view.getDOMNode().querySelector(".entry-settings-help"));

        sinon.assert.calledOnce(requestStubs.OpenURL);
        sinon.assert.calledWithExactly(requestStubs.OpenURL, supportUrl);
      });

      it("should close the panel", function() {
        view = mountTestComponent();

        TestUtils.Simulate
          .click(view.getDOMNode().querySelector(".entry-settings-help"));

        sinon.assert.calledOnce(fakeWindow.close);
      });
    });

    describe("Submit feedback", function() {
      var view, feedbackUrl;

      function mountTestComponent() {
        return TestUtils.renderIntoDocument(
          React.createElement(loop.panel.SettingsDropdown));
      }

      beforeEach(function() {
        feedbackUrl = "https://example.com";
        LoopMochaUtils.stubLoopRequest({
          GetLoopPref: function(pref) {
            if (pref === "feedback.manualFormURL") {
              return feedbackUrl;
            }

            return 1;
          }
        });
      });

      it("should open a tab to the feedback page", function() {
        view = mountTestComponent();

        TestUtils.Simulate
          .click(view.getDOMNode().querySelector(".entry-settings-feedback"));

        sinon.assert.calledOnce(requestStubs.OpenURL);
        sinon.assert.calledWithExactly(requestStubs.OpenURL, feedbackUrl);
      });

      it("should close the panel", function() {
        view = mountTestComponent();

        TestUtils.Simulate
          .click(view.getDOMNode().querySelector(".entry-settings-feedback"));

        sinon.assert.calledOnce(fakeWindow.close);
      });
    });

    describe("#render", function() {
      it("should not render a ToSView when gettingStarted.latestFTUVersion is equal to or greater than FTU_VERSION", function() {
        loop.storedRequests["GetLoopPref|gettingStarted.latestFTUVersion"] = 2;
        var view = createTestPanelView();

        expect(function() {
          TestUtils.findRenderedComponentWithType(view, loop.panel.ToSView);
        }).to.Throw(/not find/);
      });

      it("should render a ToSView when gettingStarted.latestFTUVersion is less than FTU_VERSION", function() {
        loop.storedRequests["GetLoopPref|gettingStarted.latestFTUVersion"] = 0;
        var view = createTestPanelView();

        expect(function() {
          TestUtils.findRenderedComponentWithType(view, loop.panel.ToSView);
        }).to.not.Throw();
      });


      it("should render a GettingStarted view when gettingStarted.latestFTUVersion is less than FTU_VERSION", function() {
        loop.storedRequests["GetLoopPref|gettingStarted.latestFTUVersion"] = 0;
        var view = createTestPanelView();
        expect(function() {
          TestUtils.findRenderedComponentWithType(view, loop.panel.GettingStartedView);
        }).to.not.Throw();
      });

      it("should not render a GettingStartedView when the view has been seen", function() {
        var view = createTestPanelView();

        try {
          TestUtils.findRenderedComponentWithType(view, loop.panel.GettingStartedView);
          sinon.assert.fail("Should not find the GettingStartedView if it has been seen");
        } catch (ex) {
          // Do nothing
        }
      });

      it("should render a SignInRequestView when mozLoop.hasEncryptionKey is false", function() {
        loop.storedRequests.GetHasEncryptionKey = false;

        var view = createTestPanelView();

        expect(function() {
          TestUtils.findRenderedComponentWithType(view, loop.panel.SignInRequestView);
        }).to.not.Throw();
      });

      it("should not render a SignInRequestView when mozLoop.hasEncryptionKey is true", function() {
        var view = createTestPanelView();

        try {
          TestUtils.findRenderedComponentWithType(view, loop.panel.SignInRequestView);
          sinon.assert.fail("Should not find the GettingStartedView if it has been seen");
        } catch (ex) {
          // Do nothing
        }
      });

      it("should render a E10sNotSupported when multiprocess is enabled and active", function() {
        loop.storedRequests.IsMultiProcessActive = true;
        loop.storedRequests["GetLoopPref|remote.autostart"] = false;

        var view = createTestPanelView();

        expect(function() {
          TestUtils.findRenderedComponentWithType(view, loop.panel.E10sNotSupported);
        }).to.not.Throw();
      });

    });

    describe("GettingStartedView", function() {
      it("should render the Slidehow when clicked on the button", function() {
        loop.storedRequests["GetLoopPref|gettingStarted.latestFTUVersion"] = 0;

        var view = createTestPanelView();

        TestUtils.Simulate.click(view.getDOMNode().querySelector(".fte-get-started-button"));

        sinon.assert.calledOnce(requestStubs.OpenGettingStartedTour);
      });
    });
  });

  describe("loop.panel.RoomEntry", function() {
    var dispatcher;

    beforeEach(function() {
      dispatcher = new loop.Dispatcher();
    });

    function mountRoomEntry(props) {
      props = _.extend({
        dispatcher: dispatcher
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.RoomEntry, props));
    }

    describe("handleClick", function() {
      var view;

      beforeEach(function() {
        // Stub to prevent warnings due to stores not being set up to handle
        // the actions we are triggering.
        sandbox.stub(dispatcher, "dispatch");

        view = mountRoomEntry({
          isOpenedRoom: false,
          room: new loop.store.Room(roomData)
        });
      });

      // XXX Current version of React cannot use TestUtils.Simulate, please
      // enable when we upgrade.
      it.skip("should close the menu when you move out the cursor", function() {
        expect(view.refs.contextActions.state.showMenu).to.eql(false);
      });

      it("should set eventPosY when handleClick is called", function() {
        view.handleClick(fakeEvent);

        expect(view.state.eventPosY).to.eql(fakeEvent.pageY);
      });

      it("toggle state.showMenu when handleClick is called", function() {
        var prevState = view.state.showMenu;
        view.handleClick(fakeEvent);

        expect(view.state.showMenu).to.eql(!prevState);
      });

      it("should toggle the menu when the button is clicked", function() {
        var prevState = view.state.showMenu;
        var node = view.refs.contextActions.refs["menu-button"].getDOMNode();

        TestUtils.Simulate.click(node, fakeEvent);

        expect(view.state.showMenu).to.eql(!prevState);
      });
    });

    describe("Copy button", function() {
      var roomEntry, openURLStub;

      beforeEach(function() {
        // Stub to prevent warnings where no stores are set up to handle the
        // actions we are testing.
        sandbox.stub(dispatcher, "dispatch");
        openURLStub = sinon.stub();

        LoopMochaUtils.stubLoopRequest({
          GetSelectedTabMetadata: function() {
            return {
              url: "http://invalid.com",
              description: "fakeSite"
            };
          },
          OpenURL: openURLStub
        });

        roomEntry = mountRoomEntry({
          deleteRoom: sandbox.stub(),
          isOpenedRoom: false,
          room: new loop.store.Room(roomData)
        });
      });

      it("should render context actions button", function() {
        expect(roomEntry.refs.contextActions).to.not.eql(null);
      });

      describe("OpenRoom", function() {
        it("should dispatch an OpenRoom action when button is clicked", function() {
          TestUtils.Simulate.click(roomEntry.refs.roomEntry.getDOMNode());

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.OpenRoom({ roomToken: roomData.roomToken }));
        });

        it("should dispatch an OpenRoom action when callback is called", function() {
          roomEntry.handleClickEntry(fakeEvent);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.OpenRoom({ roomToken: roomData.roomToken }));
        });

        it("should call window.close", function() {
          roomEntry.handleClickEntry(fakeEvent);

          sinon.assert.calledOnce(fakeWindow.close);
        });

        it("should not dispatch an OpenRoom action when button is clicked if room is already opened", function() {
          roomEntry = mountRoomEntry({
            deleteRoom: sandbox.stub(),
            isOpenedRoom: true,
            room: new loop.store.Room(roomData)
          });

          TestUtils.Simulate.click(roomEntry.refs.roomEntry.getDOMNode());

          sinon.assert.notCalled(dispatcher.dispatch);
        });

        it("should open a new tab with the room context if it is not the same as the currently open tab", function() {
          TestUtils.Simulate.click(roomEntry.refs.roomEntry.getDOMNode());
          sinon.assert.calledOnce(openURLStub);
          sinon.assert.calledWithExactly(openURLStub, "http://testurl.com");
        });

        it("should open a new tab with the FTU Getting Started URL if the room context is blank", function() {
          var roomDataNoURL = {
            roomToken: "QzBbvGmIZWU",
            roomUrl: "http://sample/QzBbvGmIZWU",
            decryptedContext: {
              roomName: roomName,
              urls: [{
                location: ""
              }]
            },
            maxSize: 2,
            participants: [{
              displayName: "Alexis",
              account: "alexis@example.com",
              roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
            }, {
              displayName: "Adam",
              roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
            }],
            ctime: 1405517418
          };
          roomEntry = mountRoomEntry({
            deleteRoom: sandbox.stub(),
            isOpenedRoom: false,
            room: new loop.store.Room(roomDataNoURL)
          });
          var ftuURL = requestStubs.GettingStartedURL() + "?noopenpanel=1";

          TestUtils.Simulate.click(roomEntry.refs.roomEntry.getDOMNode());

          sinon.assert.calledOnce(openURLStub);
          sinon.assert.calledWithExactly(openURLStub, ftuURL);
        });

        it("should not open a new tab if the context is the same as the currently open tab", function() {
          LoopMochaUtils.stubLoopRequest({
            GetSelectedTabMetadata: function() {
              return {
                url: "http://testurl.com",
                description: "fakeSite"
              };
            }
          });

          roomEntry = mountRoomEntry({
            deleteRoom: sandbox.stub(),
            isOpenedRoom: false,
            room: new loop.store.Room(roomData)
          });

          TestUtils.Simulate.click(roomEntry.refs.roomEntry.getDOMNode());
          sinon.assert.notCalled(openURLStub);
        });
      });
    });

    describe("Context Indicator", function() {
      var roomEntry;

      function mountEntryForContext() {
        return mountRoomEntry({
          isOpenedRoom: false,
          room: new loop.store.Room(roomData)
        });
      }

      it("should display a default context indicator if the room doesn't have any", function() {
        roomEntry = mountEntryForContext();

        expect(roomEntry.getDOMNode().querySelector(".room-entry-context-item")).not.eql(null);
      });

      it("should a context indicator if the room specifies context", function() {
        roomData.decryptedContext.urls = [{
          description: "invalid entry",
          location: "http://invalid",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }];

        roomEntry = mountEntryForContext();

        expect(roomEntry.getDOMNode().querySelector(".room-entry-context-item")).not.eql(null);
      });

      it("should call mozLoop.openURL to open a new url", function() {
        roomData.decryptedContext.urls = [{
          description: "invalid entry",
          location: "http://invalid/",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }];

        roomEntry = mountEntryForContext();

        TestUtils.Simulate.click(roomEntry.getDOMNode().querySelector("a"));

        sinon.assert.calledOnce(requestStubs.OpenURL);
        sinon.assert.calledWithExactly(requestStubs.OpenURL, "http://invalid/");
      });

      it("should call close the panel after opening a url", function() {
        roomData.decryptedContext.urls = [{
          description: "invalid entry",
          location: "http://invalid/",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }];

        roomEntry = mountEntryForContext();

        TestUtils.Simulate.click(roomEntry.getDOMNode().querySelector("a"));

        sinon.assert.calledOnce(fakeWindow.close);
      });
    });

    describe("Room name updated", function() {
      var roomEntry;

      beforeEach(function() {
        roomEntry = mountRoomEntry({
          dispatcher: dispatcher,
          isOpenedRoom: false,
          room: new loop.store.Room(roomData)
        });
      });

      it("should update room name", function() {
        var updatedRoom = new loop.store.Room(_.extend({}, roomData, {
          decryptedContext: {
            roomName: "New room name"
          },
          ctime: new Date().getTime()
        }));

        roomEntry.setProps({ room: updatedRoom });

        expect(
          roomEntry.getDOMNode().textContent)
        .eql("New room name");
      });

      it("should enter in edit mode when edit button is clicked", function() {
        roomEntry.handleEditButtonClick(fakeEvent);

        expect(roomEntry.state.editMode).eql(true);
      });

      it("should render an input while edit mode is active", function() {
        roomEntry.setState({
          editMode: true
        });

        expect(roomEntry.getDOMNode().querySelector("input")).not.eql(null);
      });

      it("should exit edit mode and update the room name when input lose focus", function() {
        roomEntry.setState({
          editMode: true
        });

        sandbox.stub(dispatcher, "dispatch");

        var input = roomEntry.getDOMNode().querySelector("input");
        input.value = "fakeName";
        TestUtils.Simulate.change(input);
        TestUtils.Simulate.blur(input);

        expect(roomEntry.state.editMode).eql(false);
        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.UpdateRoomContext({
          roomToken: roomData.roomToken,
          newRoomName: "fakeName"
        }));
      });

      it("should exit edit mode and update the room name when Enter key is pressed", function() {
        roomEntry.setState({
          editMode: true
        });

        sandbox.stub(dispatcher, "dispatch");

        var input = roomEntry.getDOMNode().querySelector("input");
        input.value = "fakeName";
        TestUtils.Simulate.change(input);
        TestUtils.Simulate.keyDown(input, {
          key: "Enter",
          which: 13
        });

        expect(roomEntry.state.editMode).eql(false);
        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.UpdateRoomContext({
          roomToken: roomData.roomToken,
          newRoomName: "fakeName"
        }));
      });
    });

    describe("Room name priority", function() {
      var roomEntry;
      beforeEach(function() {
        roomEntry = mountRoomEntry({
          dispatcher: dispatcher,
          isOpenedRoom: false,
          room: new loop.store.Room(roomData)
        });
      });

      function setDecryptedContext(newDecryptedContext) {
        return new loop.store.Room(_.extend({}, roomData, {
          decryptedContext: newDecryptedContext,
          ctime: new Date().getTime()
        }));
      }

      it("should use room name by default", function() {
        var updatedRoom = setDecryptedContext({
          roomName: "Room name",
          urls: [
            {
              description: "Website title",
              location: "https://fakeurl.com"
            }
          ]
        });

        roomEntry.setProps({ room: updatedRoom });

        expect(roomEntry.getDOMNode().textContent).eql("Room name");
      });

      it("should use context title when there's no room title", function() {
        var updatedRoom = setDecryptedContext({
          urls: [
            {
              description: "Website title",
              location: "https://fakeurl.com"
            }
          ]
        });

        roomEntry.setProps({ room: updatedRoom });

        expect(roomEntry.getDOMNode().textContent).eql("Website title");
      });

      it("should use website url when there's no room title nor website", function() {
        var updatedRoom = setDecryptedContext({
          urls: [
            {
              location: "https://fakeurl.com"
            }
          ]
        });

        roomEntry.setProps({ room: updatedRoom });

        expect(roomEntry.getDOMNode().textContent).eql("https://fakeurl.com");
      });
    });
  });

  describe("loop.panel.RoomList", function() {
    var roomStore, dispatcher, fakeEmail, dispatch;

    beforeEach(function() {
      fakeEmail = "fakeEmail@example.com";
      dispatcher = new loop.Dispatcher();
      roomStore = new loop.store.RoomStore(dispatcher, {
        constants: {}
      });
      roomStore.setStoreState({
        openedRoom: null,
        pendingCreation: false,
        pendingInitialRetrieval: false,
        rooms: [],
        error: undefined
      });

      dispatch = sandbox.stub(dispatcher, "dispatch");
    });

    function createTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.RoomList, {
          store: roomStore,
          dispatcher: dispatcher,
          userDisplayName: fakeEmail,
          userProfile: null
        }));
    }

    it("should dispatch a GetAllRooms action on mount", function() {
      createTestComponent();

      sinon.assert.calledOnce(dispatch);
      sinon.assert.calledWithExactly(dispatch, new sharedActions.GetAllRooms());
    });

    it("should not close the panel once a room is created and there is no error", function() {
      createTestComponent();

      roomStore.setStoreState({ pendingCreation: true });

      sinon.assert.notCalled(fakeWindow.close);

      roomStore.setStoreState({ pendingCreation: false });

      sinon.assert.notCalled(fakeWindow.close);
    });

    it("should have FTE element and not room-list element when room-list is empty", function() {
      var view = createTestComponent();
      var node = view.getDOMNode();
      expect(node.querySelectorAll(".fte-get-started-content").length).to.eql(1);
      expect(node.querySelectorAll(".room-list").length).to.eql(0);
    });

    it("should display a loading animation when rooms are pending", function() {
      var view = createTestComponent();
      roomStore.setStoreState({ pendingInitialRetrieval: true });

      expect(view.getDOMNode().querySelectorAll(".room-list-loading").length).to.eql(1);
    });

    it("should show multiple rooms in list with no opened room", function() {
      roomStore.setStoreState({ rooms: roomList });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelectorAll(".room-opened").length).to.eql(0);
      expect(node.querySelectorAll(".room-entry").length).to.eql(2);
    });

    it("should show gradient when more than 5 rooms in list", function() {
      var sixRoomList = [
        new loop.store.Room(roomData),
        new loop.store.Room(roomData2),
        new loop.store.Room(roomData3),
        new loop.store.Room(roomData4),
        new loop.store.Room(roomData5),
        new loop.store.Room(roomData6)
      ];
      roomStore.setStoreState({ rooms: sixRoomList });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelectorAll(".room-entry").length).to.eql(6);
      expect(node.querySelectorAll(".room-list-blur").length).to.eql(1);
    });

    it("should not show gradient when 5 or less rooms in list", function() {
      var fiveRoomList = [
        new loop.store.Room(roomData),
        new loop.store.Room(roomData2),
        new loop.store.Room(roomData3),
        new loop.store.Room(roomData4),
        new loop.store.Room(roomData5)
      ];
      roomStore.setStoreState({ rooms: fiveRoomList });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelectorAll(".room-entry").length).to.eql(5);
      expect(node.querySelectorAll(".room-list-blur").length).to.eql(0);
    });

    it("should not show gradient when no rooms in list", function() {
      var zeroRoomList = [];
      roomStore.setStoreState({ rooms: zeroRoomList });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelectorAll(".room-entry").length).to.eql(0);
      expect(node.querySelectorAll(".room-list-blur").length).to.eql(0);
    });

    it("should only show the opened room you're in when you're in a room", function() {
      roomStore.setStoreState({ rooms: roomList, openedRoom: roomList[0].roomToken });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelectorAll(".room-opened").length).to.eql(1);
      expect(node.querySelectorAll(".room-entry").length).to.eql(1);
      expect(node.querySelectorAll(".room-opened h2")[0].textContent).to.equal(roomName);
    });

    it("should show Page Title as Room Name if a Room Name is not given", function() {
      var urlsRoomData = {
        roomToken: "QzBbvGmIZWU",
        roomUrl: "http://sample/QzBbvGmIZWU",
        decryptedContext: {
          urls: [{
            description: "Page Title",
            location: "http://example.com"
          }]
        },
        maxSize: 2,
        participants: [{
          displayName: "Alexis",
          account: "alexis@example.com",
          roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
        }, {
          displayName: "Adam",
          roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
        }],
        ctime: 1405517418
      };
      roomStore.setStoreState({ rooms: [new loop.store.Room(urlsRoomData)] });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelector(".room-entry h2").textContent).to.equal("Page Title");
    });

    it("should show Page URL as Room Name if a Room Name and Page Title are not available", function() {
      var urlsRoomData = {
        roomToken: "QzBbvGmIZWU",
        roomUrl: "http://sample/QzBbvGmIZWU",
        decryptedContext: {
          urls: [{
            description: "",
            location: "http://example.com"
          }]
        },
        maxSize: 2,
        participants: [{
          displayName: "Alexis",
          account: "alexis@example.com",
          roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
        }, {
          displayName: "Adam",
          roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
        }],
        ctime: 1405517418
      };
      roomStore.setStoreState({ rooms: [new loop.store.Room(urlsRoomData)] });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelector(".room-entry h2").textContent).to.equal("http://example.com");
    });

    it("should show Fallback Title as Room Name if a Room Name,Page Title and Page Url are not available", function() {
      var urlsRoomData = {
        roomToken: "QzBbvGmIZWU",
        roomUrl: "http://sample/QzBbvGmIZWU",
        decryptedContext: {
          urls: [{
            description: "",
            location: ""
          }]
        },
        maxSize: 2,
        participants: [{
          displayName: "Alexis",
          account: "alexis@example.com",
          roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
        }, {
          displayName: "Adam",
          roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
        }],
        ctime: 1405517418
      };
      roomStore.setStoreState({ rooms: [new loop.store.Room(urlsRoomData)] });

      var view = createTestComponent();

      var node = view.getDOMNode();
      expect(node.querySelector(".room-entry h2").textContent).to.equal("Fake title");
    });

    describe("computeAdjustedTopPosition", function() {
      it("should return 0 if clickYPos, menuNodeHeight, listTop, listHeight and clickOffset cause it to be less than 0",
        function() {
          var topPosTest = loop.panel.computeAdjustedTopPosition(119, 124, 0, 152, 10) < 0;

          expect(topPosTest).to.equal(false);
        });

    });
  });

  describe("loop.panel.NewRoomView", function() {
    var roomStore, dispatcher, fakeEmail, dispatch;

    beforeEach(function() {
      fakeEmail = "fakeEmail@example.com";
      dispatcher = new loop.Dispatcher();
      roomStore = new loop.store.RoomStore(dispatcher, {
        constants: {}
      });
      roomStore.setStoreState({
        pendingCreation: false,
        pendingInitialRetrieval: false,
        rooms: [],
        error: undefined
      });
      dispatch = sandbox.stub(dispatcher, "dispatch");
    });

    function createTestComponent(extraProps) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.NewRoomView, _.extend({
          dispatcher: dispatcher,
          userDisplayName: fakeEmail
        }, extraProps)));
    }

    it("should dispatch a CreateRoom action with context when clicking on the " +
       "Start a conversation button", function() {
      var favicon = "data:image/x-icon;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";
      LoopMochaUtils.stubLoopRequest({
        GetUserProfile: function() { return { email: fakeEmail }; },
        GetSelectedTabMetadata: function() {
          return {
            url: "http://invalid.com",
            description: "fakeSite",
            favicon: favicon,
            previews: ["fakeimage.png"]
          };
        }
      });

      var view = createTestComponent({
        inRoom: false,
        pendingOperation: false
      });

      // Simulate being visible
      view.onDocumentVisible();

      var node = view.getDOMNode();

      TestUtils.Simulate.click(node.querySelector(".new-room-button"));

      sinon.assert.calledWith(dispatch, new sharedActions.CreateRoom({
        urls: [{
          location: "http://invalid.com",
          description: "fakeSite",
          thumbnail: favicon
        }]
      }));
    });

    it("should disable the create button when pendingOperation is true",
      function() {
        var view = createTestComponent({
          inRoom: false,
          pendingOperation: true
        });

        var buttonNode = view.getDOMNode().querySelector(".new-room-button[disabled]");
        expect(buttonNode).to.not.equal(null);
      });

    it("should not have a create button when inRoom is true", function() {
      var view = createTestComponent({
        inRoom: true,
        pendingOperation: false
      });

      var buttonNode = view.getDOMNode().querySelector(".new-room-button");
      expect(buttonNode).to.equal(null);
    });

    it("should have a stop sharing button when inRoom is true", function() {
      var view = createTestComponent({
        inRoom: true,
        pendingOperation: false
      });

      var buttonNode = view.getDOMNode().querySelector(".stop-sharing-button");
      expect(buttonNode).to.not.equal(null);
    });

    it("should hangup any window when stop sharing is clicked", function() {
      var stub = sinon.stub();
      LoopMochaUtils.stubLoopRequest({
        HangupAllChatWindows: stub
      });

      var view = createTestComponent({
        inRoom: true,
        pendingOperation: false
      });

      var node = view.getDOMNode();
      TestUtils.Simulate.click(node.querySelector(".stop-sharing-button"));

      sinon.assert.calledOnce(stub);
    });
  });

  describe("loop.panel.SignInRequestView", function() {
    var view;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.SignInRequestView));
    }

    it("should call login with forced re-authentication when sign-in is clicked", function() {
      view = mountTestComponent();

      TestUtils.Simulate.click(view.getDOMNode().querySelector("button"));

      sinon.assert.calledOnce(requestStubs.LoginToFxA);
      sinon.assert.calledWithExactly(requestStubs.LoginToFxA, true);
    });

    it("should logout when use as guest is clicked", function() {
      view = mountTestComponent();

      TestUtils.Simulate.click(view.getDOMNode().querySelector("a"));

      sinon.assert.calledOnce(requestStubs.LogoutFromFxA);
    });
  });

  describe("ConversationDropdown", function() {
    var view;

    function createTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.ConversationDropdown, {
          handleCopyButtonClick: sandbox.stub(),
          handleDeleteButtonClick: sandbox.stub(),
          handleEditButtonClick: sandbox.stub(),
          handleEmailButtonClick: sandbox.stub(),
          eventPosY: 0
        }));
    }

    beforeEach(function() {
      view = createTestComponent();
    });

    it("should trigger handleCopyButtonClick when copy button is clicked",
       function() {
         TestUtils.Simulate.click(view.refs.copyButton.getDOMNode());

         sinon.assert.calledOnce(view.props.handleCopyButtonClick);
       });

    it("should trigger handleEmailButtonClick when email button is clicked",
       function() {
         TestUtils.Simulate.click(view.refs.emailButton.getDOMNode());

         sinon.assert.calledOnce(view.props.handleEmailButtonClick);
       });

    it("should trigger handleDeleteButtonClick when delete button is clicked",
       function() {
         TestUtils.Simulate.click(view.refs.deleteButton.getDOMNode());

         sinon.assert.calledOnce(view.props.handleDeleteButtonClick);
       });

    it("should trigger handleEditButtonClick when edit button is clicked",
       function() {
         TestUtils.Simulate.click(view.refs.editButton.getDOMNode());

         sinon.assert.calledOnce(view.props.handleEditButtonClick);
       });
  });

  describe("RoomEntryContextButtons", function() {
    var view, dispatcher;

    function createTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher,
        eventPosY: 0,
        showMenu: false,
        room: roomData,
        toggleDropdownMenu: sandbox.stub(),
        handleClick: sandbox.stub(),
        handleEditButtonClick: sandbox.stub()
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.RoomEntryContextButtons, props));
    }

    beforeEach(function() {
      dispatcher = new loop.Dispatcher();
      sandbox.stub(dispatcher, "dispatch");

      view = createTestComponent();
    });

    it("should render ConversationDropdown if state.showMenu=true", function() {
      view = createTestComponent({ showMenu: true });

      expect(view.refs.menu).to.not.eql(undefined);
    });

    it("should not render ConversationDropdown by default", function() {
      view = createTestComponent({ showMenu: false });

      expect(view.refs.menu).to.eql(undefined);
    });

    it("should call toggleDropdownMenu after link is emailed", function() {
      view.handleEmailButtonClick(fakeEvent);

      sinon.assert.calledOnce(view.props.toggleDropdownMenu);
    });

    it("should call toggleDropdownMenu after conversation deleted", function() {
      view.handleDeleteButtonClick(fakeEvent);

      sinon.assert.calledOnce(view.props.toggleDropdownMenu);
    });

    it("should call toggleDropdownMenu after link is copied", function() {
      view.handleCopyButtonClick(fakeEvent);

      sinon.assert.calledOnce(view.props.toggleDropdownMenu);
    });

    it("should copy the URL when the callback is called", function() {
      view.handleCopyButtonClick(fakeEvent);

      sinon.assert.called(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.CopyRoomUrl({
        roomUrl: roomData.roomUrl,
        from: "panel"
      }));
    });

    it("should dispatch a delete action when callback is called", function() {
      view.handleDeleteButtonClick(fakeEvent);

      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.DeleteRoom({ roomToken: roomData.roomToken }));
    });
  });

  describe("SharePanelView", function() {
    var view, dispatcher, roomStore;

    function createTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher,
        onSharePanelDisplayChange: sinon.stub(),
        store: roomStore
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.SharePanelView, props));
    }

    beforeEach(function() {
      dispatcher = new loop.Dispatcher();
      sandbox.stub(dispatcher, "dispatch");
      roomStore = new loop.store.RoomStore(dispatcher, {
        constants: {}
      });

      roomStore.setStoreState({
        activeRoom: {
          roomToken: "fakeToken"
        },
        openedRoom: null,
        pendingCreation: false,
        pendingInitialRetrieval: false,
        rooms: [],
        error: undefined
      });

      view = createTestComponent();
      sandbox.stub(view, "closeWindow");
    });

    it("should not open the panel if there is no room pending of creation", function() {
      expect(view.getDOMNode()).eql(null);
    });

    it("should open the panel after room creation", function() {
      var clock = sinon.useFakeTimers();
      // Simulate that the user has click the browse button
      roomStore.setStoreState({
        pendingCreation: true
      });

      // Room has been created succesfully
      roomStore.setStoreState({
        pendingCreation: false
      });

      var panel = view.getDOMNode();
      clock.tick(loop.panel.SharePanelView.SHOW_PANEL_DELAY);

      expect(view.state.showPanel).eql(true);
      expect(panel.classList.contains("share-panel-open")).eql(true);
    });

    it("should close the share panel when clicking the overlay", function() {
      view.setState({
        showPanel: true
      });

      var overlay = view.getDOMNode().querySelector(".share-panel-overlay");
      var panel = view.getDOMNode();

      TestUtils.Simulate.click(overlay);

      expect(view.state.showPanel).eql(false);
      expect(panel.classList.contains("share-panel-open")).eql(false);
    });

    it("should close the share panel when clicking outside the panel", function() {
      view.setState({
        showPanel: true
      });

      var panel = view.getDOMNode();

      view._onDocumentVisibilityChanged({
        target: {
          hidden: true
        }
      });

      expect(view.state.showPanel).eql(false);
      expect(panel.classList.contains("share-panel-open")).eql(false);
    });

    it("should close the hello panel when clicking outside the panel", function() {
      view.setState({
        showPanel: true
      });

      view._onDocumentVisibilityChanged({
        target: {
          hidden: true
        }
      });

      expect(view.state.showPanel).eql(false);
      sinon.assert.calledOnce(view.closeWindow);
    });

    it("should call openRoom when hello panel is closed", function() {
      var openRoomSpy = sinon.spy(view, "openRoom");
      view.setState({
        showPanel: true
      });

      view._onDocumentVisibilityChanged({
        target: {
          hidden: true
        }
      });

      sinon.assert.calledOnce(openRoomSpy);
    });

    it("should invoke onSharePanelDisplayChange when hello panel is closed", function() {
      view.setState({
        showPanel: true
      });

      view._onDocumentVisibilityChanged({
        target: {
          hidden: true
        }
      });

      sinon.assert.calledOnce(view.props.onSharePanelDisplayChange);
    });

    it("should invoke onSharePanelDisplayChange when hello panel is displayed", function() {
      // Simulate that the user has click the browse button
      roomStore.setStoreState({
        pendingCreation: true
      });

      // Room has been created succesfully
      roomStore.setStoreState({
        pendingCreation: false
      });

      sinon.assert.calledOnce(view.props.onSharePanelDisplayChange);
    });
  });
});
