#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import getopt
import telemetry
import utils


"""
The analysis is based on the following query:
https://sql.telemetry.mozilla.org/queries/78691/source?p_day=28&p_month=03&p_year=2021

SELECT UNIX_MILLIS(timestamp) AS submit_timeabs,
       session_start_time,
       submission_date,
       build_id,
       client_id,
       session_id,
       event_timestamp,
       CAST(mozfun.map.get_key(event_map_values, "seq") AS INT64) AS seq,
       mozfun.map.get_key(event_map_values, "context") AS context,
       mozfun.map.get_key(event_map_values, "source_file") AS source_file,
       mozfun.map.get_key(event_map_values, "source_line") AS source_line,
       mozfun.map.get_key(event_map_values, "severity") AS severity,
       mozfun.map.get_key(event_map_values, "result") AS result,
FROM telemetry.events
WHERE submission_date >= CAST('{{ year }}-{{ month }}-{{ day }}' AS DATE)
  AND event_category='dom.quota.try'
  AND build_id >= '{{ build }}'
  AND UNIX_MILLIS(timestamp) > {{ last }}
ORDER BY submit_timeabs
LIMIT 600000

We fetch events in chronological order, as we want to keep track of where we already
arrived with our analysis. To accomplish this we write our runs into qmexecutions.json.

[
    {
        "workdir": ".",
        "daysback": 1,
        "numrows": 17377,
        "lasteventtime": 1617303855145,
        "rawfile": "./qmrows_until_1617303855145.json"
    }
]

lasteventtime is the highest value of event_timeabs we found in our data.

analyze_qm_failures instead needs the rows to be ordered by
client_id
    session_id
        seq
Thus we sort the rows accordingly before writing them.
"""


def usage():
    print(
        "fetch_qm_faiures.py -k <apikey> -b <minimum build=20210329000000>"
        "-d <days back=1> -l <last event time> -w <workdir=.>"
    )
    print("")
    print("Invokes the query 78691 and stores the result in a JSON file.")
    print("-k <apikey>:          Your personal telemetry API key (not the query key!).")
    print("-d <daysback>:        Number of days to go back. Default is 1.")
    print("-b <minimum build>:   The lowest build id we will fetch data for.")
    print("-l <last event time>: Fetch only events after this. Default is 0.")
    print("-w <workdir>:         Working directory, default is '.'")
    sys.exit(2)


days = 1
lasteventtime = 0
key = "undefined"
workdir = "."
minbuild = "20210329000000"

try:
    opts, args = getopt.getopt(
        sys.argv[1:],
        "k:b:d:l:w:",
        ["key=", "build=", "days=", "lasteventtime=", "workdir="],
    )
    for opt, arg in opts:
        if opt == "-k":
            key = arg
        elif opt == "-d":
            days = int(arg)
        elif opt == "-l":
            lasteventtime = int(arg)
        elif opt == "-b":
            minbuild = arg
        elif opt == "-w":
            workdir = arg
except getopt.GetoptError:
    usage()

if key == "undefined":
    usage()

start = utils.dateback(days)
year = start.year
month = start.month
day = start.day

run = {}
lastrun = utils.getLastRunFromExecutionFile(workdir)
if "lasteventtime" in lastrun:
    lasteventtime = lastrun["lasteventtime"]
run["workdir"] = workdir
run["daysback"] = days
run["minbuild"] = minbuild

p_params = "p_year={:04d}&p_month={:02d}&p_day={:02d}&p_build={}" "&p_last={}".format(
    year, month, day, minbuild, lasteventtime
)
print(p_params)
result = telemetry.query(key, 78691, p_params)
rows = result["query_result"]["data"]["rows"]
run["numrows"] = len(rows)
if run["numrows"] > 0:
    lasteventtime = telemetry.getLastEventTimeAbs(rows)
    run["lasteventtime"] = lasteventtime
    rows.sort(
        key=lambda row: "{}.{}.{:06d}".format(
            row["client_id"], row["session_id"], int(row["seq"])
        ),
        reverse=False,
    )
    outfile = "{}/qmrows_until_{}.json".format(workdir, lasteventtime)
    utils.writeJSONFile(outfile, rows)
    run["rawfile"] = outfile
else:
    print("No results found, maybe next time.")
    run["lasteventtime"] = lasteventtime

utils.addNewRunToExecutionFile(workdir, run)
