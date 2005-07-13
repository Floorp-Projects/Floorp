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
 * The Original Code is the Narcissus JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Brendan Eich <brendan@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * Narcissus - JS implemented in JS.
 *
 * Native objects and classes implemented metacircularly:
 *      the global object (singleton)
 *      eval
 *      function objects, Function
 *
 * SpiderMonkey extensions used:
 *      catch guards
 *      const declarations
 *      get and set functions in object initialisers
 *      Object.prototype.__defineGetter__
 *      Object.prototype.__defineSetter__
 *      Object.prototype.__defineProperty__
 *      Object.prototype.__proto__
 *      filename and line number arguments to *Error constructors
 *      callable regular expression objects
 *
 * SpiderMonkey extensions supported metacircularly:
 *      catch guards
 *      const declarations
 *      get and set functions in object initialisers
 */

/*
 * Loads a file relative to the calling script's (our) source directory, and not
 * the directory that the executing shell is being run out of.
 */
function my_load(filename) {
    evaluate(snarf(filename), filename, 1);
}

my_load('jsdefs.js');
my_load('jsparse.js');
my_load('jsexec.js');

