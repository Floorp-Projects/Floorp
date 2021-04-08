# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import requests
import time


def query(key, query, p_params):
    headers = {"Authorization": "Key {}".format(key)}
    start_url = "https://sql.telemetry.mozilla.org/api/" "queries/{}/refresh?{}".format(
        query, p_params
    )
    print(start_url)
    resp = requests.post(url=start_url, headers=headers)
    job = resp.json()["job"]
    jid = job["id"]
    print("Started job {}".format(jid))

    poll_url = "https://sql.telemetry.mozilla.org/api/" "jobs/{}".format(jid)
    print(poll_url)
    poll = True
    status = 0
    qresultid = 0
    while poll:
        print(".", end="", flush=True)
        resp = requests.get(url=poll_url, headers=headers)
        status = resp.json()["job"]["status"]
        if status > 2:
            print(resp.json())
            poll = False
            qresultid = resp.json()["job"]["query_result_id"]
        else:
            time.sleep(0.2)
    print(".")
    print("Finished with status {}".format(status))

    if status == 3:
        fetch_url = (
            "https://sql.telemetry.mozilla.org/api/"
            "queries/78691/results/{}.json".format(qresultid)
        )
        print(fetch_url)
        resp = requests.get(url=fetch_url, headers=headers)
        return resp.json()

    return {"query_result": {"data": {"rows": {}}}}


def getLastEventTimeAbs(rows):
    if len(rows) == 0:
        return 0
    return rows[len(rows) - 1]["submit_timeabs"]
