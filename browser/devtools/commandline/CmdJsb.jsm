/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const XMLHttpRequest =
  Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1");

let EXPORTED_SYMBOLS = [ ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/gcli.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "js_beautify",
                                  "resource:///modules/devtools/Jsbeautify.jsm");

/**
 * jsb command.
 */
gcli.addCommand({
  name: 'jsb',
  description: gcli.lookup('jsbDesc'),
  returnValue:'string',
  params: [
    {
      name: 'url',
      type: 'string',
      description: gcli.lookup('jsbUrlDesc')
    },
    {
      name: 'indentSize',
      type: 'number',
      description: gcli.lookup('jsbIndentSizeDesc'),
      manual: gcli.lookup('jsbIndentSizeManual'),
      defaultValue: 2
    },
    {
      name: 'indentChar',
      type: {
        name: 'selection',
        lookup: [
          { name: "space", value: " " },
          { name: "tab", value: "\t" }
        ]
      },
      description: gcli.lookup('jsbIndentCharDesc'),
      manual: gcli.lookup('jsbIndentCharManual'),
      defaultValue: ' ',
    },
    {
      name: 'preserveNewlines',
      type: 'boolean',
      description: gcli.lookup('jsbPreserveNewlinesDesc'),
      manual: gcli.lookup('jsbPreserveNewlinesManual')
    },
    {
      name: 'preserveMaxNewlines',
      type: 'number',
      description: gcli.lookup('jsbPreserveMaxNewlinesDesc'),
      manual: gcli.lookup('jsbPreserveMaxNewlinesManual'),
      defaultValue: -1
    },
    {
      name: 'jslintHappy',
      type: 'boolean',
      description: gcli.lookup('jsbJslintHappyDesc'),
      manual: gcli.lookup('jsbJslintHappyManual')
    },
    {
      name: 'braceStyle',
      type: {
        name: 'selection',
        data: ['collapse', 'expand', 'end-expand', 'expand-strict']
      },
      description: gcli.lookup('jsbBraceStyleDesc'),
      manual: gcli.lookup('jsbBraceStyleManual'),
      defaultValue: "collapse"
    },
    {
      name: 'spaceBeforeConditional',
      type: 'boolean',
      description: gcli.lookup('jsbSpaceBeforeConditionalDesc'),
      manual: gcli.lookup('jsbSpaceBeforeConditionalManual')
    },
    {
      name: 'unescapeStrings',
      type: 'boolean',
      description: gcli.lookup('jsbUnescapeStringsDesc'),
      manual: gcli.lookup('jsbUnescapeStringsManual')
    }
  ],
  exec: function(args, context) {
    let opts = {
      indent_size: args.indentSize,
      indent_char: args.indentChar,
      preserve_newlines: args.preserveNewlines,
      max_preserve_newlines: args.preserveMaxNewlines == -1 ?
                             undefined : args.preserveMaxNewlines,
      jslint_happy: args.jslintHappy,
      brace_style: args.braceStyle,
      space_before_conditional: args.spaceBeforeConditional,
      unescape_strings: args.unescapeStrings
    }

    let xhr = new XMLHttpRequest();

    try {
      xhr.open("GET", args.url, true);
    } catch(e) {
      return gcli.lookup('jsbInvalidURL');
    }

    let promise = context.createPromise();

    xhr.onreadystatechange = function(aEvt) {
      if (xhr.readyState == 4) {
        if (xhr.status == 200 || xhr.status == 0) {
          let browserDoc = context.environment.chromeDocument;
          let browserWindow = browserDoc.defaultView;
          let browser = browserWindow.gBrowser;
  
          browser.selectedTab = browser.addTab("data:text/plain;base64," +
            browserWindow.btoa(js_beautify(xhr.responseText, opts)));
          promise.resolve();
        }
        else {
          promise.resolve("Unable to load page to beautify: " + args.url + " " +
                          xhr.status + " " + xhr.statusText);
        }
      };
    }
    xhr.send(null);
    return promise;
  }
});
