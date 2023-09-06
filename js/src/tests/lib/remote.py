# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# remote.py -- Python library for Android devices.

import os
import posixpath


def push_libs(options, device, dest_dir):
    # This saves considerable time in pushing unnecessary libraries
    # to the device but needs to be updated if the dependencies change.
    required_libs = [
        "libnss3.so",
        "libmozglue.so",
        "libnspr4.so",
        "libplc4.so",
        "libplds4.so",
    ]

    for file in os.listdir(options.local_lib):
        if file in required_libs:
            local_file = os.path.join(options.local_lib, file)
            remote_file = posixpath.join(dest_dir, file)
            assert os.path.isfile(local_file)
            device.push(local_file, remote_file)
            device.chmod(remote_file)


def push_progs(options, device, progs, dest_dir):
    assert isinstance(progs, list)
    for local_file in progs:
        remote_file = posixpath.join(dest_dir, os.path.basename(local_file))
        assert os.path.isfile(local_file)
        device.push(local_file, remote_file)
        device.chmod(remote_file)


def init_remote_dir(device, path):
    device.rm(path, recursive=True, force=True)
    device.mkdir(path, parents=True)


# We only have one device per test run.
DEVICE = None


def init_device(options):
    # Initialize the device
    global DEVICE

    assert options.remote and options.js_shell

    if DEVICE is not None:
        return DEVICE

    from mozdevice import ADBDeviceFactory, ADBError, ADBTimeoutError

    try:
        if not options.local_lib:
            # if not specified, use the local directory containing
            # the js binary to find the necessary libraries.
            options.local_lib = posixpath.dirname(options.js_shell)

        # Try to find 'adb' off the build environment to automatically use the
        # .mozbuild version if possible. In test automation, we don't have
        # mozbuild available so use the default 'adb' that automation provides.
        try:
            from mozbuild.base import MozbuildObject
            from mozrunner.devices.android_device import get_adb_path

            context = MozbuildObject.from_environment()
            adb_path = get_adb_path(context)
        except ImportError:
            adb_path = "adb"

        DEVICE = ADBDeviceFactory(
            adb=adb_path,
            device=options.device_serial,
            test_root=options.remote_test_root,
        )

        bin_dir = posixpath.join(options.remote_test_root, "bin")
        tests_dir = posixpath.join(options.remote_test_root, "tests")
        temp_dir = posixpath.join(options.remote_test_root, "tmp")

        # Create directory structure on device
        init_remote_dir(DEVICE, options.remote_test_root)
        init_remote_dir(DEVICE, tests_dir)
        init_remote_dir(DEVICE, bin_dir)
        init_remote_dir(DEVICE, temp_dir)

        # Push js shell and libraries.
        push_libs(options, DEVICE, bin_dir)
        push_progs(options, DEVICE, [options.js_shell], bin_dir)

        # update options.js_shell to point to the js binary on the device
        options.js_shell = os.path.join(bin_dir, "js")

        return DEVICE

    except (ADBError, ADBTimeoutError):
        print("TEST-UNEXPECTED-FAIL | remote.py : Device initialization failed")
        raise
