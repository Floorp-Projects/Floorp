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

  beforeEach(function(done) {
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
    };

    document.mozL10n.initialize(navigator.mozLoop);
    // XXX prevent a race whenever mozL10n hasn't been initialized yet
    setTimeout(done, 0);
  });

  describe("ContactDetailsForm", function() {
    describe("#render", function() {
      describe("add mode", function() {
        it("should render 'add' header", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "add"}));

          var header = view.getDOMNode().querySelector("header");
          expect(header).to.not.equal(null);
          expect(header.textContent).to.eql(fakeAddContactButtonText);
        });
        it("should render name input", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "add"}));

          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          expect(nameInput).to.not.equal(null);
        });
        it("should render email input", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "add"}));

          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          expect(emailInput).to.not.equal(null);
        });
        it("should render tel input", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "add"}));

          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          expect(telInput).to.not.equal(null);
        });
        it("should render 'add contact' button", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "add"}));

          var addButton = view.getDOMNode().querySelector(".button-accept");
          expect(addButton).to.not.equal(null);
          expect(addButton.textContent).to.eql(fakeAddContactButtonText);
        });
      });
      describe("edit mode", function() {
        it("should render 'edit' header", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "edit"}));

          var header = view.getDOMNode().querySelector("header");
          expect(header).to.not.equal(null);
          expect(header.textContent).to.eql(fakeEditContactButtonText);
        });
        it("should render name input", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "edit"}));

          var nameInput = view.getDOMNode().querySelector("input[type='text']");
          expect(nameInput).to.not.equal(null);
        });
        it("should render email input", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "edit"}));

          var emailInput = view.getDOMNode().querySelector("input[type='email']");
          expect(emailInput).to.not.equal(null);
        });
        it("should render tel input", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "edit"}));

          var telInput = view.getDOMNode().querySelector("input[type='tel']");
          expect(telInput).to.not.equal(null);
        });
        it("should render 'done' button", function() {
          var view = TestUtils.renderIntoDocument(
            loop.contacts.ContactDetailsForm({mode: "edit"}));

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
          loop.contacts._setPreferred(contact, {field: "fakeField", value: "", optional: true});

          expect(contact).to.not.have.property("fakeField");
       });
    it("should clear the value on the object if the new value is empty," +
       " it existed before, and it is optional", function() {
          var contact = {fakeField: [{value: "foobar"}]};
          loop.contacts._setPreferred(contact, {field: "fakeField", value: "", optional: true});

          expect(contact["fakeField"][0].value).to.eql("");
       });
    it("should set the value on the object if the new value is empty," +
       " and it did not exist before", function() {
          var contact = {};
          loop.contacts._setPreferred(contact, {field: "fakeField", value: ""});

          expect(contact).to.have.property("fakeField");
       });
    it("should set the value on the object if the new value is empty," +
       " and it did not exist before", function() {
          var contact = {fakeField: [{value: "foobar"}]};
          loop.contacts._setPreferred(contact, {field: "fakeField", value: "barbaz"});

          expect(contact["fakeField"][0].value).to.eql("barbaz");
       });
  });
});
