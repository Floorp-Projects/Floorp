/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop:true, React */

var loop = loop || {};
loop.contacts = (function(_, mozL10n) {
  "use strict";

  // Number of contacts to add to the list at the same time.
  const CONTACTS_CHUNK_SIZE = 100;

  const ContactDetail = React.createClass({displayName: 'ContactDetail',
    propTypes: {
      handleContactClick: React.PropTypes.func,
      contact: React.PropTypes.object.isRequired
    },

    handleContactClick: function() {
      if (this.props.handleContactClick) {
        this.props.handleContactClick(this.props.key);
      }
    },

    getContactNames: function() {
      // The model currently does not enforce a name to be present, but we're
      // going to assume it is awaiting more advanced validation of required fields
      // by the model. (See bug 1069918)
      // NOTE: this method of finding a firstname and lastname is not i18n-proof.
      let names = this.props.contact.name[0].split(" ");
      return {
        firstName: names.shift(),
        lastName: names.join(" ")
      };
    },

    getPreferredEmail: function() {
      // The model currently does not enforce a name to be present, but we're
      // going to assume it is awaiting more advanced validation of required fields
      // by the model. (See bug 1069918)
      let email = this.props.contact.email[0];
      this.props.contact.email.some(function(address) {
        if (address.pref) {
          email = address;
          return true;
        }
        return false;
      });
      return email;
    },

    render: function() {
      let names = this.getContactNames();
      let email = this.getPreferredEmail();
      let cx = React.addons.classSet;
      let contactCSSClass = cx({
        contact: true,
        blocked: this.props.contact.blocked
      });

      return (
        React.DOM.li({onClick: this.handleContactClick, className: contactCSSClass}, 
          React.DOM.div({className: "avatar"}, 
            React.DOM.img({src: navigator.mozLoop.getUserAvatar(email.value)})
          ), 
          React.DOM.div({className: "details"}, 
            React.DOM.div({className: "username"}, React.DOM.strong(null, names.firstName), " ", names.lastName, 
              React.DOM.i({className: cx({"icon icon-google": this.props.contact.category[0] == "google"})}), 
              React.DOM.i({className: cx({"icon icon-blocked": this.props.contact.blocked})})
            ), 
            React.DOM.div({className: "email"}, email.value)
          ), 
          React.DOM.div({className: "icons"}, 
            React.DOM.i({className: "icon icon-video"}), 
            React.DOM.i({className: "icon icon-caret-down"})
          )
        )
      );
    }
  });

  const ContactsList = React.createClass({displayName: 'ContactsList',
    getInitialState: function() {
      return {
        contacts: {}
      };
    },

    componentDidMount: function() {
      let contactsAPI = navigator.mozLoop.contacts;

      contactsAPI.getAll((err, contacts) => {
        if (err) {
          throw err;
        }

        // Add contacts already present in the DB. We do this in timed chunks to
        // circumvent blocking the main event loop.
        let addContactsInChunks = () => {
          contacts.splice(0, CONTACTS_CHUNK_SIZE).forEach(contact => {
            this.handleContactAddOrUpdate(contact);
          });
          if (contacts.length) {
            setTimeout(addContactsInChunks, 0);
          }
        };

        addContactsInChunks(contacts);

        // Listen for contact changes/ updates.
        contactsAPI.on("add", (eventName, contact) => {
          this.handleContactAddOrUpdate(contact);
        });
        contactsAPI.on("remove", (eventName, contact) => {
          this.handleContactRemove(contact);
        });
        contactsAPI.on("removeAll", () => {
          this.handleContactRemoveAll();
        });
        contactsAPI.on("update", (eventName, contact) => {
          this.handleContactAddOrUpdate(contact);
        });
      });
    },

    handleContactAddOrUpdate: function(contact) {
      let contacts = this.state.contacts;
      let guid = String(contact._guid);
      contacts[guid] = contact;
      this.setState({});
    },

    handleContactRemove: function(contact) {
      let contacts = this.state.contacts;
      let guid = String(contact._guid);
      if (!contacts[guid]) {
        return;
      }
      delete contacts[guid];
      this.setState({});
    },

    handleContactRemoveAll: function() {
      this.setState({contacts: {}});
    },

    sortContacts: function(contact1, contact2) {
      let comp = contact1.name[0].localeCompare(contact2.name[0]);
      if (comp !== 0) {
        return comp;
      }
      // If names are equal, compare against unique ids to make sure we have
      // consistent ordering.
      return contact1._guid - contact2._guid;
    },

    render: function() {
      let viewForItem = item => {
        return ContactDetail({key: item._guid, contact: item})
      };

      let shownContacts = _.groupBy(this.state.contacts, function(contact) {
        return contact.blocked ? "blocked" : "available";
      });

      return (
        React.DOM.div({className: "listWrapper"}, 
          React.DOM.div({ref: "listSlider", className: "listPanels"}, 
            React.DOM.div({className: "faded"}, 
              React.DOM.ul(null, 
                shownContacts.available ?
                  shownContacts.available.sort(this.sortContacts).map(viewForItem) :
                  null, 
                shownContacts.blocked ?
                  React.DOM.h3({className: "header"}, mozL10n.get("contacts_blocked_contacts")) :
                  null, 
                shownContacts.blocked ?
                  shownContacts.blocked.sort(this.sortContacts).map(viewForItem) :
                  null
              )
            )
          )
        )
      );
    }
  });

  return {
    ContactsList: ContactsList
  };
})(_, document.mozL10n);
