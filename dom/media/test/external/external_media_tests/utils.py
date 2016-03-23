# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import time
import types

from marionette_driver.errors import TimeoutException


def timestamp_now():
    return int(time.mktime(datetime.datetime.now().timetuple()))


def verbose_until(wait, target, condition, message=""):
    """
    Performs a `wait`.until(condition)` and adds information about the state of
    `target` to any resulting `TimeoutException`.

    :param wait: a `marionette.Wait` instance
    :param target: the object you want verbose output about if a
        `TimeoutException` is raised
        This is usually the input value provided to the `condition` used by
        `wait`. Ideally, `target` should implement `__str__`
    :param condition: callable function used by `wait.until()`
    :param message: optional message to log when exception occurs

    :return: the result of `wait.until(condition)`
    """
    if isinstance(condition, types.FunctionType):
        name = condition.__name__
    else:
        name = str(condition)
    err_message = '\n'.join([message,
                             'condition: ' + name,
                             str(target)])

    return wait.until(condition, message=err_message)



def save_memory_report(marionette):
    """
    Saves memory report (like about:memory) to current working directory.

    :param marionette: Marionette instance to use for executing.
    """
    with marionette.using_context('chrome'):
        marionette.execute_async_script("""
            Components.utils.import("resource://gre/modules/Services.jsm");
            let Cc = Components.classes;
            let Ci = Components.interfaces;
            let dumper = Cc["@mozilla.org/memory-info-dumper;1"].
                        getService(Ci.nsIMemoryInfoDumper);
            // Examples of dirs: "CurProcD" usually 'browser' dir in
            // current FF dir; "DfltDwnld" default download dir
            let file = Services.dirsvc.get("CurProcD", Ci.nsIFile);
            file.append("media-memory-report");
            file.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0777);
            file.append("media-memory-report.json.gz");
            dumper.dumpMemoryReportsToNamedFile(file.path, null, null, false);
            log('Saved memory report to ' + file.path);
            // for dmd-enabled build
            dumper.dumpMemoryInfoToTempDir("media", false, false);
            marionetteScriptFinished(true);
            return;
        """, script_timeout=30000)
