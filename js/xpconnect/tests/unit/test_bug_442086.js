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
 * The Original Code is Module loader test component.
 *
 * The Initial Developer of the Original Code is
 * Jason Orendorff <jason.orendorff@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// Bug 442086 - XPConnect creates doubles without checking for
//              the INT_FITS_IN_JSVAL case

var types = [
    'PRUint8',
    'PRUint16',
    'PRUint32',
    'PRUint64',
    'PRInt16',
    'PRInt32',
    'PRInt64',
    'float',
    'double'
];

function run_test()
{
  var i;
  for (i = 0; i < types.length; i++) {
    var name = types[i];
    var cls = Components.classes["@mozilla.org/supports-" + name + ";1"];
    var ifname = ("nsISupports" + name.charAt(0).toUpperCase() +
                  name.substring(1));
    var f = cls.createInstance(Components.interfaces[ifname]);

    f.data = 0;
    switch (f.data) {
      case 0: /*ok*/ break;
      default: do_throw("FAILED - bug 442086 (type=" + name + ")");
    }
  }
}
