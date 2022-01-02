# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import datetime
import requests


def readJSONFile(FileName):
    f = open(FileName, "r")
    p = json.load(f)
    f.close()
    return p


def writeJSONFile(FileName, Content):
    with open(FileName, "w") as outfile:
        json.dump(Content, outfile, indent=4)


def dateback(days):
    today = datetime.date.today()
    delta = datetime.timedelta(days)
    return today - delta


def lastweek():
    today = datetime.date.today()
    delta = datetime.timedelta(days=7)
    return today - delta


# Given a set of build ids, fetch the repository base URL for each id.
def fetchBuildRevisions(buildids):
    buildhub_url = "https://buildhub.moz.tools/api/search"
    delids = {}
    for bid in buildids:
        print("Fetching revision for build {}.".format(bid))
        body = {"size": 1, "query": {"term": {"build.id": bid}}}
        resp = requests.post(url=buildhub_url, json=body)
        hits = resp.json()["hits"]["hits"]
        if len(hits) > 0:
            buildids[bid] = (
                hits[0]["_source"]["source"]["repository"]
                + "/annotate/"
                + hits[0]["_source"]["source"]["revision"]
            )
        else:
            print("No revision for build.id {}".format(bid))
            delids[bid] = "x"
    for bid in delids:
        buildids.pop(bid)


def readExecutionFile(workdir):
    exefile = "{}/qmexecutions.json".format(workdir)
    try:
        return readJSONFile(exefile)
    except OSError:
        return []


def writeExecutionFile(workdir, executions):
    exefile = "{}/qmexecutions.json".format(workdir)
    try:
        writeJSONFile(exefile, executions)
    except OSError:
        print("Error writing execution record.")


def getLastRunFromExecutionFile(workdir):
    executions = readExecutionFile(workdir)
    if len(executions) > 0:
        return executions[len(executions) - 1]
    return {}


def updateLastRunToExecutionFile(workdir, run):
    executions = readExecutionFile(workdir)
    executions[len(executions) - 1] = run
    writeExecutionFile(workdir, executions)


def addNewRunToExecutionFile(workdir, run):
    executions = readExecutionFile(workdir)
    executions.append(run)
    writeExecutionFile(workdir, executions)
