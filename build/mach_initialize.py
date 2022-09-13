# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import division, print_function, unicode_literals

import math
import os
import shutil
import sys
from pathlib import Path

if sys.version_info[0] < 3:
    import __builtin__ as builtins

    class MetaPathFinder(object):
        pass


else:
    from importlib.abc import MetaPathFinder


from types import ModuleType


STATE_DIR_FIRST_RUN = """
Mach and the build system store shared state in a common directory
on the filesystem. The following directory will be created:

  {}

If you would like to use a different directory, hit CTRL+c, set the
MOZBUILD_STATE_PATH environment variable to the directory you'd like to
use, and run Mach again.

Press ENTER/RETURN to continue or CTRL+c to abort.
""".strip()


CATEGORIES = {
    "build": {
        "short": "Build Commands",
        "long": "Interact with the build system",
        "priority": 80,
    },
    "post-build": {
        "short": "Post-build Commands",
        "long": "Common actions performed after completing a build.",
        "priority": 70,
    },
    "testing": {
        "short": "Testing",
        "long": "Run tests.",
        "priority": 60,
    },
    "ci": {
        "short": "CI",
        "long": "Taskcluster commands",
        "priority": 59,
    },
    "devenv": {
        "short": "Development Environment",
        "long": "Set up and configure your development environment.",
        "priority": 50,
    },
    "build-dev": {
        "short": "Low-level Build System Interaction",
        "long": "Interact with specific parts of the build system.",
        "priority": 20,
    },
    "misc": {
        "short": "Potpourri",
        "long": "Potent potables and assorted snacks.",
        "priority": 10,
    },
    "release": {
        "short": "Release automation",
        "long": "Commands for used in release automation.",
        "priority": 5,
    },
    "disabled": {
        "short": "Disabled",
        "long": "The disabled commands are hidden by default. Use -v to display them. "
        "These commands are unavailable for your current context, "
        'run "mach <command>" to see why.',
        "priority": 0,
    },
}


def _activate_python_environment(topsrcdir, get_state_dir):
    from mach.site import MachSiteManager

    mach_environment = MachSiteManager.from_environment(
        topsrcdir,
        get_state_dir,
    )
    mach_environment.activate()


def _maybe_activate_mozillabuild_environment():
    if sys.platform != "win32":
        return

    mozillabuild = Path(os.environ.get("MOZILLABUILD", r"C:\mozilla-build"))
    os.environ.setdefault("MOZILLABUILD", str(mozillabuild))
    assert mozillabuild.exists(), (
        f'MozillaBuild was not found at "{mozillabuild}".\n'
        "If it's installed in a different location, please "
        'set the "MOZILLABUILD" environment variable '
        "accordingly."
    )

    use_msys2 = (mozillabuild / "msys2").exists()
    if use_msys2:
        mozillabuild_msys_tools_path = mozillabuild / "msys2" / "usr" / "bin"
    else:
        mozillabuild_msys_tools_path = mozillabuild / "msys" / "bin"

    paths_to_add = [mozillabuild_msys_tools_path, mozillabuild / "bin"]
    existing_paths = [Path(p) for p in os.environ.get("PATH", "").split(os.pathsep)]
    for new_path in paths_to_add:
        if new_path not in existing_paths:
            os.environ["PATH"] += f"{os.pathsep}{new_path}"


