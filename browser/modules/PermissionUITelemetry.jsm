/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PermissionUITelemetry"];

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CryptoUtils",
  "resource://services-crypto/utils.js"
);

const TELEMETRY_STAT_REMOVAL_LEAVE_PAGE = 6;

var PermissionUITelemetry = {
  // Returns a hash of the host name in combination with a unique local user id.
  // This allows us to track duplicate prompts on sites while not revealing the user's
  // browsing history.
  _uniqueHostHash(host) {
    // Gets a unique user ID as salt, that needs to stay local to this profile and not be
    // sent to any server!
    let salt = Services.prefs.getStringPref(
      "permissions.eventTelemetry.salt",
      null
    );
    if (!salt) {
      salt = Cc["@mozilla.org/uuid-generator;1"]
        .getService(Ci.nsIUUIDGenerator)
        .generateUUID()
        .toString();
      Services.prefs.setStringPref("permissions.eventTelemetry.salt", salt);
    }

    let domain;
    try {
      domain = Services.eTLD.getBaseDomainFromHost(host);
    } catch (e) {
      domain = host;
    }

    return CryptoUtils.sha256(domain + salt);
  },

  _previousVisitCount(host) {
    let historyService = Cc[
      "@mozilla.org/browser/nav-history-service;1"
    ].getService(Ci.nsINavHistoryService);

    let options = historyService.getNewQueryOptions();
    options.resultType = options.RESULTS_AS_VISIT;

    // Search for visits to this host before today
    let query = historyService.getNewQuery();
    query.endTimeReference = query.TIME_RELATIVE_TODAY;
    query.endTime = 0;
    query.domain = host;

    let result = historyService.executeQuery(query, options);
    result.root.containerOpen = true;
    let cc = result.root.childCount;
    result.root.containerOpen = false;
    return cc;
  },

  _collectExtraKeys(prompt) {
    let lastInteraction = 0;
    // "storageAccessAPI" is the name of the permission that tells us whether the
    // user has interacted with a particular site in the first-party context before.
    let interactionPermission = Services.perms.getPermissionObject(
      prompt.principal,
      "storageAccessAPI",
      false
    );
    if (interactionPermission) {
      lastInteraction = interactionPermission.modificationTime;
    }

    let allPermsDenied = 0;
    let allPermsGranted = 0;
    let thisPermDenied = 0;
    let thisPermGranted = 0;

    let commonPermissions = [
      "geo",
      "desktop-notification",
      "camera",
      "microphone",
      "screen",
    ];
    for (let perm of Services.perms.enumerator) {
      if (!commonPermissions.includes(perm.type)) {
        continue;
      }

      if (perm.capability == Services.perms.ALLOW_ACTION) {
        allPermsGranted++;
        if (perm.type == prompt.permissionKey) {
          thisPermGranted++;
        }
      }

      if (perm.capability == Services.perms.DENY_ACTION) {
        allPermsDenied++;
        if (perm.type == prompt.permissionKey) {
          thisPermDenied++;
        }
      }
    }

    let promptHost = prompt.principal.URI.host;

    return {
      previousVisits: this._previousVisitCount(promptHost).toString(),
      timeOnPage: (
        Date.now() - prompt.documentDOMContentLoadedTimestamp
      ).toString(),
      hasUserInput: prompt.isHandlingUserInput.toString(),
      docHasUserInput: prompt.userHadInteractedWithDocument.toString(),
      lastInteraction: lastInteraction.toString(),
      allPermsDenied: allPermsDenied.toString(),
      allPermsGranted: allPermsGranted.toString(),
      thisPermDenied: thisPermDenied.toString(),
      thisPermGranted: thisPermGranted.toString(),
    };
  },

  onShow(prompt) {
    let object = prompt.permissionTelemetryKey;
    if (!object) {
      return;
    }

    let extraKeys = this._collectExtraKeys(prompt);
    let hostHash = this._uniqueHostHash(prompt.principal.URI.host);
    Services.telemetry.recordEvent(
      "security.ui.permissionprompt",
      "show",
      object,
      hostHash,
      extraKeys
    );
  },

  onRemoved(prompt, buttonAction, telemetryReason) {
    let object = prompt.permissionTelemetryKey;
    if (!object) {
      return;
    }

    let method = "other";
    if (buttonAction == "accept") {
      method = "accept";
    } else if (buttonAction == "deny") {
      method = "deny";
    } else if (buttonAction == "never") {
      method = "never";
    } else if (telemetryReason == TELEMETRY_STAT_REMOVAL_LEAVE_PAGE) {
      method = "leave";
    }

    let extraKeys = this._collectExtraKeys(prompt);
    let hostHash = this._uniqueHostHash(prompt.principal.URI.host);
    Services.telemetry.recordEvent(
      "security.ui.permissionprompt",
      method,
      object,
      hostHash,
      extraKeys
    );
  },
};
