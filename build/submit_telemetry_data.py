# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import datetime
import json
import logging
import os
import sys

HERE = os.path.abspath(os.path.dirname(__file__))
PYTHIRDPARTY = os.path.join(HERE, '..', 'third_party', 'python')

# Add some required files to $PATH to ensure they are available
sys.path.append(os.path.join(HERE, '..', 'python', 'mozbuild', 'mozbuild'))
sys.path.append(os.path.join(PYTHIRDPARTY, 'requests'))
sys.path.append(os.path.join(PYTHIRDPARTY, 'voluptuous'))

import requests
import voluptuous
import voluptuous.humanize

from mozbuild.telemetry import schema as build_telemetry_schema

BUILD_TELEMETRY_URL = 'https://incoming.telemetry.mozilla.org/{endpoint}'
SUBMIT_ENDPOINT = 'submit/eng-workflow/build/1/{ping_uuid}'
STATUS_ENDPOINT = 'status'


def delete_expired_files(directory, days=30):
    '''Discards files in a directory older than a specified number
    of days
    '''
    now = datetime.datetime.now()
    for filename in os.listdir(directory):
        filepath = os.path.join(directory, filename)

        ctime = os.path.getctime(filepath)
        then = datetime.datetime.fromtimestamp(ctime)

        if (now - then) > datetime.timedelta(days=days):
            os.remove(filepath)

    return


def check_edge_server_status(session):
    '''Returns True if the Telemetry Edge Server
    is ready to accept data
    '''
    status_url = BUILD_TELEMETRY_URL.format(endpoint=STATUS_ENDPOINT)
    response = session.get(status_url)
    if response.status_code != 200:
        return False
    return True


def send_telemetry_ping(session, data, ping_uuid):
    '''Sends a single build telemetry ping to the
    edge server, returning the response object
    '''
    resource_url = SUBMIT_ENDPOINT.format(ping_uuid=str(ping_uuid))
    url = BUILD_TELEMETRY_URL.format(endpoint=resource_url)
    response = session.post(url, json=data)

    return response


def submit_telemetry_data(outgoing, submitted):
    '''Sends information about `./mach build` invocations to
    the Telemetry pipeline
    '''
    with requests.Session() as session:
        # Confirm the server is OK
        if not check_edge_server_status(session):
            logging.error('Error posting to telemetry: server status is not "200 OK"')
            return 1

        for filename in os.listdir(outgoing):
            path = os.path.join(outgoing, filename)

            if os.path.isdir(path) or not path.endswith('.json'):
                logging.info('skipping item {}'.format(path))
                continue

            ping_uuid = os.path.splitext(filename)[0]  # strip ".json" to get ping UUID

            try:
                with open(path, 'r') as f:
                    data = json.load(f)

                # Verify the data matches the schema
                voluptuous.humanize.validate_with_humanized_errors(
                    data, build_telemetry_schema
                )

                response = send_telemetry_ping(session, data, ping_uuid)
                if response.status_code != 200:
                    msg = 'response code {code} sending {uuid} to telemetry: {body}'.format(
                        body=response.content,
                        code=response.status_code,
                        uuid=ping_uuid,
                    )
                    logging.error(msg)
                    continue

                # Move from "outgoing" to "submitted"
                os.rename(os.path.join(outgoing, filename),
                          os.path.join(submitted, filename))

                logging.info('successfully posted {} to telemetry'.format(ping_uuid))

            except ValueError as ve:
                # ValueError is thrown if JSON cannot be decoded
                logging.exception('exception parsing JSON at %s: %s'
                                  % (path, str(ve)))
                os.remove(path)

            except voluptuous.Error as e:
                # Invalid is thrown if some data does not fit
                # the correct Schema
                logging.exception('invalid data found at %s: %s'
                                  % (path, e.message))
                os.remove(path)

            except Exception as e:
                logging.error('exception posting to telemetry '
                              'server: %s' % str(e))
                break

    delete_expired_files(submitted)

    return 0


def verify_statedir(statedir):
    '''Verifies the statedir is structured according to the assumptions of
    this script

    Requires presence of the following directories; will raise if absent:
    - statedir/telemetry
    - statedir/telemetry/outgoing

    Creates the following directories and files if absent (first submission):
    - statedir/telemetry/submitted
    '''

    telemetry_dir = os.path.join(statedir, 'telemetry')
    outgoing = os.path.join(telemetry_dir, 'outgoing')
    submitted = os.path.join(telemetry_dir, 'submitted')
    telemetry_log = os.path.join(telemetry_dir, 'telemetry.log')

    if not os.path.isdir(telemetry_dir):
        raise Exception('{} does not exist'.format(telemetry_dir))

    if not os.path.isdir(outgoing):
        raise Exception('{} does not exist'.format(outgoing))

    if not os.path.isdir(submitted):
        os.mkdir(submitted)

    return outgoing, submitted, telemetry_log


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: python submit_telemetry_data.py <statedir>')
        sys.exit(1)

    statedir = sys.argv[1]

    try:
        outgoing, submitted, telemetry_log = verify_statedir(statedir)

        # Configure logging
        logging.basicConfig(filename=telemetry_log,
                            format='%(asctime)s %(message)s',
                            level=logging.DEBUG)

        sys.exit(submit_telemetry_data(outgoing, submitted))

    except Exception as e:
        # Handle and print messages from `statedir` verification
        print(e.message)
        sys.exit(1)
