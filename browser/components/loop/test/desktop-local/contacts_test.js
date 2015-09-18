/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.contacts", function() {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;

  var CALL_TYPES = loop.shared.utils.CALL_TYPES;

  var fakeAddContactButtonText = "Fake Add Contact Button";
  var fakeAddContactTitleText = "Fake Add Contact Title";
  var fakeEditContactButtonText = "Fake Edit Contact";
  var fakeDoneButtonText = "Fake Done";

  var sandbox, fakeWindow, fakeMozLoop, mozL10nGetSpy, listView, notifications;
  var fakeManyContacts, fakeFewerContacts;
  var oldMozLoop = navigator.mozLoop;

  // The fake contacts array is copied each time mozLoop.contacts.getAll() is called.
  function getFakeContacts() {
    // Return a copy, so that tests that affect it, don't have impact on each other.
    return [].concat([
      {
        id: 1,
        _guid: 1,
        name: ["Ally Avocado"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "ally@mail.com"
        }],
        tel: [{
          "pref": true,
          "type": ["mobile"],
          "value": "+31-6-12345678"
        }],
        category: ["google"],
        published: 1406798311748,
        updated: 1406798311748
      }, {
        id: 2,
        _guid: 2,
        name: ["Bob Banana"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "bob@gmail.com"
        }],
        tel: [{
          "pref": true,
          "type": ["mobile"],
          "value": "+1-214-5551234"
        }],
        category: ["local"],
        published: 1406798311748,
        updated: 1406798311748
      }, {
        id: 3,
        _guid: 3,
        name: ["Caitlin Cantaloupe"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "caitlin.cant@hotmail.com"
        }],
        category: ["local"],
        published: 1406798311748,
        updated: 1406798311748
      }, {
        id: 4,
        _guid: 4,
        name: ["Dave Dragonfruit"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "dd@dragons.net"
        }],
        category: ["google"],
        published: 1406798311748,
        updated: 1406798311748
      }, {
        id: 5,
        _guid: 5,
        name: ["Erin J. Bazile"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "erinjbazile@armyspy.com"
        }],
        category: ["google"],
        published: 1406798311748,
        updated: 1406798311748
      }, {
        id: 6,
        _guid: 6,
        name: ["Kelly F. Maldanado"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "kellyfmaldonado@jourrapide.com"
        }],
        category: ["google"],
        published: 1406798311748,
        updated: 1406798311748
      }, {
        id: 7,
        _guid: 7,
        name: ["John J. Brown"],
        email: [{
          "pref": true,
          "type": ["work"],
          "value": "johnjbrow@johndoe.com"
        }],
        category: ["google"],
        published: 1406798311748,
        updated: 1406798311748,
        blocked: true
      }
    ]);
  }

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    mozL10nGetSpy = sandbox.spy(document.mozL10n, "get");

    fakeManyContacts = getFakeContacts();
    fakeFewerContacts = fakeManyContacts.slice(0, 4);

    fakeMozLoop = navigator.mozLoop = {
      getStrings: function(entityName) {
        var textContentValue = "fakeText";
        if (entityName === "add_contact_title") {
          textContentValue = fakeAddContactTitleText;
        } else if (entityName === "add_contact_button") {
          textContentValue = fakeAddContactButtonText;
        } else if (entityName === "edit_contact_title") {
          textContentValue = fakeEditContactButtonText;
        } else if (entityName === "edit_contact_done_button") {
          textContentValue = fakeDoneButtonText;
        }
        return JSON.stringify({textContent: textContentValue});
      },
      getLoopPref: function(pref) {
        if (pref === "contacts.gravatars.promo") {
          return true;
        } else if (pref === "contacts.gravatars.show") {
          return false;
        }
        return "";
      },
      setLoopPref: sandbox.stub(),
      getUserAvatar: function() {
        if (this.getLoopPref("contacts.gravatars.show")) {
          return "gravatarsEnabled";
        }
        return "gravatarsDisabled";
      },
      contacts: {
        getAll: function(callback) {
          callback(null, [].concat(fakeFewerContacts));
        },
        add: sandbox.stub(),
        on: sandbox.stub()
      },
      calls: {
        startDirectCall: sinon.stub(),
        clearCallInProgress: sinon.stub()
      },
      generateUUID: sandbox.stub()
    };

    fakeWindow = {
      close: sandbox.stub(),
      addEventListener: function() {},
      removeEventListener: function() {}
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    notifications = new loop.shared.models.NotificationCollection();

    document.mozL10n.initialize(fakeMozLoop);
  });

  afterEach(function() {
    listView = null;
    loop.shared.mixins.setRootObject(window);
    navigator.mozLoop = oldMozLoop;
    sandbox.restore();
  });

  describe("GravatarsPromo", function() {
    function checkGravatarContacts(enabled) {
      var node = listView.getDOMNode();

      // When gravatars are enabled, contacts should be rendered with gravatars.
      var gravatars = node.querySelectorAll(".contact img[src=gravatarsEnabled]");
      expect(gravatars.length).to.equal(enabled ? fakeFewerContacts.length : 0);
      // Sanity check the reverse:
      gravatars = node.querySelectorAll(".contact img[src=gravatarsDisabled]");
      expect(gravatars.length).to.equal(enabled ? 0 : fakeFewerContacts.length);
    }

    it("should show the gravatars promo box", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.not.equal(null);

      var avatars = listView.getDOMNode().querySelectorAll(".contacts-gravatar-avatars img");
      expect(avatars).to.have.length(2, "two example avatars are shown");

      checkGravatarContacts(false);
    });

    it("should not show the gravatars promo box when the 'contacts.gravatars.promo' pref is set", function() {
      sandbox.stub(fakeMozLoop, "getLoopPref", function(pref) {
        if (pref === "contacts.gravatars.promo") {
          return false;
        } else if (pref === "contacts.gravatars.show") {
          return true;
        }
        return "";
      });

      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);

      checkGravatarContacts(true);
    });

    it("should hide the gravatars promo box when the 'use' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .secondary:last-child"));

      sinon.assert.calledTwice(fakeMozLoop.setLoopPref);

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);
    });

    it("should should set the prefs correctly when the 'use' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .secondary:last-child"));

      sinon.assert.calledTwice(fakeMozLoop.setLoopPref);
      sinon.assert.calledWithExactly(fakeMozLoop.setLoopPref, "contacts.gravatars.promo", false);
      sinon.assert.calledWithExactly(fakeMozLoop.setLoopPref, "contacts.gravatars.show", true);
    });

    it("should hide the gravatars promo box when the 'close' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .secondary:first-child"));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);
    });

    it("should set prefs correctly when the 'close' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .secondary:first-child"));

      sinon.assert.calledOnce(fakeMozLoop.setLoopPref);
      sinon.assert.calledWithExactly(fakeMozLoop.setLoopPref,
        "contacts.gravatars.promo", false);
    });

    it("should hide the gravatars promo box when the 'close' X button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .button-close"));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);
    });

    it("should set prefs correctly when the 'close' X button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          mozLoop: fakeMozLoop,
          notifications: notifications,
          switchToContactAdd: sandbox.stub(),
          switchToContactEdit: sandbox.stub()
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .button-close"));

      sinon.assert.calledOnce(fakeMozLoop.setLoopPref);
      sinon.assert.calledWithExactly(fakeMozLoop.setLoopPref,
        "contacts.gravatars.promo", false);
    });
  });

  describe("ContactsControllerView - contactAdd", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsControllerView, {
          initialSelectedTabComponent: "contactAdd",
          mozLoop: fakeMozLoop,
          notifications: notifications
        }));
    });

    it("should switch component to Contact List view", function() {
      view.switchComponentView("contactList")();

      expect(view.refs.contacts_list).to.not.eql(null);
    });
  });

  describe("ContactsControllerView - contactEdit", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsControllerView, {
          initialSelectedTabComponent: "contactEdit",
          mozLoop: fakeMozLoop,
          notifications: notifications
        }));
    });

    it("should switch component to Contact List view", function() {
      view.switchComponentView("contactList")();

      expect(view.refs.contacts_list).to.not.eql(null);
    });
  });

  describe("ContactsControllerView - contactList", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsControllerView, {
          initialSelectedTabComponent: "contactList",
          mozLoop: fakeMozLoop,
          notifications: notifications
        }));
    });

    it("should switch component to Contact Add view", function() {
      view.handleAddEditContact("contactAdd")({});

      expect(view.refs.contacts_add).to.not.eql(null);
    });

    it("should switch component to Contact Edit view", function() {
      view.handleAddEditContact("contactEdit")();

      expect(view.refs.contacts_edit).to.not.eql(null);
    });
  });

  describe("ContactsList", function () {
    var node;

    describe("#RenderNoContacts", function() {
      beforeEach(function() {
        sandbox.stub(fakeMozLoop.contacts, "getAll", function(cb) {
          cb(null, []);
        });
        listView = TestUtils.renderIntoDocument(
          React.createElement(loop.contacts.ContactsList, {
            mozLoop: fakeMozLoop,
            notifications: notifications,
            switchToContactAdd: sandbox.stub(),
            switchToContactEdit: sandbox.stub()
          }));
        node = listView.getDOMNode();
      });

      it("should not show a contacts title if no contacts", function() {
        expect(node.querySelector(".contact-list-title")).to.eql(null);
        sinon.assert.neverCalledWith(mozL10nGetSpy, "contact_list_title");
      });

      it("should show the no contacts view", function() {
        expect(node.querySelector(".contact-list-empty")).to.not.eql(null);
      });

      it("should display the no contacts strings", function() {
        sinon.assert.calledWithExactly(mozL10nGetSpy,
                                       "no_contacts_message_heading2");
        sinon.assert.calledWithExactly(mozL10nGetSpy,
                                       "no_contacts_import_or_add2");
      });
    });

    describe("#RenderWithContacts", function() {
      beforeEach(function() {
        sandbox.stub(fakeMozLoop.contacts, "getAll", function(cb) {
          cb(null, [].concat(fakeFewerContacts));
        });
        listView = TestUtils.renderIntoDocument(
          React.createElement(loop.contacts.ContactsList, {
            mozLoop: fakeMozLoop,
            notifications: notifications,
            switchToContactAdd: sandbox.stub(),
            switchToContactEdit: sandbox.stub()
          }));
        node = listView.getDOMNode();
      });

      it("should show a contacts title", function() {
        expect(node.querySelector(".contact-list-title")).not.to.eql(null);
        sinon.assert.calledWithExactly(mozL10nGetSpy, "contact_list_title");
      });

      it("should not render the filter view unless MIN_CONTACTS_FOR_FILTERING",
         function() {
           var filterView = listView.getDOMNode()
             .querySelector(".contact-filter-container");

           expect(filterView).to.eql(null);
         });
    });

    describe("ContactsFiltering", function() {
      beforeEach(function() {
        fakeMozLoop.contacts = {
          getAll: function(callback) {
            callback(null, [].concat(fakeManyContacts));
          },
          on: sandbox.stub()
        };
        listView = TestUtils.renderIntoDocument(
          React.createElement(loop.contacts.ContactsList, {
            mozLoop: fakeMozLoop,
            notifications: notifications,
            switchToContactAdd: sandbox.stub(),
            switchToContactEdit: sandbox.stub()
          }));
        node = listView.getDOMNode();
      });

      it("should filter a non-existent user name", function() {
        expect(listView.filterContact("foo")(fakeFewerContacts[0]))
          .to.eql(false);
      });

      it("should display search returned no contacts view", function() {
        listView.setState({
          filter: "foo"
        });

        var view = node.querySelector(".contact-search-list-empty");

        expect(view).to.not.eql(null);
      });

      it("should display the no search results strings", function() {
        listView.setState({
          filter: "foo"
        });

        sinon.assert.calledWithExactly(mozL10nGetSpy,
                                       "contacts_no_search_results");
      });

      it("should filter the user name correctly", function() {
        expect(listView.filterContact("ally")(fakeFewerContacts[0]))
          .to.eql(true);
      });

      it("should filter and render a contact", function() {
        listView.setState({
          filter: "Ally"
        });

        var contacts = node.querySelectorAll(".contact");

        expect(contacts.length).to.eql(1);
      });

      it("should render a list of contacts", function() {
        var contactList = listView.getDOMNode().querySelectorAll(".contact");

        expect(contactList.length).to.eql(fakeManyContacts.length);
      });

      it("should render the filter view for >= MIN_CONTACTS_FOR_FILTERING",
         function() {
           var filterView = listView.getDOMNode()
             .querySelector(".contact-filter-container");

           expect(filterView).to.not.eql(null);
         });

      it("should filter by name", function() {
        var input = listView.getDOMNode()
          .querySelector(".contact-filter-container input");

        React.addons.TestUtils.Simulate.change(input,
                                               { target: { value: "Ally" } });
        var contactList = listView.getDOMNode().querySelectorAll(".contact");

        expect(contactList.length).to.eql(1);
      });

      it("should filter by email", function() {
        var input = listView.getDOMNode()
          .querySelector(".contact-filter-container input");

        React.addons.TestUtils.Simulate.change(input,
                                               { target: { value: "@hotmail" } });
        var contactList = listView.getDOMNode().querySelectorAll(".contact");

        expect(contactList.length).to.eql(1);
      });

      it("should filter by phone number", function() {
        var input = listView.getDOMNode()
          .querySelector(".contact-filter-container input");

        React.addons.TestUtils.Simulate.change(input,
                                               { target: { value: "12345678" } });
        var contactList = listView.getDOMNode().querySelectorAll(".contact");

        expect(contactList.length).to.eql(1);
      });
    });

    describe("#handleContactAddEdit", function() {

      beforeEach(function() {
        listView = TestUtils.renderIntoDocument(
          React.createElement(loop.contacts.ContactsList, {
            mozLoop: fakeMozLoop,
            notifications: notifications,
            switchToContactAdd: sandbox.stub(),
            switchToContactEdit: sandbox.stub()
          }));
      });

      it("should call switchToContactAdd function when Add Contact button is clicked",
        function() {
          var addContactBttn = listView.getDOMNode().querySelector(".contact-controls .primary");

          React.addons.TestUtils.Simulate.click(addContactBttn);

          sinon.assert.calledOnce(listView.props.switchToContactAdd);
        });

      it("should call switchToContactEdit function when selecting to Edit Contact",
        function() {
          listView.handleContactAction({}, "edit");

          sinon.assert.calledOnce(listView.props.switchToContactEdit);
        });
    });

    describe("#handleImportButtonClick", function() {
      beforeEach(function() {
        sandbox.stub(fakeMozLoop.contacts, "getAll", function(cb) {
          cb(null, []);
        });
        listView = TestUtils.renderIntoDocument(
          React.createElement(loop.contacts.ContactsList, {
            mozLoop: fakeMozLoop,
            notifications: notifications,
            switchToContactAdd: sandbox.stub(),
            switchToContactEdit: sandbox.stub()
          }));
        node = listView.getDOMNode();
      });

      it("should notify the end user from a successful import", function() {
        sandbox.stub(notifications, "successL10n");
        fakeMozLoop.startImport = function(opts, cb) {
          cb(null, {success: 42});
        };

        listView.handleImportButtonClick();

        sinon.assert.calledWithExactly(
          notifications.successL10n,
          "import_contacts_success_message",
          // Num is for the plural selection.
          {num: 42, total: 42});
      });

      it("should notify the end user from any encountered error", function() {
        sandbox.stub(notifications, "errorL10n");
        fakeMozLoop.startImport = function(opts, cb) {
          cb(new Error("fake error"));
        };

        listView.handleImportButtonClick();

        sinon.assert.calledWithExactly(notifications.errorL10n,
                                       "import_contacts_failure_message");
      });
    });
    describe("Individual Contacts", function() {
      describe("Contact Menu", function() {
        var view, contactMenu, contact;

        function mountTestComponent(options) {
          var props = _.extend({
            mozLoop: fakeMozLoop,
            notifications: notifications,
            switchToContactAdd: sandbox.stub(),
            switchToContactEdit: sandbox.stub()
          }, options);
          return TestUtils.renderIntoDocument(
            React.createElement(loop.contacts.ContactsList, props));
        }

        beforeEach(function() {
          contact = fakeFewerContacts[0];

          fakeMozLoop.contacts.getAll = function(callback) {
            callback(null, [contact]);
          };

          view = mountTestComponent();
          node = view.getDOMNode();

          // Open the menu
          var menuButton = node.querySelector(".icon-contact-menu-button");
          var eventStub = {"pageY": 20};
          TestUtils.Simulate.click(menuButton, eventStub);

          // Get the menu for use in the tests.
          contactMenu = node.querySelector(".contact > .dropdown-menu");
        });

        describe("Video Conversation button", function() {
          it("should call startDirectCall when the button is clicked", function() {
            TestUtils.Simulate.click(contactMenu.querySelector(".video-call-item"));

            sinon.assert.calledOnce(fakeMozLoop.calls.startDirectCall);
            sinon.assert.calledWithExactly(fakeMozLoop.calls.startDirectCall,
              contact,
              CALL_TYPES.AUDIO_VIDEO);
          });

          it("should close the window when the button is clicked", function() {
            TestUtils.Simulate.click(contactMenu.querySelector(".video-call-item"));

            sinon.assert.calledOnce(fakeWindow.close);
          });

          it("should not do anything if the contact is blocked", function() {
            contact.blocked = true;

            TestUtils.Simulate.click(contactMenu.querySelector(".video-call-item"));

            sinon.assert.notCalled(fakeMozLoop.calls.startDirectCall);
            sinon.assert.notCalled(fakeWindow.close);
          });
        });

        describe("Audio Conversation button", function() {
          it("should call startDirectCall when the button is clicked", function() {
            TestUtils.Simulate.click(contactMenu.querySelector(".audio-call-item"));

            sinon.assert.calledOnce(fakeMozLoop.calls.startDirectCall);
            sinon.assert.calledWithExactly(fakeMozLoop.calls.startDirectCall,
              contact,
              CALL_TYPES.AUDIO_ONLY);
          });

          it("should close the window when the button is clicked", function() {
            TestUtils.Simulate.click(contactMenu.querySelector(".audio-call-item"));

            sinon.assert.calledOnce(fakeWindow.close);
          });

          it("should not do anything if the contact is blocked", function() {
            contact.blocked = true;

            TestUtils.Simulate.click(contactMenu.querySelector(".audio-call-item"));

            sinon.assert.notCalled(fakeMozLoop.calls.startDirectCall);
            sinon.assert.notCalled(fakeWindow.close);
          });
        });
      });
    });
  });

  describe("ContactDetailsForm", function() {
    describe("#render", function() {
      describe("add mode", function() {
        var view;

        beforeEach(function() {
          view = TestUtils.renderIntoDocument(
            React.createElement(loop.contacts.ContactDetailsForm, {
              contactFormData: {},
              mode: "add",
              mozLoop: fakeMozLoop,
              switchToInitialView: sandbox.stub()
            }));
        });

        it("should render 'add' header", function() {

          var header = view.getDOMNode().querySelector("header");
          expect(header).to.not.equal(null);
          expect(header.textContent).to.eql(fakeAddContactTitleText);
        });

        it("should render name input", function() {

          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          expect(nameInput).to.not.equal(null);
        });

        it("should render email input", function() {

          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          expect(emailInput).to.not.equal(null);
        });

        it("should render tel input", function() {

          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          expect(telInput).to.not.equal(null);
        });

        it("should render 'add contact' button", function() {

          var addButton = view.getDOMNode().querySelector(".button-accept");
          expect(addButton).to.not.equal(null);
          expect(addButton.textContent).to.eql(fakeAddContactButtonText);
        });

        it("should have all fields required by default", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          var emailInput = view.getDOMNode().querySelector("input[type='email']");

          expect(nameInput.required).to.equal(true);
          expect(emailInput.required).to.equal(true);
          expect(telInput.required).to.equal(true);
        });

        it("should have email and tel required after a name is input", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          TestUtils.Simulate.change(nameInput, {target: {value: "Jenny"}});
          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          var emailInput = view.getDOMNode().querySelector("input[type='email']");

          expect(nameInput.required).to.equal(true);
          expect(emailInput.required).to.equal(true);
          expect(telInput.required).to.equal(true);
        });

        it("should allow a contact with only a name and a phone number", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          TestUtils.Simulate.change(nameInput, {target: {value: "Jenny"}});
          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          TestUtils.Simulate.change(telInput, {target: {value: "867-5309"}});
          var emailInput = view.getDOMNode().querySelector("input[type='email']");

          expect(nameInput.checkValidity()).to.equal(true, "nameInput");
          expect(emailInput.required).to.equal(false, "emailInput");
          expect(telInput.checkValidity()).to.equal(true, "telInput");
        });

        it("should allow a contact with only a name and email", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          TestUtils.Simulate.change(nameInput, {target: {value: "Example"}});
          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          TestUtils.Simulate.change(emailInput, {target: {value: "test@example.com"}});
          var telInput = view.getDOMNode().querySelector("input[type='tel']");

          expect(nameInput.checkValidity()).to.equal(true);
          expect(emailInput.checkValidity()).to.equal(true);
          expect(telInput.required).to.equal(false);
        });

        it("should not allow a contact with only a name", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          TestUtils.Simulate.change(nameInput, {target: {value: "Example"}});
          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          var telInput = view.getDOMNode().querySelector("input[type='tel']");

          expect(nameInput.checkValidity()).to.equal(true);
          expect(emailInput.checkValidity()).to.equal(false);
          expect(telInput.checkValidity()).to.equal(false);
        });

        it("should not allow a contact without name", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          TestUtils.Simulate.change(emailInput, {target: {value: "test@example.com"}});
          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          TestUtils.Simulate.change(telInput, {target: {value: "867-5309"}});

          expect(nameInput.checkValidity()).to.equal(false);
          expect(emailInput.checkValidity()).to.equal(true);
          expect(telInput.checkValidity()).to.equal(true);
        });

        it("should call switchToInitialView when Add Contact button is clicked", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          var addButton = view.getDOMNode().querySelector(".button-accept");

          TestUtils.Simulate.change(nameInput, {target: {value: "Example"}});
          TestUtils.Simulate.change(emailInput, {target: {value: "test@example.com"}});
          React.addons.TestUtils.Simulate.click(addButton);

          sinon.assert.calledOnce(view.props.switchToInitialView);
        });

        it("should call switchToInitialView when Cancel button is clicked", function() {
          var cancelButton = view.getDOMNode().querySelector(".button-cancel");

          React.addons.TestUtils.Simulate.click(cancelButton);

          sinon.assert.calledOnce(view.props.switchToInitialView);
        });
      });

      describe("edit mode", function() {
        var view;

        beforeEach(function() {
          view = TestUtils.renderIntoDocument(
            React.createElement(loop.contacts.ContactDetailsForm, {
              contactFormData: {},
              mode: "edit",
              mozLoop: fakeMozLoop,
              switchToInitialView: sandbox.stub()
            }));
        });

        it("should render 'edit' header", function() {
          var header = view.getDOMNode().querySelector("header");
          expect(header).to.not.equal(null);
          expect(header.textContent).to.eql(fakeEditContactButtonText);
        });

        it("should render name input", function() {
          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          expect(nameInput).to.not.equal(null);
        });

        it("should render email input", function() {
          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          expect(emailInput).to.not.equal(null);
        });

        it("should render tel input", function() {
          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          expect(telInput).to.not.equal(null);
        });

        it("should render 'done' button", function() {
          var doneButton = view.getDOMNode().querySelector(".button-accept");
          expect(doneButton).to.not.equal(null);
          expect(doneButton.textContent).to.eql(fakeDoneButtonText);
        });
      });
    });
  });

  describe("_getPreferred", function() {
    it("should return an dummy object if the field doesn't exist", function() {
      var obj = loop.contacts._getPreferred({}, "fieldThatDoesntExist");

      expect(obj.value).to.eql("");
    });

    it("should return the preferred value when the field exists", function() {
      var correctValue = "correct value";
      var fakeContact = {fakeField: [{value: "wrong value"}, {value: correctValue, pref: true}]};
      var obj = loop.contacts._getPreferred(fakeContact, "fakeField");

      expect(obj.value).to.eql(correctValue);
    });
  });

  describe("_setPreferred", function() {
    it("should not set the value on the object if the new value is empty," +
       " it didn't exist before, and it is optional", function() {
          var contact = {};
          loop.contacts._setPreferred(contact, "fakeField", "");

          expect(contact).to.not.have.property("fakeField");
       });

    it("should clear the value on the object if the new value is empty," +
       " it existed before, and it is optional", function() {
          var contact = {fakeField: [{value: "foobar"}]};
          loop.contacts._setPreferred(contact, "fakeField", "");

          expect(contact.fakeField[0].value).to.eql("");
       });

    it("should set the value on the object if the new value is empty," +
       " and it did not exist before", function() {
          var contact = {fakeField: [{value: "foobar"}]};
          loop.contacts._setPreferred(contact, "fakeField", "barbaz");

          expect(contact.fakeField[0].value).to.eql("barbaz");
       });
  });
});
