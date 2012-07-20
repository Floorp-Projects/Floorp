# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from abc import abstractmethod
import datetime
from mozprocess import ProcessHandlerMixin
import multiprocessing
import os
import re
import shutil
import socket
import subprocess
from telnetlib import Telnet
import tempfile
import time

from emulator_battery import EmulatorBattery


class LogcatProc(ProcessHandlerMixin):
    """Process handler for logcat which saves all output to a logfile.
    """

    def __init__(self, logfile, cmd, **kwargs):
        self.logfile = logfile
        kwargs.setdefault('processOutputLine', []).append(self.log_output)
        ProcessHandlerMixin.__init__(self, cmd, **kwargs)

    def log_output(self, line):
        f = open(self.logfile, 'a')
        f.write(line + "\n")
        f.flush()


class Emulator(object):

    deviceRe = re.compile(r"^emulator-(\d+)(\s*)(.*)$")

    def __init__(self, noWindow=False, logcat_dir=None, arch="x86",
                 emulatorBinary=None, res='480x800', userdata=None,
                 memory='512', partition_size='512'):
        self.port = None
        self._emulator_launched = False
        self.proc = None
        self.local_port = None
        self.telnet = None
        self._tmp_userdata = None
        self._adb_started = False
        self.logcat_dir = logcat_dir
        self.logcat_proc = None
        self.arch = arch
        self.binary = emulatorBinary
        self.memory = str(memory)
        self.partition_size = str(partition_size)
        self.res = res
        self.battery = EmulatorBattery(self)
        self.noWindow = noWindow
        self.dataImg = userdata
        self.copy_userdata = self.dataImg is None

    def __del__(self):
        if self.telnet:
            self.telnet.write('exit\n')
            self.telnet.read_all()

    @property
    def args(self):
        qemuArgs = [self.binary,
                    '-kernel', self.kernelImg,
                    '-sysdir', self.sysDir,
                    '-data', self.dataImg]
        if self.noWindow:
            qemuArgs.append('-no-window')
        qemuArgs.extend(['-memory', self.memory,
                         '-partition-size', self.partition_size,
                         '-verbose',
                         '-skin', self.res,
                         '-gpu', 'on',
                         '-qemu'] + self.tail_args)
        return qemuArgs

    @property
    def is_running(self):
        if self._emulator_launched:
            return self.proc is not None and self.proc.poll() is None
        else:
            return self.port is not None

    def _check_for_adb(self):
        if not os.path.exists(self.adb):
            adb = subprocess.Popen(['which', 'adb'],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
            retcode = adb.wait()
            if retcode:
                raise Exception('adb not found!')
            out = adb.stdout.read().strip()
            if len(out) and out.find('/') > -1:
                self.adb = out

    def _run_adb(self, args):
        args.insert(0, self.adb)
        adb = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        retcode = adb.wait()
        if retcode:
            raise Exception('adb terminated with exit code %d: %s'
            % (retcode, adb.stdout.read()))
        return adb.stdout.read()

    def _get_telnet_response(self, command=None):
        output = []
        assert(self.telnet)
        if command is not None:
            self.telnet.write('%s\n' % command)
        while True:
            line = self.telnet.read_until('\n')
            output.append(line.rstrip())
            if line.startswith('OK'):
                return output
            elif line.startswith('KO:'):
                raise Exception('bad telnet response: %s' % line)

    def _run_telnet(self, command):
        if not self.telnet:
            self.telnet = Telnet('localhost', self.port)
            self._get_telnet_response()
        return self._get_telnet_response(command)

    def close(self):
        if self.is_running and self._emulator_launched:
            self.proc.terminate()
            self.proc.wait()
        if self._adb_started:
            self._run_adb(['kill-server'])
            self._adb_started = False
        if self.proc:
            retcode = self.proc.poll()
            self.proc = None
            if self._tmp_userdata:
                os.remove(self._tmp_userdata)
                self._tmp_userdata = None
            return retcode
        if self.logcat_proc:
            self.logcat_proc.kill()
        return 0

    def _get_adb_devices(self):
        offline = set()
        online = set()
        output = self._run_adb(['devices'])
        for line in output.split('\n'):
            m = self.deviceRe.match(line)
            if m:
                if m.group(3) == 'offline':
                    offline.add(m.group(1))
                else:
                    online.add(m.group(1))
        return (online, offline)

    def restart(self):
        if not self._emulator_launched:
            return
        self.close()
        self.start()

    def start_adb(self):
        result = self._run_adb(['start-server'])
        # We keep track of whether we've started adb or not, so we know
        # if we need to kill it.
        if 'daemon started successfully' in result:
            self._adb_started = True
        else:
            self._adb_started = False

    def connect(self):
        self._check_for_adb()
        self.start_adb()

        online, offline = self._get_adb_devices()
        now = datetime.datetime.now()
        while online == set([]):
            time.sleep(1)
            if datetime.datetime.now() - now > datetime.timedelta(seconds=60):
                raise Exception('timed out waiting for emulator to be available')
            online, offline = self._get_adb_devices()
        self.port = int(list(online)[0])

    @abstractmethod
    def _locate_files(self):
        pass

    def start(self):
        self._locate_files()
        self.start_adb()

        qemu_args = self.args[:]
        if self.copy_userdata:
            # Make a copy of the userdata.img for this instance of the emulator
            # to use.
            self._tmp_userdata = tempfile.mktemp(prefix='emulator')
            shutil.copyfile(self.dataImg, self._tmp_userdata)
            qemu_args[qemu_args.index('-data') + 1] = self._tmp_userdata

        original_online, original_offline = self._get_adb_devices()

        self.proc = subprocess.Popen(qemu_args,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)

        online, offline = self._get_adb_devices()
        now = datetime.datetime.now()
        while online - original_online == set([]):
            time.sleep(1)
            if datetime.datetime.now() - now > datetime.timedelta(seconds=60):
                raise Exception('timed out waiting for emulator to start')
            online, offline = self._get_adb_devices()
        self.port = int(list(online - original_online)[0])
        self._emulator_launched = True

        if self.logcat_dir:
            self.save_logcat()

        # setup DNS fix for networking
        self._run_adb(['-s', 'emulator-%d' % self.port,
                       'shell', 'setprop', 'net.dns1', '10.0.2.3'])

    def _save_logcat_proc(self, filename, cmd):
        self.logcat_proc = LogcatProc(filename, cmd)
        self.logcat_proc.run()
        self.logcat_proc.waitForFinish()
        self.logcat_proc = None

    def rotate_log(self, srclog, index=1):
        """ Rotate a logfile, by recursively rotating logs further in the sequence,
            deleting the last file if necessary.
        """
        destlog = os.path.join(self.logcat_dir, 'emulator-%d.%d.log' % (self.port, index))
        if os.path.exists(destlog):
            if index == 3:
                os.remove(destlog)
            else:
                self.rotate_log(destlog, index + 1)
        shutil.move(srclog, destlog)

    def save_logcat(self):
        """ Save the output of logcat to a file.
        """
        filename = os.path.join(self.logcat_dir, "emulator-%d.log" % self.port)
        if os.path.exists(filename):
            self.rotate_log(filename)
        cmd = [self.adb, '-s', 'emulator-%d' % self.port, 'logcat']

        # We do this in a separate process because we call mozprocess's
        # waitForFinish method to process logcat's output, and this method
        # blocks.
        proc = multiprocessing.Process(target=self._save_logcat_proc, args=(filename, cmd))
        proc.daemon = True
        proc.start()

    def setup_port_forwarding(self, remote_port):
        """ Set up TCP port forwarding to the specified port on the device,
            using any availble local port, and return the local port.
        """

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(("", 0))
        local_port = s.getsockname()[1]
        s.close()

        output = self._run_adb(['-s', 'emulator-%d' % self.port,
                                'forward',
                                'tcp:%d' % local_port,
                                'tcp:%d' % remote_port])

        self.local_port = local_port

        return local_port

    def wait_for_port(self, timeout=300):
        assert(self.local_port)
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(('localhost', self.local_port))
                data = sock.recv(16)
                sock.close()
                if '"from"' in data:
                    return True
            except:
                import traceback
                print traceback.format_exc()
            time.sleep(1)
        return False