def initialize(topsrcdir):
    # This directory was deleted in bug 1666345, but there may be some ignored
    # files here. We can safely just delete it for the user so they don't have
    # to clean the repo themselves.
    deleted_dir = os.path.join(topsrcdir, "third_party", "python", "psutil")
    if os.path.exists(deleted_dir):
        shutil.rmtree(deleted_dir, ignore_errors=True)

    # We need the "mach" module to access the logic to parse virtualenv
    # requirements. Since that depends on "packaging" (and, transitively,
    # "pyparsing"), we add those to the path too.
    sys.path[0:0] = [
        os.path.join(topsrcdir, module)
        for module in (
            os.path.join("python", "mach"),
            os.path.join("third_party", "python", "packaging"),
            os.path.join("third_party", "python", "pyparsing"),
        )
    ]

    from mach.util import setenv, get_state_dir

    state_dir = _create_state_dir()

    # normpath state_dir to normalize msys-style slashes.
    _activate_python_environment(
        topsrcdir, lambda: os.path.normpath(get_state_dir(True, topsrcdir=topsrcdir))
    )
    _maybe_activate_mozillabuild_environment()

    import mach.base
    import mach.main

    from mach.main import MachCommandReference

    # Centralized registry of available mach commands
    MACH_COMMANDS = {
        "valgrind-test": MachCommandReference("build/valgrind/mach_commands.py"),
        "devtools-css-db": MachCommandReference(
            "devtools/shared/css/generated/mach_commands.py"
        ),
        "webidl-example": MachCommandReference("dom/bindings/mach_commands.py"),
        "webidl-parser-test": MachCommandReference("dom/bindings/mach_commands.py"),
        "hazards": MachCommandReference(
            "js/src/devtools/rootAnalysis/mach_commands.py"
        ),
        "reftest": MachCommandReference("layout/tools/reftest/mach_commands.py"),
        "jstestbrowser": MachCommandReference("layout/tools/reftest/mach_commands.py"),
        "crashtest": MachCommandReference("layout/tools/reftest/mach_commands.py"),
        "android": MachCommandReference("mobile/android/mach_commands.py"),
        "gradle": MachCommandReference("mobile/android/mach_commands.py"),
        "gradle-install": MachCommandReference("mobile/android/mach_commands.py"),
        "mach-commands": MachCommandReference(
            "python/mach/mach/commands/commandinfo.py"
        ),
        "mach-debug-commands": MachCommandReference(
            "python/mach/mach/commands/commandinfo.py"
        ),
        "mach-completion": MachCommandReference(
            "python/mach/mach/commands/commandinfo.py"
        ),
        "settings": MachCommandReference("python/mach/mach/commands/settings.py"),
        "python": MachCommandReference("python/mach_commands.py"),
        "python-test": MachCommandReference("python/mach_commands.py"),
        "bootstrap": MachCommandReference("python/mozboot/mozboot/mach_commands.py"),
        "vcs-setup": MachCommandReference("python/mozboot/mozboot/mach_commands.py"),
        "artifact": MachCommandReference(
            "python/mozbuild/mozbuild/artifact_commands.py"
        ),
        "ide": MachCommandReference(
            "python/mozbuild/mozbuild/backend/mach_commands.py"
        ),
        "build": MachCommandReference("python/mozbuild/mozbuild/build_commands.py"),
        "configure": MachCommandReference("python/mozbuild/mozbuild/build_commands.py"),
        "resource-usage": MachCommandReference(
            "python/mozbuild/mozbuild/build_commands.py"
        ),
        "build-backend": MachCommandReference(
            "python/mozbuild/mozbuild/build_commands.py"
        ),
        "clang-tidy": MachCommandReference(
            "python/mozbuild/mozbuild/code_analysis/mach_commands.py"
        ),
        "static-analysis": MachCommandReference(
            "python/mozbuild/mozbuild/code_analysis/mach_commands.py"
        ),
        "prettier-format": MachCommandReference(
            "python/mozbuild/mozbuild/code_analysis/mach_commands.py"
        ),
        "clang-format": MachCommandReference(
            "python/mozbuild/mozbuild/code_analysis/mach_commands.py"
        ),
        "compileflags": MachCommandReference(
            "python/mozbuild/mozbuild/compilation/codecomplete.py"
        ),
        "mozbuild-reference": MachCommandReference(
            "python/mozbuild/mozbuild/frontend/mach_commands.py"
        ),
        "file-info": MachCommandReference(
            "python/mozbuild/mozbuild/frontend/mach_commands.py"
        ),
        "vendor": MachCommandReference(
            "python/mozbuild/mozbuild/vendor/mach_commands.py"
        ),
        "watch": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "cargo": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "doctor": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "clobber": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "show-log": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "warnings-summary": MachCommandReference(
            "python/mozbuild/mozbuild/mach_commands.py"
        ),
        "warnings-list": MachCommandReference(
            "python/mozbuild/mozbuild/mach_commands.py"
        ),
        "gtest": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "package": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "install": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "run": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "buildsymbols": MachCommandReference(
            "python/mozbuild/mozbuild/mach_commands.py"
        ),
        "environment": MachCommandReference(
            "python/mozbuild/mozbuild/mach_commands.py"
        ),
        "repackage": MachCommandReference("python/mozbuild/mozbuild/mach_commands.py"),
        "package-multi-locale": MachCommandReference(
            "python/mozbuild/mozbuild/mach_commands.py"
        ),
        "perftest": MachCommandReference(
            "python/mozperftest/mozperftest/mach_commands.py"
        ),
        "perftest-test": MachCommandReference(
            "python/mozperftest/mozperftest/mach_commands.py"
        ),
        "release": MachCommandReference(
            "python/mozrelease/mozrelease/mach_commands.py"
        ),
        "remote": MachCommandReference("remote/mach_commands.py"),
        "puppeteer-test": MachCommandReference("remote/mach_commands.py"),
        "generate-test-certs": MachCommandReference(
            "security/manager/tools/mach_commands.py"
        ),
        "taskgraph": MachCommandReference("taskcluster/mach_commands.py"),
        "taskcluster-load-image": MachCommandReference("taskcluster/mach_commands.py"),
        "taskcluster-build-image": MachCommandReference("taskcluster/mach_commands.py"),
        "taskcluster-image-digest": MachCommandReference(
            "taskcluster/mach_commands.py"
        ),
        "release-history": MachCommandReference("taskcluster/mach_commands.py"),
        "awsy-test": MachCommandReference("testing/awsy/mach_commands.py"),
        "fetch-condprofile": MachCommandReference(
            "testing/condprofile/mach_commands.py"
        ),
        "run-condprofile": MachCommandReference("testing/condprofile/mach_commands.py"),
        "firefox-ui-functional": MachCommandReference(
            "testing/firefox-ui/mach_commands.py"
        ),
        "geckodriver": MachCommandReference("testing/geckodriver/mach_commands.py"),
        "addtest": MachCommandReference("testing/mach_commands.py"),
        "test": MachCommandReference("testing/mach_commands.py"),
        "cppunittest": MachCommandReference("testing/mach_commands.py"),
        "jstests": MachCommandReference("testing/mach_commands.py"),
        "jit-test": MachCommandReference("testing/mach_commands.py"),
        "jsapi-tests": MachCommandReference("testing/mach_commands.py"),
        "jsshell-bench": MachCommandReference("testing/mach_commands.py"),
        "cramtest": MachCommandReference("testing/mach_commands.py"),
        "test-info": MachCommandReference("testing/mach_commands.py"),
        "rusttests": MachCommandReference("testing/mach_commands.py"),
        "fluent-migration-test": MachCommandReference("testing/mach_commands.py"),
        "marionette-test": MachCommandReference("testing/marionette/mach_commands.py"),
        "mochitest": MachCommandReference("testing/mochitest/mach_commands.py"),
        "geckoview-junit": MachCommandReference("testing/mochitest/mach_commands.py"),
        "mozharness": MachCommandReference("testing/mozharness/mach_commands.py"),
        "raptor": MachCommandReference("testing/raptor/mach_commands.py"),
        "raptor-test": MachCommandReference("testing/raptor/mach_commands.py"),
        "talos-test": MachCommandReference("testing/talos/mach_commands.py"),
        "tps-build": MachCommandReference("testing/tps/mach_commands.py"),
        "web-platform-tests": MachCommandReference(
            "testing/web-platform/mach_commands.py"
        ),
        "wpt": MachCommandReference("testing/web-platform/mach_commands.py"),
        "web-platform-tests-update": MachCommandReference(
            "testing/web-platform/mach_commands.py"
        ),
        "wpt-update": MachCommandReference("testing/web-platform/mach_commands.py"),
        "wpt-manifest-update": MachCommandReference(
            "testing/web-platform/mach_commands.py"
        ),
        "wpt-serve": MachCommandReference("testing/web-platform/mach_commands.py"),
        "wpt-metadata-summary": MachCommandReference(
            "testing/web-platform/mach_commands.py"
        ),
        "wpt-metadata-merge": MachCommandReference(
            "testing/web-platform/mach_commands.py"
        ),
        "wpt-unittest": MachCommandReference("testing/web-platform/mach_commands.py"),
        "wpt-test-paths": MachCommandReference("testing/web-platform/mach_commands.py"),
        "wpt-fission-regressions": MachCommandReference(
            "testing/web-platform/mach_commands.py"
        ),
        "xpcshell-test": MachCommandReference("testing/xpcshell/mach_commands.py"),
        "telemetry-tests-client": MachCommandReference(
            "toolkit/components/telemetry/tests/marionette/mach_commands.py"
        ),
        "data-review": MachCommandReference(
            "toolkit/components/glean/build_scripts/mach_commands.py"
        ),
        "perf-data-review": MachCommandReference(
            "toolkit/components/glean/build_scripts/mach_commands.py"
        ),
        "update-glean-tags": MachCommandReference(
            "toolkit/components/glean/build_scripts/mach_commands.py"
        ),
        "update-glean": MachCommandReference(
            "toolkit/components/glean/build_scripts/mach_commands.py"
        ),
        "browsertime": MachCommandReference("tools/browsertime/mach_commands.py"),
        "compare-locales": MachCommandReference(
            "tools/compare-locales/mach_commands.py"
        ),
        "l10n-cross-channel": MachCommandReference("tools/lint/mach_commands.py"),
        "busted": MachCommandReference("tools/mach_commands.py"),
        "pastebin": MachCommandReference("tools/mach_commands.py"),
        "mozregression": MachCommandReference("tools/mach_commands.py"),
        "node": MachCommandReference("tools/mach_commands.py"),
        "npm": MachCommandReference("tools/mach_commands.py"),
        "logspam": MachCommandReference("tools/mach_commands.py"),
        "doc": MachCommandReference("tools/moztreedocs/mach_commands.py"),
        "install-moz-phab": MachCommandReference("tools/phabricator/mach_commands.py"),
        "power": MachCommandReference("tools/power/mach_commands.py"),
        "try": MachCommandReference("tools/tryselect/mach_commands.py"),
        "import-pr": MachCommandReference("tools/vcs/mach_commands.py"),
        "test-interventions": MachCommandReference(
            "testing/webcompat/mach_commands.py"
        ),
        "esmify": MachCommandReference("tools/esmify/mach_commands.py"),
        "xpcshell": MachCommandReference("js/xpconnect/mach_commands.py"),
        "uniffi": MachCommandReference(
            "toolkit/components/uniffi-bindgen-gecko-js/mach_commands.py"
        ),
        "storybook": MachCommandReference(
            "browser/components/storybook/mach_commands.py"
        ),
    }

    # Set a reasonable limit to the number of open files.
    #
    # Some linux systems set `ulimit -n` to a very high number, which works
    # well for systems that run servers, but this setting causes performance
    # problems when programs close file descriptors before forking, like
    # Python's `subprocess.Popen(..., close_fds=True)` (close_fds=True is the
    # default in Python 3), or Rust's stdlib.  In some cases, Firefox does the
    # same thing when spawning processes.  We would prefer to lower this limit
    # to avoid such performance problems; processes spawned by `mach` will
    # inherit the limit set here.
    #
    # The Firefox build defaults the soft limit to 1024, except for builds that
    # do LTO, where the soft limit is 8192.  We're going to default to the
    # latter, since people do occasionally do LTO builds on their local
    # machines, and requiring them to discover another magical setting after
    # setting up an LTO build in the first place doesn't seem good.
    #
    # This code mimics the code in taskcluster/scripts/run-task.
    try:
        import resource

        # Keep the hard limit the same, though, allowing processes to change
        # their soft limit if they need to (Firefox does, for instance).
        (soft, hard) = resource.getrlimit(resource.RLIMIT_NOFILE)
        # Permit people to override our default limit if necessary via
        # MOZ_LIMIT_NOFILE, which is the same variable `run-task` uses.
        limit = os.environ.get("MOZ_LIMIT_NOFILE")
        if limit:
            limit = int(limit)
        else:
            # If no explicit limit is given, use our default if it's less than
            # the current soft limit.  For instance, the default on macOS is
            # 256, so we'd pick that rather than our default.
            limit = min(soft, 8192)
        # Now apply the limit, if it's different from the original one.
        if limit != soft:
            resource.setrlimit(resource.RLIMIT_NOFILE, (limit, hard))
    except ImportError:
        # The resource module is UNIX only.
        pass

    def resolve_repository():
        import mozversioncontrol

        try:
            # This API doesn't respect the vcs binary choices from configure.
            # If we ever need to use the VCS binary here, consider something
            # more robust.
            return mozversioncontrol.get_repository_object(path=topsrcdir)
        except (mozversioncontrol.InvalidRepoPath, mozversioncontrol.MissingVCSTool):
            return None

    def pre_dispatch_handler(context, handler, args):
        # If --disable-tests flag was enabled in the mozconfig used to compile
        # the build, tests will be disabled. Instead of trying to run
        # nonexistent tests then reporting a failure, this will prevent mach
        # from progressing beyond this point.
        if handler.category == "testing" and not handler.ok_if_tests_disabled:
            from mozbuild.base import BuildEnvironmentNotFoundException

            try:
                from mozbuild.base import MozbuildObject

                # all environments should have an instance of build object.
                build = MozbuildObject.from_environment()
                if build is not None and not getattr(
                    build, "substs", {"ENABLE_TESTS": True}
                ).get("ENABLE_TESTS"):
                    print(
                        "Tests have been disabled with --disable-tests.\n"
                        + "Remove the flag, and re-compile to enable tests."
                    )
                    sys.exit(1)
            except BuildEnvironmentNotFoundException:
                # likely automation environment, so do nothing.
                pass

    def post_dispatch_handler(
        context, handler, instance, success, start_time, end_time, depth, args
    ):
        """Perform global operations after command dispatch.


        For now,  we will use this to handle build system telemetry.
        """

        # Don't finalize telemetry data if this mach command was invoked as part of
        # another mach command.
        if depth != 1:
            return

        _finalize_telemetry_glean(
            context.telemetry, handler.name == "bootstrap", success
        )

    def populate_context(key=None):
        if key is None:
            return
        if key == "state_dir":
            return state_dir

        if key == "local_state_dir":
            return get_state_dir(specific_to_topsrcdir=True)

        if key == "topdir":
            return topsrcdir

        if key == "pre_dispatch_handler":
            return pre_dispatch_handler

        if key == "post_dispatch_handler":
            return post_dispatch_handler

        if key == "repository":
            return resolve_repository()

        raise AttributeError(key)

    # Note which process is top-level so that recursive mach invocations can avoid writing
    # telemetry data.
    if "MACH_MAIN_PID" not in os.environ:
        setenv("MACH_MAIN_PID", str(os.getpid()))

    driver = mach.main.Mach(os.getcwd())
    driver.populate_context_handler = populate_context

    if not driver.settings_paths:
        # default global machrc location
        driver.settings_paths.append(state_dir)
    # always load local repository configuration
    driver.settings_paths.append(topsrcdir)

    for category, meta in CATEGORIES.items():
        driver.define_category(category, meta["short"], meta["long"], meta["priority"])

    # Sparse checkouts may not have all mach_commands.py files. Ignore
    # errors from missing files. Same for spidermonkey tarballs.
    repo = resolve_repository()
    missing_ok = (
        repo is not None and repo.sparse_checkout_present()
    ) or os.path.exists(os.path.join(topsrcdir, "INSTALL"))

    driver.load_commands_from_spec(MACH_COMMANDS, topsrcdir, missing_ok=missing_ok)

    return driver


