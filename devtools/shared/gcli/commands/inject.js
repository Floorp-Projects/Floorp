/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { listenOnce } = require("devtools/shared/async-utils");
const l10n = require("gcli/l10n");

exports.items = [
  {
    item: "command",
    runAt: "server",
    name: "inject",
    description: l10n.lookup("injectDesc"),
    manual: l10n.lookup("injectManual2"),
    params: [{
      name: "library",
      type: {
        name: "union",
        alternatives: [
          {
            name: "selection",
            lookup: [
              {
                name: "jQuery",
                value: {
                  name: "jQuery",
                  src: Services.prefs.getCharPref("devtools.gcli.jquerySrc")
                }
              },
              {
                name: "lodash",
                value: {
                  name: "lodash",
                  src: Services.prefs.getCharPref("devtools.gcli.lodashSrc")
                }
              },
              {
                name: "underscore",
                value: {
                  name: "underscore",
                  src: Services.prefs.getCharPref("devtools.gcli.underscoreSrc")
                }
              }
            ]
          },
          {
            name: "url"
          }
        ]
      },
      description: l10n.lookup("injectLibraryDesc")
    }],
    exec: function* (args, context) {
      let document = context.environment.document;
      let library = args.library;

      let name;
      let src;

      if (library.type === "selection") {
        name = library.selection.name;
        src = library.selection.src;
      } else {
        // The library url is a URL object, let's turn it into a string.
        let urlStr = library.url + "";
        name = src = urlStr;
      }

      if (context.environment.window.location.protocol == "https:") {
        src = src.replace(/^http:/, "https:");
      }

      try {
        // Check if URI is valid
        Services.io.newURI(src);
      } catch (e) {
        return l10n.lookupFormat("injectFailed", [name]);
      }

      let newSource = document.createElement("script");
      newSource.setAttribute("src", src);

      let loadPromise = listenOnce(newSource, "load");
      document.head.appendChild(newSource);

      yield loadPromise;

      return l10n.lookupFormat("injectLoaded", [name]);
    }
  }
];
