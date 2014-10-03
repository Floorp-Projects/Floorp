/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop:true, React */

var loop = loop || {};
loop.contacts = (function(_, mozL10n) {
  "use strict";

  const Button = loop.shared.views.Button;
  const ButtonGroup = loop.shared.views.ButtonGroup;

  // Number of contacts to add to the list at the same time.
  const CONTACTS_CHUNK_SIZE = 100;

  // At least this number of contacts should be present for the filter to appear.
  const MIN_CONTACTS_FOR_FILTERING = 7;

  let getContactNames = function(contact) {
    // The model currently does not enforce a name to be present, but we're
    // going to assume it is awaiting more advanced validation of required fields
    // by the model. (See bug 1069918)
    // NOTE: this method of finding a firstname and lastname is not i18n-proof.
    let names = contact.name[0].split(" ");
    return {
      firstName: names.shift(),
      lastName: names.join(" ")
    };
  };

  let getPreferredEmail = function(contact) {
    // A contact may not contain email addresses, but only a phone number.
    if (!contact.email || contact.email.length == 0) {
      return { value: "" };
    }
    return contact.email.find(e => e.pref) || contact.email[0];
  };

  const ContactDropdown = React.createClass({
    propTypes: {
      handleAction: React.PropTypes.func.isRequired,
      canEdit: React.PropTypes.bool
    },

    getInitialState: function () {
      return {
        openDirUp: false,
      };
    },

    componentDidMount: function () {
      // This method is called once when the dropdown menu is added to the DOM
      // inside the contact item.  If the menu extends outside of the visible
      // area of the scrollable list, it is re-rendered in different direction.

      let menuNode = this.getDOMNode();
      let menuNodeRect = menuNode.getBoundingClientRect();

      let listNode = document.getElementsByClassName("contact-list")[0];
      let listNodeRect = listNode.getBoundingClientRect();

      if (menuNodeRect.top + menuNodeRect.height >=
          listNodeRect.top + listNodeRect.height) {
        this.setState({
          openDirUp: true,
        });
      }
    },

    onItemClick: function(event) {
      this.props.handleAction(event.currentTarget.dataset.action);
    },

    render: function() {
      var cx = React.addons.classSet;

      let blockAction = this.props.blocked ? "unblock" : "block";
      let blockLabel = this.props.blocked ? "unblock_contact_menu_button"
                                          : "block_contact_menu_button";

      return (
        <ul className={cx({ "dropdown-menu": true,
                            "dropdown-menu-up": this.state.openDirUp })}>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": true })}
              onClick={this.onItemClick} data-action="video-call">
            <i className="icon icon-video-call" />
            {mozL10n.get("video_call_menu_button")}
          </li>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": true })}
              onClick={this.onItemClick} data-action="audio-call">
            <i className="icon icon-audio-call" />
            {mozL10n.get("audio_call_menu_button")}
          </li>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": !this.props.canEdit })}
              onClick={this.onItemClick} data-action="edit">
            <i className="icon icon-edit" />
            {mozL10n.get("edit_contact_menu_button")}
          </li>
          <li className="dropdown-menu-item"
              onClick={this.onItemClick} data-action={blockAction}>
            <i className={"icon icon-" + blockAction} />
            {mozL10n.get(blockLabel)}
          </li>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": !this.props.canEdit })}
              onClick={this.onItemClick} data-action="remove">
            <i className="icon icon-remove" />
            {mozL10n.get("remove_contact_menu_button")}
          </li>
        </ul>
      );
    }
  });

  const ContactDetail = React.createClass({
    getInitialState: function() {
      return {
        showMenu: false,
      };
    },

    propTypes: {
      handleContactAction: React.PropTypes.func,
      contact: React.PropTypes.object.isRequired
    },

    _onBodyClick: function() {
      // Hide the menu after other click handlers have been invoked.
      setTimeout(this.hideDropdownMenu, 10);
    },

    showDropdownMenu: function() {
      document.body.addEventListener("click", this._onBodyClick);
      this.setState({showMenu: true});
    },

    hideDropdownMenu: function() {
      document.body.removeEventListener("click", this._onBodyClick);
      // Since this call may be deferred, we need to guard it, for example in
      // case the contact was removed in the meantime.
      if (this.isMounted()) {
        this.setState({showMenu: false});
      }
    },

    componentWillUnmount: function() {
      document.body.removeEventListener("click", this._onBodyClick);
    },

    componentShouldUpdate: function(nextProps, nextState) {
      let currContact = this.props.contact;
      let nextContact = nextProps.contact;
      return (
        currContact.name[0] !== nextContact.name[0] ||
        currContact.blocked !== nextContact.blocked ||
        getPreferredEmail(currContact).value !== getPreferredEmail(nextContact).value
      );
    },

    handleAction: function(actionName) {
      if (this.props.handleContactAction) {
        this.props.handleContactAction(this.props.contact, actionName);
      }
    },

    canEdit: function() {
      // We cannot modify imported contacts.  For the moment, the check for
      // determining whether the contact is imported is based on its category.
      return this.props.contact.category[0] != "google";
    },

    render: function() {
      let names = getContactNames(this.props.contact);
      let email = getPreferredEmail(this.props.contact);
      let cx = React.addons.classSet;
      let contactCSSClass = cx({
        contact: true,
        blocked: this.props.contact.blocked
      });

      return (
        <li className={contactCSSClass} onMouseLeave={this.hideDropdownMenu}>
          <div className="avatar" />
          <div className="details">
            <div className="username"><strong>{names.firstName}</strong> {names.lastName}
              <i className={cx({"icon icon-google": this.props.contact.category[0] == "google"})} />
              <i className={cx({"icon icon-blocked": this.props.contact.blocked})} />
            </div>
            <div className="email">{email.value}</div>
          </div>
          <div className="icons">
            <i className="icon icon-video"
               onClick={this.handleAction.bind(null, "video-call")} />
            <i className="icon icon-caret-down"
               onClick={this.showDropdownMenu} />
          </div>
          {this.state.showMenu
            ? <ContactDropdown handleAction={this.handleAction}
                               canEdit={this.canEdit()}
                               blocked={this.props.contact.blocked} />
            : null
          }
        </li>
      );
    }
  });

  const ContactsList = React.createClass({
    mixins: [React.addons.LinkedStateMixin],

    getInitialState: function() {
      return {
        contacts: {},
        importBusy: false,
        filter: "",
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
            this.handleContactAddOrUpdate(contact, false);
          });
          if (contacts.length) {
            setTimeout(addContactsInChunks, 0);
          }
          this.forceUpdate();
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

    handleContactAddOrUpdate: function(contact, render = true) {
      let contacts = this.state.contacts;
      let guid = String(contact._guid);
      contacts[guid] = contact;
      if (render) {
        this.forceUpdate();
      }
    },

    handleContactRemove: function(contact) {
      let contacts = this.state.contacts;
      let guid = String(contact._guid);
      if (!contacts[guid]) {
        return;
      }
      delete contacts[guid];
      this.forceUpdate();
    },

    handleContactRemoveAll: function() {
      this.setState({contacts: {}});
    },

    handleImportButtonClick: function() {
      this.setState({ importBusy: true });
      navigator.mozLoop.startImport({
        service: "google"
      }, (err, stats) => {
        this.setState({ importBusy: false });
        // TODO: bug 1076764 - proper error and success reporting.
        if (err) {
          throw err;
        }
      });
    },

    handleAddContactButtonClick: function() {
      this.props.startForm("contacts_add");
    },

    handleContactAction: function(contact, actionName) {
      switch (actionName) {
        case "edit":
          this.props.startForm("contacts_edit", contact);
          break;
        case "remove":
        case "block":
        case "unblock":
          // Invoke the API named like the action.
          navigator.mozLoop.contacts[actionName](contact._guid, err => {
            if (err) {
              throw err;
            }
          });
          break;
        default:
          console.error("Unrecognized action: " + actionName);
          break;
      }
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
        return <ContactDetail key={item._guid} contact={item}
                              handleContactAction={this.handleContactAction} />
      };

      let shownContacts = _.groupBy(this.state.contacts, function(contact) {
        return contact.blocked ? "blocked" : "available";
      });

      let showFilter = Object.getOwnPropertyNames(this.state.contacts).length >=
                       MIN_CONTACTS_FOR_FILTERING;
      if (showFilter) {
        let filter = this.state.filter.trim().toLocaleLowerCase();
        if (filter) {
          let filterFn = contact => {
            return contact.name[0].toLocaleLowerCase().contains(filter) ||
                   getPreferredEmail(contact).value.toLocaleLowerCase().contains(filter);
          };
          if (shownContacts.available) {
            shownContacts.available = shownContacts.available.filter(filterFn);
          }
          if (shownContacts.blocked) {
            shownContacts.blocked = shownContacts.blocked.filter(filterFn);
          }
        }
      }

      // TODO: bug 1076767 - add a spinner whilst importing contacts.
      return (
        <div>
          <div className="content-area">
            <ButtonGroup>
              <Button caption={this.state.importBusy
                               ? mozL10n.get("importing_contacts_progress_button")
                               : mozL10n.get("import_contacts_button")}
                      disabled={this.state.importBusy}
                      onClick={this.handleImportButtonClick} />
              <Button caption={mozL10n.get("new_contact_button")}
                      onClick={this.handleAddContactButtonClick} />
            </ButtonGroup>
            {showFilter ?
            <input className="contact-filter"
                   placeholder={mozL10n.get("contacts_search_placesholder")}
                   valueLink={this.linkState("filter")} />
            : null }
          </div>
          <ul className="contact-list">
            {shownContacts.available ?
              shownContacts.available.sort(this.sortContacts).map(viewForItem) :
              null}
            {shownContacts.blocked && shownContacts.blocked.length > 0 ?
              <div className="contact-separator">{mozL10n.get("contacts_blocked_contacts")}</div> :
              null}
            {shownContacts.blocked ?
              shownContacts.blocked.sort(this.sortContacts).map(viewForItem) :
              null}
          </ul>
        </div>
      );
    }
  });

  const ContactDetailsForm = React.createClass({
    mixins: [React.addons.LinkedStateMixin],

    propTypes: {
      mode: React.PropTypes.string
    },

    getInitialState: function() {
      return {
        contact: null,
        pristine: true,
        name: "",
        email: "",
      };
    },

    initForm: function(contact) {
      let state = this.getInitialState();
      if (contact) {
        state.contact = contact;
        state.name = contact.name[0];
        state.email = contact.email[0].value;
      }
      this.setState(state);
    },

    handleAcceptButtonClick: function() {
      // Allow validity error indicators to be displayed.
      this.setState({
        pristine: false,
      });

      if (!this.refs.name.getDOMNode().checkValidity() ||
          !this.refs.email.getDOMNode().checkValidity()) {
        return;
      }

      this.props.selectTab("contacts");

      let contactsAPI = navigator.mozLoop.contacts;

      switch (this.props.mode) {
        case "edit":
          this.state.contact.name[0] = this.state.name.trim();
          this.state.contact.email[0].value = this.state.email.trim();
          contactsAPI.update(this.state.contact, err => {
            if (err) {
              throw err;
            }
          });
          this.setState({
            contact: null,
          });
          break;
        case "add":
          contactsAPI.add({
            id: navigator.mozLoop.generateUUID(),
            name: [this.state.name.trim()],
            email: [{
              pref: true,
              type: ["home"],
              value: this.state.email.trim()
            }],
            category: ["local"]
          }, err => {
            if (err) {
              throw err;
            }
          });
          break;
      }
    },

    handleCancelButtonClick: function() {
      this.props.selectTab("contacts");
    },

    render: function() {
      let cx = React.addons.classSet;
      return (
        <div className="content-area contact-form">
          <header>{this.props.mode == "add"
                   ? mozL10n.get("add_contact_button")
                   : mozL10n.get("edit_contact_title")}</header>
          <label>{mozL10n.get("edit_contact_name_label")}</label>
          <input ref="name" required pattern="\s*\S.*"
                 className={cx({pristine: this.state.pristine})}
                 valueLink={this.linkState("name")} />
          <label>{mozL10n.get("edit_contact_email_label")}</label>
          <input ref="email" required type="email"
                 className={cx({pristine: this.state.pristine})}
                 valueLink={this.linkState("email")} />
          <ButtonGroup>
            <Button additionalClass="button-cancel"
                    caption={mozL10n.get("cancel_button")}
                    onClick={this.handleCancelButtonClick} />
            <Button additionalClass="button-accept"
                    caption={this.props.mode == "add"
                             ? mozL10n.get("add_contact_button")
                             : mozL10n.get("edit_contact_done_button")}
                    onClick={this.handleAcceptButtonClick} />
          </ButtonGroup>
        </div>
      );
    }
  });

  return {
    ContactsList: ContactsList,
    ContactDetailsForm: ContactDetailsForm,
  };
})(_, document.mozL10n);
