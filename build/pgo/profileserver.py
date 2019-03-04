#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

from buildconfig import substs
from mozbuild.base import MozbuildObject
from mozfile import TemporaryDirectory
from mozhttpd import MozHttpd
from mozprofile import Preferences
from mozrunner import CLI
from six import string_types

from marionette_driver.marionette import Marionette

PORT = 8888

PATH_MAPPINGS = {
    '/js-input/webkit/PerformanceTests': 'third_party/webkit/PerformanceTests',
}

if __name__ == '__main__':
    cli = CLI()
    debug_args, interactive = cli.debugger_arguments()
    runner_args = cli.runner_args()

    build = MozbuildObject.from_environment()

    binary = runner_args.get('binary')
    if not binary:
        binary = build.get_binary_path(where="staged-package")

    path_mappings = {
        k: os.path.join(build.topsrcdir, v)
        for k, v in PATH_MAPPINGS.items()
    }
    httpd = MozHttpd(
        port=PORT,
        docroot=os.path.join(build.topsrcdir, "build", "pgo"),
        path_mappings=path_mappings)
    httpd.start(block=False)

    with TemporaryDirectory() as profilePath:
        # TODO: refactor this into mozprofile
        profile_data_dir = os.path.join(build.topsrcdir, 'testing', 'profiles')
        with open(os.path.join(profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['profileserver']

        prefpaths = [
            os.path.join(profile_data_dir, profile, 'user.js')
            for profile in base_profiles
        ]

        prefs = {}
        for path in prefpaths:
            prefs.update(Preferences.read_prefs(path))

        interpolation = {
            "server": "%s:%d" % httpd.httpd.server_address,
            "OOP": "false"
        }
        for k, v in prefs.items():
            if isinstance(v, string_types):
                v = v.format(**interpolation)
            prefs[k] = Preferences.cast(v)

        env = os.environ
        env["XPCOM_DEBUG_BREAK"] = "1"

        # For VC12+, make sure we can find the right bitness of pgort1x0.dll
        if not substs.get('HAVE_64BIT_BUILD'):
            for e in ('VS140COMNTOOLS', 'VS120COMNTOOLS'):
                if e not in env:
                    continue

                vcdir = os.path.abspath(os.path.join(env[e], '../../VC/bin'))
                if os.path.exists(vcdir):
                    env['PATH'] = '%s;%s' % (vcdir, env['PATH'])
                    break

        # Add MOZ_OBJDIR to the env so that cygprofile.cpp can use it.
        env["MOZ_OBJDIR"] = build.topobjdir

        jarlog = env.get("JARLOG_FILE")
        if jarlog:
            abs_jarlog = os.path.abspath(jarlog)
            env["MOZ_JAR_LOG_FILE"] = abs_jarlog
            print("jarlog: %s" % abs_jarlog)

        # Write to an output file if we're running in automation
        process_args = []
        logfile = None
        if 'UPLOAD_PATH' in env:
            logfile = os.path.join(env['UPLOAD_PATH'], 'profile-run.log')

        # Run Firefox a first time to initialize its profile
        driver = Marionette(bin=binary, prefs=prefs, gecko_log=logfile)
        driver.start_session()
        driver.restart(in_app=True)

        # Now generate the profile and wait for it to complete
        driver.navigate("http://localhost:%d/index.html" % PORT)
        driver.execute_async_script(
            '''
            const [resolve] = arguments;
            window.addEventListener('message', event => {
                if (event.data === 'quit') {
                    resolve();
                }
            });
            ''',
            script_timeout=999999999)

        driver.quit(in_app=True)
        httpd.stop()
