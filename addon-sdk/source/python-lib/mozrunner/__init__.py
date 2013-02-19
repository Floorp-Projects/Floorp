# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import copy
import tempfile
import signal
import commands
import zipfile
import optparse
import killableprocess
import subprocess
import platform
import shutil
from StringIO import StringIO
from xml.dom import minidom

from distutils import dir_util
from time import sleep

# conditional (version-dependent) imports
try:
    import simplejson
except ImportError:
    import json as simplejson

import logging
logger = logging.getLogger(__name__)

# Use dir_util for copy/rm operations because shutil is all kinds of broken
copytree = dir_util.copy_tree
rmtree = dir_util.remove_tree

def findInPath(fileName, path=os.environ['PATH']):
    dirs = path.split(os.pathsep)
    for dir in dirs:
        if os.path.isfile(os.path.join(dir, fileName)):
            return os.path.join(dir, fileName)
        if os.name == 'nt' or sys.platform == 'cygwin':
            if os.path.isfile(os.path.join(dir, fileName + ".exe")):
                return os.path.join(dir, fileName + ".exe")
    return None

stdout = sys.stdout
stderr = sys.stderr
stdin = sys.stdin

def run_command(cmd, env=None, **kwargs):
    """Run the given command in killable process."""
    killable_kwargs = {'stdout':stdout ,'stderr':stderr, 'stdin':stdin}
    killable_kwargs.update(kwargs)

    if sys.platform != "win32":
        return killableprocess.Popen(cmd, preexec_fn=lambda : os.setpgid(0, 0),
                                     env=env, **killable_kwargs)
    else:
        return killableprocess.Popen(cmd, env=env, **killable_kwargs)

def getoutput(l):
    tmp = tempfile.mktemp()
    x = open(tmp, 'w')
    subprocess.call(l, stdout=x, stderr=x)
    x.close(); x = open(tmp, 'r')
    r = x.read() ; x.close()
    os.remove(tmp)
    return r

def get_pids(name, minimun_pid=0):
    """Get all the pids matching name, exclude any pids below minimum_pid."""
    if os.name == 'nt' or sys.platform == 'cygwin':
        import wpk

        pids = wpk.get_pids(name)

    else:
        data = getoutput(['ps', 'ax']).splitlines()
        pids = [int(line.split()[0]) for line in data if line.find(name) is not -1]

    matching_pids = [m for m in pids if m > minimun_pid]
    return matching_pids

def makedirs(name):

    head, tail = os.path.split(name)
    if not tail:
        head, tail = os.path.split(head)
    if head and tail and not os.path.exists(head):
        try:
            makedirs(head)
        except OSError, e:
            pass
        if tail == os.curdir:           # xxx/newdir/. exists if xxx/newdir exists
            return
    try:
        os.mkdir(name)
    except:
        pass

# addon_details() copied from mozprofile
def addon_details(install_rdf_fh):
    """
    returns a dictionary of details about the addon
    - addon_path : path to the addon directory
    Returns:
    {'id':      u'rainbow@colors.org', # id of the addon
     'version': u'1.4',                # version of the addon
     'name':    u'Rainbow',            # name of the addon
     'unpack': # whether to unpack the addon
    """

    details = {
        'id': None,
        'unpack': False,
        'name': None,
        'version': None
    }

    def get_namespace_id(doc, url):
        attributes = doc.documentElement.attributes
        namespace = ""
        for i in range(attributes.length):
            if attributes.item(i).value == url:
                if ":" in attributes.item(i).name:
                    # If the namespace is not the default one remove 'xlmns:'
                    namespace = attributes.item(i).name.split(':')[1] + ":"
                    break
        return namespace

    def get_text(element):
        """Retrieve the text value of a given node"""
        rc = []
        for node in element.childNodes:
            if node.nodeType == node.TEXT_NODE:
                rc.append(node.data)
        return ''.join(rc).strip()

    doc = minidom.parse(install_rdf_fh)

    # Get the namespaces abbreviations
    em = get_namespace_id(doc, "http://www.mozilla.org/2004/em-rdf#")
    rdf = get_namespace_id(doc, "http://www.w3.org/1999/02/22-rdf-syntax-ns#")

    description = doc.getElementsByTagName(rdf + "Description").item(0)
    for node in description.childNodes:
        # Remove the namespace prefix from the tag for comparison
        entry = node.nodeName.replace(em, "")
        if entry in details.keys():
            details.update({ entry: get_text(node) })

    # turn unpack into a true/false value
    if isinstance(details['unpack'], basestring):
        details['unpack'] = details['unpack'].lower() == 'true'

    return details

