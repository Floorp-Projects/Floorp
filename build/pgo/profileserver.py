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
from mozprofile import FirefoxProfile, Profile, Preferences
from mozprofile.permissions import ServerLocations
from mozrunner import FirefoxRunner, CLI
from six import string_types

PORT = 8888

PATH_MAPPINGS = {
    '/js-input/webkit/PerformanceTests': 'third_party/webkit/PerformanceTests',
}


if __name__ == '__main__':
    cli = CLI()
    debug_args, interactive = cli.debugger_arguments()

    build = MozbuildObject.from_environment()
    path_mappings = {
        k: os.path.join(build.topsrcdir, v)
        for k, v in PATH_MAPPINGS.items()
    }
    httpd = MozHttpd(port=PORT,
                     docroot=os.path.join(build.topsrcdir, "build", "pgo"),
                     path_mappings=path_mappings)
    httpd.start(block=False)

    locations = ServerLocations()
    locations.add_host(host='127.0.0.1',
                       port=PORT,
                       options='primary,privileged')

    with TemporaryDirectory() as profilePath:
        # TODO: refactor this into mozprofile
        profile_data_dir = os.path.join(build.topsrcdir, 'testing', 'profiles')
        with open(os.path.join(profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['profileserver']

        prefpaths = [os.path.join(profile_data_dir, profile, 'user.js')
                     for profile in base_profiles]
        prefpaths.append(os.path.join(build.topsrcdir, "build", "pgo", "prefs_override.js"))

        prefs = {}
        for path in prefpaths:
            prefs.update(Preferences.read_prefs(path))

        interpolation = {"server": "%s:%d" % httpd.httpd.server_address,
                         "OOP": "false"}
        for k, v in prefs.items():
            if isinstance(v, string_types):
                v = v.format(**interpolation)
            prefs[k] = Preferences.cast(v)

        profile = FirefoxProfile(profile=profilePath,
                                 preferences=prefs,
                                 addons=[os.path.join(
                                     build.topsrcdir, 'tools', 'quitter', 'quitter@mozilla.org.xpi')],
                                 locations=locations)

        env = os.environ.copy()
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["XPCOM_DEBUG_BREAK"] = "warn"

        # For VC12+, make sure we can find the right bitness of pgort1x0.dll
        if not substs.get('HAVE_64BIT_BUILD'):
            for e in ('VS140COMNTOOLS', 'VS120COMNTOOLS'):
                if e not in env:
                    continue

                vcdir = os.path.abspath(os.path.join(env[e], '../../VC/bin'))
                if os.path.exists(vcdir):
                    env['PATH'] = '%s;%s' % (vcdir, env['PATH'])
                    break

        # Run Firefox a first time to initialize its profile
        runner = FirefoxRunner(profile=profile,
                               binary=build.get_binary_path(
                                   where="staged-package"),
                               cmdargs=['javascript:Quitter.quit()'],
                               env=env)
        runner.start()
        runner.wait()

        jarlog = os.getenv("JARLOG_FILE")
        if jarlog:
            env["MOZ_JAR_LOG_FILE"] = os.path.abspath(jarlog)
            print "jarlog: %s" % env["MOZ_JAR_LOG_FILE"]

        cmdargs = ["http://localhost:%d/index.html" % PORT]
        runner = FirefoxRunner(profile=profile,
                               binary=build.get_binary_path(
                                   where="staged-package"),
                               cmdargs=cmdargs,
                               env=env)
        runner.start(debug_args=debug_args, interactive=interactive)
        runner.wait()
        httpd.stop()
