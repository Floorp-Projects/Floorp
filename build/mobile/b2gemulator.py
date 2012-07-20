# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import platform

from emulator import Emulator


class B2GEmulator(Emulator):

    def __init__(self, homedir=None, noWindow=False, logcat_dir=None, arch="x86",
                 emulatorBinary=None, res='480x800', userdata=None,
                 memory='512', partition_size='512'):
        super(B2GEmulator, self).__init__(noWindow=noWindow, logcat_dir=logcat_dir,
                                          arch=arch, emulatorBinary=emulatorBinary,
                                          res=res, userdata=userdata,
                                          memory=memory, partition_size=partition_size)
        self.homedir = homedir
        if self.homedir is not None:
            self.homedir = os.path.expanduser(homedir)

    def _check_file(self, filePath):
        if not os.path.exists(filePath):
            raise Exception(('File not found: %s; did you pass the B2G home '
                             'directory as the homedir parameter, or set '
                             'B2G_HOME correctly?') % filePath)

    def _check_for_adb(self, host_dir):
        self.adb = os.path.join(self.homedir, 'out', 'host', host_dir, 'bin', 'adb')
        if not os.path.exists(self.adb):
            self.adb = os.path.join(self.homedir, 'bin/adb')
        super(B2GEmulator, self)._check_for_adb()

    def _locate_files(self):
        if self.homedir is None:
            self.homedir = os.getenv('B2G_HOME')
        if self.homedir is None:
            raise Exception('Must define B2G_HOME or pass the homedir parameter')
        self._check_file(self.homedir)

        if self.arch not in ("x86", "arm"):
            raise Exception("Emulator architecture must be one of x86, arm, got: %s" %
                            self.arch)

        host_dir = "linux-x86"
        if platform.system() == "Darwin":
            host_dir = "darwin-x86"

        host_bin_dir = os.path.join("out", "host", host_dir, "bin")

        if self.arch == "x86":
            binary = os.path.join(host_bin_dir, "emulator-x86")
            kernel = "prebuilts/qemu-kernel/x86/kernel-qemu"
            sysdir = "out/target/product/generic_x86"
            self.tail_args = []
        else:
            binary = os.path.join(host_bin_dir, "emulator")
            kernel = "prebuilts/qemu-kernel/arm/kernel-qemu-armv7"
            sysdir = "out/target/product/generic"
            self.tail_args = ["-cpu", "cortex-a8"]

        self._check_for_adb(host_dir)

        if not self.binary:
            self.binary = os.path.join(self.homedir, binary)

        self._check_file(self.binary)

        self.kernelImg = os.path.join(self.homedir, kernel)
        self._check_file(self.kernelImg)

        self.sysDir = os.path.join(self.homedir, sysdir)
        self._check_file(self.sysDir)

        if not self.dataImg:
            self.dataImg = os.path.join(self.sysDir, 'userdata.img')
        self._check_file(self.dataImg)