class Profile(object):
    """Handles all operations regarding profile. Created new profiles, installs extensions,
    sets preferences and handles cleanup."""

    def __init__(self, binary=None, profile=None, addons=None,
                 preferences=None):

        self.binary = binary

        self.create_new = not(bool(profile))
        if profile:
            self.profile = profile
        else:
            self.profile = self.create_new_profile(self.binary)

        self.addons_installed = []
        self.addons = addons or []

        ### set preferences from class preferences
        preferences = preferences or {}
        if hasattr(self.__class__, 'preferences'):
            self.preferences = self.__class__.preferences.copy()
        else:
            self.preferences = {}
        self.preferences.update(preferences)

        for addon in self.addons:
            self.install_addon(addon)

        self.set_preferences(self.preferences)

    def create_new_profile(self, binary):
        """Create a new clean profile in tmp which is a simple empty folder"""
        profile = tempfile.mkdtemp(suffix='.mozrunner')
        return profile

    def unpack_addon(self, xpi_zipfile, addon_path):
        for name in xpi_zipfile.namelist():
            if name.endswith('/'):
                makedirs(os.path.join(addon_path, name))
            else:
                if not os.path.isdir(os.path.dirname(os.path.join(addon_path, name))):
                    makedirs(os.path.dirname(os.path.join(addon_path, name)))
                data = xpi_zipfile.read(name)
                f = open(os.path.join(addon_path, name), 'wb')
                f.write(data) ; f.close()
                zi = xpi_zipfile.getinfo(name)
                os.chmod(os.path.join(addon_path,name), (zi.external_attr>>16))

    def install_addon(self, path):
        """Installs the given addon or directory of addons in the profile."""

        extensions_path = os.path.join(self.profile, 'extensions')
        if not os.path.exists(extensions_path):
            os.makedirs(extensions_path)

        addons = [path]
        if not path.endswith('.xpi') and not os.path.exists(os.path.join(path, 'install.rdf')):
            addons = [os.path.join(path, x) for x in os.listdir(path)]

        for addon in addons:
            if addon.endswith('.xpi'):
                xpi_zipfile = zipfile.ZipFile(addon, "r")
                details = addon_details(StringIO(xpi_zipfile.read('install.rdf')))
                addon_path = os.path.join(extensions_path, details["id"])
                if details.get("unpack", True):
                    self.unpack_addon(xpi_zipfile, addon_path)
                    self.addons_installed.append(addon_path)
                else:
                    shutil.copy(addon, addon_path + '.xpi')
            else:
                # it's already unpacked, but we need to extract the id so we
                # can copy it
                details = addon_details(open(os.path.join(addon, "install.rdf"), "rb"))
                addon_path = os.path.join(extensions_path, details["id"])
                shutil.copytree(addon, addon_path, symlinks=True)

    def set_preferences(self, preferences):
        """Adds preferences dict to profile preferences"""
        prefs_file = os.path.join(self.profile, 'user.js')
        # Ensure that the file exists first otherwise create an empty file
        if os.path.isfile(prefs_file):
            f = open(prefs_file, 'a+')
        else:
            f = open(prefs_file, 'w')

        f.write('\n#MozRunner Prefs Start\n')

        pref_lines = ['user_pref(%s, %s);' %
                      (simplejson.dumps(k), simplejson.dumps(v) ) for k, v in
                       preferences.items()]
        for line in pref_lines:
            f.write(line+'\n')
        f.write('#MozRunner Prefs End\n')
        f.flush() ; f.close()

    def pop_preferences(self):
        """
        pop the last set of preferences added
        returns True if popped
        """

        # our magic markers
        delimeters = ('#MozRunner Prefs Start', '#MozRunner Prefs End')

        lines = file(os.path.join(self.profile, 'user.js')).read().splitlines()
        def last_index(_list, value):
            """
            returns the last index of an item;
            this should actually be part of python code but it isn't
            """
            for index in reversed(range(len(_list))):
                if _list[index] == value:
                    return index
        s = last_index(lines, delimeters[0])
        e = last_index(lines, delimeters[1])

        # ensure both markers are found
        if s is None:
            assert e is None, '%s found without %s' % (delimeters[1], delimeters[0])
            return False # no preferences found
        elif e is None:
            assert e is None, '%s found without %s' % (delimeters[0], delimeters[1])

        # ensure the markers are in the proper order
        assert e > s, '%s found at %s, while %s found at %s' (delimeter[1], e, delimeter[0], s)

        # write the prefs
        cleaned_prefs = '\n'.join(lines[:s] + lines[e+1:])
        f = file(os.path.join(self.profile, 'user.js'), 'w')
        f.write(cleaned_prefs)
        f.close()
        return True

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        while True:
            if not self.pop_preferences():
                break

    def clean_addons(self):
        """Cleans up addons in the profile."""
        for addon in self.addons_installed:
            if os.path.isdir(addon):
                rmtree(addon)

    def cleanup(self):
        """Cleanup operations on the profile."""
        def oncleanup_error(function, path, excinfo):
            #TODO: How should we handle this?
            print "Error Cleaning up: " + str(excinfo[1])
        if self.create_new:
            shutil.rmtree(self.profile, False, oncleanup_error)
        else:
            self.clean_preferences()
            self.clean_addons()

