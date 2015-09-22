/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");
const { OS } = require("resource://gre/modules/osfile.jsm");
const { devtools } = require("resource://gre/modules/devtools/shared/Loader.jsm");
const gcli = require("gcli/index");
const l10n = require("gcli/l10n");

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"]
                           .getService(Ci.nsIStringBundleService)
                           .createBundle("chrome://branding/locale/brand.properties")
                           .GetStringFromName("brandShortName");

exports.items = [
  {
    name: "tools",
    description: l10n.lookupFormat("toolsDesc2", [ BRAND_SHORT_NAME ]),
    manual: l10n.lookupFormat("toolsManual2", [ BRAND_SHORT_NAME ]),
    get hidden() {
      return gcli.hiddenByChromePref();
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "tools srcdir",
    description: l10n.lookup("toolsSrcdirDesc"),
    manual: l10n.lookupFormat("toolsSrcdirManual2", [ BRAND_SHORT_NAME ]),
    get hidden() {
      return gcli.hiddenByChromePref();
    },
    params: [
      {
        name: "srcdir",
        type: "string" /* {
          name: "file",
          filetype: "directory",
          existing: "yes"
        } */,
        description: l10n.lookup("toolsSrcdirDir")
      }
    ],
    returnType: "string",
    exec: function(args, context) {
      let clobber = OS.Path.join(args.srcdir, "CLOBBER");
      return OS.File.exists(clobber).then(function(exists) {
        if (exists) {
          let str = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);
          str.data = args.srcdir;
          Services.prefs.setComplexValue("devtools.loader.srcdir",
                                         Ci.nsISupportsString, str);
          devtools.reload();

          return l10n.lookupFormat("toolsSrcdirReloaded2", [ args.srcdir ]);
        }

        return l10n.lookupFormat("toolsSrcdirNotFound2", [ args.srcdir ]);
      });
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "tools builtin",
    description: l10n.lookup("toolsBuiltinDesc"),
    manual: l10n.lookup("toolsBuiltinManual"),
    get hidden() {
      return gcli.hiddenByChromePref();
    },
    returnType: "string",
    exec: function(args, context) {
      Services.prefs.clearUserPref("devtools.loader.srcdir");
      devtools.reload();
      return l10n.lookup("toolsBuiltinReloaded");
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "tools reload",
    description: l10n.lookup("toolsReloadDesc"),
    get hidden() {
      return gcli.hiddenByChromePref() ||
             !Services.prefs.prefHasUserValue("devtools.loader.srcdir");
    },

    returnType: "string",
    exec: function(args, context) {
      devtools.reload();
      return l10n.lookup("toolsReloaded2");
    }
  }
];
