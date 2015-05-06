/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");

this.EXPORTED_SYMBOLS = ["GoogleImporter"];

let log = Log.repository.getLogger("Loop.Importer.Google");
log.level = Log.Level.Debug;
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

/**
 * Helper function that reads and maps the respective node value from specific
 * XML DOMNodes to fields on a `target` object.
 * Example: the value for field 'fullName' can be read from the XML DOMNode
 *          'name', so that's the mapping we need to make; get the nodeValue of
 *          the node called 'name' and tack it to the target objects' 'fullName'
 *          property.
 *
 * @param {Map}        fieldMap    Map object containing the field name -> node
 *                                 name mapping
 * @param {XMLDOMNode} node        DOM node to fetch the values from for each field
 * @param {String}     ns XML      namespace for the DOM nodes to retrieve. Optional.
 * @param {Object}     target      Object to store the values found. Optional.
 *                                 Defaults to a new object.
 * @param {Boolean}    wrapInArray Indicates whether to map the field values in
 *                                 an Array. Optional. Defaults to `false`.
 * @returns The `target` object with the node values mapped to the appropriate fields.
 */
const extractFieldsFromNode = function(fieldMap, node, ns = null, target = {}, wrapInArray = false) {
  for (let [field, nodeName] of fieldMap) {
    let nodeList = ns ? node.getElementsByTagNameNS(ns, nodeName) :
                        node.getElementsByTagName(nodeName);
    if (nodeList.length) {
      if (!nodeList[0].firstChild) {
        continue;
      }
      let value = nodeList[0].textContent;
      target[field] = wrapInArray ? [value] : value;
    }
  }
  return target;
};

/**
 * Helper function that reads the type of (email-)address or phone number from an
 * XMLDOMNode.
 *
 * @param {XMLDOMNode} node
 * @returns String that depicts the type of field value.
 */
const getFieldType = function(node) {
  if (node.hasAttribute("rel")) {
    let rel = node.getAttribute("rel");
    // The 'rel' attribute is formatted like: http://schemas.google.com/g/2005#work.
    return rel.substr(rel.lastIndexOf("#") + 1);
  }
  if (node.hasAttribute("label")) {
    return node.getAttribute("label");
  }
  return "other";
};

/**
 * Fetch the preferred entry of a contact. Returns the first entry when no
 * preferred flag is set.
 *
 * @param {Object} contact The contact object to check for preferred entries
 * @param {String} which   Type of entry to check. Optional, defaults to 'email'
 * @throws An Error when no (preferred) entries are listed for this contact.
 */
const getPreferred = function(contact, which = "email") {
  if (!(which in contact) || !contact[which].length) {
    throw new Error("No " + which + " entry available.");
  }
  let preferred = contact[which][0];
  contact[which].some(function(entry) {
    if (entry.pref) {
      preferred = entry;
      return true;
    }
    return false;
  });
  return preferred;
};

/**
 * Fetch an auth token (clientID or client secret), which may be overridden by
 * a pref if it's set.
 *
 * @param {String}  paramValue Initial, default, value of the parameter
 * @param {String}  prefName   Fully qualified name of the pref to check for
 * @param {Boolean} encode     Whether to URLEncode the param string
 */
const getUrlParam = function(paramValue, prefName, encode = true) {
  if (Services.prefs.getPrefType(prefName))
    paramValue = Services.prefs.getCharPref(prefName);
  paramValue = Services.urlFormatter.formatURL(paramValue);

  return encode ? encodeURIComponent(paramValue) : paramValue;
};

let gAuthWindow, gProfileId;
const kAuthWindowSize = {
  width: 420,
  height: 460
};
const kContactsMaxResults = 10000000;
const kContactsChunkSize = 100;
const kTitlebarPollTimeout = 200;
const kNS_GD = "http://schemas.google.com/g/2005";

/**
 * GoogleImporter class.
 *
 * Main entrypoint is the `startImport` method which calls several tasks necessary
 * to import contacts from Google.
 * Authentication is performed using an OAuth strategy which is loaded in a popup
 * window.
 */
this.GoogleImporter = function() {};

