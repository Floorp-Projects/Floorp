/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const l10n = require("gcli/l10n");
const XMLHttpRequest = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"];

loader.lazyImporter(this, "Preferences", "resource://gre/modules/Preferences.jsm");

loader.lazyRequireGetter(this, "beautify", "devtools/shared/jsbeautify/beautify");

exports.items = [
  {
    item: "command",
    runAt: "client",
    name: "jsb",
    description: l10n.lookup("jsbDesc"),
    returnValue:"string",
    params: [
      {
        name: "url",
        type: "string",
        description: l10n.lookup("jsbUrlDesc")
      },
      {
        group: l10n.lookup("jsbOptionsDesc"),
        params: [
          {
            name: "indentSize",
            type: "number",
            description: l10n.lookup("jsbIndentSizeDesc"),
            manual: l10n.lookup("jsbIndentSizeManual"),
            defaultValue: Preferences.get("devtools.editor.tabsize", 2),
          },
          {
            name: "indentChar",
            type: {
              name: "selection",
              lookup: [
                { name: "space", value: " " },
                { name: "tab", value: "\t" }
              ]
            },
            description: l10n.lookup("jsbIndentCharDesc"),
            manual: l10n.lookup("jsbIndentCharManual"),
            defaultValue: " ",
          },
          {
            name: "doNotPreserveNewlines",
            type: "boolean",
            description: l10n.lookup("jsbDoNotPreserveNewlinesDesc")
          },
          {
            name: "preserveMaxNewlines",
            type: "number",
            description: l10n.lookup("jsbPreserveMaxNewlinesDesc"),
            manual: l10n.lookup("jsbPreserveMaxNewlinesManual"),
            defaultValue: -1
          },
          {
            name: "jslintHappy",
            type: "boolean",
            description: l10n.lookup("jsbJslintHappyDesc"),
            manual: l10n.lookup("jsbJslintHappyManual")
          },
          {
            name: "braceStyle",
            type: {
              name: "selection",
              data: ["collapse", "expand", "end-expand", "expand-strict"]
            },
            description: l10n.lookup("jsbBraceStyleDesc2"),
            manual: l10n.lookup("jsbBraceStyleManual2"),
            defaultValue: "collapse"
          },
          {
            name: "noSpaceBeforeConditional",
            type: "boolean",
            description: l10n.lookup("jsbNoSpaceBeforeConditionalDesc")
          },
          {
            name: "unescapeStrings",
            type: "boolean",
            description: l10n.lookup("jsbUnescapeStringsDesc"),
            manual: l10n.lookup("jsbUnescapeStringsManual")
          }
        ]
      }
    ],
    exec: function(args, context) {
      let opts = {
        indent_size: args.indentSize,
        indent_char: args.indentChar,
        preserve_newlines: !args.doNotPreserveNewlines,
        max_preserve_newlines: args.preserveMaxNewlines == -1 ?
                              undefined : args.preserveMaxNewlines,
        jslint_happy: args.jslintHappy,
        brace_style: args.braceStyle,
        space_before_conditional: !args.noSpaceBeforeConditional,
        unescape_strings: args.unescapeStrings
      };

      let xhr = new XMLHttpRequest();

      try {
        xhr.open("GET", args.url, true);
      } catch(e) {
        return l10n.lookup("jsbInvalidURL");
      }

      let deferred = context.defer();

      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          if (xhr.status == 200 || xhr.status == 0) {
            let browserDoc = context.environment.chromeDocument;
            let browserWindow = browserDoc.defaultView;
            let gBrowser = browserWindow.gBrowser;
            let result = beautify.js(xhr.responseText, opts);

            browserWindow.Scratchpad.ScratchpadManager.openScratchpad({text: result});

            deferred.resolve();
          } else {
            deferred.reject("Unable to load page to beautify: " + args.url + " " +
                            xhr.status + " " + xhr.statusText);
          }
        };
      }
      xhr.send(null);
      return deferred.promise;
    }
  }
];
