/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test for bug 414201
 */

function run_test()
{
    var ms = Components.classes["@mozilla.org/mime;1"].getService(Components.interfaces.nsIMIMEService);

    /* Test a few common image types to make sure that they get the right extension */
    var types = {
        "image/jpeg": ["jpg", "jpeg"], /* accept either */
        "image/gif": ["gif"],
        "image/png": ["png"]
    };

    /* Check whether the primary extension is what we'd expect */
    for (var mimetype in types) {
        var exts = types[mimetype];
        var primary = ms.getFromTypeAndExtension(mimetype, null).primaryExtension.toLowerCase();

        do_check_true (exts.indexOf(primary) != -1);
    }
}
