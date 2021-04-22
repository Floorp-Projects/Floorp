#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import getopt
import utils
import stackanalysis
import fn_anchors

"""
The analysis is based on stack frames of the following form:

[
    {
        "event_timeabs": 1617121013137,
        "session_startabs": 1617120840000,
        "build_id": "20210329095128",
        "client_id": "0013a68f-9893-461a-93d4-2d7a2f85583f",
        "session_id": "8cd37159-bd5c-481c-99ad-9eace9ea726a",
        "seq": 1,
        "context": "Initialization::TemporaryStorage",
        "source_file": "dom/localstorage/ActorsParent.cpp",
        "source_line": "1018",
        "severity": "ERROR",
        "result": "NS_ERROR_FILE_NOT_FOUND"
    },
...
]

The location of the input file is expected to be found in the
last item of the list inside qmexecutions.json.
"""


def usage():
    print("analyze_qm_faiures.py -w <workdir=.>")
    print("")
    print("Analyzes the results from fetch_qm_failures.py's JSON file.")
    print(
        "Writes out several JSON results as files and a bugzilla markup table on stdout."
    )
    print("-w <workdir>:       Working directory, default is '.'")
    sys.exit(2)


days = 1
workdir = "."

try:
    opts, args = getopt.getopt(sys.argv[1:], "w:", ["workdir="])
    for opt, arg in opts:
        if opt == "-w":
            workdir = arg
except getopt.GetoptError:
    usage()

run = utils.getLastRunFromExecutionFile(workdir)
if "numrows" not in run:
    print("No previous execution from fetch_qm_failures.py found.")
    usage()
if run["numrows"] == 0:
    print("The last execution yielded no result.")

infile = run["rawfile"]


def getFname(prefix):
    return "{}/{}_until_{}.json".format(workdir, prefix, run["lasteventtime"])


# read rows from JSON
rows = utils.readJSONFile(getFname("qmrows"))
print("Found {} rows of data.".format(len(rows)))
rows = stackanalysis.sanitize(rows)

# enrich rows with hg locations
buildids = stackanalysis.extractBuildIDs(rows)
utils.fetchBuildRevisions(buildids)
stackanalysis.constructHGLinks(buildids, rows)

# transform rows to unique stacks
raw_stacks = stackanalysis.collectRawStacks(rows)
all_stacks = stackanalysis.mergeEqualStacks(raw_stacks)

# enrich with function anchors
for stack in all_stacks:
    for frame in stack["frames"]:
        frame["anchor"] = "{}:{}".format(
            frame["source_file"], fn_anchors.getFunctionName(frame["location"])
        )

# separate stacks for relevance
error_stacks = []
warn_stacks = []
info_stacks = []
abort_stacks = []
stackanalysis.filterStacksForPropagation(
    all_stacks, error_stacks, warn_stacks, info_stacks, abort_stacks
)
run["errorfile"] = getFname("qmerrors")
utils.writeJSONFile(run["errorfile"], error_stacks)
run["warnfile"] = getFname("qmwarnings")
utils.writeJSONFile(run["warnfile"], warn_stacks)
run["infofile"] = getFname("qminfo")
utils.writeJSONFile(run["infofile"], info_stacks)
run["abortfile"] = getFname("qmabort")
utils.writeJSONFile(run["abortfile"], abort_stacks)
utils.updateLastRunToExecutionFile(workdir, run)


# print results to stdout
print("Found {} error stacks.".format(len(error_stacks)))
print("Found {} warning stacks.".format(len(warn_stacks)))
print("Found {} info stacks.".format(len(info_stacks)))
print("Found {} aborted stacks.".format(len(abort_stacks)))
print("")
print("Error stacks:")
print(stackanalysis.printStacks(error_stacks))
print("")
print("Error stacks grouped by anchors:")
anchors = stackanalysis.groupStacksForAnchors(error_stacks)
anchornames = list(anchors.keys())
for a in anchornames:
    print(stackanalysis.printStacks(anchors[a]["stacks"]))
    print("")
print("")
print("Warning stacks:")
print(stackanalysis.printStacks(warn_stacks))
print("")
print("Info stacks:")
print(stackanalysis.printStacks(info_stacks))
