# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */
from __future__ import print_function, unicode_literals, division

import subprocess
import sys
from datetime import datetime, timedelta
from progressbar import ProgressBar
from results import TestOutput
from threading import Thread
from Queue import Queue, Empty


class EndMarker:
    pass


class TaskFinishedMarker:
    pass


def _do_work(qTasks, qResults, qWatch, prefix, timeout):
    while True:
        test = qTasks.get(block=True, timeout=sys.maxint)
        if test is EndMarker:
            qWatch.put(EndMarker)
            qResults.put(EndMarker)
            return

        # Spawn the test task.
        cmd = test.get_command(prefix)
        tStart = datetime.now()
        proc = subprocess.Popen(cmd,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)

        # Push the task to the watchdog -- it will kill the task
        # if it goes over the timeout while we keep its stdout
        # buffer clear on the "main" worker thread.
        qWatch.put(proc)
        out, err = proc.communicate()
        qWatch.put(TaskFinishedMarker)

        # Create a result record and forward to result processing.
        dt = datetime.now() - tStart
        result = TestOutput(test, cmd, out, err, proc.returncode, dt.total_seconds(),
                            dt > timedelta(seconds=timeout))
        qResults.put(result)


def _do_watch(qWatch, timeout):
    while True:
        proc = qWatch.get(True)
        if proc == EndMarker:
            return
        try:
            fin = qWatch.get(block=True, timeout=timeout)
            assert fin is TaskFinishedMarker, "invalid finish marker"
        except Empty:
            # Timed out, force-kill the test.
            proc.terminate()
            fin = qWatch.get(block=True, timeout=sys.maxint)
            assert fin is TaskFinishedMarker, "invalid finish marker"


def run_all_tests_gen(tests, prefix, results, options):
    """
    Uses scatter-gather to a thread-pool to manage children.
    """
    qTasks, qResults = Queue(), Queue()

    workers = []
    watchdogs = []
    for _ in range(options.worker_count):
        qWatch = Queue()
        watcher = Thread(target=_do_watch, args=(qWatch, options.timeout))
        watcher.setDaemon(True)
        watcher.start()
        watchdogs.append(watcher)
        worker = Thread(target=_do_work, args=(qTasks, qResults, qWatch,
                                               prefix, options.timeout))
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
    delay = ProgressBar.update_granularity().total_seconds()
    while ended < len(workers):
        try:
            result = qResults.get(block=True, timeout=delay)
            if result is EndMarker:
                ended += 1
            else:
                yield result
        except Empty:
            results.pb.poke()

    # Cleanup and exit.
    for worker in workers:
        worker.join()
    for watcher in watchdogs:
        watcher.join()
    assert qTasks.empty(), "Send queue not drained"
    assert qResults.empty(), "Result queue not drained"


def run_all_tests(tests, prefix, results, options):
    for result in run_all_tests_gen(tests, prefix, results, options):
        results.push(result)
    return True