class FirefoxProfile(Profile):
    """Specialized Profile subclass for Firefox"""
    preferences = {# Don't automatically update the application
                   'app.update.enabled' : False,
                   # Don't restore the last open set of tabs if the browser has crashed
                   'browser.sessionstore.resume_from_crash': False,
                   # Don't check for the default web browser
                   'browser.shell.checkDefaultBrowser' : False,
                   # Don't warn on exit when multiple tabs are open
                   'browser.tabs.warnOnClose' : False,
                   # Don't warn when exiting the browser
                   'browser.warnOnQuit': False,
                   # Only install add-ons from the profile and the app folder
                   'extensions.enabledScopes' : 5,
                   # Don't automatically update add-ons
                   'extensions.update.enabled'    : False,
                   # Don't open a dialog to show available add-on updates
                   'extensions.update.notifyUser' : False,
                   }

    # The possible names of application bundles on Mac OS X, in order of
    # preference from most to least preferred.
    # Note: Nightly is obsolete, as it has been renamed to FirefoxNightly,
    # but it will still be present if users update an older nightly build
    # via the app update service.
    bundle_names = ['Firefox', 'FirefoxNightly', 'Nightly']

    # The possible names of binaries, in order of preference from most to least
    # preferred.
    @property
    def names(self):
        if sys.platform == 'darwin':
            return ['firefox', 'nightly', 'shiretoko']
        if (sys.platform == 'linux2') or (sys.platform in ('sunos5', 'solaris')):
            return ['firefox', 'mozilla-firefox', 'iceweasel']
        if os.name == 'nt' or sys.platform == 'cygwin':
            return ['firefox']

class ThunderbirdProfile(Profile):
    preferences = {'extensions.update.enabled'    : False,
                   'extensions.update.notifyUser' : False,
                   'browser.shell.checkDefaultBrowser' : False,
                   'browser.tabs.warnOnClose' : False,
                   'browser.warnOnQuit': False,
                   'browser.sessionstore.resume_from_crash': False,
                   }

    # The possible names of application bundles on Mac OS X, in order of
    # preference from most to least preferred.
    bundle_names = ["Thunderbird", "Shredder"]

    # The possible names of binaries, in order of preference from most to least
    # preferred.
    names = ["thunderbird", "shredder"]


