#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import yaml
from mozbuild.shellutil import quote as shellquote
from vsdownload import (
    getArgsParser,
    getManifest,
    getPackages,
    getSelectedPackages,
    lowercaseIgnores,
    setPackageSelection,
)

if __name__ == "__main__":
    parser = getArgsParser()
    parser.add_argument("-o", dest="output", required=True, help="Output file")
    parser.add_argument(
        "--exclude", default=[], nargs="+", help="Patterns of file names to exclude"
    )
    args = parser.parse_args()
    lowercaseIgnores(args)

    packages = getPackages(getManifest(args))
    setPackageSelection(args, packages)
    selected = getSelectedPackages(packages, args)
    reduced = []
    # Filter-out data we won't be using.
    for s in selected:
        type = s["type"]
        if type == "Component" or type == "Workload" or type == "Group":
            continue
        if type == "Vsix" or s["id"].startswith(("Win10SDK", "Win11SDK")):
            filtered = {k: v for k, v in s.items() if k in ("type", "id", "version")}
            filtered["payloads"] = [
                {
                    k: v
                    for k, v in payload.items()
                    if k in ("fileName", "sha256", "size", "url")
                }
                for payload in s["payloads"]
                if payload["fileName"].endswith((".cab", ".msi", ".vsix"))
                and not any(e in payload["fileName"] for e in args.exclude)
            ]
            reduced.append(filtered)
    with open(args.output, "w") as out:
        print("# Generated with:", file=out)
        print(
            "# ./mach python --virtualenv build build/vs/generate_yaml.py \\", file=out
        )
        for i, arg_ in enumerate(sys.argv[1:]):
            arg = shellquote(arg_)
            if i < len(sys.argv) - 2:
                print("#  ", arg, "\\", file=out)
            else:
                print("#  ", arg, file=out)
        print(yaml.dump(reduced), file=out)
