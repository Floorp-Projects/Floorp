/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
require("devtools/server/actors/inspector");
const {
  BoxModelHighlighter,
  HighlighterEnvironment
} = require("devtools/server/actors/highlighters");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools-shared/locale/gclicommands.properties");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");

// How many maximum nodes can be highlighted in parallel
const MAX_HIGHLIGHTED_ELEMENTS = 100;

// Store the environment object used to create highlighters so it can be
// destroyed later.
var highlighterEnv;

// Stores the highlighters instances so they can be destroyed later.
// also export them so tests can access those and assert they got created
// correctly.
exports.highlighters = [];

/**
 * Destroy all existing highlighters
 */
function unhighlightAll() {
  for (let highlighter of exports.highlighters) {
    highlighter.destroy();
  }
  exports.highlighters.length = 0;

  if (highlighterEnv) {
    highlighterEnv.destroy();
    highlighterEnv = null;
  }
}

exports.items = [
  {
    item: "command",
    runAt: "server",
    name: "highlight",
    description: l10n.lookup("highlightDesc"),
    manual: l10n.lookup("highlightManual"),
    params: [
      {
        name: "selector",
        type: "nodelist",
        description: l10n.lookup("highlightSelectorDesc"),
        manual: l10n.lookup("highlightSelectorManual")
      },
      {
        group: l10n.lookup("highlightOptionsDesc"),
        params: [
          {
            name: "hideguides",
            type: "boolean",
            description: l10n.lookup("highlightHideGuidesDesc"),
            manual: l10n.lookup("highlightHideGuidesManual")
          },
          {
            name: "showinfobar",
            type: "boolean",
            description: l10n.lookup("highlightShowInfoBarDesc"),
            manual: l10n.lookup("highlightShowInfoBarManual")
          },
          {
            name: "showall",
            type: "boolean",
            description: l10n.lookup("highlightShowAllDesc"),
            manual: l10n.lookup("highlightShowAllManual")
          },
          {
            name: "region",
            type: {
              name: "selection",
              data: ["content", "padding", "border", "margin"]
            },
            description: l10n.lookup("highlightRegionDesc"),
            manual: l10n.lookup("highlightRegionManual"),
            defaultValue: "border"
          },
          {
            name: "fill",
            type: "string",
            description: l10n.lookup("highlightFillDesc"),
            manual: l10n.lookup("highlightFillManual"),
            defaultValue: null
          },
          {
            name: "keep",
            type: "boolean",
            description: l10n.lookup("highlightKeepDesc"),
            manual: l10n.lookup("highlightKeepManual")
          }
        ]
      }
    ],
    exec: function(args, context) {
      // Remove all existing highlighters unless told otherwise
      if (!args.keep) {
        unhighlightAll();
      }

      let env = context.environment;
      highlighterEnv = new HighlighterEnvironment();
      highlighterEnv.initFromWindow(env.window);

      // Unhighlight on navigate
      highlighterEnv.once("will-navigate", unhighlightAll);

      let i = 0;
      for (let node of args.selector) {
        if (!args.showall && i >= MAX_HIGHLIGHTED_ELEMENTS) {
          break;
        }

        let highlighter = new BoxModelHighlighter(highlighterEnv);
        if (args.fill) {
          highlighter.regionFill[args.region] = args.fill;
        }
        highlighter.show(node, {
          region: args.region,
          hideInfoBar: !args.showinfobar,
          hideGuides: args.hideguides,
          showOnly: args.region
        });
        exports.highlighters.push(highlighter);
        i++;
      }

      let highlightText = L10N.getStr("highlightOutputConfirm2");
      let output = PluralForm.get(args.selector.length, highlightText)
                             .replace("%1$S", args.selector.length);
      if (args.selector.length > i) {
        output = l10n.lookupFormat("highlightOutputMaxReached",
          ["" + args.selector.length, "" + i]);
      }

      return output;
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "unhighlight",
    description: l10n.lookup("unhighlightDesc"),
    manual: l10n.lookup("unhighlightManual"),
    exec: unhighlightAll
  }
];
