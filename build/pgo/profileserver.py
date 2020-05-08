#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys
import glob
import subprocess

import mozcrash
from mozbuild.base import MozbuildObject, BinaryNotFoundException
from mozfile import TemporaryDirectory
from mozhttpd import MozHttpd
from mozprofile import FirefoxProfile, Preferences
from mozprofile.permissions import ServerLocations
from mozrunner import FirefoxRunner, CLI
from six import string_types

PORT = 8888

PATH_MAPPINGS = {
    '/webkit/PerformanceTests': 'third_party/webkit/PerformanceTests',
    # It is tempting to map to `testing/talos/talos/tests` instead, to avoid
    # writing `tests/` in every path, but we can't do that because some files
    # refer to scripts located in `../..`.
    '/talos': 'testing/talos/talos',
}


def get_crashreports(directory, name=None):
    rc = 0
    upload_path = os.environ.get('UPLOAD_PATH')
    if upload_path:
        # For automation, log the minidumps with stackwalk and get them moved to
        # the artifacts directory.
        fetches_dir = os.environ.get('MOZ_FETCHES_DIR')
        if not fetches_dir:
            raise Exception("Unable to process minidump in automation because "
                            "$MOZ_FETCHES_DIR is not set in the environment")
        stackwalk_binary = os.path.join(fetches_dir, 'minidump_stackwalk', 'minidump_stackwalk')
        if sys.platform == 'win32':
            stackwalk_binary += '.exe'
        minidump_path = os.path.join(directory, "minidumps")
        rc = mozcrash.check_for_crashes(
            minidump_path,
            symbols_path=fetches_dir,
            stackwalk_binary=stackwalk_binary,
            dump_save_path=upload_path,
            test_name=name,
        )
    return rc


if __name__ == '__main__':
    cli = CLI()
    debug_args, interactive = cli.debugger_arguments()
    runner_args = cli.runner_args()

    build = MozbuildObject.from_environment()

    binary = runner_args.get('binary')
    if not binary:
        try:
            binary = build.get_binary_path(where="staged-package")
        except BinaryNotFoundException as e:
            print('{}\n\n{}\n'.format(e, e.help()))
            sys.exit(1)
    binary = os.path.normpath(os.path.abspath(binary))

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

    old_profraw_files = glob.glob('*.profraw')
    for f in old_profraw_files:
        os.remove(f)

    with TemporaryDirectory() as profilePath:
        # TODO: refactor this into mozprofile
        profile_data_dir = os.path.join(build.topsrcdir, 'testing', 'profiles')
        with open(os.path.join(profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['profileserver']

        prefpaths = [os.path.join(profile_data_dir, profile, 'user.js')
                     for profile in base_profiles]

        prefs = {}
        for path in prefpaths:
            prefs.update(Preferences.read_prefs(path))

        interpolation = {"server": "%s:%d" % httpd.httpd.server_address}
        for k, v in prefs.items():
            if isinstance(v, string_types):
                v = v.format(**interpolation)
            prefs[k] = Preferences.cast(v)

        # Enforce e10s. This isn't in one of the user.js files because those
        # are shared with android, which doesn't want this on. We can't
        # interpolate because the formatting code only works for strings,
        # and this is a bool pref.
        prefs["browser.tabs.remote.autostart"] = True

        profile = FirefoxProfile(profile=profilePath,
                                 preferences=prefs,
                                 addons=[os.path.join(
                                     build.topsrcdir, 'tools', 'quitter',
                                     'quitter@mozilla.org.xpi')],
                                 locations=locations)

        env = os.environ.copy()
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["XPCOM_DEBUG_BREAK"] = "warn"
        # We disable sandboxing to make writing profiling data actually work
        # Bug 1553850 considers fixing this.
        env["MOZ_DISABLE_CONTENT_SANDBOX"] = "1"
        env["MOZ_DISABLE_RDD_SANDBOX"] = "1"

        # Ensure different pids write to different files
        env["LLVM_PROFILE_FILE"] = "default_%p_random_%m.profraw"

        # Write to an output file if we're running in automation
        process_args = {'universal_newlines': True}
        if 'UPLOAD_PATH' in env:
            process_args['logfile'] = os.path.join(env['UPLOAD_PATH'], 'profile-run-1.log')

        # Run Firefox a first time to initialize its profile
        runner = FirefoxRunner(profile=profile,
                               binary=binary,
                               cmdargs=['data:text/html,<script>Quitter.quit()</script>'],
                               env=env,
                               process_args=process_args)
        runner.start()
        ret = runner.wait()
        if ret:
            print("Firefox exited with code %d during profile initialization"
                  % ret)
            logfile = process_args.get('logfile')
            if logfile:
                print("Firefox output (%s):" % logfile)
                with open(logfile) as f:
                    print(f.read())
            httpd.stop()
            get_crashreports(profilePath, name='Profile initialization')
            sys.exit(ret)

        jarlog = os.getenv("JARLOG_FILE")
        if jarlog:
            env["MOZ_JAR_LOG_FILE"] = os.path.abspath(jarlog)
            print("jarlog: %s" % env["MOZ_JAR_LOG_FILE"])
            if os.path.exists(jarlog):
                os.remove(jarlog)

        if 'UPLOAD_PATH' in env:
            process_args['logfile'] = os.path.join(env['UPLOAD_PATH'], 'profile-run-2.log')
        cmdargs = ["http://localhost:%d/index.html" % PORT]
        runner = FirefoxRunner(profile=profile,
                               binary=binary,
                               cmdargs=cmdargs,
                               env=env,
                               process_args=process_args)
        runner.start(debug_args=debug_args, interactive=interactive)
        ret = runner.wait()
        httpd.stop()
        if ret:
            print("Firefox exited with code %d during profiling" % ret)
            logfile = process_args.get('logfile')
            if logfile:
                print("Firefox output (%s):" % logfile)
                with open(logfile) as f:
                    print(f.read())
            get_crashreports(profilePath, name='Profiling run')
            sys.exit(ret)

        # Try to move the crash reports to the artifacts even if Firefox appears
        # to exit successfully, in case there's a crash that doesn't set the
        # return code to non-zero for some reason.
        if get_crashreports(profilePath, name='Firefox exited successfully?') != 0:
            print("Firefox exited successfully, but produced a crashreport")
            sys.exit(1)

        llvm_profdata = env.get('LLVM_PROFDATA')
        if llvm_profdata:
            profraw_files = glob.glob('*.profraw')
            if not profraw_files:
                print('Could not find profraw files in the current directory: %s' % os.getcwd())
                sys.exit(1)
            merge_cmd = [
                llvm_profdata,
                'merge',
                '-o',
                'merged.profdata',
            ] + profraw_files
            rc = subprocess.call(merge_cmd)
            if rc != 0:
                print('INFRA-ERROR: Failed to merge profile data. Corrupt profile?')
                # exit with TBPL_RETRY
                sys.exit(4)
