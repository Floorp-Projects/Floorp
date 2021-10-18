# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import os
import sys
import textwrap

from mach.base import FailedCommandError, MachError
from mach.decorators import (
    CommandArgument,
    Command,
    SubCommand,
)
from mach.registrar import Registrar

from mozbuild.mozconfig import MozconfigLoader

# Command files like this are listed in build/mach_initialize.py in alphabetical
# order, but we need to access commands earlier in the sorted order to grab
# their arguments. Force them to load now.
import mozbuild.artifact_commands  # NOQA: F401
import mozbuild.build_commands  # NOQA: F401


# Use a decorator to copy command arguments off of the named command. Instead
# of a decorator, this could be straight code that edits eg
# MachCommands.build_shell._mach_command.arguments, but that looked uglier.
def inherit_command_args(command, subcommand=None):
    """Decorator for inheriting all command-line arguments from `mach build`.

    This should come earlier in the source file than @Command or @SubCommand,
    because it relies on that decorator having run first."""

    def inherited(func):
        handler = Registrar.command_handlers.get(command)
        if handler is not None and subcommand is not None:
            handler = handler.subcommand_handlers.get(subcommand)
        if handler is None:
            raise MachError(
                "{} command unknown or not yet loaded".format(
                    command if subcommand is None else command + " " + subcommand
                )
            )
        func._mach_command.arguments.extend(handler.arguments)
        return func

    return inherited


def state_dir():
    return os.environ.get("MOZBUILD_STATE_PATH", os.path.expanduser("~/.mozbuild"))


def tools_dir():
    if os.environ.get("MOZ_FETCHES_DIR"):
        # In automation, tools are provided by toolchain dependencies.
        return os.path.join(os.environ["HOME"], os.environ["MOZ_FETCHES_DIR"])

    # In development, `mach hazard bootstrap` installs the tools separately
    # to avoid colliding with the "main" compiler versions, which can
    # change separately (and the precompiled sixgill and compiler version
    # must match exactly).
    return os.path.join(state_dir(), "hazard-tools")


def sixgill_dir():
    return os.path.join(tools_dir(), "sixgill")


def gcc_dir():
    return os.path.join(tools_dir(), "gcc")


def script_dir(command_context):
    return os.path.join(command_context.topsrcdir, "js/src/devtools/rootAnalysis")


def get_work_dir(command_context, application, given):
    if given is not None:
        return given
    return os.path.join(command_context.topsrcdir, "haz-" + application)


def ensure_dir_exists(dir):
    os.makedirs(dir, exist_ok=True)
    return dir


# Force the use of hazard-compatible installs of tools.
def setup_env_for_tools(env):
    gccbin = os.path.join(gcc_dir(), "bin")
    env["CC"] = os.path.join(gccbin, "gcc")
    env["CXX"] = os.path.join(gccbin, "g++")
    env["PATH"] = "{sixgill_dir}/usr/bin:{gccbin}:{PATH}".format(
        sixgill_dir=sixgill_dir(), gccbin=gccbin, PATH=env["PATH"]
    )


def setup_env_for_shell(env, shell):
    """Add JS shell directory to dynamic lib search path"""
    for var in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        env[var] = ":".join(p for p in (env.get(var), os.path.dirname(shell)) if p)


@Command(
    "hazards",
    category="build",
    order="declaration",
    description="Commands for running the static analysis for GC rooting hazards",
)
def hazards(command_context):
    """Commands related to performing the GC rooting hazard analysis"""
    print("See `mach hazards --help` for a list of subcommands")


@inherit_command_args("artifact", "toolchain")
@SubCommand(
    "hazards",
    "bootstrap",
    description="Install prerequisites for the hazard analysis",
)
def bootstrap(command_context, **kwargs):
    orig_dir = os.getcwd()
    os.chdir(ensure_dir_exists(tools_dir()))
    try:
        kwargs["from_build"] = ("linux64-gcc-sixgill", "linux64-gcc-9")
        command_context._mach_context.commands.dispatch(
            "artifact", command_context._mach_context, subcommand="toolchain", **kwargs
        )
    finally:
        os.chdir(orig_dir)