def _finalize_telemetry_glean(telemetry, is_bootstrap, success):
    """Submit telemetry collected by Glean.

    Finalizes some metrics (command success state and duration, system information) and
    requests Glean to send the collected data.
    """

    from mach.telemetry import MACH_METRICS_PATH
    from mozbuild.telemetry import (
        get_cpu_brand,
        get_distro_and_version,
        get_psutil_stats,
        get_shell_info,
        get_vscode_running,
    )

    mach_metrics = telemetry.metrics(MACH_METRICS_PATH)
    mach_metrics.mach.duration.stop()
    mach_metrics.mach.success.set(success)
    system_metrics = mach_metrics.mach.system
    cpu_brand = get_cpu_brand()
    if cpu_brand:
        system_metrics.cpu_brand.set(cpu_brand)
    distro, version = get_distro_and_version()
    system_metrics.distro.set(distro)
    system_metrics.distro_version.set(version)

    vscode_terminal, ssh_connection = get_shell_info()
    system_metrics.vscode_terminal.set(vscode_terminal)
    system_metrics.ssh_connection.set(ssh_connection)
    system_metrics.vscode_running.set(get_vscode_running())

    has_psutil, logical_cores, physical_cores, memory_total = get_psutil_stats()
    if has_psutil:
        # psutil may not be available (we may not have been able to download
        # a wheel or build it from source).
        system_metrics.logical_cores.add(logical_cores)
        if physical_cores is not None:
            system_metrics.physical_cores.add(physical_cores)
        if memory_total is not None:
            system_metrics.memory.accumulate(
                int(math.ceil(float(memory_total) / (1024 * 1024 * 1024)))
            )
    telemetry.submit(is_bootstrap)


