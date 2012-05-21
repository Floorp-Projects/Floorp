/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