@inherit_command_args("build")
@SubCommand(
    "hazards", "build-shell", description="Build a shell for the hazard analysis"
)
@CommandArgument(
    "--mozconfig",
    default=None,
    metavar="FILENAME",
    help="Build with the given mozconfig.",
)
def build_shell(command_context, **kwargs):
    """Build a JS shell to use to run the rooting hazard analysis."""
    # The JS shell requires some specific configuration settings to execute
    # the hazard analysis code, and configuration is done via mozconfig.
    # Subprocesses find MOZCONFIG in the environment, so we can't just
    # modify the settings in this process's loaded version. Pass it through
    # the environment.

    default_mozconfig = "js/src/devtools/rootAnalysis/mozconfig.haz_shell"
    mozconfig_path = (
        kwargs.pop("mozconfig", None)
        or os.environ.get("MOZCONFIG")
        or default_mozconfig
    )
    mozconfig_path = os.path.join(command_context.topsrcdir, mozconfig_path)
    loader = MozconfigLoader(command_context.topsrcdir)
    mozconfig = loader.read_mozconfig(mozconfig_path)

    # Validate the mozconfig settings in case the user overrode the default.
    configure_args = mozconfig["configure_args"]
    if "--enable-ctypes" not in configure_args:
        raise FailedCommandError(
            "ctypes required in hazard JS shell, mozconfig=" + mozconfig_path
        )

    # Transmit the mozconfig location to build subprocesses.
    os.environ["MOZCONFIG"] = mozconfig_path

    setup_env_for_tools(os.environ)

    # Set a default objdir for the shell, for developer builds.
    os.environ.setdefault(
        "MOZ_OBJDIR", os.path.join(command_context.topsrcdir, "obj-haz-shell")
    )

    return command_context._mach_context.commands.dispatch(
        "build", command_context._mach_context, **kwargs
    )


def read_json_file(filename):
    with open(filename) as fh:
        return json.load(fh)


def ensure_shell(command_context, objdir):
    if objdir is None:
        objdir = os.path.join(command_context.topsrcdir, "obj-haz-shell")

    try:
        binaries = read_json_file(os.path.join(objdir, "binaries.json"))
        info = [b for b in binaries["programs"] if b["program"] == "js"][0]
        return os.path.join(objdir, info["install_target"], "js")
    except (OSError, KeyError):
        raise FailedCommandError(
            """\
no shell found in %s -- must build the JS shell with `mach hazards build-shell` first"""
            % objdir
        )


@inherit_command_args("build")
@SubCommand(
    "hazards",
    "gather",
    description="Gather analysis data by compiling the given application",
)
@CommandArgument(
    "--application", default="browser", help="Build the given application."
)
@CommandArgument(
    "--haz-objdir", default=None, help="Write object files to this directory."
)
@CommandArgument(
    "--work-dir", default=None, help="Directory for output and working files."
)
def gather_hazard_data(command_context, **kwargs):
    """Gather analysis information by compiling the tree"""
    application = kwargs["application"]
    objdir = kwargs["haz_objdir"]
    if objdir is None:
        objdir = os.environ.get("HAZ_OBJDIR")
    if objdir is None:
        objdir = os.path.join(command_context.topsrcdir, "obj-analyzed-" + application)

    work_dir = get_work_dir(command_context, application, kwargs["work_dir"])
    ensure_dir_exists(work_dir)
    with open(os.path.join(work_dir, "defaults.py"), "wt") as fh:
        data = textwrap.dedent(
            """\
            analysis_scriptdir = "{script_dir}"
            objdir = "{objdir}"
            source = "{srcdir}"
            sixgill = "{sixgill_dir}/usr/libexec/sixgill"
            sixgill_bin = "{sixgill_dir}/usr/bin"
            gcc_bin = "{gcc_dir}/bin"
        """
        ).format(
            script_dir=script_dir(command_context),
            objdir=objdir,
            srcdir=command_context.topsrcdir,
            sixgill_dir=sixgill_dir(),
            gcc_dir=gcc_dir(),
        )
        fh.write(data)

    buildscript = " ".join(
        [
            command_context.topsrcdir + "/mach hazards compile",
            "--job-size=3.0",  # Conservatively estimate 3GB/process
            "--application=" + application,
            "--haz-objdir=" + objdir,
        ]
    )
    args = [
        sys.executable,
        os.path.join(script_dir(command_context), "analyze.py"),
        "dbs",
        "-v",
        "--buildcommand=" + buildscript,
    ]

    return command_context.run_process(args=args, cwd=work_dir, pass_thru=True)