this.GoogleImporter.prototype = {
  /**
   * Start the import process of contacts from the Google service, using its Contacts
   * API - https://developers.google.com/google-apps/contacts/v3/.
   * The import consists of four tasks:
   * 1. Get the authentication code which can be used to retrieve an OAuth token
   *    pair. This is the bulk of the authentication flow that will be handled in
   *    a popup window by Google. The user will need to login to the Google service
   *    with his or her account and grant permission to our app to manage their
   *    contacts.
   * 2. Get the tokenset from the Google service, using the authentication code
   *    that was retrieved in task 1.
   * 3. Fetch all the contacts from the Google service, using the OAuth tokenset
   *    that was retrieved in task 2.
   * 4. Process the contacts, map them to the MozContact format and store each
   *    contact in the database, if it doesn't exist yet.
   *
   * @param {Object}       options   Options to control the behavior of the import.
   *                                 Not used by this importer class.
   * @param {Function}     callback  Function to invoke when the import process
   *                                 is done or when an error occurs that halts
   *                                 the import process. The first argument passed
   *                                 in an Error object or `null` and the second
   *                                 argument is an object with import statistics.
   * @param {LoopContacts} db        Instance of the LoopContacts database object,
   *                                 which will store the newly found contacts
   * @param {nsIDomWindow} windowRef Reference to the ChromeWindow the import is
   *                                 invoked from. It will be used to be able to
   *                                 open a window for the OAuth process with chrome
   *                                 privileges.
   */
  startImport: function(options, callback, db, windowRef) {
    Task.spawn(function* () {
      let code = yield this._promiseAuthCode(windowRef);
      let tokenSet = yield this._promiseTokenSet(code);
      let contactEntries = yield this._getContactEntries(tokenSet);
      let {total, success, ids} = yield this._processContacts(contactEntries, db, tokenSet);
      yield this._purgeContacts(ids, db);

      return {
        total: total,
        success: success
      };
    }.bind(this)).then(stats => callback(null, stats),
                       error => callback(error))
                 .then(null, ex => log.error(ex.fileName + ":" + ex.lineNumber + ": " + ex.message));
  },

  /**
   * Task that yields an authentication code that is returned after the user signs
   * in to the Google service. This code can be used by this class to retrieve an
   * OAuth tokenset.
   *
   * @param {nsIDOMWindow} windowRef Reference to the ChromeWindow the import is
   *                                 invoked from. It will be used to be able to
   *                                 open a window for the OAuth process with chrome
   *                                 privileges.
   * @throws An `Error` object when authentication fails, or the authentication
   *         code as a String.
   */
  _promiseAuthCode: Task.async(function* (windowRef) {
    // Close a window that got lost in a previous login attempt.
    if (gAuthWindow && !gAuthWindow.closed) {
      gAuthWindow.close();
      gAuthWindow = null;
    }

    let url = getUrlParam("https://accounts.google.com/o/oauth2/",
                          "loop.oauth.google.URL", false) +
              "auth?response_type=code&client_id=" +
              getUrlParam("%GOOGLE_OAUTH_API_CLIENTID%", "loop.oauth.google.clientIdOverride");
    for (let param of ["redirect_uri", "scope"]) {
      url += "&" + param + "=" + encodeURIComponent(
             Services.prefs.getCharPref("loop.oauth.google." + param));
    }
    const features = "centerscreen,resizable=yes,toolbar=no,menubar=no,status=no,directories=no," +
                     "width=" + kAuthWindowSize.width + ",height=" + kAuthWindowSize.height;
    gAuthWindow = windowRef.openDialog(windowRef.getBrowserURL(), "_blank", features, url);
    gAuthWindow.focus();

    let code;

    function promiseTimeOut() {
      return new Promise(resolve => {
        setTimeout(resolve, kTitlebarPollTimeout);
      });
    }

    // The following loops runs as long as the OAuth windows' titlebar doesn't
    // yield a response from the Google service. If an error occurs, the loop
    // will terminate early.
    while (!code) {
      if (!gAuthWindow || gAuthWindow.closed) {
        throw new Error("Popup window was closed before authentication succeeded");
      }

      let matches = gAuthWindow.document.title.match(/(error|code)=([^\s]+)/);
      if (matches && matches.length) {
        let [, type, message] = matches;
        gAuthWindow.close();
        gAuthWindow = null;
        if (type == "error") {
          throw new Error("Google authentication failed with error: " + message.trim());
        } else if (type == "code") {
          code = message.trim();
        } else {
          throw new Error("Unknown response from Google");
        }
      } else {
        yield promiseTimeOut();
      }
    }

    return code;
  }),

  /**
   * Fetch an OAuth tokenset, that will be used to authenticate Google API calls,
   * using the authentication token retrieved in `_promiseAuthCode`.
   *
   * @param {String} code The authentication code.
   * @returns an `Error` object upon failure or an object containing OAuth tokens.
   */
  _promiseTokenSet: function(code) {
    return new Promise(function(resolve, reject) {
      let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                      .createInstance(Ci.nsIXMLHttpRequest);

      request.open("POST", getUrlParam("https://accounts.google.com/o/oauth2/",
                                       "loop.oauth.google.URL",
                                       false) + "token");

      request.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");

      request.onload = function() {
        if (request.status < 400) {
          let tokenSet = JSON.parse(request.responseText);
          tokenSet.date = Date.now();
          resolve(tokenSet);
        } else {
          reject(new Error(request.status + " " + request.statusText));
        }
      };

      request.onerror = function(error) {
        reject(error);
      };

      let body = "grant_type=authorization_code&code=" + encodeURIComponent(code) +
                 "&client_id=" + getUrlParam("%GOOGLE_OAUTH_API_CLIENTID%",
                                             "loop.oauth.google.clientIdOverride") +
                 "&client_secret=" + getUrlParam("%GOOGLE_OAUTH_API_KEY%",
                                                 "loop.oauth.google.clientSecretOverride") +
                 "&redirect_uri=" + encodeURIComponent(Services.prefs.getCharPref(
                                                       "loop.oauth.google.redirect_uri"));

      request.send(body);
    });
  },

  _promiseRequestXML: function(URL, tokenSet) {
    return new Promise((resolve, reject) => {
      let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                      .createInstance(Ci.nsIXMLHttpRequest);

      request.open("GET", URL);

      request.setRequestHeader("Content-Type", "application/xml; charset=utf-8");
      request.setRequestHeader("GData-Version", "3.0");
      request.setRequestHeader("Authorization", "Bearer " + tokenSet.access_token);

      request.onload = function() {
        if (request.status < 400) {
          let doc = request.responseXML;
          // First get the profile id, which is present in each XML request.
          let currNode = doc.documentElement.firstChild;
          while (currNode) {
            if (currNode.nodeType == 1 && currNode.localName == "id") {
              gProfileId = currNode.textContent;
              break;
            }
            currNode = currNode.nextSibling;
          }

          resolve(doc);
        } else {
          reject(new Error(request.status + " " + request.statusText));
        }
      };

      request.onerror = function(error) {
        reject(error);
      };

      request.send();
    });
  },

  /**
   * Fetches all the contacts in a users' address book.
   *
   * @see https://developers.google.com/google-apps/contacts/v3/#retrieving_all_contacts
   *
   * @param {Object} tokenSet OAuth tokenset used to authenticate the request
   * @returns An `Error` object upon failure or an Array of contact XML nodes.
   */
  _getContactEntries: Task.async(function* (tokenSet) {
    let URL = getUrlParam("https://www.google.com/m8/feeds/contacts/default/full",
                          "loop.oauth.google.getContactsURL",
                          false) + "?max-results=" + kContactsMaxResults;
    let xmlDoc = yield this._promiseRequestXML(URL, tokenSet);
    // Then kick of the importing of contact entries.
    return Array.prototype.slice.call(xmlDoc.querySelectorAll("entry"));
  }),

  /**
   * Fetches the default group from a users' address book, called 'Contacts'.
   *
   * @see https://developers.google.com/google-apps/contacts/v3/#retrieving_all_contact_groups
   *
   * @param {Object} tokenSet OAuth tokenset used to authenticate the request
   * @returns An `Error` object upon failure or the String group ID.
   */
  _getContactsGroupId: Task.async(function* (tokenSet) {
    let URL = getUrlParam("https://www.google.com/m8/feeds/groups/default/full",
                          "loop.oauth.google.getGroupsURL",
                          false) + "?max-results=" + kContactsMaxResults;
    let xmlDoc = yield this._promiseRequestXML(URL, tokenSet);
    let contactsEntry = xmlDoc.querySelector("systemGroup[id=\"Contacts\"]");
    if (!contactsEntry) {
      throw new Error("Contacts group not present");
    }
    // Select the actual <entry> node, which is the parent of the <systemGroup>
    // node we just selected.
    contactsEntry = contactsEntry.parentNode;
    return contactsEntry.getElementsByTagName("id")[0].textContent;
  }),

  /**
   * Process the contact XML nodes that Google provides, convert them to the MozContact
   * format, check if the contact already exists in the database and when it doesn't,
   * store it permanently.
   * During this process statistics are collected about the amount of successful
   * imports. The consumer of this class may use these statistics to inform the
   * user.
   * Note: only contacts that are part of the 'Contacts' system group will be
   *       imported.
   *
   * @param {Array}        contactEntries List of XML DOMNodes contact entries.
   * @param {LoopContacts} db             Instance of the LoopContacts database
   *                                      object, which will store the newly found
   *                                      contacts.
   * @param {Object}       tokenSet       OAuth tokenset used to authenticate a
   *                                      request
   * @returns An `Error` object upon failure or an Object with statistics in the
   *          following format: `{ total: 25, success: 13, ids: {} }`.
   */
  _processContacts: Task.async(function* (contactEntries, db, tokenSet) {
    let stats = {
      total: contactEntries.length,
      success: 0,
      ids: {}
    };

    // Contacts that are _not_ part of the 'Contacts' group will be ignored.
    let contactsGroupId = yield this._getContactsGroupId(tokenSet);

    for (let entry of contactEntries) {
      let contact = this._processContactFields(entry);

      stats.ids[contact.id] = 1;
      let existing = yield db.promise("getByServiceId", contact.id);
      if (existing) {
        yield db.promise("remove", existing._guid);
      }

      // After contact removal, check if the entry is part of the correct group.
      if (!entry.querySelector("groupMembershipInfo[deleted=\"false\"][href=\"" +
                               contactsGroupId + "\"]")) {
        continue;
      }

      // If the contact contains neither email nor phone number, then it is not
      // useful in the Loop address book: do not add.
      if (!("email" in contact) && !("tel" in contact)) {
        continue;
      }

      yield db.promise("add", contact);
      stats.success++;
    }

    return stats;
  }),

  /**
   * Parse an XML node to map the appropriate data to MozContact field equivalents.
   *
   * @param {XMLDOMNode} entry The contact XML node in Google format to process.
   * @returns `null` if the contact entry appears to be invalid or an Object containing
   *          all the contact data found in the XML.
   */
  _processContactFields: function(entry) {
    // Basic fields in the main 'atom' namespace.
    let contact = extractFieldsFromNode(new Map([
      ["id", "id"],
      // published: n/a
      ["updated", "updated"]
      // bday: n/a
    ]), entry);

    // Fields that need to wrapped in an Array.
    extractFieldsFromNode(new Map([
      ["name", "fullName"],
      ["givenName", "givenName"],
      ["familyName", "familyName"],
      ["additionalName", "additionalName"]
    ]), entry, kNS_GD, contact, true);

    // The 'note' field needs to wrapped in an array, but its source node is not
    // namespaced.
    extractFieldsFromNode(new Map([
      ["note", "content"]
    ]), entry, null, contact, true);

    // Process physical, earthly addresses.
    let addressNodes = entry.getElementsByTagNameNS(kNS_GD, "structuredPostalAddress");
    if (addressNodes.length) {
      contact.adr = [];
      for (let [,addressNode] of Iterator(addressNodes)) {
        let adr = extractFieldsFromNode(new Map([
          ["countryName", "country"],
          ["locality", "city"],
          ["postalCode", "postcode"],
          ["region", "region"],
          ["streetAddress", "street"]
        ]), addressNode, kNS_GD);
        if (Object.keys(adr).length) {
          adr.pref = (addressNode.getAttribute("primary") == "true");
          adr.type = [getFieldType(addressNode)];
          contact.adr.push(adr);
        }
      }
    }

    // Process email addresses.
    let emailNodes = entry.getElementsByTagNameNS(kNS_GD, "email");
    if (emailNodes.length) {
      contact.email = [];
      for (let [,emailNode] of Iterator(emailNodes)) {
        contact.email.push({
          pref: (emailNode.getAttribute("primary") == "true"),
          type: [getFieldType(emailNode)],
          value: emailNode.getAttribute("address")
        });
      }
    }

    // Process telephone numbers.
    let phoneNodes = entry.getElementsByTagNameNS(kNS_GD, "phoneNumber");
    if (phoneNodes.length) {
      contact.tel = [];
      for (let [,phoneNode] of Iterator(phoneNodes)) {
        let phoneNumber = phoneNode.hasAttribute("uri") ?
          phoneNode.getAttribute("uri").replace("tel:", "") :
          phoneNode.textContent;
        contact.tel.push({
          pref: (phoneNode.getAttribute("primary") == "true"),
          type: [getFieldType(phoneNode)],
          value: phoneNumber
        });
      }
    }

    let orgNodes = entry.getElementsByTagNameNS(kNS_GD, "organization");
    if (orgNodes.length) {
      contact.org = [];
      contact.jobTitle = [];
      for (let [,orgNode] of Iterator(orgNodes)) {
        let orgElement = orgNode.getElementsByTagNameNS(kNS_GD, "orgName")[0];
        let titleElement = orgNode.getElementsByTagNameNS(kNS_GD, "orgTitle")[0];
        contact.org.push(orgElement ? orgElement.textContent : "");
        contact.jobTitle.push(titleElement ? titleElement.textContent : "");
      }
    }

    contact.category = ["google"];

    // Basic sanity checking: make sure the name field isn't empty
    if (!("name" in contact) || contact.name[0].length == 0) {
      if (("familyName" in contact) && ("givenName" in contact)) {
        // First, try to synthesize a full name from the name fields.
        // Ordering is culturally sensitive, but we don't have
        // cultural origin information available here. The best we
        // can really do is "family, given additional"
        contact.name = [contact.familyName[0] + ", " + contact.givenName[0]];
        if (("additionalName" in contact)) {
          contact.name[0] += " " + contact.additionalName[0];
        }
      } else {
        let profileTitle = extractFieldsFromNode(new Map([["title", "title"]]), entry);
        if (("title" in profileTitle)) {
          contact.name = [profileTitle.title];
        } else if ("familyName" in contact) {
          contact.name = [contact.familyName[0]];
        } else if ("givenName" in contact) {
          contact.name = [contact.givenName[0]];
        } else if ("org" in contact) {
          contact.name = [contact.org[0]];
        } else {
          let email;
          try {
            email = getPreferred(contact);
          } catch (ex) {}
          if (email) {
            contact.name = [email.value];
          } else {
            let tel;
            try {
              tel = getPreferred(contact, "tel");
            } catch (ex) {}
            if (tel) {
              contact.name = [tel.value];
            }
          }
        }
      }
    }

    return contact;
  },

  /**
   * Remove all contacts from the database that are not present anymore in the
   * remote data-source.
   *
   * @param {Object}       ids Map of IDs collected earlier of all the contacts
   *                           that are available on the remote data-source
   * @param {LoopContacts} db  Instance of the LoopContacts database object, which
   *                           will store the newly found contacts
   */
  _purgeContacts: Task.async(function* (ids, db) {
    let contacts = yield db.promise("getAll");
    let profileId = "https://www.google.com/m8/feeds/contacts/" + encodeURIComponent(gProfileId);
    let processed = 0;

    function promiseSkipABeat() {
      return new Promise(resolve => Services.tm.currentThread.dispatch(resolve,
                                      Ci.nsIThread.DISPATCH_NORMAL));
    }

    for (let [guid, contact] of Iterator(contacts)) {
      if (++processed % kContactsChunkSize === 0) {
        // Skip a beat every time we processed a chunk.
        yield promiseSkipABeat;
      }

      if (contact.id.indexOf(profileId) >= 0 && !ids[contact.id]) {
        yield db.promise("remove", guid);
      }
    }
  })
};