def _create_state_dir():
    # Global build system and mach state is stored in a central directory. By
    # default, this is ~/.mozbuild. However, it can be defined via an
    # environment variable. We detect first run (by lack of this directory
    # existing) and notify the user that it will be created. The logic for
    # creation is much simpler for the "advanced" environment variable use
    # case. For default behavior, we educate users and give them an opportunity
    # to react.
    state_dir = os.environ.get("MOZBUILD_STATE_PATH")
    if state_dir:
        if not os.path.exists(state_dir):
            print(
                "Creating global state directory from environment variable: {}".format(
                    state_dir
                )
            )
    else:
        state_dir = os.path.expanduser("~/.mozbuild")
        if not os.path.exists(state_dir):
            if not os.environ.get("MOZ_AUTOMATION"):
                print(STATE_DIR_FIRST_RUN.format(state_dir))
                try:
                    sys.stdin.readline()
                    print("\n")
                except KeyboardInterrupt:
                    sys.exit(1)

            print("Creating default state directory: {}".format(state_dir))

    os.makedirs(state_dir, mode=0o770, exist_ok=True)
    return state_dir


# Hook import such that .pyc/.pyo files without a corresponding .py file in
# the source directory are essentially ignored. See further below for details
# and caveats.
# Objdirs outside the source directory are ignored because in most cases, if
# a .pyc/.pyo file exists there, a .py file will be next to it anyways.
class ImportHook(object):
    def __init__(self, original_import):
        self._original_import = original_import
        # Assume the source directory is the parent directory of the one
        # containing this file.
        self._source_dir = (
            os.path.normcase(
                os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
            )
            + os.sep
        )
        self._modules = set()

    def __call__(self, name, globals=None, locals=None, fromlist=None, level=-1):
        if sys.version_info[0] >= 3 and level < 0:
            level = 0

        # name might be a relative import. Instead of figuring out what that
        # resolves to, which is complex, just rely on the real import.
        # Since we don't know the full module name, we can't check sys.modules,
        # so we need to keep track of which modules we've already seen to avoid
        # to stat() them again when they are imported multiple times.
        module = self._original_import(name, globals, locals, fromlist, level)

        # Some tests replace modules in sys.modules with non-module instances.
        if not isinstance(module, ModuleType):
            return module

        resolved_name = module.__name__
        if resolved_name in self._modules:
            return module
        self._modules.add(resolved_name)

        # Builtin modules don't have a __file__ attribute.
        if not getattr(module, "__file__", None):
            return module

        # Note: module.__file__ is not always absolute.
        path = os.path.normcase(os.path.abspath(module.__file__))
        # Note: we could avoid normcase and abspath above for non pyc/pyo
        # files, but those are actually rare, so it doesn't really matter.
        if not path.endswith((".pyc", ".pyo")):
            return module

        # Ignore modules outside our source directory
        if not path.startswith(self._source_dir):
            return module

        # If there is no .py corresponding to the .pyc/.pyo module we're
        # loading, remove the .pyc/.pyo file, and reload the module.
        # Since we already loaded the .pyc/.pyo module, if it had side
        # effects, they will have happened already, and loading the module
        # with the same name, from another directory may have the same side
        # effects (or different ones). We assume it's not a problem for the
        # python modules under our source directory (either because it
        # doesn't happen or because it doesn't matter).
        if not os.path.exists(module.__file__[:-1]):
            if os.path.exists(module.__file__):
                os.remove(module.__file__)
            del sys.modules[module.__name__]
            module = self(name, globals, locals, fromlist, level)

        return module


