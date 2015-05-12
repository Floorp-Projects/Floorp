/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.contacts", function() {
  "use strict";

  var fakeAddContactButtonText = "Fake Add Contact";
  var fakeEditContactButtonText = "Fake Edit Contact";
  var fakeDoneButtonText = "Fake Done";
  // The fake contacts array is copied each time mozLoop.contacts.getAll() is called.
  var fakeContacts = [{
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
  }];
  var sandbox;
  var fakeWindow;
  var notifications;
  var listView;
  var oldMozLoop = navigator.mozLoop;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    navigator.mozLoop = {
      getStrings: function(entityName) {
        var textContentValue = "fakeText";
        if (entityName == "add_contact_button") {
          textContentValue = fakeAddContactButtonText;
        } else if (entityName == "edit_contact_title") {
          textContentValue = fakeEditContactButtonText;
        } else if (entityName == "edit_contact_done_button") {
          textContentValue = fakeDoneButtonText;
        }
        return JSON.stringify({textContent: textContentValue});
      },
      getLoopPref: function(pref) {
        if (pref == "contacts.gravatars.promo") {
          return true;
        } else if (pref == "contacts.gravatars.show") {
          return false;
        }
        return "";
      },
      setLoopPref: sandbox.stub(),
      getUserAvatar: function() {
        if (navigator.mozLoop.getLoopPref("contacts.gravatars.show")) {
          return "gravatarsEnabled";
        }
        return "gravatarsDisabled";
      },
      contacts: {
        getAll: function(callback) {
          callback(null, [].concat(fakeContacts));
        },
        on: sandbox.stub()
      }
    };

    fakeWindow = {
      close: sandbox.stub()
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    notifications = new loop.shared.models.NotificationCollection();

    document.mozL10n.initialize(navigator.mozLoop);
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
      expect(gravatars.length).to.equal(enabled ? fakeContacts.length : 0);
      // Sanity check the reverse:
      gravatars = node.querySelectorAll(".contact img[src=gravatarsDisabled]");
      expect(gravatars.length).to.equal(enabled ? 0 : fakeContacts.length);
    }

    it("should show the gravatars promo box", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.not.equal(null);

      checkGravatarContacts(false);
    });

    it("should not show the gravatars promo box when the 'contacts.gravatars.promo' pref is set", function() {
      navigator.mozLoop.getLoopPref = function(pref) {
        if (pref == "contacts.gravatars.promo") {
          return false;
        } else if (pref == "contacts.gravatars.show") {
          return true;
        }
        return "";
      };
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);

      checkGravatarContacts(true);
    });

    it("should hide the gravatars promo box when the 'use' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .button-accept"));

      sinon.assert.calledTwice(navigator.mozLoop.setLoopPref);

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);
    });

    it("should should set the prefs correctly when the 'use' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .button-accept"));

      sinon.assert.calledTwice(navigator.mozLoop.setLoopPref);
      sinon.assert.calledWithExactly(navigator.mozLoop.setLoopPref, "contacts.gravatars.promo", false);
      sinon.assert.calledWithExactly(navigator.mozLoop.setLoopPref, "contacts.gravatars.show", true);
    });

    it("should hide the gravatars promo box when the 'close' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .button-close"));

      var promo = listView.getDOMNode().querySelector(".contacts-gravatar-promo");
      expect(promo).to.equal(null);
    });

    it("should set prefs correctly when the 'close' button is clicked", function() {
      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));

      React.addons.TestUtils.Simulate.click(listView.getDOMNode().querySelector(
        ".contacts-gravatar-promo .button-close"));

      sinon.assert.calledOnce(navigator.mozLoop.setLoopPref);
      sinon.assert.calledWithExactly(navigator.mozLoop.setLoopPref,
        "contacts.gravatars.promo", false);
    });
  });

  describe("ContactsList", function () {
    beforeEach(function() {
      navigator.mozLoop.calls = {
        startDirectCall: sandbox.stub(),
        clearCallInProgress: sandbox.stub()
      };
      navigator.mozLoop.contacts = {getAll: sandbox.stub()};

      listView = TestUtils.renderIntoDocument(
        React.createElement(loop.contacts.ContactsList, {
          notifications: notifications
        }));
    });

    describe("#handleContactAction", function() {
      it("should call window.close when called with 'video-call' action",
        function() {
          listView.handleContactAction({}, "video-call");

          sinon.assert.calledOnce(fakeWindow.close);
      });

      it("should call window.close when called with 'audio-call' action",
        function() {
          listView.handleContactAction({}, "audio-call");

          sinon.assert.calledOnce(fakeWindow.close);
        });
    });

    describe("#handleImportButtonClick", function() {
      it("should notify the end user from a succesful import", function() {
        sandbox.stub(notifications, "successL10n");
        navigator.mozLoop.startImport = function(opts, cb) {
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
        navigator.mozLoop.startImport = function(opts, cb) {
          cb(new Error("fake error"));
        };

        listView.handleImportButtonClick();

        sinon.assert.calledWithExactly(notifications.errorL10n,
                                       "import_contacts_failure_message");
      });
    });
  });

  describe("ContactDetailsForm", function() {
    describe("#render", function() {
      describe("add mode", function() {
        var view;

        beforeEach(function() {
          view = TestUtils.renderIntoDocument(
            React.createElement(
              loop.contacts.ContactDetailsForm, {mode: "add"}));
        });

        it("should render 'add' header", function() {

          var header = view.getDOMNode().querySelector("header");
          expect(header).to.not.equal(null);
          expect(header.textContent).to.eql(fakeAddContactButtonText);
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
      });

      describe("edit mode", function() {
        var view;

        beforeEach(function() {
          view = TestUtils.renderIntoDocument(
            React.createElement(
              loop.contacts.ContactDetailsForm, {mode: "edit"}));
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
