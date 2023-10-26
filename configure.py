# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import codecs
import errno
import io
import itertools
import logging
import os
import pprint
import sys
import textwrap
from collections.abc import Iterable

base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(base_dir, "python", "mach"))
sys.path.insert(0, os.path.join(base_dir, "python", "mozboot"))
sys.path.insert(0, os.path.join(base_dir, "python", "mozbuild"))
sys.path.insert(0, os.path.join(base_dir, "third_party", "python", "packaging"))
sys.path.insert(0, os.path.join(base_dir, "third_party", "python", "six"))
sys.path.insert(0, os.path.join(base_dir, "third_party", "python", "looseversion"))
import mozpack.path as mozpath
import six
from mach.requirements import MachEnvRequirements
from mach.site import (
    CommandSiteManager,
    ExternalPythonSite,
    MachSiteManager,
    MozSiteMetadata,
    SitePackagesSource,
)
from mozbuild.backend.configenvironment import PartialConfigEnvironment
from mozbuild.configure import TRACE, ConfigureSandbox
from mozbuild.pythonutil import iter_modules_in_path

if "MOZ_CONFIGURE_BUILDSTATUS" in os.environ:

    def buildstatus(message):
        print("BUILDSTATUS", message)

else:

    def buildstatus(message):
        return


def main(argv):
    # Check for CRLF line endings.
    with open(__file__, "r") as fh:
        data = fh.read()
        if "\r" in data:
            print(
                "\n ***\n"
                " * The source tree appears to have Windows-style line endings.\n"
                " *\n"
                " * If using Git, Git is likely configured to use Windows-style\n"
                " * line endings.\n"
                " *\n"
                " * To convert the working copy to UNIX-style line endings, run\n"
                " * the following:\n"
                " *\n"
                " * $ git config core.autocrlf false\n"
                " * $ git config core.eof lf\n"
                " * $ git rm --cached -r .\n"
                " * $ git reset --hard\n"
                " *\n"
                " * If not using Git, the tool you used to obtain the source\n"
                " * code likely converted files to Windows line endings. See\n"
                " * usage information for that tool for more.\n"
                " ***",
                file=sys.stderr,
            )
            return 1

    config = {}

    if "OLD_CONFIGURE" not in os.environ:
        os.environ["OLD_CONFIGURE"] = os.path.join(base_dir, "old-configure")

    sandbox = ConfigureSandbox(config, os.environ, argv)

    if not sandbox._help:
        # This limitation has mostly to do with GNU make. Since make can't represent
        # variables with spaces without correct quoting and many paths are used
        # without proper quoting, using paths with spaces commonly results in
        # targets or dependencies being treated as multiple paths. This, of course,
        # undermines the ability for make to perform up-to-date checks and makes
        # the build system not work very efficiently. In theory, a non-make build
        # backend will make this limitation go away. But there is likely a long tail
        # of things that will need fixing due to e.g. lack of proper path quoting.
        topsrcdir = os.path.realpath(os.path.dirname(__file__))
        if len(topsrcdir.split()) > 1:
            print(
                "Source directory cannot be located in a path with spaces: %s"
                % topsrcdir,
                file=sys.stderr,
            )
            return 1
        topobjdir = os.path.realpath(os.curdir)
        if len(topobjdir.split()) > 1:
            print(
                "Object directory cannot be located in a path with spaces: %s"
                % topobjdir,
                file=sys.stderr,
            )
            return 1

        # Do not allow topobjdir == topsrcdir
        if os.path.samefile(topsrcdir, topobjdir):
            print(
                "  ***\n"
                "  * Building directly in the main source directory is not allowed.\n"
                "  *\n"
                "  * To build, you must run configure from a separate directory\n"
                "  * (referred to as an object directory).\n"
                "  *\n"
                "  * If you are building with a mozconfig, you will need to change your\n"
                "  * mozconfig to point to a different object directory.\n"
                "  ***",
                file=sys.stderr,
            )
            return 1
        buildstatus("START_configure activate virtualenv")
        _activate_build_virtualenv()
        buildstatus("END_configure activate virtualenv")

    clobber_file = "CLOBBER"
    if not os.path.exists(clobber_file):
        # Simply touch the file.
        with open(clobber_file, "a"):
            pass

    if os.environ.get("MOZ_CONFIGURE_TRACE"):
        sandbox._logger.setLevel(TRACE)

    buildstatus("START_configure read moz.configure")
    sandbox.include_file(os.path.join(os.path.dirname(__file__), "moz.configure"))
    buildstatus("END_configure read moz.configure")
    buildstatus("START_configure run moz.configure")
    sandbox.run()
    buildstatus("END_configure run moz.configure")

    if sandbox._help:
        return 0

    buildstatus("START_configure config.status")
    logging.getLogger("moz.configure").info("Creating config.status")

    old_js_configure_substs = config.pop("OLD_JS_CONFIGURE_SUBSTS", None)
    old_js_configure_defines = config.pop("OLD_JS_CONFIGURE_DEFINES", None)
    try:
        if old_js_configure_substs or old_js_configure_defines:
            js_config = config.copy()
            pwd = os.getcwd()
            try:
                try:
                    os.makedirs("js/src")
                except OSError as e:
                    if e.errno != errno.EEXIST:
                        raise

                os.chdir("js/src")
                js_config["OLD_CONFIGURE_SUBSTS"] = old_js_configure_substs
                js_config["OLD_CONFIGURE_DEFINES"] = old_js_configure_defines
                # The build system frontend expects $objdir/js/src/config.status
                # to have $objdir/js/src as topobjdir.
                # We want forward slashes on all platforms.
                js_config["TOPOBJDIR"] += "/js/src"
                ret = config_status(js_config, execute=False)
                if ret:
                    return ret
            finally:
                os.chdir(pwd)

        return config_status(config)
    finally:
        buildstatus("END_configure config.status")


