# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

import mozpack.path as mozpath
from mach.decorators import Command, SubCommand
from mozpack.manifests import InstallManifest


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
    storybook_manifest(command_context)
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
    return run_npm(command_context, args=["ci"])


@SubCommand(
    "storybook",
    "build",
    description="Build the Storybook for export.",
)
def storybook_build(command_context):
    storybook_manifest(command_context)
    return run_npm(command_context, args=["run", "build-storybook"])


@SubCommand(
    "storybook",
    "manifest",
    description="Create rewrites.js which has mappings from chrome:// URIs to local paths. "
    "Requires a ./mach build faster build. Required for our chrome-uri-loader.js webpack loader.",
)
def storybook_manifest(command_context):
    config_environment = command_context.config_environment
    # The InstallManifest object will have mappings of JAR entries to paths on disk.
    unified_manifest = InstallManifest(
        mozpath.join(config_environment.topobjdir, "faster", "unified_install_dist_bin")
    )
    paths = {}

    for dest, entry in unified_manifest._dests.items():
        # dest in the JAR path
        # entry can be many things, but we care about the [1, file_path] form
        #   1 essentially means this is a file
        if (
            entry[0] == 1
            and (dest.endswith(".js") or dest.endswith(".mjs"))
            and (
                dest.startswith("chrome/toolkit/") or dest.startswith("browser/chrome/")
            )
        ):
            try:
                # Try to map the dest to a chrome URI. This could fail for some weird cases that
                # don't seem like they're worth handling.
                chrome_uri = _parse_dest_to_chrome_uri(dest)
                # Since we run through mach we're relative to the project root here.
                paths[chrome_uri] = os.path.relpath(entry[1])
            except Exception as e:
                # Log the files that failed, this could get noisy but the list is short for now.
                print('Error rewriting to chrome:// URI "{}" [{}]'.format(dest, e))
                pass

    with open("browser/components/storybook/.storybook/rewrites.js", "w") as f:
        f.write("module.exports = ")
        json.dump(paths, f, indent=2)


def _parse_dest_to_chrome_uri(dest):
    """Turn a jar destination into a chrome:// URI. Will raise an error on unknown input."""

    global_start = dest.find("global/")
    content_start = dest.find("content/")
    skin_classic_browser = "skin/classic/browser/"
    browser_skin_start = dest.find(skin_classic_browser)
    package, provider, path = "", "", ""

    if global_start != -1:
        # e.g. chrome/toolkit/content/global/vendor/lit.all.mjs
        #      chrome://global/content/vendor/lit.all.mjs
        # If the jar location has global in it, then:
        #   * the package is global,
        #   * the portion after global should be the path,
        #   * the provider is in the path somewhere (we want skin or content).
        package = "global"
        provider = "skin" if "/skin/" in dest else "content"
        path = dest[global_start + len("global/") :]
    elif content_start != -1:
        # e.g. browser/chrome/browser/content/browser/aboutDialog.js
        #      chrome://browser/content/aboutDialog.js
        # e.g. chrome/toolkit/content/mozapps/extensions/shortcuts.js
        #      chrome://mozapps/content/extensions/shortcuts.js
        # If the jar location has content/ in it, then:
        #   * starting from "content/" split on slashes and,
        #   * the provider is "content",
        #   * the package is the next part,
        #   * the path is the remainder.
        provider, package, path = dest[content_start:].split("/", 2)
    elif browser_skin_start != -1:
        # e.g. browser/chrome/browser/skin/classic/browser/browser.css
        #      chrome://browser/skin/browser.css
        # If the jar location has skin/classic/browser/ in it, then:
        #   * the package is browser,
        #   * the provider is skin,
        #   * the path is what remains after sking/classic/browser.
        package = "browser"
        provider = "skin"
        path = dest[browser_skin_start + len(skin_classic_browser) :]

    return "chrome://{package}/{provider}/{path}".format(
        package=package, provider=provider, path=path
    )
