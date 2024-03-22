#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import pathlib

import click

from qm_try_analysis import telemetry, utils
from qm_try_analysis.logging import info

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
client_id, session_id, thread_id, submit_timeabs, seq
Thus we sort the rows accordingly before writing them.
"""


@click.command()
@click.option(
    "-k",
    "--key",
    required=True,
    help="Your personal telemetry API key.",
)
@click.option(
    "-b",
    "--minbuild",
    default="20210329000000",
    help="The lowest build id we will fetch data for. This should have the following format: `yyyymmddhhmmss`.",
)
@click.option("-d", "--days", type=int, default=7, help="Number of days to go back.")
@click.option(
    "-l",
    "--lasteventtime",
    type=int,
    default=0,
    help="Fetch only events after this number of Unix milliseconds.",
)
@click.option(
    "-w",
    "--workdir",
    type=click.Path(file_okay=False, writable=True, path_type=pathlib.Path),
    default="output",
    help="Working directory",
)
def fetch_qm_failures(key, minbuild, days, lasteventtime, workdir):
    """
    Invokes the query 78691 and stores the result in a JSON file.
    """
    # Creeate output dir if it does not exist
    workdir.mkdir(exist_ok=True)

    start = utils.dateback(days)
    year, month, day = start.year, start.month, start.day

    run = {}
    lastrun = utils.getLastRunFromExecutionFile(workdir)
    if "lasteventtime" in lastrun:
        lasteventtime = lastrun["lasteventtime"]

    run["workdir"] = workdir.as_posix()
    run["daysback"] = days
    run["minbuild"] = minbuild

    p_params = f"p_year={year:04d}&p_month={month:02d}&p_day={day:02d}&p_build={minbuild}&p_last={lasteventtime}"

    # Read string at the start of the file for more information on query 78691
    result = telemetry.query(key, 78691, p_params)
    rows = result["query_result"]["data"]["rows"]
    run["numrows"] = len(rows)

    if run["numrows"] > 0:
        lasteventtime = telemetry.getLastEventTimeAbs(rows)
        run["lasteventtime"] = lasteventtime
        rows.sort(
            key=lambda row: "{}.{}.{}.{}.{:06d}".format(
                row["client_id"],
                row["session_id"],
                row["seq"] >> 32,  # thread_id
                row["submit_timeabs"],
                row["seq"] & 0x00000000FFFFFFFF,  # seq,
            ),
            reverse=False,
        )
        outfile = f"{workdir}/qmrows_until_{lasteventtime}.json"
        utils.writeJSONFile(outfile, rows)
        run["rawfile"] = outfile
    else:
        info("No results found, maybe next time.")
        run["lasteventtime"] = lasteventtime

    utils.addNewRunToExecutionFile(workdir, run)


if __name__ == "__main__":
    fetch_qm_failures()
