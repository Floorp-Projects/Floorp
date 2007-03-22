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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// Some misc command-related plumbing used by the controller.


/**
 * A tiny wrapper class for super-simple command handlers.
 *
 * @param commandHandlerMap An object containing name/value pairs where
 *                          the name is command name (string) and value
 *                          is the function to execute for that command
 */
function PROT_CommandController(commandHandlerMap) {
  this.debugZone = "commandhandler";
  this.cmds_ = commandHandlerMap;
}

/**
 * @param cmd Command to query support for
 * @returns Boolean indicating if this controller supports cmd
 */
PROT_CommandController.prototype.supportsCommand = function(cmd) { 
  return (cmd in this.cmds_); 
}

/**
 * Trivial implementation
 *
 * @param cmd Command to query status of
 */
PROT_CommandController.prototype.isCommandEnabled = function(cmd) { 
  return true; 
}
  
/**
 * Execute a command
 *
 * @param cmd Command to execute
 */
PROT_CommandController.prototype.doCommand = function(cmd) {
  return this.cmds_[cmd](); 
}
 
/**
 * Trivial implementation
 */
PROT_CommandController.prototype.onEvent = function(cmd) { }