# Hook import such that .pyc/.pyo files without a corresponding .py file in
# the source directory are essentially ignored. See further below for details
# and caveats.
# Objdirs outside the source directory are ignored because in most cases, if
# a .pyc/.pyo file exists there, a .py file will be next to it anyways.
class FinderHook(MetaPathFinder):
    def __init__(self, klass):
        # Assume the source directory is the parent directory of the one
        # containing this file.
        self._source_dir = (
            os.path.normcase(
                os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
            )
            + os.sep
        )
        self.finder_class = klass

    def find_spec(self, full_name, paths=None, target=None):
        spec = self.finder_class.find_spec(full_name, paths, target)

        # Some modules don't have an origin.
        if spec is None or spec.origin is None:
            return spec

        # Normalize the origin path.
        path = os.path.normcase(os.path.abspath(spec.origin))
        # Note: we could avoid normcase and abspath above for non pyc/pyo
        # files, but those are actually rare, so it doesn't really matter.
        if not path.endswith((".pyc", ".pyo")):
            return spec

        # Ignore modules outside our source directory
        if not path.startswith(self._source_dir):
            return spec

        # If there is no .py corresponding to the .pyc/.pyo module we're
        # resolving, remove the .pyc/.pyo file, and try again.
        if not os.path.exists(spec.origin[:-1]):
            if os.path.exists(spec.origin):
                os.remove(spec.origin)
            spec = self.finder_class.find_spec(full_name, paths, target)

        return spec


# Additional hook for python >= 3.8's importlib.metadata.
class MetadataHook(FinderHook):
    def find_distributions(self, *args, **kwargs):
        return self.finder_class.find_distributions(*args, **kwargs)


def hook(finder):
    has_find_spec = hasattr(finder, "find_spec")
    has_find_distributions = hasattr(finder, "find_distributions")
    if has_find_spec and has_find_distributions:
        return MetadataHook(finder)
    elif has_find_spec:
        return FinderHook(finder)
    return finder


# Install our hook. This can be deleted when the Python 3 migration is complete.
if sys.version_info[0] < 3:
    builtins.__import__ = ImportHook(builtins.__import__)
else:
    sys.meta_path = [hook(c) for c in sys.meta_path]