class Runner(object):
    """Handles all running operations. Finds bins, runs and kills the process."""

    def __init__(self, binary=None, profile=None, cmdargs=[], env=None,
                 kp_kwargs={}):
        if binary is None:
            self.binary = self.find_binary()
        elif sys.platform == 'darwin' and binary.find('Contents/MacOS/') == -1:
            self.binary = os.path.join(binary, 'Contents/MacOS/%s-bin' % self.names[0])
        else:
            self.binary = binary

        if not os.path.exists(self.binary):
            raise Exception("Binary path does not exist "+self.binary)

        if sys.platform == 'linux2' and self.binary.endswith('-bin'):
            dirname = os.path.dirname(self.binary)
            if os.environ.get('LD_LIBRARY_PATH', None):
                os.environ['LD_LIBRARY_PATH'] = '%s:%s' % (os.environ['LD_LIBRARY_PATH'], dirname)
            else:
                os.environ['LD_LIBRARY_PATH'] = dirname

        # Disable the crash reporter by default
        os.environ['MOZ_CRASHREPORTER_NO_REPORT'] = '1'

        self.profile = profile

        self.cmdargs = cmdargs
        if env is None:
            self.env = copy.copy(os.environ)
            self.env.update({'MOZ_NO_REMOTE':"1",})
        else:
            self.env = env
        self.kp_kwargs = kp_kwargs or {}

    def find_binary(self):
        """Finds the binary for self.names if one was not provided."""
        binary = None
        if sys.platform in ('linux2', 'sunos5', 'solaris'):
            for name in reversed(self.names):
                binary = findInPath(name)
        elif os.name == 'nt' or sys.platform == 'cygwin':

            # find the default executable from the windows registry
            try:
                import _winreg
            except ImportError:
                pass
            else:
                sam_flags = [0]
                # KEY_WOW64_32KEY etc only appeared in 2.6+, but that's OK as
                # only 2.6+ has functioning 64bit builds.
                if hasattr(_winreg, "KEY_WOW64_32KEY"):
                    if "64 bit" in sys.version:
                        # a 64bit Python should also look in the 32bit registry
                        sam_flags.append(_winreg.KEY_WOW64_32KEY)
                    else:
                        # possibly a 32bit Python on 64bit Windows, so look in
                        # the 64bit registry incase there is a 64bit app.
                        sam_flags.append(_winreg.KEY_WOW64_64KEY)
                for sam_flag in sam_flags:
                    try:
                        # assumes self.app_name is defined, as it should be for
                        # implementors
                        keyname = r"Software\Mozilla\Mozilla %s" % self.app_name
                        sam = _winreg.KEY_READ | sam_flag
                        app_key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, keyname, 0, sam)
                        version, _type = _winreg.QueryValueEx(app_key, "CurrentVersion")
                        version_key = _winreg.OpenKey(app_key, version + r"\Main")
                        path, _ = _winreg.QueryValueEx(version_key, "PathToExe")
                        return path
                    except _winreg.error:
                        pass

            # search for the binary in the path            
            for name in reversed(self.names):
                binary = findInPath(name)
                if sys.platform == 'cygwin':
                    program_files = os.environ['PROGRAMFILES']
                else:
                    program_files = os.environ['ProgramFiles']

                if binary is None:
                    for bin in [(program_files, 'Mozilla Firefox', 'firefox.exe'),
                                (os.environ.get("ProgramFiles(x86)"),'Mozilla Firefox', 'firefox.exe'),
                                (program_files, 'Nightly', 'firefox.exe'),
                                (os.environ.get("ProgramFiles(x86)"),'Nightly', 'firefox.exe'),
                                (program_files, 'Aurora', 'firefox.exe'),
                                (os.environ.get("ProgramFiles(x86)"),'Aurora', 'firefox.exe')
                                ]:
                        path = os.path.join(*bin)
                        if os.path.isfile(path):
                            binary = path
                            break
        elif sys.platform == 'darwin':
            for bundle_name in self.bundle_names:
                # Look for the application bundle in the user's home directory
                # or the system-wide /Applications directory.  If we don't find
                # it in one of those locations, we move on to the next possible
                # bundle name.
                appdir = os.path.join("~/Applications/%s.app" % bundle_name)
                if not os.path.isdir(appdir):
                    appdir = "/Applications/%s.app" % bundle_name
                if not os.path.isdir(appdir):
                    continue

                # Look for a binary with any of the possible binary names
                # inside the application bundle.
                for binname in self.names:
                    binpath = os.path.join(appdir,
                                           "Contents/MacOS/%s-bin" % binname)
                    if (os.path.isfile(binpath)):
                        binary = binpath
                        break

                if binary:
                    break

        if binary is None:
            raise Exception('Mozrunner could not locate your binary, you will need to set it.')
        return binary

    @property
    def command(self):
        """Returns the command list to run."""
        cmd = [self.binary, '-profile', self.profile.profile]
        # On i386 OS X machines, i386+x86_64 universal binaries need to be told
        # to run as i386 binaries.  If we're not running a i386+x86_64 universal
        # binary, then this command modification is harmless.
        if sys.platform == 'darwin':
            if hasattr(platform, 'architecture') and platform.architecture()[0] == '32bit':
                cmd = ['arch', '-i386'] + cmd
        return cmd

    def get_repositoryInfo(self):
        """Read repository information from application.ini and platform.ini."""
        import ConfigParser

        config = ConfigParser.RawConfigParser()
        dirname = os.path.dirname(self.binary)
        repository = { }

        for entry in [['application', 'App'], ['platform', 'Build']]:
            (file, section) = entry
            config.read(os.path.join(dirname, '%s.ini' % file))

            for entry in [['SourceRepository', 'repository'], ['SourceStamp', 'changeset']]:
                (key, id) = entry

                try:
                    repository['%s_%s' % (file, id)] = config.get(section, key);
                except:
                    repository['%s_%s' % (file, id)] = None

        return repository

    def start(self):
        """Run self.command in the proper environment."""
        if self.profile is None:
            self.profile = self.profile_class()
        self.process_handler = run_command(self.command+self.cmdargs, self.env, **self.kp_kwargs)

    def wait(self, timeout=None):
        """Wait for the browser to exit."""
        self.process_handler.wait(timeout=timeout)

        if sys.platform != 'win32':
            for name in self.names:
                for pid in get_pids(name, self.process_handler.pid):
                    self.process_handler.pid = pid
                    self.process_handler.wait(timeout=timeout)

    def kill(self, kill_signal=signal.SIGTERM):
        """Kill the browser"""
        if sys.platform != 'win32':
            self.process_handler.kill()
            for name in self.names:
                for pid in get_pids(name, self.process_handler.pid):
                    self.process_handler.pid = pid
                    self.process_handler.kill()
        else:
            try:
                self.process_handler.kill(group=True)
                # On windows, it sometimes behooves one to wait for dust to settle
                # after killing processes.  Let's try that.
                # TODO: Bug 640047 is invesitgating the correct way to handle this case
                self.process_handler.wait(timeout=10)
            except Exception, e:
                logger.error('Cannot kill process, '+type(e).__name__+' '+e.message)

    def stop(self):
        self.kill()

