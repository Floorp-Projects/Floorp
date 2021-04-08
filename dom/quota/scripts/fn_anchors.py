# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import subprocess

cached_functions = {}


def getMetricsJson(src_url):
    print("Fetching source for function extraction: {}".format(src_url))
    metrics = subprocess.check_output(["./fetch_fn_names.sh", src_url])

    try:
        return json.loads(metrics)
    except ValueError:
        return {"kind": "empty", "name": "anonymous", "spaces": []}


def getSpaceFunctionsRecursive(metrics_space):
    functions = []
    if (
        metrics_space["kind"] == "function"
        and metrics_space["name"]
        and metrics_space["name"] != "<anonymous>"
    ):
        functions.append(
            {
                "name": metrics_space["name"],
                "start_line": int(metrics_space["start_line"]),
                "end_line": int(metrics_space["end_line"]),
            }
        )
    for space in metrics_space["spaces"]:
        functions += getSpaceFunctionsRecursive(space)
    return functions


def getSourceFunctions(src_url):
    if src_url not in cached_functions:
        metrics_space = getMetricsJson(src_url)
        cached_functions[src_url] = getSpaceFunctionsRecursive(metrics_space)

    return cached_functions[src_url]


def getFunctionName(location):
    location.replace("annotate", "raw-file")
    pieces = location.split("#l")
    src_url = pieces[0]
    line = int(pieces[1])
    closest_name = "<Unknown {}>".format(line)
    closest_start = 0
    functions = getSourceFunctions(src_url)
    for fn in functions:
        if (
            fn["start_line"] > closest_start
            and line >= fn["start_line"]
            and line <= fn["end_line"]
        ):
            closest_start = fn["start_line"]
            closest_name = fn["name"]
    return closest_name
