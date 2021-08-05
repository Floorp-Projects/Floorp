# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# script to pull and import Firefox Translations's extension source code

import os.path
from zipfile import ZipFile
import subprocess
import shutil
import sys

# the version number is the only part of this script that needs to be updated
# in order to update the in-tree version with the latest xpi version
extension_version = input(
    "Type the extension's version (tag) on Github you want to import: "
)
print("Importing version:", extension_version)
if not extension_version:
    sys.exit("Value can't be empty.")

extension_folder = "firefox-translations-src"

if not os.path.exists("import_xpi.py"):
    sys.exit("This script is intended to be executed from its local folder")

have_xpi = "N"
local_xpi_file = (
    "firefox-translations-src/dist/production/firefox/"
    "firefox-infobar-ui/firefox-translations-" + extension_version + ".xpi"
)
if os.path.isfile(local_xpi_file):
    have_xpi = input(
        "Extension xpi exists. Press Y to use it or any other key to rebuild it."
    )

if have_xpi.lower() != "y":
    # deleting old files if any
    shutil.rmtree(extension_folder, ignore_errors=True)
    # cloning the extension
    subprocess.call(
        (
            "git clone -b v" + extension_version + " "
            "https://github.com/mozilla-extensions/firefox-translations/ "
            + extension_folder
            + " "
        ).split()
    )
    # setting up the repo
    subprocess.call("yarn install".split(), cwd=extension_folder)
    # pulling bergamot-translator submodule, the repo containing the port of the
    # neural machine translation engine to wasm
    subprocess.call(
        "git submodule update --init --recursive".split(),
        cwd=extension_folder,
    )
    # build the wasm nmt module
    subprocess.call(
        "./bergamot-translator/build-wasm.sh".split(),
        cwd=extension_folder,
    )
    # import the generated wasm module to the extension
    subprocess.call(
        "./import-bergamot-translator.sh ./bergamot-translator/build-wasm/".split(),
        cwd=extension_folder,
    )
    # build the final xpi
    env = {
        **os.environ,
        "MC": str(1),
    }
    subprocess.call(
        "yarn build:firefox-infobar-ui".split(), cwd=extension_folder, env=env
    )

shutil.rmtree("extension", ignore_errors=True)
os.mkdir("extension")
file_exceptions = [
    "META-INF",
    ".md",
    "BRANCH",
    "COMMITHASH",
    "LASTCOMMITDATETIME",
    "VERSION",
    ".map",
    ".yaml",
]


def isValidFile(filename):
    for exception in file_exceptions:
        if exception in filename:
            return False
    return True


file_set = set()
# read xpi files
with ZipFile(local_xpi_file, "r") as zip:
    namelist = zip.namelist()
    cleared_namelist = []
    for filename in namelist:
        if isValidFile(filename):
            full_file_path = zip.extract(filename, "extension")
            if filename.endswith(".js"):
                filename = "browser/extensions/translations/{}".format(full_file_path)
                subprocess.call(
                    str(
                        "./mach lint --linter license {} --fix".format(filename)
                    ).split(),
                    cwd="../../../",
                )

# patching BrowserGlue.jsm
with open("../../components/BrowserGlue.jsm") as fp:
    count = 0
    Lines = fp.readlines()
    for line in Lines:
        if "resource://builtin-addons/translations/" in line:
            Lines[count - 1] = '            "{}",\n'.format(extension_version)
            with open("../../components/BrowserGlue.jsm", "w") as outfile:
                outfile.write("".join(Lines))
            break
        count += 1


print("Import finalized successfully")
