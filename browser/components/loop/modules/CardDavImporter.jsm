/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");

this.EXPORTED_SYMBOLS = ["CardDavImporter"];

let log = Log.repository.getLogger("Loop.Importer.CardDAV");
log.level = Log.Level.Debug;
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

const DEPTH_RESOURCE_ONLY = "0";
const DEPTH_RESOURCE_AND_CHILDREN = "1";
const DEPTH_RESOURCE_AND_ALL_DESCENDENTS = "infinity";

this.CardDavImporter = function() {
};

/**
 * CardDAV Address Book importer for Loop.
 *
 * The model for address book importers is to have a single public method,
 * "startImport." When the import is done (or upon a fatal error), the
 * caller's callback method is called.
 *
 * The current model for this importer is based on the subset of CardDAV
 * implemented by Google. In theory, it should work with other CardDAV
 * sources, but it has only been tested against Google at the moment.
 *
 * At the moment, this importer assumes that no local changes will be made
 * to data retreived from a remote source: when performing a re-import,
 * any records that have been previously imported will be completely
 * removed and replaced with the data received from the CardDAV server.
 * Witout this behavior, it would be impossible for users to take any
 * actions to remove fields that are no longer valid.
 */

this.CardDavImporter.prototype = {
  /**
   * Begin import of an address book from a CardDAV server.
   *
   * @param {Object}   options  Information needed to perform the address
   *                            book import. The following fields are currently
   *                            defined:
   *                              - "host": CardDAV server base address
   *                                (e.g., "google.com")
   *                              - "auth": Authentication mechanism to use.
   *                                Currently, only "basic" is implemented.
   *                              - "user": Username to use for basic auth
   *                              - "password": Password to use for basic auth
   * @param {Function} callback Callback function that will be invoked once the
   *                            import operation is complete. The first argument
   *                            passed to the callback will be an 'Error' object
   *                            or 'null'. If the import operation was
   *                            successful, then the second parameter will be a
   *                            count of the number of contacts that were
   *                            successfully imported.
   * @param {Object}   db       Database to add imported contacts into.
   *                            Nominally, this is the LoopContacts API. In
   *                            practice, anything with the same interface
   *                            should work here.
   */

  startImport: function(options, callback, db) {
    let auth;
    if (!("auth" in options)) {
      callback(new Error("No authentication specified"));
      return;
    }

    if (options.auth === "basic") {
      if (!("user" in options) || !("password" in options)) {
        callback(new Error("Missing user or password for basic authentication"));
        return;
      }
      auth = { method: "basic",
               user: options.user,
               password: options.password };
    } else {
      callback(new Error("Unknown authentication method"));
      return;
    }

    if (!("host" in options)){
      callback(new Error("Missing host for CardDav import"));
      return;
    }
    let host = options.host;

    Task.spawn(function* () {
      log.info("Starting CardDAV import from " + host);
      let baseURL = "https://" + host;
      let startURL = baseURL + "/.well-known/carddav";
      let abookURL;

      // Get list of contact URLs
      let body = "<d:propfind xmlns:d='DAV:'><d:prop><d:getetag />" +
                 "</d:prop></d:propfind>";
      let abook = yield this._davPromise("PROPFIND", startURL, auth,
                                         DEPTH_RESOURCE_AND_CHILDREN, body);

      // Build multiget REPORT body from URLs in PROPFIND result
      let contactElements = abook.responseXML.
                            getElementsByTagNameNS("DAV:", "href");

      body = "<c:addressbook-multiget xmlns:d='DAV:' " +
             "xmlns:c='urn:ietf:params:xml:ns:carddav'>" +
             "<d:prop><d:getetag /> <c:address-data /></d:prop>\n";

      for (let element of contactElements) {
        let href = element.textContent;
        if (href.substr(-1) == "/") {
          abookURL = baseURL + href;
        } else {
          body += "<d:href>" + href + "</d:href>\n";
        }
      }
      body += "</c:addressbook-multiget>";

      // Retreive contact URL contents
      let allEntries = yield this._davPromise("REPORT", abookURL, auth,
                                              DEPTH_RESOURCE_AND_CHILDREN,
                                              body);

      // Parse multiget entites and add to DB
      let addressData = allEntries.responseXML.getElementsByTagNameNS(
        "urn:ietf:params:xml:ns:carddav", "address-data");

      log.info("Retreived " + addressData.length + " contacts from " +
                   host + "; importing into database");

      let importCount = 0;
      for (let i = 0; i < addressData.length; i++) {
        let vcard = addressData.item(i).textContent;
        let contact = this._convertVcard(vcard);
        contact.id += "@" + host;
        contact.category = ["carddav@" + host];

        let existing = yield this._dbPromise(db, "getByServiceId", contact.id);
        if (existing) {
          yield this._dbPromise(db, "remove", existing._guid);
        }

        // If the contact contains neither email nor phone number, then it
        // is not useful in the Loop address book: do not add.
        if (!("tel" in contact) && !("email" in contact)) {
          continue;
        }

        yield this._dbPromise(db, "add", contact);
        importCount++;
      }

      return importCount;
    }.bind(this)).then(
      (result) => {
        log.info("Import complete: " + result + " contacts imported.");
        callback(null, result);
      },
      (error) => {
        log.error("Aborting import: " + error.fileName + ":" +
                      error.lineNumber + ": " + error.message);
        callback(error);
    }).then(null,
      (error) => {
        log.error("Error in callback: " + error.fileName +
                      ":" + error.lineNumber + ": " + error.message);
      callback(error);
    }).then(null,
      (error) => {
        log.error("Error calling failure callback, giving up: " +
                      error.fileName + ":" + error.lineNumber + ": " +
                      error.message);
    });
  },

  /**
   * Wrap a LoopContacts-style operation in a promise. The operation is run
   * immediately, and a corresponding Promise is returned. Error callbacks
   * cause the promise to be rejected, and success cause it to be resolved.
   *
   * @param {Object} db     Object the operation is to be performed on
   * @param {String} method Name of operation being wrapped
   * @param {Object} param  Parameter to be passed to the operation
   *
   * @return {Object} Promise corresponding to the result of the operation.
   */

  _dbPromise: function(db, method, param) {
    return new Promise((resolve, reject) => {
      db[method](param, (error, result) => {
        if (error) {
          reject(error);
        } else {
          resolve(result);
        }
      });
    });
  },

  /**
   * Converts a contact in VCard format (see RFC 6350) to the format used
   * by the LoopContacts class.
   *
   * @param {String} vcard  The contact to convert, in vcard format
   * @return {Object}  a LoopContacts-style contact object containing
   *                   the relevant fields from the vcard.
   */

  _convertVcard: function(vcard) {
    let contact = {};
    let nickname;
    vcard.split(/[\r\n]+(?! )/).forEach(
      function (contentline) {
        contentline = contentline.replace(/[\r\n]+ /g, "");
        let match = /^(.*?[^\\]):(.*)$/.exec(contentline);
        if (match) {
          let nameparam = match[1];
          let value = match[2];

          // Poor-man's unescaping
          value = value.replace(/\\:/g, ":");
          value = value.replace(/\\,/g, ",");
          value = value.replace(/\\n/gi, "\n");
          value = value.replace(/\\\\/g, "\\");

          let param = nameparam.split(/;/);
          let name = param[0];
          let pref = false;
          let type = [];

          for (let i = 1; i < param.length; i++) {
            if (/^PREF/.exec(param[i]) || /^TYPE=PREF/.exec(param[i])) {
              pref = true;
            }
            let typeMatch = /^TYPE=(.*)/.exec(param[i]);
            if (typeMatch) {
              type.push(typeMatch[1].toLowerCase());
            }
          }

          if (!type.length) {
            type.push("other");
          }

          if (name === "FN") {
            value = value.replace(/\\;/g, ";");
            contact.name = [value];
          }

          if (name === "N") {
            // Because we don't have lookbehinds, matching unescaped
            // semicolons is a pain. Luckily, we know that \r and \n
            // cannot appear in the strings, so we use them to swap
            // unescaped semicolons for \n.
            value = value.replace(/\\;/g, "\r");
            value = value.replace(/;/g, "\n");
            value = value.replace(/\r/g, ";");

            let family, given, additional, prefix, suffix;
            let values = value.split(/\n/);
            if (values.length >= 5) {
              [family, given, additional, prefix, suffix] = values;
              if (prefix.length) {
                contact.honorificPrefix = [prefix];
              }
              if (given.length) {
                contact.givenName = [given];
              }
              if (additional.length) {
                contact.additionalName = [additional];
              }
              if (family.length) {
                contact.familyName = [family];
              }
              if (suffix.length) {
                contact.honorificSuffix = [suffix];
              }
            }
          }

          if (name === "EMAIL") {
            value = value.replace(/\\;/g, ";");
            if (!("email" in contact)) {
              contact.email = [];
            }
            contact.email.push({
              pref: pref,
              type: type,
              value: value
            });
          }

          if (name === "NICKNAME") {
            value = value.replace(/\\;/g, ";");
            // We don't store nickname in contact because it's not
            // a supported field. We're saving it off here in case we
            // need to use it if the fullname is blank.
            nickname = value;
          }

          if (name === "ADR") {
            value = value.replace(/\\;/g, "\r");
            value = value.replace(/;/g, "\n");
            value = value.replace(/\r/g, ";");
            let pobox, extra, street, locality, region, code, country;
            let values = value.split(/\n/);
            if (values.length >= 7) {
              [pobox, extra, street, locality, region, code, country] = values;
              if (!("adr" in contact)) {
                contact.adr = [];
              }
              contact.adr.push({
                pref: pref,
                type: type,
                streetAddress: (street || pobox) + (extra ? (" " + extra) : ""),
                locality: locality,
                region: region,
                postalCode: code,
                countryName: country
              });
            }
          }

          if (name === "TEL") {
            value = value.replace(/\\;/g, ";");
            if (!("tel" in contact)) {
              contact.tel = [];
            }
            contact.tel.push({
              pref: pref,
              type: type,
              value: value
            });
          }

          if (name === "ORG") {
            value = value.replace(/\\;/g, "\r");
            value = value.replace(/;/g, "\n");
            value = value.replace(/\r/g, ";");
            if (!("org" in contact)) {
              contact.org = [];
            }
            contact.org.push(value.replace(/\n.*/, ""));
          }

          if (name === "TITLE") {
            value = value.replace(/\\;/g, ";");
            if (!("jobTitle" in contact)) {
              contact.jobTitle = [];
            }
            contact.jobTitle.push(value);
          }

          if (name === "BDAY") {
            value = value.replace(/\\;/g, ";");
            contact.bday = Date.parse(value);
          }

          if (name === "UID") {
            contact.id = value;
          }

          if (name === "NOTE") {
            value = value.replace(/\\;/g, ";");
            if (!("note" in contact)) {
              contact.note = [];
            }
            contact.note.push(value);
          }

        }
      }
    );

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
        if (nickname) {
          contact.name = [nickname];
        } else if ("familyName" in contact) {
          contact.name = [contact.familyName[0]];
        } else if ("givenName" in contact) {
          contact.name = [contact.givenName[0]];
        } else if ("org" in contact) {
          contact.name = [contact.org[0]];
        } else if ("email" in contact) {
          contact.name = [contact.email[0].value];
        } else if ("tel" in contact) {
          contact.name = [contact.tel[0].value];
        }
      }
    }

    return contact;
  },

  /**
   * Issues a CardDAV request (see RFC 6352) and returns a Promise to represent
   * the success or failure state of the request.
   *
   * @param {String} method WebDAV method to use (e.g., "PROPFIND")
   * @param {String} url    HTTP URL to use for the request
   * @param {Object} auth   Object with authentication-related configuration.
   *                        See documentation for startImport for details.
   * @param {Number} depth  Value to use for the WebDAV (HTTP) "Depth" header
   * @param {String} body   Body to include in the WebDAV (HTTP) request
   *
   * @return {Object} Promise representing the request operation outcome.
   *                  If resolved, the resolution value is the XMLHttpRequest
   *                  that was used to perform the request.
   */
  _davPromise: function(method, url, auth, depth, body) {
    return new Promise((resolve, reject) => {
      let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance(Ci.nsIXMLHttpRequest);
      let user = "";
      let password = "";

      if (auth.method == "basic") {
        user = auth.user;
        password = auth.password;
      }

      req.open(method, url, true, user, password);

      req.setRequestHeader("Depth", depth);
      req.setRequestHeader("Content-Type", "application/xml; charset=utf-8");

      req.onload = function() {
        if (req.status < 400) {
          resolve(req);
        } else {
          reject(new Error(req.status + " " + req.statusText));
        }
      };

      req.onerror = function(error) {
        reject(error);
      };

      req.send(body);
    });
  }
};