class FirefoxRunner(Runner):
    """Specialized Runner subclass for running Firefox."""

    app_name = 'Firefox'
    profile_class = FirefoxProfile

    # The possible names of application bundles on Mac OS X, in order of
    # preference from most to least preferred.
    # Note: Nightly is obsolete, as it has been renamed to FirefoxNightly,
    # but it will still be present if users update an older nightly build
    # only via the app update service.
    bundle_names = ['Firefox', 'FirefoxNightly', 'Nightly']

    @property
    def names(self):
        if sys.platform == 'darwin':
            return ['firefox', 'nightly', 'shiretoko']
        if (sys.platform == 'linux2') or (sys.platform in ('sunos5', 'solaris')):
            return ['firefox', 'mozilla-firefox', 'iceweasel']
        if os.name == 'nt' or sys.platform == 'cygwin':
            return ['firefox']

class ThunderbirdRunner(Runner):
    """Specialized Runner subclass for running Thunderbird"""

    app_name = 'Thunderbird'
    profile_class = ThunderbirdProfile

    # The possible names of application bundles on Mac OS X, in order of
    # preference from most to least preferred.
    bundle_names = ["Thunderbird", "Shredder"]

    # The possible names of binaries, in order of preference from most to least
    # preferred.
    names = ["thunderbird", "shredder"]

