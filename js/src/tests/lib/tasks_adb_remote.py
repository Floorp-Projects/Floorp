# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */
from __future__ import print_function, unicode_literals, division

import os
import posixpath
import sys
import tempfile

from datetime import timedelta
from mozdevice import ADBDevice, ADBError, ADBTimeoutError, ADBProcessError

from .results import TestOutput, escape_cmdline
from .adaptor import xdr_annotate
from .remote import init_device

TESTS_LIB_DIR = os.path.dirname(os.path.abspath(__file__))
JS_DIR = os.path.dirname(os.path.dirname(TESTS_LIB_DIR))
JS_TESTS_DIR = posixpath.join(JS_DIR, "tests")
TEST_DIR = os.path.join(JS_DIR, "jit-test", "tests")


def aggregate_script_stdout(stdout_lines, prefix, tempdir, uniq_tag, tests, options):
    test = None
    tStart = None
    cmd = ""
    stdout = ""

    # Use to debug this script in case of assertion failure.
    meta_history = []
    last_line = ""

    # Assert that the streamed content is not interrupted.
    ended = False

    # Check if the tag is present, if so, this is controlled output
    # produced by the test runner, otherwise this is stdout content.
    try:
        for line in stdout_lines:
            last_line = line
            if line.startswith(uniq_tag):
                meta = line[len(uniq_tag) :].strip()
                meta_history.append(meta)
                if meta.startswith("START="):
                    assert test is None
                    params = meta[len("START=") :].split(",")
                    test_idx = int(params[0])
                    test = tests[test_idx]
                    tStart = timedelta(seconds=float(params[1]))
                    cmd = test.command(
                        prefix,
                        posixpath.join(options.remote_test_root, "lib/"),
                        posixpath.join(options.remote_test_root, "modules/"),
                        tempdir,
                        posixpath.join(options.remote_test_root, "tests"),
                    )
                    stdout = ""
                    if options.show_cmd:
                        print(escape_cmdline(cmd))
                elif meta.startswith("STOP="):
                    assert test is not None
                    params = meta[len("STOP=") :].split(",")
                    exitcode = int(params[0])
                    dt = timedelta(seconds=float(params[1])) - tStart
                    yield TestOutput(
                        test,
                        cmd,
                        stdout,
                        # NOTE: mozdevice fuse stdout and stderr. Thus, we are
                        # using stdout for both stdout and stderr. So far,
                        # doing so did not cause any issues.
                        stdout,
                        exitcode,
                        dt.total_seconds(),
                        dt > timedelta(seconds=int(options.timeout)),
                    )
                    stdout = ""
                    cmd = ""
                    test = None
                elif meta.startswith("RETRY="):
                    # On timeout, we discard the first timeout to avoid a
                    # random hang on pthread_join.
                    assert test is not None
                    stdout = ""
                    cmd = ""
                    test = None
                else:
                    assert meta.startswith("THE_END")
                    ended = True
            else:
                assert uniq_tag not in line
                stdout += line

        # This assertion fails if the streamed content is interrupted, either
        # by unplugging the phone or some adb failures.
        assert ended
    except AssertionError as e:
        sys.stderr.write("Metadata history:\n{}\n".format("\n".join(meta_history)))
        sys.stderr.write("Last line: {}\n".format(last_line))
        raise e


def setup_device(prefix, options):
    try:
        device = init_device(options)

        def replace_lib_file(path, name):
            localfile = os.path.join(JS_TESTS_DIR, *path)
            remotefile = posixpath.join(options.remote_test_root, "lib", name)
            device.push(localfile, remotefile, timeout=10)

        prefix[0] = posixpath.join(options.remote_test_root, "bin", "js")
        tempdir = posixpath.join(options.remote_test_root, "tmp")

        # Push tests & lib directories.
        device.push(os.path.dirname(TEST_DIR), options.remote_test_root, timeout=600)

        # Substitute lib files which are aliasing non262 files.
        replace_lib_file(["non262", "shell.js"], "non262.js")
        replace_lib_file(["non262", "reflect-parse", "Match.js"], "match.js")
        replace_lib_file(["non262", "Math", "shell.js"], "math.js")
        device.chmod(options.remote_test_root, recursive=True)

        print("tasks_adb_remote.py : Device initialization completed")
        return device, tempdir
    except (ADBError, ADBTimeoutError):
        print(
            "TEST-UNEXPECTED-FAIL | tasks_adb_remote.py : "
            + "Device initialization failed"
        )
        raise


