/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() { 
    var tm = Components.classes["@mozilla.org/thread-manager;1"].getService();
    var thr = tm.newThread(0);

    var foundThreadError = false;
 
    var listener = {
        observe: function(message) {
            if (/JS function on a different thread/.test(message.message))
                foundThreadError = true;
        }
    };

    var cs = Components.classes["@mozilla.org/consoleservice;1"].
        getService(Components.interfaces.nsIConsoleService);
    cs.registerListener(listener);

    thr.dispatch({
        run: function() {
            do_check_true(false);
        }
    }, Components.interfaces.nsIThread.DISPATCH_NORMAL);

    thr.shutdown();

    cs.unregisterListener(listener);
    do_check_true(foundThreadError);
}

