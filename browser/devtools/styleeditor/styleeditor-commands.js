/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require("gcli/index");

exports.items = [{
  name: "edit",
  description: gcli.lookup("editDesc"),
  manual: gcli.lookup("editManual2"),
  params: [
     {
       name: 'resource',
       type: {
         name: 'resource',
         include: 'text/css'
       },
       description: gcli.lookup("editResourceDesc")
     },
     {
       name: "line",
       defaultValue: 1,
       type: {
         name: "number",
         min: 1,
         step: 10
       },
       description: gcli.lookup("editLineToJumpToDesc")
     }
   ],
   exec: function(args, context) {
     let target = context.environment.target;
     let gDevTools = require("resource:///modules/devtools/gDevTools.jsm").gDevTools;
     return gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
       let styleEditor = toolbox.getCurrentPanel();
       styleEditor.selectStyleSheet(args.resource.element, args.line);
       return null;
     });
   }
}];
