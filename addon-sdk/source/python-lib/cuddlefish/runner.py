# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import time
import tempfile
import atexit
import shlex
import subprocess
import re
import shutil

import mozrunner
from cuddlefish.prefs import DEFAULT_COMMON_PREFS
from cuddlefish.prefs import DEFAULT_FIREFOX_PREFS
from cuddlefish.prefs import DEFAULT_THUNDERBIRD_PREFS
from cuddlefish.prefs import DEFAULT_FENNEC_PREFS

# Used to remove noise from ADB output
CLEANUP_ADB = re.compile(r'^(I|E)/(stdout|stderr|GeckoConsole)\s*\(\s*\d+\):\s*(.*)$')
# Used to filter only messages send by `console` module
FILTER_ONLY_CONSOLE_FROM_ADB = re.compile(r'^I/(stdout|stderr)\s*\(\s*\d+\):\s*((info|warning|error|debug): .*)$')

# Used to detect the currently running test
PARSEABLE_TEST_NAME = re.compile(r'TEST-START \| ([^\n]+)\n')

# Maximum time we'll wait for tests to finish, in seconds.
# The purpose of this timeout is to recover from infinite loops.  It should be
# longer than the amount of time any test run takes, including those on slow
# machines running slow (debug) versions of Firefox.
RUN_TIMEOUT = 30 * 60 # 30 minutes

# Maximum time we'll wait for tests to emit output, in seconds.
# The purpose of this timeout is to recover from hangs.  It should be longer
# than the amount of time any test takes to report results.
OUTPUT_TIMEOUT = 60 # one minute

def follow_file(filename):
    """
    Generator that yields the latest unread content from the given
    file, or None if no new content is available.

    For example:

      >>> f = open('temp.txt', 'w')
      >>> f.write('hello')
      >>> f.flush()
      >>> tail = follow_file('temp.txt')
      >>> tail.next()
      'hello'
      >>> tail.next() is None
      True
      >>> f.write('there')
      >>> f.flush()
      >>> tail.next()
      'there'
      >>> f.close()
      >>> os.remove('temp.txt')
    """

    last_pos = 0
    last_size = 0
    while True:
        newstuff = None
        if os.path.exists(filename):
            size = os.stat(filename).st_size
            if size > last_size:
                last_size = size
                f = open(filename, 'r')
                f.seek(last_pos)
                newstuff = f.read()
                last_pos = f.tell()
                f.close()
        yield newstuff

# subprocess.check_output only appeared in python2.7, so this code is taken
# from python source code for compatibility with py2.5/2.6
class CalledProcessError(Exception):
    def __init__(self, returncode, cmd, output=None):
        self.returncode = returncode
        self.cmd = cmd
        self.output = output
    def __str__(self):
        return "Command '%s' returned non-zero exit status %d" % (self.cmd, self.returncode)

def check_output(*popenargs, **kwargs):
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overridden.')
    process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise CalledProcessError(retcode, cmd, output=output)
    return output


class FennecProfile(mozrunner.Profile):
    preferences = {}
    names = ['fennec']

class FennecRunner(mozrunner.Runner):
    profile_class = FennecProfile

    names = ['fennec']

    __DARWIN_PATH = '/Applications/Fennec.app/Contents/MacOS/fennec'

    def __init__(self, binary=None, **kwargs):
        if sys.platform == 'darwin' and binary and binary.endswith('.app'):
            # Assume it's a Fennec app dir.
            binary = os.path.join(binary, 'Contents/MacOS/fennec')

        self.__real_binary = binary

        mozrunner.Runner.__init__(self, **kwargs)

    def find_binary(self):
        if not self.__real_binary:
            if sys.platform == 'darwin':
                if os.path.exists(self.__DARWIN_PATH):
                    return self.__DARWIN_PATH
            self.__real_binary = mozrunner.Runner.find_binary(self)
        return self.__real_binary

