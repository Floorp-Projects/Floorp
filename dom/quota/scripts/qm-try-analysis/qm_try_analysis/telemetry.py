# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

import requests

from qm_try_analysis.logging import info

TELEMETRY_BASE_URL = "https://sql.telemetry.mozilla.org/api/"


def query(key, query, p_params):
    headers = {"Authorization": "Key {}".format(key)}
    start_url = TELEMETRY_BASE_URL + f"queries/{query}/refresh?{p_params}"
    info(f"Starting job using url {start_url}")
    resp = requests.post(url=start_url, headers=headers)
    job = resp.json()["job"]
    job_id = job["id"]
    info(f"Started job {job_id}")

    poll_url = TELEMETRY_BASE_URL + f"jobs/{job_id}"
    info(f"Polling query status from {poll_url}")
    poll = True
    status = 0
    qresultid = 0
    while poll:
        print(".", end="", flush=True)
        resp = requests.get(url=poll_url, headers=headers)
        status = resp.json()["job"]["status"]
        if status > 2:
            # print(resp.json())
            poll = False
            qresultid = resp.json()["job"]["query_result_id"]
        else:
            time.sleep(0.2)
    print(".")
    info(f"Finished with status {status}")

    if status == 3:
        results_url = TELEMETRY_BASE_URL + f"queries/78691/results/{qresultid}.json"

        info(f"Querying result from {results_url}")
        resp = requests.get(url=results_url, headers=headers)
        return resp.json()

    return {"query_result": {"data": {"rows": {}}}}


def getLastEventTimeAbs(rows):
    if len(rows) == 0:
        return 0
    return rows[len(rows) - 1]["submit_timeabs"]