def check_unicode(obj):
    """Recursively check that all strings in the object are unicode strings."""
    if isinstance(obj, dict):
        result = True
        for k, v in six.iteritems(obj):
            if not check_unicode(k):
                print("%s key is not unicode." % k, file=sys.stderr)
                result = False
            elif not check_unicode(v):
                print("%s value is not unicode." % k, file=sys.stderr)
                result = False
        return result
    if isinstance(obj, bytes):
        return False
    if isinstance(obj, six.text_type):
        return True
    if isinstance(obj, Iterable):
        return all(check_unicode(o) for o in obj)
    return True


def config_status(config, execute=True):
    # Sanitize config data to feed config.status
    # Ideally, all the backend and frontend code would handle the booleans, but
    # there are so many things involved, that it's easier to keep config.status
    # untouched for now.
    def sanitize_config(v):
        if v is True:
            return "1"
        if v is False:
            return ""
        # Serialize types that look like lists and tuples as lists.
        if not isinstance(v, (bytes, six.text_type, dict)) and isinstance(v, Iterable):
            return list(v)
        return v

    sanitized_config = {}
    sanitized_config["substs"] = {
        k: sanitize_config(v)
        for k, v in six.iteritems(config)
        if k
        not in (
            "DEFINES",
            "TOPSRCDIR",
            "TOPOBJDIR",
            "CONFIG_STATUS_DEPS",
            "OLD_CONFIGURE_SUBSTS",
            "OLD_CONFIGURE_DEFINES",
        )
    }
    for k, v in config["OLD_CONFIGURE_SUBSTS"]:
        sanitized_config["substs"][k] = sanitize_config(v)
    sanitized_config["defines"] = {
        k: sanitize_config(v) for k, v in six.iteritems(config["DEFINES"])
    }
    for k, v in config["OLD_CONFIGURE_DEFINES"]:
        sanitized_config["defines"][k] = sanitize_config(v)
    sanitized_config["topsrcdir"] = config["TOPSRCDIR"]
    sanitized_config["topobjdir"] = config["TOPOBJDIR"]
    sanitized_config["mozconfig"] = config.get("MOZCONFIG")

    if not check_unicode(sanitized_config):
        print("Configuration should be all unicode.", file=sys.stderr)
        print("Please file a bug for the above.", file=sys.stderr)
        return 1

    # Create config.status. Eventually, we'll want to just do the work it does
    # here, when we're able to skip configure tests/use cached results/not rely
    # on autoconf.
    with codecs.open("config.status", "w", "utf-8") as fh:
        fh.write(
            textwrap.dedent(
                """\
            #!%(python)s
            # coding=utf-8
            from mozbuild.configure.constants import *
        """
            )
            % {"python": config["PYTHON3"]}
        )
        for k, v in sorted(six.iteritems(sanitized_config)):
            fh.write("%s = " % k)
            pprint.pprint(v, stream=fh, indent=4)
        fh.write(
            "__all__ = ['topobjdir', 'topsrcdir', 'defines', " "'substs', 'mozconfig']"
        )

        if execute:
            fh.write(
                textwrap.dedent(
                    """
                if __name__ == '__main__':
                    from mozbuild.config_status import config_status
                    args = dict([(name, globals()[name]) for name in __all__])
                    config_status(**args)
            """
                )
            )

    partial_config = PartialConfigEnvironment(config["TOPOBJDIR"])
    partial_config.write_vars(sanitized_config)

    # Write out a file so the build backend knows to re-run configure when
    # relevant Python changes.
    with io.open("config_status_deps.in", "w", encoding="utf-8", newline="\n") as fh:
        for f in sorted(
            itertools.chain(
                config["CONFIG_STATUS_DEPS"],
                iter_modules_in_path(config["TOPOBJDIR"], config["TOPSRCDIR"]),
            )
        ):
            fh.write("%s\n" % mozpath.normpath(f))

    # Other things than us are going to run this file, so we need to give it
    # executable permissions.
    os.chmod("config.status", 0o755)
    if execute:
        from mozbuild.config_status import config_status

        return config_status(args=[], **sanitized_config)
    return 0


def _activate_build_virtualenv():
    """Ensure that the build virtualenv is activated

    configure.py may be executed through Mach, or via "./configure, make".
    In the first case, the build virtualenv should already be activated.
    In the second case, we're likely being executed with the system Python, and must
    prepare the virtualenv and activate it ourselves.
    """

    version = ".".join(str(i) for i in sys.version_info[0:3])
    print(f"Using Python {version} from {sys.executable}")

    active_site = MozSiteMetadata.from_runtime()
    if active_site and active_site.site_name == "build":
        # We're already running within the "build" virtualenv, no additional work is
        # needed.
        return

    # We're using the system python (or are nested within a non-build mach-managed
    # virtualenv), so we should activate the build virtualenv as expected by the rest of
    # configure.

    topsrcdir = os.path.realpath(os.path.dirname(__file__))

    mach_site = MachSiteManager(
        topsrcdir,
        None,
        MachEnvRequirements(),
        ExternalPythonSite(sys.executable),
        SitePackagesSource.NONE,
    )
    mach_site.activate()

    from mach.util import get_virtualenv_base_dir

    build_site = CommandSiteManager.from_environment(
        topsrcdir,
        None,
        "build",
        get_virtualenv_base_dir(topsrcdir),
    )
    if not build_site.ensure():
        print("Created Python 3 virtualenv")
    build_site.activate()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
