# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generate xpi for the simulator addon by:
# - building a special gaia profile for it, as we need:
#     * more languages, and,
#     * less apps
#   than b2g desktop's one
# - retrieve usefull app version metadata from the build system
# - finally, use addon sdk's cfx tool to build the addon xpi
#   that ships:
#     * b2g desktop runtime
#     * gaia profile

import sys, os, re, subprocess
from mozbuild.preprocessor import Preprocessor
from mozbuild.base import MozbuildObject
from mozbuild.util import ensureParentDir
from mozpack.mozjar import JarWriter
from zipfile import ZipFile
from distutils.version import LooseVersion

ftp_root_path = "/pub/mozilla.org/labs/fxos-simulator"
UPDATE_URL = "https://ftp.mozilla.org" + ftp_root_path + "/%(update_path)s/update.rdf"
XPI_NAME = "fxos-simulator-%(version)s-%(platform)s.xpi"

class GaiaBuilder(object):
    def __init__(self, build, gaia_path):
        self.build = build
        self.gaia_path = gaia_path

    def clean(self):
        self.build._run_make(target="clean", directory=self.gaia_path)

    def profile(self, env):
        self.build._run_make(target="profile", directory=self.gaia_path, num_jobs=1, silent=False, append_env=env)

    def override_prefs(self, srcfile):
        # Note that each time we call `make profile` in gaia, a fresh new pref file is created
        # cat srcfile >> profile/user.js
        with open(os.path.join(self.gaia_path, "profile", "user.js"), "a") as userJs:
            userJs.write(open(srcfile).read())

def preprocess_file(src, dst, version, app_buildid, update_url):
    ensureParentDir(dst)

    defines = {
        "ADDON_ID": "fxos_" + version.replace(".", "_") + "_simulator@mozilla.org",
        # (reduce the app build id to only the build date
        # as addon manager doesn't handle big ints in addon versions)
        "ADDON_VERSION": ("%s.%s" % (version, app_buildid[:8])),
        "ADDON_NAME": "Firefox OS " + version + " Simulator",
        "ADDON_DESCRIPTION": "a Firefox OS " + version + " simulator",
        "ADDON_UPDATE_URL": update_url
    }
    pp = Preprocessor(defines=defines)
    pp.do_filter("substitution")
    with open(dst, "w") as output:
        with open(src, "r") as input:
            pp.processFile(input=input, output=output)

def add_dir_to_zip(jar, top, pathInZip, blacklist=()):
    for dirpath, subdirs, files in os.walk(top):
        dir_relpath = os.path.relpath(dirpath, top)
        if dir_relpath.startswith(blacklist):
            continue
        for filename in files:
            relpath = os.path.join(dir_relpath, filename)
            if relpath in blacklist:
                continue
            path = os.path.normpath(os.path.join(pathInZip, relpath))
            file = open(os.path.join(dirpath, filename), "rb")
            mode = os.stat(os.path.join(dirpath, filename)).st_mode
            jar.add(path.encode("ascii"), file, mode=mode)

def add_file_to_zip(jar, path, pathInZip):
    file = open(path, "rb")
    mode = os.stat(path).st_mode
    jar.add(pathInZip.encode("ascii"), file, mode=mode)

def main(platform):
    build = MozbuildObject.from_environment()
    topsrcdir = build.topsrcdir
    distdir = build.distdir

    srcdir = os.path.join(topsrcdir, "b2g", "simulator")

    app_buildid = open(os.path.join(build.topobjdir, "config", "buildid")).read().strip()

    # The simulator uses a shorter version string,
    # it only keeps the major version digits A.B
    # whereas MOZ_B2G_VERSION is A.B.C.D
    b2g_version = build.config_environment.defines["MOZ_B2G_VERSION"].replace('"', '')
    version = ".".join(str(n) for n in LooseVersion(b2g_version).version[0:2])

    # Build a gaia profile specific to the simulator in order to:
    # - disable the FTU
    # - set custom prefs to enable devtools debugger server
    # - set custom settings to disable lockscreen and screen timeout
    # - only ship production apps
    gaia_path = build.config_environment.substs["GAIADIR"]
    builder = GaiaBuilder(build, gaia_path)
    builder.clean()
    env = {
      "NOFTU": "1",
      "GAIA_APP_TARGET": "production",
      "SETTINGS_PATH": os.path.join(srcdir, "custom-settings.json")
    }
    builder.profile(env)
    builder.override_prefs(os.path.join(srcdir, "custom-prefs.js"))

    # Build the simulator addon xpi
    xpi_name = XPI_NAME % {"version": version, "platform": platform}
    xpi_path = os.path.join(distdir, xpi_name)

    update_path = "%s/%s" % (version, platform)
    update_url = UPDATE_URL % {"update_path": update_path}

    # Preprocess some files...
    manifest = os.path.join(build.topobjdir, "b2g", "simulator", "install.rdf")
    preprocess_file(os.path.join(srcdir, "install.rdf.in"),
                    manifest,
                    version,
                    app_buildid,
                    update_url)

    with JarWriter(xpi_path, optimize=False) as zip:
        # Ship addon files into the .xpi
        add_file_to_zip(zip, manifest, "install.rdf")
        add_file_to_zip(zip, os.path.join(srcdir, "bootstrap.js"), "bootstrap.js")
        add_file_to_zip(zip, os.path.join(srcdir, "icon.png"), "icon.png")
        add_file_to_zip(zip, os.path.join(srcdir, "icon64.png"), "icon64.png")

        # Ship b2g-desktop, but prevent its gaia profile to be shipped in the xpi
        add_dir_to_zip(zip, os.path.join(distdir, "b2g"), "b2g",
                       ("gaia", "B2G.app/Contents/MacOS/gaia",
                        "B2G.app/Contents/Resources/gaia"))
        # Then ship our own gaia profile
        add_dir_to_zip(zip, os.path.join(gaia_path, "profile"), "profile")

if __name__ == '__main__':
    if 2 != len(sys.argv):
        print("""Usage:
  python {0} MOZ_PKG_PLATFORM
""".format(sys.argv[0]))
        sys.exit(1)
    main(*sys.argv[1:])
