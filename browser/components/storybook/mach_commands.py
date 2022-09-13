# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import (
    Command,
    SubCommand,
)


def run_mach(command_context, cmd, **kwargs):
    return command_context._mach_context.commands.dispatch(
        cmd, command_context._mach_context, **kwargs
    )


def run_npm(command_context, args):
    return run_mach(
        command_context, "npm", args=[*args, "--prefix=browser/components/storybook"]
    )


@Command(
    "storybook",
    category="misc",
    description="Start the Storybook server",
)
def storybook_run(command_context):
    return run_npm(command_context, args=["run", "storybook"])


@SubCommand("storybook", "launch", description="Start Storybook in your local build.")
def storybook_launch(command_context):
    return run_mach(command_context, "run", argv=["http://localhost:5703"])


@SubCommand(
    "storybook",
    "install",
    description="Install Storybook node dependencies.",
)
def storybook_install(command_context):
    return run_npm(command_context, args=["install"])


@SubCommand(
    "storybook",
    "build",
    description="Build the Storybook for export.",
)
def storybook_build(command_context):
    return run_npm(command_context, args=["run", "build-storybook"])