FENNEC_REMOTE_PATH = '/mnt/sdcard/jetpack-profile'

class RemoteFennecRunner(mozrunner.Runner):
    profile_class = FennecProfile

    names = ['fennec']

    _INTENT_PREFIX = 'org.mozilla.'

    _adb_path = None

    def __init__(self, binary=None, **kwargs):
        # Check that we have a binary set
        if not binary:
            raise ValueError("You have to define `--binary` option set to the "
                            "path to your ADB executable.")
        # Ensure that binary refer to a valid ADB executable
        output = subprocess.Popen([binary], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE).communicate()
        output = "".join(output)
        if not ("Android Debug Bridge" in output):
            raise ValueError("`--binary` option should be the path to your "
                            "ADB executable.")
        self.binary = binary

        mobile_app_name = kwargs['cmdargs'][0]
        self.profile = kwargs['profile']
        self._adb_path = binary

        # This pref has to be set to `false` otherwise, we do not receive
        # output of adb commands!
        subprocess.call([self._adb_path, "shell",
                        "setprop log.redirect-stdio false"])

        # Android apps are launched by their "intent" name,
        # Automatically detect already installed firefox by using `pm` program
        # or use name given as cfx `--mobile-app` argument.
        intents = self.getIntentNames()
        if not intents:
            raise ValueError("Unable to found any Firefox "
                             "application on your device.")
        elif mobile_app_name:
            if not mobile_app_name in intents:
                raise ValueError("Unable to found Firefox application "
                                 "with intent name '%s'\n"
                                 "Available ones are: %s" %
                                 (mobile_app_name, ", ".join(intents)))
            self._intent_name = self._INTENT_PREFIX + mobile_app_name
        else:
            if "firefox" in intents:
                self._intent_name = self._INTENT_PREFIX + "firefox"
            elif "firefox_beta" in intents:
                self._intent_name = self._INTENT_PREFIX + "firefox_beta"
            elif "firefox_nightly" in intents:
                self._intent_name = self._INTENT_PREFIX + "firefox_nightly"
            else:
                self._intent_name = self._INTENT_PREFIX + intents[0]

        print "Launching mobile application with intent name " + self._intent_name

        # First try to kill firefox if it is already running
        pid = self.getProcessPID(self._intent_name)
        if pid != None:
            print "Killing running Firefox instance ..."
            subprocess.call([self._adb_path, "shell",
                             "am force-stop " + self._intent_name])
            time.sleep(2)
            if self.getProcessPID(self._intent_name) != None:
                raise Exception("Unable to automatically kill running Firefox" +
                                " instance. Please close it manually before " +
                                "executing cfx.")

        print "Pushing the addon to your device"

        # Create a clean empty profile on the sd card
        subprocess.call([self._adb_path, "shell", "rm -r " + FENNEC_REMOTE_PATH])
        subprocess.call([self._adb_path, "shell", "mkdir " + FENNEC_REMOTE_PATH])

        # Push the profile folder created by mozrunner to the device
        # (we can't simply use `adb push` as it doesn't copy empty folders)
        localDir = self.profile.profile
        remoteDir = FENNEC_REMOTE_PATH
        for root, dirs, files in os.walk(localDir, followlinks='true'):
            relRoot = os.path.relpath(root, localDir)
            # Note about os.path usage below:
            # Local files may be using Windows `\` separators but
            # remote are always `/`, so we need to convert local ones to `/`
            for file in files:
                localFile = os.path.join(root, file)
                remoteFile = remoteDir.replace("/", os.sep)
                if relRoot != ".":
                    remoteFile = os.path.join(remoteFile, relRoot)
                remoteFile = os.path.join(remoteFile, file)
                remoteFile = "/".join(remoteFile.split(os.sep))
                subprocess.Popen([self._adb_path, "push", localFile, remoteFile],
                                 stderr=subprocess.PIPE).wait()
            for dir in dirs:
                targetDir = remoteDir.replace("/", os.sep)
                if relRoot != ".":
                    targetDir = os.path.join(targetDir, relRoot)
                targetDir = os.path.join(targetDir, dir)
                targetDir = "/".join(targetDir.split(os.sep))
                # `-p` option is not supported on all devices!
                subprocess.call([self._adb_path, "shell", "mkdir " + targetDir])

    @property
    def command(self):
        """Returns the command list to run."""
        return [self._adb_path,
            "shell",
            "am start " +
                "-a android.activity.MAIN " +
                "-n " + self._intent_name + "/" + self._intent_name + ".App " +
                "--es args \"-profile " + FENNEC_REMOTE_PATH + "\""
        ]

    def start(self):
        subprocess.call(self.command)

    def getProcessPID(self, processName):
        p = subprocess.Popen([self._adb_path, "shell", "ps"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        line = p.stdout.readline()
        while line:
            columns = line.split()
            pid = columns[1]
            name = columns[-1]
            line = p.stdout.readline()
            if processName in name:
                return pid
        return None

    def getIntentNames(self):
        p = subprocess.Popen([self._adb_path, "shell", "pm list packages"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        names = []
        for line in p.stdout.readlines():
            line = re.sub("(^package:)|\s", "", line)
            if self._INTENT_PREFIX in line:
                names.append(line.replace(self._INTENT_PREFIX, ""))
        return names


class XulrunnerAppProfile(mozrunner.Profile):
    preferences = {}
    names = []

class XulrunnerAppRunner(mozrunner.Runner):
    """
    Runner for any XULRunner app. Can use a Firefox binary in XULRunner
    mode to execute the app, or can use XULRunner itself. Expects the
    app's application.ini to be passed in as one of the items in
    'cmdargs' in the constructor.

    This class relies a lot on the particulars of mozrunner.Runner's
    implementation, and does some unfortunate acrobatics to get around
    some of the class' limitations/assumptions.
    """

    profile_class = XulrunnerAppProfile

    # This is a default, and will be overridden in the instance if
    # Firefox is used in XULRunner mode.
    names = ['xulrunner']

    # Default location of XULRunner on OS X.
    __DARWIN_PATH = "/Library/Frameworks/XUL.framework/xulrunner-bin"
    __LINUX_PATH  = "/usr/bin/xulrunner"

    # What our application.ini's path looks like if it's part of
    # an "installed" XULRunner app on OS X.
    __DARWIN_APP_INI_SUFFIX = '.app/Contents/Resources/application.ini'

    def __init__(self, binary=None, **kwargs):
        if sys.platform == 'darwin' and binary and binary.endswith('.app'):
            # Assume it's a Firefox app dir.
            binary = os.path.join(binary, 'Contents/MacOS/firefox-bin')

        self.__app_ini = None
        self.__real_binary = binary

        mozrunner.Runner.__init__(self, **kwargs)

        # See if we're using a genuine xulrunner-bin from the XULRunner SDK,
        # or if we're being asked to use Firefox in XULRunner mode.
        self.__is_xulrunner_sdk = 'xulrunner' in self.binary

        if sys.platform == 'linux2' and not self.env.get('LD_LIBRARY_PATH'):
            self.env['LD_LIBRARY_PATH'] = os.path.dirname(self.binary)

        newargs = []
        for item in self.cmdargs:
            if 'application.ini' in item:
                self.__app_ini = item
            else:
                newargs.append(item)
        self.cmdargs = newargs

        if not self.__app_ini:
            raise ValueError('application.ini not found in cmdargs')
        if not os.path.exists(self.__app_ini):
            raise ValueError("file does not exist: '%s'" % self.__app_ini)

        if (sys.platform == 'darwin' and
            self.binary == self.__DARWIN_PATH and
            self.__app_ini.endswith(self.__DARWIN_APP_INI_SUFFIX)):
            # If the application.ini is in an app bundle, then
            # it could be inside an "installed" XULRunner app.
            # If this is the case, use the app's actual
            # binary instead of the XUL framework's, so we get
            # a proper app icon, etc.
            new_binary = '/'.join(self.__app_ini.split('/')[:-2] +
                                  ['MacOS', 'xulrunner'])
            if os.path.exists(new_binary):
                self.binary = new_binary

    @property
    def command(self):
        """Returns the command list to run."""

        if self.__is_xulrunner_sdk:
            return [self.binary, self.__app_ini, '-profile',
                    self.profile.profile]
        else:
            return [self.binary, '-app', self.__app_ini, '-profile',
                    self.profile.profile]

    def __find_xulrunner_binary(self):
        if sys.platform == 'darwin':
            if os.path.exists(self.__DARWIN_PATH):
                return self.__DARWIN_PATH
        if sys.platform == 'linux2':
            if os.path.exists(self.__LINUX_PATH):
                return self.__LINUX_PATH
        return None

    def find_binary(self):
        # This gets called by the superclass constructor. It will
        # always get called, even if a binary was passed into the
        # constructor, because we want to have full control over
        # what the exact setting of self.binary is.

        if not self.__real_binary:
            self.__real_binary = self.__find_xulrunner_binary()
            if not self.__real_binary:
                dummy_profile = {}
                runner = mozrunner.FirefoxRunner(profile=dummy_profile)
                self.__real_binary = runner.find_binary()
                self.names = runner.names
        return self.__real_binary

def set_overloaded_modules(env_root, app_type, addon_id, preferences, overloads):
    # win32 file scheme needs 3 slashes
    desktop_file_scheme = "file://"
    if not env_root.startswith("/"):
      desktop_file_scheme = desktop_file_scheme + "/"

    pref_prefix = "extensions.modules." + addon_id + ".path"

    # Set preferences that will map require prefix to a given path
    for name, path in overloads.items():
        if len(name) == 0:
            prefName = pref_prefix
        else:
            prefName = pref_prefix + "." + name
        if app_type == "fennec-on-device":
            # For testing on device, we have to copy overloaded files from fs
            # to the device and use device path instead of local fs path.
            # Actual copy of files if done after the call to Profile constructor
            preferences[prefName] = "file://" + \
                FENNEC_REMOTE_PATH + "/overloads/" + name
        else:
            preferences[prefName] = desktop_file_scheme + \
                path.replace("\\", "/") + "/"

def run_app(harness_root_dir, manifest_rdf, harness_options,
            app_type, binary=None, profiledir=None, verbose=False,
            parseable=False, enforce_timeouts=False,
            logfile=None, addons=None, args=None, extra_environment={},
            norun=None,
            used_files=None, enable_mobile=False,
            mobile_app_name=None,
            env_root=None,
            is_running_tests=False,
            overload_modules=False,
            bundle_sdk=True):
    if binary:
        binary = os.path.expanduser(binary)

    if addons is None:
        addons = []
    else:
        addons = list(addons)

    cmdargs = []
    preferences = dict(DEFAULT_COMMON_PREFS)

    # For now, only allow running on Mobile with --force-mobile argument
    if app_type in ["fennec", "fennec-on-device"] and not enable_mobile:
        print """
  WARNING: Firefox Mobile support is still experimental.
  If you would like to run an addon on this platform, use --force-mobile flag:

    cfx --force-mobile"""
        return 0

    if app_type == "fennec-on-device":
        profile_class = FennecProfile
        preferences.update(DEFAULT_FENNEC_PREFS)
        runner_class = RemoteFennecRunner
        # We pass the intent name through command arguments
        cmdargs.append(mobile_app_name)
    elif enable_mobile or app_type == "fennec":
        profile_class = FennecProfile
        preferences.update(DEFAULT_FENNEC_PREFS)
        runner_class = FennecRunner
    elif app_type == "xulrunner":
        profile_class = XulrunnerAppProfile
        runner_class = XulrunnerAppRunner
        cmdargs.append(os.path.join(harness_root_dir, 'application.ini'))
    elif app_type == "firefox":
        profile_class = mozrunner.FirefoxProfile
        preferences.update(DEFAULT_FIREFOX_PREFS)
        runner_class = mozrunner.FirefoxRunner
    elif app_type == "thunderbird":
        profile_class = mozrunner.ThunderbirdProfile
        preferences.update(DEFAULT_THUNDERBIRD_PREFS)
        runner_class = mozrunner.ThunderbirdRunner
    else:
        raise ValueError("Unknown app: %s" % app_type)
    if sys.platform == 'darwin' and app_type != 'xulrunner':
        cmdargs.append('-foreground')

    if args:
        cmdargs.extend(shlex.split(args))

    # TODO: handle logs on remote device
    if app_type != "fennec-on-device":
        # tempfile.gettempdir() was constant, preventing two simultaneous "cfx
        # run"/"cfx test" on the same host. On unix it points at /tmp (which is
        # world-writeable), enabling a symlink attack (e.g. imagine some bad guy
        # does 'ln -s ~/.ssh/id_rsa /tmp/harness_result'). NamedTemporaryFile
        # gives us a unique filename that fixes both problems. We leave the
        # (0-byte) file in place until the browser-side code starts writing to
        # it, otherwise the symlink attack becomes possible again.
        fileno,resultfile = tempfile.mkstemp(prefix="harness-result-")
        os.close(fileno)
        harness_options['resultFile'] = resultfile

    def maybe_remove_logfile():
        if os.path.exists(logfile):
            os.remove(logfile)

    logfile_tail = None

    # We always buffer output through a logfile for two reasons:
    # 1. On Windows, it's the only way to print console output to stdout/err.
    # 2. It enables us to keep track of the last time output was emitted,
    #    so we can raise an exception if the test runner hangs.
    if not logfile:
        fileno,logfile = tempfile.mkstemp(prefix="harness-log-")
        os.close(fileno)
    logfile_tail = follow_file(logfile)
    atexit.register(maybe_remove_logfile)

    logfile = os.path.abspath(os.path.expanduser(logfile))
    maybe_remove_logfile()

    if app_type != "fennec-on-device":
        harness_options['logFile'] = logfile

    env = {}
    env.update(os.environ)
    env['MOZ_NO_REMOTE'] = '1'
    env['XPCOM_DEBUG_BREAK'] = 'stack'
    env['NS_TRACE_MALLOC_DISABLE_STACKS'] = '1'
    env.update(extra_environment)
    if norun:
        cmdargs.append("-no-remote")

    # Create the addon XPI so mozrunner will copy it to the profile it creates.
    # We delete it below after getting mozrunner to create the profile.
    from cuddlefish.xpi import build_xpi
    xpi_path = tempfile.mktemp(suffix='cfx-tmp.xpi')
    build_xpi(template_root_dir=harness_root_dir,
              manifest=manifest_rdf,
              xpi_path=xpi_path,
              harness_options=harness_options,
              limit_to=used_files,
              bundle_sdk=bundle_sdk)
    addons.append(xpi_path)

    starttime = last_output_time = time.time()

    # Redirect runner output to a file so we can catch output not generated
    # by us.
    # In theory, we could do this using simple redirection on all platforms
    # other than Windows, but this way we only have a single codepath to
    # maintain.
    fileno,outfile = tempfile.mkstemp(prefix="harness-stdout-")
    os.close(fileno)
    outfile_tail = follow_file(outfile)
    def maybe_remove_outfile():
        if os.path.exists(outfile):
            os.remove(outfile)
    atexit.register(maybe_remove_outfile)
    outf = open(outfile, "w")
    popen_kwargs = { 'stdout': outf, 'stderr': outf}

    profile = None

    if app_type == "fennec-on-device":
        # Install a special addon when we run firefox on mobile device
        # in order to be able to kill it
        mydir = os.path.dirname(os.path.abspath(__file__))
        addon_dir = os.path.join(mydir, "mobile-utils")
        addons.append(addon_dir)

    # Overload addon-specific commonjs modules path with lib/ folder
    overloads = dict()
    if overload_modules:
        overloads[""] = os.path.join(env_root, "lib")

    # Overload tests/ mapping with test/ folder, only when running test
    if is_running_tests:
        overloads["tests"] = os.path.join(env_root, "test")

    set_overloaded_modules(env_root, app_type, harness_options["jetpackID"], \
                           preferences, overloads)

    # the XPI file is copied into the profile here
    profile = profile_class(addons=addons,
                            profile=profiledir,
                            preferences=preferences)

    # Delete the temporary xpi file
    os.remove(xpi_path)

    # Copy overloaded files registered in set_overloaded_modules
    # For testing on device, we have to copy overloaded files from fs
    # to the device and use device path instead of local fs path.
    # (has to be done after the call to profile_class() which eventualy creates
    #  profile folder)
    if app_type == "fennec-on-device":
        profile_path = profile.profile
        for name, path in overloads.items():
            shutil.copytree(path, \
                os.path.join(profile_path, "overloads", name))

    runner = runner_class(profile=profile,
                          binary=binary,
                          env=env,
                          cmdargs=cmdargs,
                          kp_kwargs=popen_kwargs)

    sys.stdout.flush(); sys.stderr.flush()

    if app_type == "fennec-on-device":
        if not enable_mobile:
            print >>sys.stderr, """
  WARNING: Firefox Mobile support is still experimental.
  If you would like to run an addon on this platform, use --force-mobile flag:

    cfx --force-mobile"""
            return 0

        # In case of mobile device, we need to get stdio from `adb logcat` cmd:

        # First flush logs in order to avoid catching previous ones
        subprocess.call([binary, "logcat", "-c"])

        # Launch adb command
        runner.start()

        # We can immediatly remove temporary profile folder
        # as it has been uploaded to the device
        profile.cleanup()
        # We are not going to use the output log file
        outf.close()

        # Then we simply display stdout of `adb logcat`
        p = subprocess.Popen([binary, "logcat", "stderr:V stdout:V GeckoConsole:V *:S"], stdout=subprocess.PIPE)
        while True:
            line = p.stdout.readline()
            if line == '':
                break
            # mobile-utils addon contains an application quit event observer
            # that will print this string:
            if "APPLICATION-QUIT" in line:
                break

            if verbose:
                # if --verbose is given, we display everything:
                # All JS Console messages, stdout and stderr.
                m = CLEANUP_ADB.match(line)
                if not m:
                    print line.rstrip()
                    continue
                print m.group(3)
            else:
                # Otherwise, display addons messages dispatched through
                # console.[info, log, debug, warning, error](msg)
                m = FILTER_ONLY_CONSOLE_FROM_ADB.match(line)
                if m:
                    print m.group(2)

        print >>sys.stderr, "Program terminated successfully."
        return 0


    print >>sys.stderr, "Using binary at '%s'." % runner.binary

    # Ensure cfx is being used with Firefox 4.0+.
    # TODO: instead of dying when Firefox is < 4, warn when Firefox is outside
    # the minVersion/maxVersion boundaries.
    version_output = check_output(runner.command + ["-v"])
    # Note: this regex doesn't handle all valid versions in the Toolkit Version
    # Format <https://developer.mozilla.org/en/Toolkit_version_format>, just the
    # common subset that we expect Mozilla apps to use.
    mo = re.search(r"Mozilla (Firefox|Iceweasel|Fennec)\b[^ ]* ((\d+)\.\S*)",
                   version_output)
    if not mo:
        # cfx may be used with Thunderbird, SeaMonkey or an exotic Firefox
        # version.
        print """
  WARNING: cannot determine Firefox version; please ensure you are running
  a Mozilla application equivalent to Firefox 4.0 or greater.
  """
    elif mo.group(1) == "Fennec":
        # For now, only allow running on Mobile with --force-mobile argument
        if not enable_mobile:
            print """
  WARNING: Firefox Mobile support is still experimental.
  If you would like to run an addon on this platform, use --force-mobile flag:

    cfx --force-mobile"""
            return
    else:
        version = mo.group(3)
        if int(version) < 4:
            print """
  cfx requires Firefox 4 or greater and is unable to find a compatible
  binary. Please install a newer version of Firefox or provide the path to
  your existing compatible version with the --binary flag:

    cfx --binary=PATH_TO_FIREFOX_BINARY"""
            return

        # Set the appropriate extensions.checkCompatibility preference to false,
        # so the tests run even if the SDK is not marked as compatible with the
        # version of Firefox on which they are running, and we don't have to
        # ensure we update the maxVersion before the version of Firefox changes
        # every six weeks.
        #
        # The regex we use here is effectively the same as BRANCH_REGEX from
        # /toolkit/mozapps/extensions/content/extensions.js, which toolkit apps
        # use to determine whether or not to load an incompatible addon.
        #
        br = re.search(r"^([^\.]+\.[0-9]+[a-z]*).*", mo.group(2), re.I)
        if br:
            prefname = 'extensions.checkCompatibility.' + br.group(1)
            profile.preferences[prefname] = False
            # Calling profile.set_preferences here duplicates the list of prefs
            # in prefs.js, since the profile calls self.set_preferences in its
            # constructor, but that is ok, because it doesn't change the set of
            # preferences that are ultimately registered in Firefox.
            profile.set_preferences(profile.preferences)

    print >>sys.stderr, "Using profile at '%s'." % profile.profile
    sys.stderr.flush()

    if norun:
        print "To launch the application, enter the following command:"
        print " ".join(runner.command) + " " + (" ".join(runner.cmdargs))
        return 0

    runner.start()

    done = False
    result = None
    test_name = "unknown"

    def Timeout(message, test_name, parseable):
        if parseable:
            sys.stderr.write("TEST-UNEXPECTED-FAIL | %s | %s\n" % (test_name, message))
            sys.stderr.flush()
        return Exception(message)

    try:
        while not done:
            time.sleep(0.05)
            for tail in (logfile_tail, outfile_tail):
                if tail:
                    new_chars = tail.next()
                    if new_chars:
                        last_output_time = time.time()
                        sys.stderr.write(new_chars)
                        sys.stderr.flush()
                        if is_running_tests and parseable:
                            match = PARSEABLE_TEST_NAME.search(new_chars)
                            if match:
                                test_name = match.group(1)
            if os.path.exists(resultfile):
                result = open(resultfile).read()
                if result:
                    if result in ['OK', 'FAIL']:
                        done = True
                    else:
                        sys.stderr.write("Hrm, resultfile (%s) contained something weird (%d bytes)\n" % (resultfile, len(result)))
                        sys.stderr.write("'"+result+"'\n")
            if enforce_timeouts:
                if time.time() - last_output_time > OUTPUT_TIMEOUT:
                    raise Timeout("Test output exceeded timeout (%ds)." %
                                  OUTPUT_TIMEOUT, test_name, parseable)
                if time.time() - starttime > RUN_TIMEOUT:
                    raise Timeout("Test run exceeded timeout (%ds)." %
                                  RUN_TIMEOUT, test_name, parseable)
    except:
        runner.stop()
        raise
    else:
        runner.wait(10)
    finally:
        outf.close()
        if profile:
            profile.cleanup()

    print >>sys.stderr, "Total time: %f seconds" % (time.time() - starttime)

    if result == 'OK':
        print >>sys.stderr, "Program terminated successfully."
        return 0
    else:
        print >>sys.stderr, "Program terminated unsuccessfully."
        return -1
