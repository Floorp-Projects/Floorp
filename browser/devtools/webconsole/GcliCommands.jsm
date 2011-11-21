/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is GCLI Commands.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker <jwalker@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


let EXPORTED_SYMBOLS = [ "GcliCommands" ];

Components.utils.import("resource:///modules/gcli.jsm");
Components.utils.import("resource:///modules/HUDService.jsm");


/**
 * 'echo' command
 */
gcli.addCommand({
  name: "echo",
  description: gcli.lookup("echoDesc"),
  params: [
    {
      name: "message",
      type: "string",
      description: gcli.lookup("echoMessageDesc")
    }
  ],
  returnType: "string",
  exec: function Command_echo(args, context) {
    return args.message;
  }
});


let canon = gcli._internal.require("gcli/canon");

/**
 * 'help' command
 */
gcli.addCommand({
  name: "help",
  returnType: "html",
  description: gcli.lookup("helpDesc"),
  exec: function Command_help(args, context) {
    let output = [];

    output.push("<strong>" + gcli.lookup("helpAvailable") + ":</strong><br/>");

    let commandNames = canon.getCommandNames();
    commandNames.sort();

    output.push("<table>");
    for (let i = 0; i < commandNames.length; i++) {
      let command = canon.getCommand(commandNames[i]);
      if (!command.hidden && command.description) {
        output.push("<tr>");
        output.push('<th class="gcli-help-right">' + command.name + "</th>");
        output.push("<td>&#x2192; " + command.description + "</td>");
        output.push("</tr>");
      }
    }
    output.push("</table>");

    return output.join("");
  }
});


/**
 * 'console' command
 */
gcli.addCommand({
  name: "console",
  description: gcli.lookup("consoleDesc"),
  manual: gcli.lookup("consoleManual")
});

/**
 * 'console clear' command
 */
gcli.addCommand({
  name: "console clear",
  description: gcli.lookup("consoleclearDesc"),
  exec: function(args, context) {
    let hud = HUDService.getHudReferenceById(context.environment.hudId);
    hud.gcliterm.clearOutput();
  }
});


/**
 * 'inspect' command
 */
gcli.addCommand({
  name: "inspect",
  description: gcli.lookup("inspectDesc"),
  manual: gcli.lookup("inspectManual"),
  params: [
    {
      name: "node",
      type: "node",
      description: gcli.lookup("inspectNodeDesc"),
      manual: gcli.lookup("inspectNodeManual")
    }
  ],
  exec: function Command_inspect(args, context) {
    let hud = HUDService.getHudReferenceById(context.environment.hudId);
    let InspectorUI = hud.gcliterm.document.defaultView.InspectorUI;
    InspectorUI.openInspectorUI(args.node);
  }
});
