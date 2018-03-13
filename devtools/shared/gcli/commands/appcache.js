/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");

loader.lazyImporter(this, "AppCacheUtils", "resource://devtools/client/shared/AppCacheUtils.jsm");

exports.items = [
  {
    item: "command",
    name: "appcache",
    description: l10n.lookup("appCacheDesc")
  },
  {
    item: "command",
    runAt: "server",
    name: "appcache validate",
    description: l10n.lookup("appCacheValidateDesc"),
    manual: l10n.lookup("appCacheValidateManual"),
    returnType: "appcacheerrors",
    params: [{
      group: "options",
      params: [
        {
          type: "string",
          name: "uri",
          description: l10n.lookup("appCacheValidateUriDesc"),
          defaultValue: null,
        }
      ]
    }],
    exec: function(args, context) {
      let utils;
      let deferred = context.defer();

      if (args.uri) {
        utils = new AppCacheUtils(args.uri);
      } else {
        utils = new AppCacheUtils(context.environment.document);
      }

      utils.validateManifest().then(function(errors) {
        deferred.resolve([errors, utils.manifestURI || "-"]);
      });

      return deferred.promise;
    }
  },
  {
    item: "converter",
    from: "appcacheerrors",
    to: "view",
    exec: function([errors, manifestURI], context) {
      if (errors.length == 0) {
        return context.createView({
          html: "<span>" + l10n.lookup("appCacheValidatedSuccessfully") + "</span>"
        });
      }

      return context.createView({
        html:
          "<div>" +
          "  <h4>Manifest URI: ${manifestURI}</h4>" +
          "  <ol>" +
          "    <li foreach='error in ${errors}'>${error.msg}</li>" +
          "  </ol>" +
          "</div>",
        data: {
          errors: errors,
          manifestURI: manifestURI
        }
      });
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "appcache clear",
    description: l10n.lookup("appCacheClearDesc"),
    manual: l10n.lookup("appCacheClearManual"),
    exec: function(args, context) {
      let utils = new AppCacheUtils(args.uri);
      utils.clearAll();

      return l10n.lookup("appCacheClearCleared");
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "appcache list",
    description: l10n.lookup("appCacheListDesc"),
    manual: l10n.lookup("appCacheListManual"),
    returnType: "appcacheentries",
    params: [{
      group: "options",
      params: [
        {
          type: "string",
          name: "search",
          description: l10n.lookup("appCacheListSearchDesc"),
          defaultValue: null,
        },
      ]
    }],
    exec: function(args, context) {
      let utils = new AppCacheUtils();
      return utils.listEntries(args.search);
    }
  },
  {
    item: "converter",
    from: "appcacheentries",
    to: "view",
    exec: function(entries, context) {
      return context.createView({
        html: "" +
          "<ul class='gcli-appcache-list'>" +
          "  <li foreach='entry in ${entries}'>" +
          "    <table class='gcli-appcache-detail'>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListKey") + "</td>" +
          "        <td>${entry.key}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListFetchCount") + "</td>" +
          "        <td>${entry.fetchCount}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListLastFetched") + "</td>" +
          "        <td>${entry.lastFetched}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListLastModified") + "</td>" +
          "        <td>${entry.lastModified}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListExpirationTime") + "</td>" +
          "        <td>${entry.expirationTime}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListDataSize") + "</td>" +
          "        <td>${entry.dataSize}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("appCacheListDeviceID") + "</td>" +
          "        <td>${entry.deviceID} <span class='gcli-out-shortcut' " +
          "onclick='${onclick}' ondblclick='${ondblclick}' " +
          "data-command='appcache viewentry ${entry.key}'" +
          ">" + l10n.lookup("appCacheListViewEntry") + "</span>" +
          "        </td>" +
          "      </tr>" +
          "    </table>" +
          "  </li>" +
          "</ul>",
        data: {
          entries: entries,
          onclick: context.update,
          ondblclick: context.updateExec
        }
      });
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "appcache viewentry",
    description: l10n.lookup("appCacheViewEntryDesc"),
    manual: l10n.lookup("appCacheViewEntryManual"),
    params: [
      {
        type: "string",
        name: "key",
        description: l10n.lookup("appCacheViewEntryKey"),
        defaultValue: null,
      }
    ],
    exec: function(args, context) {
      let utils = new AppCacheUtils();
      return utils.viewEntry(args.key);
    }
  }
];
