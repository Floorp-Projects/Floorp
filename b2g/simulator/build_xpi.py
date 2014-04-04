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
#     * a small firefox addon registering to the app manager
#     * b2g desktop runtime
#     * gaia profile

import sys, os, re, subprocess
from mozbuild.preprocessor import Preprocessor
from mozbuild.base import MozbuildObject
from mozbuild.util import ensureParentDir
from zipfile import ZipFile
from distutils.version import LooseVersion

ftp_root_path = "/pub/mozilla.org/labs/fxos-simulator"
UPDATE_LINK = "https://ftp.mozilla.org" + ftp_root_path + "/%(update_path)s/%(xpi_name)s"
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

def process_package_overload(src, dst, version, app_buildid):
    ensureParentDir(dst)
    # First replace numeric version like '1.3'
    # Then replace with 'slashed' version like '1_4'
    # Finally set the full length addon version like 1.3.20131230
    # (reduce the app build id to only the build date
    # as addon manager doesn't handle big ints in addon versions)
    defines = {
        "NUM_VERSION": version,
        "SLASH_VERSION": version.replace(".", "_"),
        "FULL_VERSION": ("%s.%s" % (version, app_buildid[:8]))
    }
    pp = Preprocessor(defines=defines)
    pp.do_filter("substitution")
    with open(dst, "w") as output:
        with open(src, "r") as input:
            pp.processFile(input=input, output=output)

def add_dir_to_zip(zip, top, pathInZip, blacklist=()):
    zf = ZipFile(zip, "a")
    for dirpath, subdirs, files in os.walk(top):
        dir_relpath = os.path.relpath(dirpath, top)
        if dir_relpath.startswith(blacklist):
            continue
        zf.write(dirpath, os.path.join(pathInZip, dir_relpath))
        for filename in files:
            relpath = os.path.join(dir_relpath, filename)
            if relpath in blacklist:
                continue
            zf.write(os.path.join(dirpath, filename),
                     os.path.join(pathInZip, relpath))
    zf.close()

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

    # Substitute version strings in the package manifest overload file
    manifest_overload = os.path.join(build.topobjdir, "b2g", "simulator", "package-overload.json")
    process_package_overload(os.path.join(srcdir, "package-overload.json.in"),
                             manifest_overload,
                             version,
                             app_buildid)

    # Build the simulator addon xpi
    xpi_name = XPI_NAME % {"version": version, "platform": platform}
    xpi_path = os.path.join(distdir, xpi_name)

    update_path = "%s/%s" % (version, platform)
    update_link = UPDATE_LINK % {"update_path": update_path, "xpi_name": xpi_name}
    update_url = UPDATE_URL % {"update_path": update_path}
    subprocess.check_call([
      build.virtualenv_manager.python_path, os.path.join(topsrcdir, "addon-sdk", "source", "bin", "cfx"), "xpi", \
      "--pkgdir", srcdir, \
      "--manifest-overload", manifest_overload, \
      "--strip-sdk", \
      "--update-link", update_link, \
      "--update-url", update_url, \
      "--static-args", "{\"label\": \"Firefox OS %s\"}" % version, \
      "--output-file", xpi_path \
    ])

    # Ship b2g-desktop, but prevent its gaia profile to be shipped in the xpi
    add_dir_to_zip(xpi_path, os.path.join(distdir, "b2g"), "b2g", ("gaia", "B2G.app/Contents/MacOS/gaia"))
    # Then ship our own gaia profile
    add_dir_to_zip(xpi_path, os.path.join(gaia_path, "profile"), "profile")

if __name__ == '__main__':
    if 2 != len(sys.argv):
        print("""Usage:
  python {0} MOZ_PKG_PLATFORM
""".format(sys.argv[0]))
        sys.exit(1)
    main(*sys.argv[1:])

