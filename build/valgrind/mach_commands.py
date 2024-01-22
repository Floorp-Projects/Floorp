# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
import os
import time

import mozinfo
from mach.decorators import Command, CommandArgument
from mozbuild.base import BinaryNotFoundException
from mozbuild.base import MachCommandConditions as conditions


def is_valgrind_build(cls):
    """Must be a build with --enable-valgrind and --disable-jemalloc."""
    defines = cls.config_environment.defines
    return "MOZ_VALGRIND" in defines and "MOZ_MEMORY" not in defines


@Command(
    "valgrind-test",
    category="testing",
    conditions=[conditions.is_firefox_or_thunderbird, is_valgrind_build],
    description="Run the Valgrind test job (memory-related errors).",
)
@CommandArgument(
    "--suppressions",
    default=[],
    action="append",
    metavar="FILENAME",
    help="Specify a suppression file for Valgrind to use. Use "
    "--suppression multiple times to specify multiple suppression "
    "files.",
)
def valgrind_test(command_context, suppressions):
    """
    Run Valgrind tests.
    """

    from mozfile import TemporaryDirectory
    from mozhttpd import MozHttpd
    from mozprofile import FirefoxProfile, Preferences
    from mozprofile.permissions import ServerLocations
    from mozrunner import FirefoxRunner
    from mozrunner.utils import findInPath
    from six import string_types
    from valgrind.output_handler import OutputHandler

    build_dir = os.path.join(command_context.topsrcdir, "build")

    # XXX: currently we just use the PGO inputs for Valgrind runs.  This may
    # change in the future.
    httpd = MozHttpd(docroot=os.path.join(build_dir, "pgo"))
    httpd.start(block=False)

    with TemporaryDirectory() as profilePath:
        # TODO: refactor this into mozprofile
        profile_data_dir = os.path.join(
            command_context.topsrcdir, "testing", "profiles"
        )
        with open(os.path.join(profile_data_dir, "profiles.json"), "r") as fh:
            base_profiles = json.load(fh)["valgrind"]

        prefpaths = [
            os.path.join(profile_data_dir, profile, "user.js")
            for profile in base_profiles
        ]
        prefs = {}
        for path in prefpaths:
            prefs.update(Preferences.read_prefs(path))

        interpolation = {
            "server": "%s:%d" % httpd.httpd.server_address,
        }
        for k, v in prefs.items():
            if isinstance(v, string_types):
                v = v.format(**interpolation)
            prefs[k] = Preferences.cast(v)

        quitter = os.path.join(
            command_context.topsrcdir, "tools", "quitter", "quitter@mozilla.org.xpi"
        )

        locations = ServerLocations()
        locations.add_host(
            host="127.0.0.1", port=httpd.httpd.server_port, options="primary"
        )

        profile = FirefoxProfile(
            profile=profilePath,
            preferences=prefs,
            addons=[quitter],
            locations=locations,
        )

        firefox_args = [httpd.get_url()]

        env = os.environ.copy()
        env["G_SLICE"] = "always-malloc"
        env["MOZ_FORCE_DISABLE_E10S"] = "1"
        env["MOZ_CC_RUN_DURING_SHUTDOWN"] = "1"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"
        env["XPCOM_DEBUG_BREAK"] = "warn"

        outputHandler = OutputHandler(command_context.log)
        kp_kwargs = {
            "processOutputLine": [outputHandler],
            "universal_newlines": True,
        }

        valgrind = "valgrind"
        if not os.path.exists(valgrind):
            valgrind = findInPath(valgrind)

        valgrind_args = [
            valgrind,
            "--sym-offsets=yes",
            "--smc-check=all-non-file",
            "--vex-iropt-register-updates=allregs-at-mem-access",
            "--gen-suppressions=all",
            "--num-callers=36",
            "--leak-check=full",
            "--show-possibly-lost=no",
            "--track-origins=yes",
            "--trace-children=yes",
            "--trace-children-skip=*/dbus-launch",
            "-v",  # Enable verbosity to get the list of used suppressions
            # Avoid excessive delays in the presence of spinlocks.
            # See bug 1309851.
            "--fair-sched=yes",
            # Keep debuginfo after library unmap.  See bug 1382280.
            "--keep-debuginfo=yes",
            # Reduce noise level on rustc and/or LLVM compiled code.
            # See bug 1365915
            "--expensive-definedness-checks=yes",
            # Compensate for the compiler inlining `new` but not `delete`
            # or vice versa.
            "--show-mismatched-frees=no",
        ]

        for s in suppressions:
            valgrind_args.append("--suppressions=" + s)

        supps_dir = os.path.join(build_dir, "valgrind")
        supps_file1 = os.path.join(supps_dir, "cross-architecture.sup")
        valgrind_args.append("--suppressions=" + supps_file1)

        if mozinfo.os == "linux":
            machtype = {
                "x86_64": "x86_64-pc-linux-gnu",
                "x86": "i386-pc-linux-gnu",
            }.get(mozinfo.processor)
            if machtype:
                supps_file2 = os.path.join(supps_dir, machtype + ".sup")
                if os.path.isfile(supps_file2):
                    valgrind_args.append("--suppressions=" + supps_file2)

        exitcode = None
        timeout = 2400
        binary_not_found_exception = None
        try:
            runner = FirefoxRunner(
                profile=profile,
                binary=command_context.get_binary_path(),
                cmdargs=firefox_args,
                env=env,
                process_args=kp_kwargs,
            )
            start_time = time.monotonic()
            runner.start(debug_args=valgrind_args)
            exitcode = runner.wait(timeout=timeout)
            end_time = time.monotonic()
            if "MOZ_AUTOMATION" in os.environ:
                data = {
                    "framework": {"name": "build_metrics"},
                    "suites": [
                        {
                            "name": "valgrind",
                            "value": end_time - start_time,
                            "lowerIsBetter": True,
                            "shouldAlert": False,
                            "subtests": [],
                        }
                    ],
                }
                if "TASKCLUSTER_INSTANCE_TYPE" in os.environ:
                    # Include the instance type so results can be grouped.
                    data["suites"][0]["extraOptions"] = [
                        "taskcluster-%s" % os.environ["TASKCLUSTER_INSTANCE_TYPE"],
                    ]
                command_context.log(
                    logging.INFO,
                    "valgrind-perfherder",
                    {"data": json.dumps(data)},
                    "PERFHERDER_DATA: {data}",
                )
        except BinaryNotFoundException as e:
            binary_not_found_exception = e
        finally:
            errs = outputHandler.error_count
            supps = outputHandler.suppression_count
            if errs != supps:
                status = 1  # turns the TBPL job orange
                command_context.log(
                    logging.ERROR,
                    "valgrind-fail-parsing",
                    {"errs": errs, "supps": supps},
                    "TEST-UNEXPECTED-FAIL | valgrind-test | error parsing: {errs} errors "
                    "seen, but {supps} generated suppressions seen",
                )

            elif errs == 0:
                status = 0
                command_context.log(
                    logging.INFO,
                    "valgrind-pass",
                    {},
                    "TEST-PASS | valgrind-test | valgrind found no errors",
                )
            else:
                status = 1  # turns the TBPL job orange
                # We've already printed details of the errors.

            if binary_not_found_exception:
                status = 2  # turns the TBPL job red
                command_context.log(
                    logging.ERROR,
                    "valgrind-fail-errors",
                    {"error": str(binary_not_found_exception)},
                    "TEST-UNEXPECTED-FAIL | valgrind-test | {error}",
                )
                command_context.log(
                    logging.INFO,
                    "valgrind-fail-errors",
                    {"help": binary_not_found_exception.help()},
                    "{help}",
                )
            elif exitcode is None:
                status = 2  # turns the TBPL job red
                command_context.log(
                    logging.ERROR,
                    "valgrind-fail-timeout",
                    {"timeout": timeout},
                    "TEST-UNEXPECTED-FAIL | valgrind-test | Valgrind timed out "
                    "(reached {timeout} second limit)",
                )
            elif exitcode != 0:
                status = 2  # turns the TBPL job red
                command_context.log(
                    logging.ERROR,
                    "valgrind-fail-errors",
                    {"exitcode": exitcode},
                    "TEST-UNEXPECTED-FAIL | valgrind-test | non-zero exit code "
                    "from Valgrind: {exitcode}",
                )

            httpd.stop()

        return status