@inherit_command_args("build")
@SubCommand("hazards", "compile", description=argparse.SUPPRESS)
@CommandArgument(
    "--mozconfig",
    default=None,
    metavar="FILENAME",
    help="Build with the given mozconfig.",
)
@CommandArgument(
    "--application", default="browser", help="Build the given application."
)
@CommandArgument(
    "--haz-objdir",
    default=os.environ.get("HAZ_OBJDIR"),
    help="Write object files to this directory.",
)
def inner_compile(command_context, **kwargs):
    """Build a source tree and gather analysis information while running
    under the influence of the analysis collection server."""

    env = os.environ

    # Check whether we are running underneath the manager (and therefore
    # have a server to talk to).
    if "XGILL_CONFIG" not in env:
        raise Exception(
            "no sixgill manager detected. `mach hazards compile` "
            + "should only be run from `mach hazards gather`"
        )

    app = kwargs.pop("application")
    default_mozconfig = "js/src/devtools/rootAnalysis/mozconfig.%s" % app
    mozconfig_path = (
        kwargs.pop("mozconfig", None) or env.get("MOZCONFIG") or default_mozconfig
    )
    mozconfig_path = os.path.join(command_context.topsrcdir, mozconfig_path)

    # Validate the mozconfig.

    # Require an explicit --enable-application=APP (even if you just
    # want to build the default browser application.)
    loader = MozconfigLoader(command_context.topsrcdir)
    mozconfig = loader.read_mozconfig(mozconfig_path)
    configure_args = mozconfig["configure_args"]
    if "--enable-application=%s" % app not in configure_args:
        raise Exception("mozconfig %s builds wrong project" % mozconfig_path)
    if not any("--with-compiler-wrapper" in a for a in configure_args):
        raise Exception("mozconfig must wrap compiles")

    # Communicate mozconfig to build subprocesses.
    env["MOZCONFIG"] = os.path.join(command_context.topsrcdir, mozconfig_path)

    # hazard mozconfigs need to find binaries in .mozbuild
    env["MOZBUILD_STATE_PATH"] = state_dir()

    # Suppress the gathering of sources, to save disk space and memory.
    env["XGILL_NO_SOURCE"] = "1"

    setup_env_for_tools(env)

    if "haz_objdir" in kwargs:
        env["MOZ_OBJDIR"] = kwargs.pop("haz_objdir")

    return command_context._mach_context.commands.dispatch(
        "build", command_context._mach_context, **kwargs
    )


@SubCommand(
    "hazards", "analyze", description="Analyzed gathered data for rooting hazards"
)
@CommandArgument(
    "--application",
    default="browser",
    help="Analyze the output for the given application.",
)
@CommandArgument(
    "--shell-objdir",
    default=None,
    help="objdir containing the optimized JS shell for running the analysis.",
)
@CommandArgument(
    "--work-dir", default=None, help="Directory for output and working files."
)
@CommandArgument(
    "extra",
    nargs=argparse.REMAINDER,
    help="Remaining non-optional arguments to analyze.py script",
)
def analyze(command_context, application, shell_objdir, work_dir, extra):
    """Analyzed gathered data for rooting hazards"""

    shell = ensure_shell(command_context, shell_objdir)
    args = [
        os.path.join(script_dir(command_context), "analyze.py"),
        "--js",
        shell,
    ]
    if extra:
        args += extra
    else:
        args += [
            "--first",
            "gcTypes",
            "-v",
        ]

    setup_env_for_tools(os.environ)
    setup_env_for_shell(os.environ, shell)

    work_dir = get_work_dir(command_context, application, work_dir)
    return command_context.run_process(args=args, cwd=work_dir, pass_thru=True)


@SubCommand(
    "hazards",
    "self-test",
    description="Run a self-test to verify hazards are detected",
)
@CommandArgument(
    "--shell-objdir",
    default=None,
    help="objdir containing the optimized JS shell for running the analysis.",
)
@CommandArgument(
    "extra",
    nargs=argparse.REMAINDER,
    help="Remaining non-optional arguments to pass to run-test.py",
)
def self_test(command_context, shell_objdir, extra):
    """Analyzed gathered data for rooting hazards"""
    shell = ensure_shell(command_context, shell_objdir)
    args = [
        os.path.join(script_dir(command_context), "run-test.py"),
        "-v",
        "--js",
        shell,
        "--sixgill",
        os.path.join(tools_dir(), "sixgill"),
        "--gccdir",
        gcc_dir(),
    ]
    args.extend(extra)

    setup_env_for_tools(os.environ)
    setup_env_for_shell(os.environ, shell)

    return command_context.run_process(args=args, pass_thru=True)
