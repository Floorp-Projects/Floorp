/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");

var MdnDocsWidget;
try {
  MdnDocsWidget = require("devtools/client/shared/widgets/MdnDocsWidget");
} catch (e) {
  // DevTools MdnDocsWidget only available in Firefox Desktop
}

exports.items = [{
  name: "mdn",
  description: l10n.lookup("mdnDesc")
}, {
  item: "command",
  runAt: "client",
  name: "mdn css",
  description: l10n.lookup("mdnCssDesc"),
  returnType: "cssPropertyOutput",
  params: [{
    name: "property",
    type: { name: "string" },
    defaultValue: null,
    description: l10n.lookup("mdnCssProp")
  }],
  exec: function(args) {
    if (!MdnDocsWidget) {
      return null;
    }

    return MdnDocsWidget.getCssDocs(args.property).then(result => {
      return {
        data: result,
        url: MdnDocsWidget.PAGE_LINK_URL + args.property,
        property: args.property
      };
    }, error => {
      return { error, property: args.property };
    });
  }
}, {
  item: "converter",
  from: "cssPropertyOutput",
  to: "dom",
  exec: function(result, context) {
    let propertyName = result.property;

    let document = context.document;
    let root = document.createElement("div");

    if (result.error) {
      // The css property specified doesn't exist.
      root.appendChild(document.createTextNode(
        l10n.lookupFormat("mdnCssPropertyNotFound", [ propertyName ]) +
        " (" + result.error + ")"));
    } else {
      let title = document.createElement("h2");
      title.textContent = propertyName;
      root.appendChild(title);

      let link = document.createElement("p");
      link.classList.add("gcli-mdn-url");
      link.textContent = l10n.lookup("mdnCssVisitPage");
      root.appendChild(link);

      link.addEventListener("click", () => {
        let mainWindow = context.environment.chromeWindow;
        mainWindow.openUILinkIn(result.url, "tab");
      });

      let summary = document.createElement("p");
      summary.textContent = result.data.summary;
      root.appendChild(summary);
    }

    return root;
  }
}];
