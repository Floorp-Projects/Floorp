# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */
from __future__ import print_function, unicode_literals, division

import sys
from threading import Thread
from Queue import Queue, Empty


class EndMarker:
    pass


def _do_work(qTasks, qResults, timeout):
    while True:
        test = qTasks.get(block=True, timeout=sys.maxint)
        if test is EndMarker:
            qResults.put(EndMarker)
            return
        qResults.put(test.run(test.js_cmd_prefix, timeout))


def run_all_tests_gen(tests, results, options):
    """
    Uses scatter-gather to a thread-pool to manage children.
    """
    qTasks, qResults = Queue(), Queue()

    workers = []
    for _ in range(options.worker_count):
        worker = Thread(target=_do_work, args=(qTasks, qResults, options.timeout))
        worker.setDaemon(True)
        worker.start()
        workers.append(worker)

    # Insert all jobs into the queue, followed by the queue-end
    # marker, one per worker. This will not block on growing the
    # queue, only on waiting for more items in the generator. The
    # workers are already started, however, so this will process as
    # fast as we can produce tests from the filesystem.
    for test in tests:
        qTasks.put(test)
    for _ in workers:
        qTasks.put(EndMarker)

    # Read from the results.
    ended = 0
    while ended < len(workers):
        result = qResults.get(block=True, timeout=sys.maxint)
        if result is EndMarker:
            ended += 1
        else:
            yield result

    # Cleanup and exit.
    for worker in workers:
        worker.join()
    assert qTasks.empty(), "Send queue not drained"
    assert qResults.empty(), "Result queue not drained"


def run_all_tests(tests, results, options):
    for result in run_all_tests_gen(tests, results, options):
        results.push(result)
    return True

