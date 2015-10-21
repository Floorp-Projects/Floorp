/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");

/**
 * The `edit` command opens the toolbox to the style editor, with a given
 * stylesheet open.
 *
 * This command is tricky. The 'edit' command uses the toolbox, so it's
 * clearly runAt:client, but it uses the 'resource' type which accesses the
 * DOM, so it must also be runAt:server.
 *
 * Our solution is to have the command technically be runAt:server, but to not
 * actually do anything other than basically `return args;`, and have the
 * converter (all converters are runAt:client) do the actual work of opening
 * a toolbox.
 *
 * For alternative solutions that we considered, see the comment on commit
 * 2645af7.
 */
exports.items = [{
  item: "command",
  runAt: "server",
  name: "edit",
  description: l10n.lookup("editDesc"),
  manual: l10n.lookup("editManual2"),
  params: [
     {
       name: 'resource',
       type: {
         name: 'resource',
         include: 'text/css'
       },
       description: l10n.lookup("editResourceDesc")
     },
     {
       name: "line",
       defaultValue: 1,
       type: {
         name: "number",
         min: 1,
         step: 10
       },
       description: l10n.lookup("editLineToJumpToDesc")
     }
   ],
   returnType: "editArgs",
   exec: args => {
     return { href: args.resource.name, line: args.line };
   }
}, {
  item: "converter",
  from: "editArgs",
  to: "dom",
   exec: function(args, context) {
     let target = context.environment.target;
     let gDevTools = require("resource://devtools/client/framework/gDevTools.jsm").gDevTools;
     return gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
       let styleEditor = toolbox.getCurrentPanel();
       styleEditor.selectStyleSheet(args.href, args.line);
       return null;
     });
   }
}];
