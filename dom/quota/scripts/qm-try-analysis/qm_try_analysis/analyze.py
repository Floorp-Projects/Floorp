#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from os import path

import click

from qm_try_analysis import fn_anchors, stackanalysis, utils
from qm_try_analysis.logging import error, info

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


@click.command()
@click.option(
    "--output-to",
    type=click.Path(dir_okay=False, writable=True),
    default="qmstacks_until_<lasteventtime>.txt",
    help="Specify the output file for the analyzed data.",
)
@click.option(
    "-w",
    "--workdir",
    type=click.Path(file_okay=False, exists=True, writable=True),
    default="output",
    help="Working directory",
)
def analyze_qm_failures(output_to, workdir):
    """
    Analyzes the results from fetch's JSON file.
    Writes out several JSON results as files and a bugzilla markup table on stdout.
    """
    run = utils.getLastRunFromExecutionFile(workdir)
    if "numrows" not in run or run["numrows"] == 0:
        error(
            "No previous execution from fetch_qm_failures.py found or the last execution yielded no result."
        )
        sys.exit(2)

    if output_to == "qmstacks_until_<lasteventtime>.txt":
        output_to = path.join(workdir, f'qmstacks_until_{run["lasteventtime"]}.txt')
    elif output_to.exists():
        error(
            f'The output file "{output_to}" already exists. This script would override it.'
        )
        sys.exit(2)
    run["stacksfile"] = output_to

    def getFname(prefix):
        return "{}/{}_until_{}.json".format(workdir, prefix, run["lasteventtime"])

    # read rows from JSON
    rows = utils.readJSONFile(getFname("qmrows"))
    info(f"Found {len(rows)} rows of data")
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

    info(f"Found {len(error_stacks)} error stacks")
    info(f"Found {len(warn_stacks)} warning stacks")
    info(f"Found {len(info_stacks)} info stacks")
    info(f"Found {len(abort_stacks)} aborted stacks")

    # Write results to the specified output file
    with open(output_to, "w") as output:

        def print_to_output(message):
            print(message, file=output)

        print_to_output("Error stacks:")
        print_to_output(stackanalysis.printStacks(error_stacks))
        print_to_output("")
        print_to_output("Error stacks grouped by anchors:")
        anchors = stackanalysis.groupStacksForAnchors(error_stacks)
        anchornames = list(anchors.keys())
        for a in anchornames:
            print_to_output(stackanalysis.printStacks(anchors[a]["stacks"]))
            print_to_output("")
        print_to_output("")
        print_to_output("Warning stacks:")
        print_to_output(stackanalysis.printStacks(warn_stacks))
        print_to_output("")
        print_to_output("Info stacks:")
        print_to_output(stackanalysis.printStacks(info_stacks))
        print_to_output("")
        print_to_output("Aborted stacks:")
        print_to_output(stackanalysis.printStacks(abort_stacks))

    info(f"Wrote results to specified output file {output_to}")


if __name__ == "__main__":
    analyze_qm_failures()
