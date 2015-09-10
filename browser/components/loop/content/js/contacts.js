/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.contacts = (function(_, mozL10n) {
  "use strict";

  var sharedMixins = loop.shared.mixins;

  const Button = loop.shared.views.Button;
  const ButtonGroup = loop.shared.views.ButtonGroup;
  const CALL_TYPES = loop.shared.utils.CALL_TYPES;

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

  /** Used to retrieve the preferred email or phone number
   *  for the contact. Both fields are optional.
   * @param   {object} contact
   *          The contact object to get the field from.
   * @param   {string} field
   *          The field that should be read out of the contact object.
   * @returns {object} An object with a 'value' property that hold a string value.
   */
  let getPreferred = function(contact, field) {
    if (!contact[field] || !contact[field].length) {
      return { value: "" };
    }
    return contact[field].find(e => e.pref) || contact[field][0];
  };

  /** Used to set the preferred email or phone number
   *  for the contact. Both fields are optional.
   * @param   {object} contact
   *          The contact object to get the field from.
   * @param   {string} field
   *          The field within the contact to set.
   * @param   {string} value
   *          The value that the field should be set to.
   */
  let setPreferred = function(contact, field, value) {
    // Don't clear the field if it doesn't exist.
    if (!value && (!contact[field] || !contact[field].length)) {
      return;
    }

    if (!contact[field]) {
      contact[field] = [];
    }

    if (!contact[field].length) {
      contact[field][0] = {"value": value};
      return;
    }
    // Set the value in the preferred tuple and return.
    for (let i in contact[field]) {
      if (contact[field][i].pref) {
        contact[field][i].value = value;
        return;
      }
    }
    contact[field][0].value = value;
  };

  const GravatarPromo = React.createClass({displayName: "GravatarPromo",
    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      handleUse: React.PropTypes.func.isRequired
    },

    getInitialState: function() {
      return {
        showMe: navigator.mozLoop.getLoopPref("contacts.gravatars.promo") &&
          !navigator.mozLoop.getLoopPref("contacts.gravatars.show")
      };
    },

    handleCloseButtonClick: function() {
      navigator.mozLoop.setLoopPref("contacts.gravatars.promo", false);
      this.setState({ showMe: false });
    },

    handleLinkClick: function(event) {
      if (!event.target || !event.target.href) {
        return;
      }

      event.preventDefault();
      navigator.mozLoop.openURL(event.target.href);
      this.closeWindow();
    },

    handleUseButtonClick: function() {
      navigator.mozLoop.setLoopPref("contacts.gravatars.promo", false);
      navigator.mozLoop.setLoopPref("contacts.gravatars.show", true);
      this.setState({ showMe: false });
      this.props.handleUse();
    },

    render: function() {
      if (!this.state.showMe) {
        return null;
      }

      let privacyUrl = navigator.mozLoop.getLoopPref("legal.privacy_url");
      let message = mozL10n.get("gravatars_promo_message", {
        "learn_more": React.renderToStaticMarkup(
          React.createElement("a", {href: privacyUrl, target: "_blank"}, 
            mozL10n.get("gravatars_promo_message_learnmore")
          )
        )
      });
      return (
        React.createElement("div", {className: "contacts-gravatar-promo"}, 
          React.createElement(Button, {additionalClass: "button-close", 
                  caption: "", 
                  onClick: this.handleCloseButtonClick}), 
          React.createElement("p", {dangerouslySetInnerHTML: {__html: message}, 
             onClick: this.handleLinkClick}), 
          React.createElement("div", {className: "contacts-gravatar-avatars"}, 
            React.createElement("img", {src: "loop/shared/img/avatars.svg#orange-avatar"}), 
            React.createElement("span", {className: "contacts-gravatar-arrow"}), 
            React.createElement("img", {src: "loop/shared/img/firefox-avatar.svg"})
          ), 
          React.createElement(ButtonGroup, {additionalClass: "contacts-gravatar-buttons"}, 
            React.createElement(Button, {additionalClass: "secondary", 
                    caption: mozL10n.get("gravatars_promo_button_nothanks2"), 
                    onClick: this.handleCloseButtonClick}), 
            React.createElement(Button, {additionalClass: "secondary", 
                    caption: mozL10n.get("gravatars_promo_button_use2"), 
                    onClick: this.handleUseButtonClick})
          )
        )
      );
    }
  });

  const ContactDropdown = React.createClass({displayName: "ContactDropdown",
    propTypes: {
      // If the contact is blocked or not.
      blocked: React.PropTypes.bool.isRequired,
      canEdit: React.PropTypes.bool,
      handleAction: React.PropTypes.func.isRequired
    },

    getInitialState: function () {
      return {
        openDirUp: false
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
          openDirUp: true
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
        React.createElement("ul", {className: cx({ "dropdown-menu": true,
                            "dropdown-menu-up": this.state.openDirUp })}, 
          React.createElement("li", {className: cx({ "dropdown-menu-item": true,
                              "disabled": !this.props.canEdit }), 
              "data-action": "edit", 
              onClick: this.onItemClick}, 
            mozL10n.get("edit_contact_title")
          ), 
          React.createElement("li", {className: "dropdown-menu-item", 
              "data-action": blockAction, 
              onClick: this.onItemClick}, 
            mozL10n.get(blockLabel)
          ), 
          React.createElement("li", {className: cx({ "dropdown-menu-item": true,
                              "disabled": !this.props.canEdit }), 
              "data-action": "remove", 
              onClick: this.onItemClick}, 
            mozL10n.get("confirm_delete_contact_remove_button")
          )
        )
      );
    }
  });

  const ContactDetail = React.createClass({displayName: "ContactDetail",
    getInitialState: function() {
      return {
        showMenu: false
      };
    },

    propTypes: {
      contact: React.PropTypes.object.isRequired,
      handleContactAction: React.PropTypes.func
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

    shouldComponentUpdate: function(nextProps, nextState) {
      let currContact = this.props.contact;
      let nextContact = nextProps.contact;
      let currContactEmail = getPreferred(currContact, "email").value;
      let nextContactEmail = getPreferred(nextContact, "email").value;
      return (
        currContact.name[0] !== nextContact.name[0] ||
        currContact.blocked !== nextContact.blocked ||
        currContactEmail !== nextContactEmail ||
        nextState.showMenu !== this.state.showMenu
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
      return this.props.contact.category[0] !== "google";
    },

    render: function() {
      let names = getContactNames(this.props.contact);
      let email = getPreferred(this.props.contact, "email");
      let avatarSrc = navigator.mozLoop.getUserAvatar(email.value);
      let cx = React.addons.classSet;
      let contactCSSClass = cx({
        contact: true,
        blocked: this.props.contact.blocked
      });
      let avatarCSSClass = cx({
        avatar: true,
        defaultAvatar: !avatarSrc
      });

      return (
        React.createElement("li", {className: contactCSSClass, onMouseLeave: this.hideDropdownMenu}, 
          React.createElement("div", {className: avatarCSSClass}, 
            avatarSrc ? React.createElement("img", {src: avatarSrc}) : null
          ), 
          React.createElement("div", {className: "details"}, 
            React.createElement("div", {className: "username"}, React.createElement("strong", null, names.firstName), " ", names.lastName, 
              React.createElement("i", {className: cx({"icon icon-blocked": this.props.contact.blocked})})
            ), 
            React.createElement("div", {className: "email"}, email.value)
          ), 
          React.createElement("div", {className: "icons"}, 
            React.createElement("i", {className: "icon icon-contact-video-call", 
               onClick: this.handleAction.bind(null, "video-call")}), 
            React.createElement("i", {className: "icon icon-vertical-ellipsis", 
               onClick: this.showDropdownMenu})
          ), 
          this.state.showMenu
            ? React.createElement(ContactDropdown, {blocked: this.props.contact.blocked, 
                               canEdit: this.canEdit(), 
                               handleAction: this.handleAction})
            : null
          
        )
      );
    }
  });

  const ContactsList = React.createClass({displayName: "ContactsList",
    mixins: [
      React.addons.LinkedStateMixin,
      loop.shared.mixins.WindowCloseMixin
    ],

    propTypes: {
      mozLoop: React.PropTypes.object.isRequired,
      notifications: React.PropTypes.instanceOf(loop.shared.models.NotificationCollection).isRequired,
      switchToContactAdd: React.PropTypes.func.isRequired,
      switchToContactEdit: React.PropTypes.func.isRequired
    },

    /**
     * Contacts collection object
     */
    contacts: null,

    /**
     * User profile
     */
    _userProfile: null,

    getInitialState: function() {
      return {
        importBusy: false,
        filter: ""
      };
    },

    refresh: function(callback = function() {}) {
      let contactsAPI = this.props.mozLoop.contacts;

      this.handleContactRemoveAll();

      contactsAPI.getAll((err, contacts) => {
        if (err) {
          callback(err);
          return;
        }

        // Add contacts already present in the DB. We do this in timed chunks to
        // circumvent blocking the main event loop.
        let addContactsInChunks = () => {
          contacts.splice(0, CONTACTS_CHUNK_SIZE).forEach(contact => {
            this.handleContactAddOrUpdate(contact, false);
          });
          if (contacts.length) {
            setTimeout(addContactsInChunks, 0);
          } else {
            callback();
          }
          this.forceUpdate();
        };

        addContactsInChunks(contacts);
      });
    },

    componentWillMount: function() {
      // Take the time to initialize class variables that are used outside
      // `this.state`.
      this.contacts = {};
      this._userProfile = this.props.mozLoop.userProfile;
    },

    componentDidMount: function() {
      window.addEventListener("LoopStatusChanged", this._onStatusChanged);

      this.refresh(err => {
        if (err) {
          throw err;
        }

        let contactsAPI = this.props.mozLoop.contacts;

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

    componentWillUnmount: function() {
      window.removeEventListener("LoopStatusChanged", this._onStatusChanged);
    },

    /*
     * Filter a user by name, email or phone number.
     * Takes in an input to filter by and returns a filter function which
     * expects a contact.
     *
     * @returns {Function}
     */
    filterContact: function(filter) {
      return function(contact) {
        return getPreferred(contact, "name").toLocaleLowerCase().includes(filter) ||
          getPreferred(contact, "email").value.toLocaleLowerCase().includes(filter) ||
          getPreferred(contact, "tel").value.toLocaleLowerCase().includes(filter);
      };
    },

    /*
     * Takes all contacts, it groups and filters them before rendering.
     */
    _filterContactsList: function() {
      let shownContacts = _.groupBy(this.contacts, function(contact) {
        return contact.blocked ? "blocked" : "available";
      });

      if (this._shouldShowFilter()) {
        let filter = this.state.filter.trim().toLocaleLowerCase();
        let filterFn = this.filterContact(filter);
        if (filter) {
          if (shownContacts.available) {
            shownContacts.available = shownContacts.available.filter(filterFn);
            // Filter can return an empty array.
            if (!shownContacts.available.length) {
              shownContacts.available = null;
            }
          }
          if (shownContacts.blocked) {
            shownContacts.blocked = shownContacts.blocked.filter(filterFn);
            // Filter can return an empty array.
            if (!shownContacts.blocked.length) {
              shownContacts.blocked = null;
            }
          }
        }
      }

      return shownContacts;
    },

    /*
     * Decide to render contacts filter based on the number of contacts.
     *
     * @returns {bool}
     */
    _shouldShowFilter: function() {
      return Object.getOwnPropertyNames(this.contacts).length >=
        MIN_CONTACTS_FOR_FILTERING;
    },

    _onStatusChanged: function() {
      let profile = this.props.mozLoop.userProfile;
      let currUid = this._userProfile ? this._userProfile.uid : null;
      let newUid = profile ? profile.uid : null;
      if (currUid !== newUid) {
        // On profile change (login, logout), reload all contacts.
        this._userProfile = profile;
        // The following will do a forceUpdate() for us.
        this.refresh();
      }
    },

    handleContactAddOrUpdate: function(contact, render = true) {
      let contacts = this.contacts;
      let guid = String(contact._guid);
      contacts[guid] = contact;
      if (render) {
        this.forceUpdate();
      }
    },

    handleContactRemove: function(contact) {
      let contacts = this.contacts;
      let guid = String(contact._guid);
      if (!contacts[guid]) {
        return;
      }
      delete contacts[guid];
      this.forceUpdate();
    },

    handleContactRemoveAll: function() {
      // Do not allow any race conditions when removing all contacts.
      this.contacts = {};
      this.forceUpdate();
    },

    handleImportButtonClick: function() {
      this.setState({ importBusy: true });
      this.props.mozLoop.startImport({
        service: "google"
      }, (err, stats) => {
        this.setState({ importBusy: false });
        if (err) {
          console.error("Contact import error", err);
          this.props.notifications.errorL10n("import_contacts_failure_message");
          return;
        }
        this.props.notifications.successL10n("import_contacts_success_message", {
          num: stats.success,
          total: stats.success
        });
      });
    },

    handleAddContactButtonClick: function() {
      this.props.switchToContactAdd();
    },

    handleContactAction: function(contact, actionName) {
      switch (actionName) {
        case "edit":
          this.props.switchToContactEdit(contact);
          break;
        case "remove":
          this.props.mozLoop.confirm({
            message: mozL10n.get("confirm_delete_contact_alert"),
            okButton: mozL10n.get("confirm_delete_contact_remove_button"),
            cancelButton: mozL10n.get("confirm_delete_contact_cancel_button")
          }, (error, result) => {
            if (error) {
              throw error;
            }

            if (!result) {
              return;
            }

            this.props.mozLoop.contacts.remove(contact._guid, err => {
              if (err) {
                throw err;
              }
            });
          });
          break;
        case "block":
        case "unblock":
          // Invoke the API named like the action.
          this.props.mozLoop.contacts[actionName](contact._guid, err => {
            if (err) {
              throw err;
            }
          });
          break;
        case "video-call":
          if (!contact.blocked) {
            this.props.mozLoop.calls.startDirectCall(contact, CALL_TYPES.AUDIO_VIDEO);
            this.closeWindow();
          }
          break;
        case "audio-call":
          if (!contact.blocked) {
            this.props.mozLoop.calls.startDirectCall(contact, CALL_TYPES.AUDIO_ONLY);
            this.closeWindow();
          }
          break;
        default:
          console.error("Unrecognized action: " + actionName);
          break;
      }
    },

    handleUseGravatar: function() {
      // We got permission to use Gravatar icons now, so we need to redraw the
      // list entirely to show them.
      this.refresh();
    },

    /*
     * Callback triggered when clicking the `X` from the contacts filter.
     * Clears the search query.
     */
    _handleFilterClear: function() {
      this.setState({
        filter: ""
      });
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

    _renderFilterClearButton: function() {
      if (this.state.filter) {
        return (
          React.createElement("button", {className: "clear-search", 
                  onClick: this._handleFilterClear})
        );
      }

      return null;
    },

    _renderContactsFilter: function() {
      if (this._shouldShowFilter()) {
        return (
          React.createElement("div", {className: "contact-filter-container"}, 
            React.createElement("input", {className: "contact-filter", 
                   placeholder: mozL10n.get("contacts_search_placesholder2"), 
                   valueLink: this.linkState("filter")}), 
            this._renderFilterClearButton()
          )
        );
      }

      return null;
    },

    _renderContactsList: function() {
      let cx = React.addons.classSet;
      let shownContacts = this._filterContactsList();
      let viewForItem = item => {
        return (
          React.createElement(ContactDetail, {contact: item, 
                         handleContactAction: this.handleContactAction, 
                         key: item._guid})
        );
      };

      // If no contacts to show and filter is set, then none match the search.
      if (!shownContacts.available && !shownContacts.blocked &&
          this.state.filter) {
        return (
          React.createElement("div", {className: "contact-search-list-empty"}, 
            React.createElement("p", {className: "panel-text-medium"}, 
              mozL10n.get("contacts_no_search_results")
            )
          )
        );
      }

      // If no contacts to show and filter is not set, we don't have contacts.
      if (!shownContacts.available && !shownContacts.blocked &&
          !this.state.filter) {
        return (
            React.createElement("div", {className: "contact-list-empty-container"}, 
              this._renderGravatarPromoMessage(), 
              React.createElement("div", {className: "contact-list-empty"}, 
                React.createElement("p", {className: "panel-text-large"}, 
                  mozL10n.get("no_contacts_message_heading2")
                ), 
                React.createElement("p", {className: "panel-text-medium"}, 
                  mozL10n.get("no_contacts_import_or_add2")
                )
              )
            )
        );
      }

      return (
          React.createElement("div", {className: "contact-list-container"}, 
            !this.state.filter ? React.createElement("div", {className: "contact-list-title"}, 
              mozL10n.get("contact_list_title")
            ) : null, 
            React.createElement("div", {className: "contact-list-wrapper"}, 
              this._renderGravatarPromoMessage(), 
              React.createElement("ul", {className: "contact-list"}, 
                shownContacts.available ?
                    shownContacts.available.sort(this.sortContacts).map(viewForItem) :
                    null, 
                shownContacts.blocked && shownContacts.blocked.length > 0 ?
                    React.createElement("div", {className: "contact-separator"}, mozL10n.get("contacts_blocked_contacts")) :
                    null, 
                shownContacts.blocked ?
                    shownContacts.blocked.sort(this.sortContacts).map(viewForItem) :
                    null
              )
            )
          )
      );
    },

    _renderAddContactButtons: function() {
      let cx = React.addons.classSet;

      if (this.state.filter) {
        return null;
      }

      return (
        React.createElement(ButtonGroup, {additionalClass: "contact-controls"}, 
          React.createElement(Button, {additionalClass: "secondary", 
            caption: this.state.importBusy ? mozL10n.get("importing_contacts_progress_button") :
                                             mozL10n.get("import_contacts_button3"), 
              disabled: this.state.importBusy, 
              onClick: this.handleImportButtonClick}, 
              React.createElement("div", {className: cx({"contact-import-spinner": true,
                                 spinner: true,
              busy: this.state.importBusy})})
          ), 
          React.createElement(Button, {additionalClass: "primary", 
            caption: mozL10n.get("new_contact_button2"), 
            onClick: this.handleAddContactButtonClick})
        )
      );
    },

    _renderGravatarPromoMessage: function() {
      if (this.state.filter) {
        return null;
      }

      return (
        React.createElement(GravatarPromo, {handleUse: this.handleUseGravatar})
      );
    },

    render: function() {
      return (
        React.createElement("div", {className: "contacts-container"}, 
          this._renderContactsFilter(), 
          this._renderContactsList(), 
          this._renderAddContactButtons()
        )
      );
    }
  });

  const ContactsControllerView = React.createClass({displayName: "ContactsControllerView",
    propTypes: {
      initialSelectedTabComponent: React.PropTypes.string,
      mozLoop: React.PropTypes.object.isRequired,
      notifications: React.PropTypes.object.isRequired
    },

    getInitialState: function() {
      return {
        currentComponent: this.props.initialSelectedTabComponent || "contactList",
        contactFormData: {}
      };
    },

    /* XXX We should have success/Fail callbacks that the children call instead of this
    * Children should not have knowledge of other views
    * However, this is being implemented in this way so the view can be directed appropriately
    * without making it too complex
    */
    switchComponentView: function(componentName) {
      return function() {
        this.setState({currentComponent: componentName});
      }.bind(this);
    },

    handleAddEditContact: function(componentName) {
      return function(contactFormData) {
        this.setState({
          contactFormData: contactFormData || {},
          currentComponent: componentName
        });
      }.bind(this);
    },

    /* XXX Consider whether linkedStated makes sense for this */
    render: function() {
      switch(this.state.currentComponent) {
        case "contactAdd":
          return (
            React.createElement(ContactDetailsForm, {
              contactFormData: this.state.contactFormData, 
              mode: "add", 
              mozLoop: this.props.mozLoop, 
              ref: "contacts_add", 
              switchToInitialView: this.switchComponentView("contactList")})
          );
        case "contactEdit":
          return (
            React.createElement(ContactDetailsForm, {
              contactFormData: this.state.contactFormData, 
              mode: "edit", 
              mozLoop: this.props.mozLoop, 
              ref: "contacts_edit", 
              switchToInitialView: this.switchComponentView("contactList")})
          );
        case "contactList":
        default:
          return (
            React.createElement(ContactsList, {
              mozLoop: this.props.mozLoop, 
              notifications: this.props.notifications, 
              ref: "contacts_list", 
              switchToContactAdd: this.handleAddEditContact("contactAdd"), 
              switchToContactEdit: this.handleAddEditContact("contactEdit")})
          );
      }
    }
  });

  const ContactDetailsForm = React.createClass({displayName: "ContactDetailsForm",
    mixins: [React.addons.LinkedStateMixin],

    propTypes: {
      contactFormData: React.PropTypes.object.isRequired,
      mode: React.PropTypes.string,
      mozLoop: React.PropTypes.object.isRequired,
      switchToInitialView: React.PropTypes.func.isRequired
    },

    componentDidMount: function() {
      this.initForm(this.props.contactFormData);
    },

    getInitialState: function() {
      return {
        contact: null,
        pristine: true,
        name: "",
        email: "",
        tel: ""
      };
    },

    initForm: function(contact) {
      let state = this.getInitialState();
      // Test for an empty contact object
      if (_.keys(contact).length > 0) {
        state.contact = contact;
        state.name = contact.name[0];
        state.email = getPreferred(contact, "email").value;
        state.tel = getPreferred(contact, "tel").value;
      }

      this.setState(state);
    },

    handleAcceptButtonClick: function() {
      // Allow validity error indicators to be displayed.
      this.setState({
        pristine: false
      });

      let emailInput = this.refs.email.getDOMNode();
      let telInput = this.refs.tel.getDOMNode();
      if (!this.refs.name.getDOMNode().checkValidity() ||
          ((emailInput.required || emailInput.value) && !emailInput.checkValidity()) ||
          ((telInput.required || telInput.value) && !telInput.checkValidity())) {
        return;
      }

      let contactsAPI = this.props.mozLoop.contacts;
      switch (this.props.mode) {
        case "edit":
          this.state.contact.name[0] = this.state.name.trim();
          setPreferred(this.state.contact, "email", this.state.email.trim());
          setPreferred(this.state.contact, "tel", this.state.tel.trim());
          contactsAPI.update(this.state.contact, err => {
            if (err) {
              throw err;
            }
          });
          this.setState({
            contact: null
          });
          break;
        case "add":
          var contact = {
            id: this.props.mozLoop.generateUUID(),
            name: [this.state.name.trim()],
            email: [{
              pref: true,
              type: ["home"],
              value: this.state.email.trim()
            }],
            category: ["local"]
          };
          var tel = this.state.tel.trim();
          if (tel) {
            contact.tel = [{
              pref: true,
              type: ["fxos"],
              value: tel
            }];
          }
          contactsAPI.add(contact, err => {
            if (err) {
              throw err;
            }
          });
          break;
      }

      this.props.switchToInitialView();
    },

    handleCancelButtonClick: function() {
      this.props.switchToInitialView();
    },

    render: function() {
      let cx = React.addons.classSet;
      let phoneOrEmailRequired = !this.state.email && !this.state.tel;
      let contactFormMode = "contact-form-mode-" + this.props.mode;
      let contentAreaClassesLiteral = {
        "content-area": true,
        "contact-form": true
      };
      contentAreaClassesLiteral[contactFormMode] = true;
      let contentAreaClasses = cx(contentAreaClassesLiteral);

      return (
        React.createElement("div", {className: contentAreaClasses}, 
          React.createElement("header", null, this.props.mode === "add"
                   ? mozL10n.get("add_contact_title")
                   : mozL10n.get("edit_contact_title")), 
          React.createElement("div", {className: cx({"form-content-container": true})}, 
            React.createElement("input", {className: cx({pristine: this.state.pristine}), 
                   pattern: "\\s*\\S.*", 
                   placeholder: mozL10n.get("contact_form_name_placeholder"), 
                   ref: "name", 
                   required: true, 
                   type: "text", 
                   valueLink: this.linkState("name")}), 
            React.createElement("input", {className: cx({pristine: this.state.pristine}), 
                   placeholder: mozL10n.get("contact_form_email_placeholder"), 
                   ref: "email", 
                   required: phoneOrEmailRequired, 
                   type: "email", 
                   valueLink: this.linkState("email")}), 
            React.createElement("input", {className: cx({pristine: this.state.pristine}), 
                   placeholder: mozL10n.get("contact_form_fxos_phone_placeholder"), 
                   ref: "tel", 
                   required: phoneOrEmailRequired, 
                   type: "tel", 
                   valueLink: this.linkState("tel")})
          ), 
          React.createElement(ButtonGroup, null, 
            React.createElement(Button, {additionalClass: "button-cancel", 
                    caption: mozL10n.get("cancel_button"), 
                    onClick: this.handleCancelButtonClick}), 
            React.createElement(Button, {additionalClass: "button-accept", 
                    caption: this.props.mode === "add"
                             ? mozL10n.get("add_contact_button")
                             : mozL10n.get("edit_contact_done_button"), 
                    onClick: this.handleAcceptButtonClick})
          )
        )
      );
    }
  });

  return {
    ContactDropdown: ContactDropdown,
    ContactsList: ContactsList,
    ContactDetail: ContactDetail,
    ContactDetailsForm: ContactDetailsForm,
    ContactsControllerView: ContactsControllerView,
    _getPreferred: getPreferred,
    _setPreferred: setPreferred
  };
})(_, document.mozL10n);