def script_preamble(tag, prefix, options):
    timeout = int(options.timeout)
    retry = int(options.timeout_retry)
    lib_path = os.path.dirname(prefix[0])
    return """
export LD_LIBRARY_PATH={lib_path}

do_test()
{{
    local idx=$1; shift;
    local attempt=$1; shift;

    # Read 10ms timestamp in seconds using shell builtins and /proc/uptime.
    local time;
    local unused;

    # When printing the tag, we prefix by a new line, in case the
    # previous command output did not contain any new line.
    read time unused < /proc/uptime
    echo '\\n{tag}START='$idx,$time
    timeout {timeout}s "$@"
    local rc=$?
    read time unused < /proc/uptime

    # Retry on timeout, to mute unlikely pthread_join hang issue.
    #
    # The timeout command send a SIGTERM signal, which should return 143
    # (=128+15). However, due to a bug in tinybox, it returns 142.
    if test \( $rc -eq 143 -o $rc -eq 142 \) -a $attempt -lt {retry}; then
      echo '\\n{tag}RETRY='$rc,$time
      attempt=$((attempt + 1))
      do_test $idx $attempt "$@"
    else
      echo '\\n{tag}STOP='$rc,$time
    fi
}}

do_end()
{{
    echo '\\n{tag}THE_END'
}}
""".format(
        tag=tag, lib_path=lib_path, timeout=timeout, retry=retry
    )


def setup_script(device, prefix, tempdir, options, uniq_tag, tests):
    timeout = int(options.timeout)
    script_timeout = 0
    try:
        print("tasks_adb_remote.py : Create batch script")
        tmpf = tempfile.NamedTemporaryFile(mode="w", delete=False)
        tmpf.write(script_preamble(uniq_tag, prefix, options))
        for i, test in enumerate(tests):
            # This test is common to all tasks_*.py files, however, jit-test do
            # not provide the `run_skipped` option, and all tests are always
            # enabled.
            assert test.enable  # and not options.run_skipped
            if options.test_reflect_stringify:
                raise ValueError("can't run Reflect.stringify tests remotely")

            cmd = test.command(
                prefix,
                posixpath.join(options.remote_test_root, "lib/"),
                posixpath.join(options.remote_test_root, "modules/"),
                tempdir,
                posixpath.join(options.remote_test_root, "tests"),
            )

            # replace with shlex.join when move to Python 3.8+
            cmd = ADBDevice._escape_command_line(cmd)

            env = {}
            if test.tz_pacific:
                env["TZ"] = "PST8PDT"
            envStr = "".join(key + "='" + val + "' " for key, val in env.items())

            tmpf.write("{}do_test {} 0 {};\n".format(envStr, i, cmd))
            script_timeout += timeout
        tmpf.write("do_end;\n")
        tmpf.close()
        script = posixpath.join(options.remote_test_root, "test_manifest.sh")
        device.push(tmpf.name, script)
        device.chmod(script)
        print("tasks_adb_remote.py : Batch script created")
    except Exception as e:
        print("tasks_adb_remote.py : Batch script failed")
        raise e
    finally:
        if tmpf:
            os.unlink(tmpf.name)
    return script, script_timeout


def start_script(
    device, prefix, tempdir, script, uniq_tag, script_timeout, tests, options
):
    env = {}

    # Allow ADBError or ADBTimeoutError to terminate the test run, but handle
    # ADBProcessError in order to support the use of non-zero exit codes in the
    # JavaScript shell tests.
    #
    # The stdout_callback will aggregate each output line, and reconstruct the
    # output produced by each test, and queue TestOutput in the qResult queue.
    try:
        adb_process = device.shell(
            "sh {}".format(script),
            env=env,
            cwd=options.remote_test_root,
            timeout=script_timeout,
            yield_stdout=True,
        )
        for test_output in aggregate_script_stdout(
            adb_process, prefix, tempdir, uniq_tag, tests, options
        ):
            yield test_output
        print("tasks_adb_remote.py : Finished")
    except ADBProcessError as e:
        # After a device error, the device is typically in a
        # state where all further tests will fail so there is no point in
        # continuing here.
        sys.stderr.write("Error running remote tests: {}".format(repr(e)))


def get_remote_results(tests, prefix, pb, options):
    """Create a script which batches the run of all tests, and spawn a thread to
    reconstruct the TestOutput for each test. This is made to avoid multiple
    `adb.shell` commands which has a high latency.
    """
    device, tempdir = setup_device(prefix, options)

    # Tests are sequentially executed in a batch. The first test executed is in
    # charge of creating the xdr file for the self-hosted code.
    if options.use_xdr:
        tests = xdr_annotate(tests, options)

    # We need tests to be subscriptable to find the test structure matching the
    # index within the generated script.
    tests = list(tests)

    # Create a script which spawn each test one after the other, and upload the
    # script
    uniq_tag = "@@@TASKS_ADB_REMOTE@@@"
    script, script_timeout = setup_script(
        device, prefix, tempdir, options, uniq_tag, tests
    )

    for test_output in start_script(
        device, prefix, tempdir, script, uniq_tag, script_timeout, tests, options
    ):
        yield test_output