class CLI(object):
    """Command line interface."""

    runner_class = FirefoxRunner
    profile_class = FirefoxProfile
    module = "mozrunner"

    parser_options = {("-b", "--binary",): dict(dest="binary", help="Binary path.",
                                                metavar=None, default=None),
                      ('-p', "--profile",): dict(dest="profile", help="Profile path.",
                                                 metavar=None, default=None),
                      ('-a', "--addons",): dict(dest="addons", 
                                                help="Addons paths to install.",
                                                metavar=None, default=None),
                      ("--info",): dict(dest="info", default=False,
                                        action="store_true",
                                        help="Print module information")
                     }

    def __init__(self):
        """ Setup command line parser and parse arguments """
        self.metadata = self.get_metadata_from_egg()
        self.parser = optparse.OptionParser(version="%prog " + self.metadata["Version"])
        for names, opts in self.parser_options.items():
            self.parser.add_option(*names, **opts)
        (self.options, self.args) = self.parser.parse_args()

        if self.options.info:
            self.print_metadata()
            sys.exit(0)
            
        # XXX should use action='append' instead of rolling our own
        try:
            self.addons = self.options.addons.split(',')
        except:
            self.addons = []
            
    def get_metadata_from_egg(self):
        import pkg_resources
        ret = {}
        dist = pkg_resources.get_distribution(self.module)
        if dist.has_metadata("PKG-INFO"):
            for line in dist.get_metadata_lines("PKG-INFO"):
                key, value = line.split(':', 1)
                ret[key] = value
        if dist.has_metadata("requires.txt"):
            ret["Dependencies"] = "\n" + dist.get_metadata("requires.txt")    
        return ret
        
    def print_metadata(self, data=("Name", "Version", "Summary", "Home-page", 
                                   "Author", "Author-email", "License", "Platform", "Dependencies")):
        for key in data:
            if key in self.metadata:
                print key + ": " + self.metadata[key]

    def create_runner(self):
        """ Get the runner object """
        runner = self.get_runner(binary=self.options.binary)
        profile = self.get_profile(binary=runner.binary,
                                   profile=self.options.profile,
                                   addons=self.addons)
        runner.profile = profile
        return runner

    def get_runner(self, binary=None, profile=None):
        """Returns the runner instance for the given command line binary argument
        the profile instance returned from self.get_profile()."""
        return self.runner_class(binary, profile)

    def get_profile(self, binary=None, profile=None, addons=None, preferences=None):
        """Returns the profile instance for the given command line arguments."""
        addons = addons or []
        preferences = preferences or {}
        return self.profile_class(binary, profile, addons, preferences)

    def run(self):
        runner = self.create_runner()
        self.start(runner)
        runner.profile.cleanup()

    def start(self, runner):
        """Starts the runner and waits for Firefox to exitor Keyboard Interrupt.
        Shoule be overwritten to provide custom running of the runner instance."""
        runner.start()
        print 'Started:', ' '.join(runner.command)
        try:
            runner.wait()
        except KeyboardInterrupt:
            runner.stop()


def cli():
    CLI().run()
