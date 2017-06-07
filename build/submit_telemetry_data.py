# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import logging
import os
import sys
import time

HERE = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(HERE, '..', 'python', 'requests'))
import requests


# Server to which to submit telemetry data
BUILD_TELEMETRY_SERVER = 'http://52.88.27.118/build-metrics-dev'


def submit_telemetry_data(statedir):

    # No data to work with anyway
    outgoing = os.path.join(statedir, 'telemetry', 'outgoing')
    if not os.path.isdir(outgoing):
        return 0

    submitted = os.path.join(statedir, 'telemetry', 'submitted')
    try:
        os.mkdir(submitted)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    session = requests.Session()
    for filename in os.listdir(outgoing):
        path = os.path.join(outgoing, filename)
        if os.path.isdir(path) or not path.endswith('.json'):
            continue
        with open(path, 'r') as f:
            data = f.read()
            try:
                r = session.post(BUILD_TELEMETRY_SERVER, data=data,
                                 headers={'Content-Type': 'application/json'})
            except Exception as e:
                logging.error('Exception posting to telemetry '
                              'server: %s' % str(e))
                break
            # TODO: some of these errors are likely not recoverable, as
            # written, we'll retry indefinitely
            if r.status_code != 200:
                logging.error('Error posting to telemetry: %s %s' %
                              (r.status_code, r.text))
                continue

        os.rename(os.path.join(outgoing, filename),
                  os.path.join(submitted, filename))

    session.close()

    # Discard submitted data that is >= 30 days old
    now = time.time()
    for filename in os.listdir(submitted):
        ctime = os.stat(os.path.join(submitted, filename)).st_ctime
        if now - ctime >= 60*60*24*30:
            os.remove(os.path.join(submitted, filename))

    return 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: python submit_telemetry_data.py <statedir>')
        sys.exit(1)
    statedir = sys.argv[1]
    logging.basicConfig(filename=os.path.join(statedir, 'telemetry', 'telemetry.log'),
                        format='%(asctime)s %(message)s')
    sys.exit(submit_telemetry_data(statedir))
