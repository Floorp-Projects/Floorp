# Multiprocess activities with a push-driven divide-process-collect model.

from __future__ import print_function

from threading import Thread, Lock
from Queue import Queue, Empty
from datetime import datetime

class Source:
    def __init__(self, task_list, results, timeout, verbose = False):
        self.tasks = Queue()
        for task in task_list:
            self.tasks.put_nowait(task)

        self.results = results
        self.timeout = timeout
        self.verbose = verbose

    def start(self, worker_count):
        t0 = datetime.now()

        sink = Sink(self.results)
        self.workers = [ Worker(_+1, self.tasks, sink, self.timeout, self.verbose) for _ in range(worker_count) ]
        if self.verbose:
            print('[P] Starting workers.')
        for w in self.workers:
            w.t0 = t0
            w.start()
        ans = self.join_workers()
        if self.verbose:
            print('[P] Finished.')
        return ans

    def join_workers(self):
        try:
            for w in self.workers:
                w.join(20000)
            return True
        except KeyboardInterrupt:
            for w in self.workers:
                w.stop = True
            return False

class Sink:
    def __init__(self, results):
        self.results = results
        self.lock = Lock()

    def push(self, result):
        self.lock.acquire()
        try:
            self.results.push(result)
        finally:
            self.lock.release()

class Worker(Thread):
    def __init__(self, id, tasks, sink, timeout, verbose):
        Thread.__init__(self)
        self.setDaemon(True)
        self.id = id
        self.tasks = tasks
        self.sink = sink
        self.timeout = timeout
        self.verbose = verbose

        self.thread = None
        self.stop = False

    def log(self, msg):
        if self.verbose:
            dd = datetime.now() - self.t0
            dt = dd.seconds + 1e-6 * dd.microseconds
            print('[W%d %.3f] %s' % (self.id, dt, msg))

    def run(self):
        try:
            while True:
                if self.stop:
                    break
                self.log('Get next task.')
                task = self.tasks.get(False)
                self.log('Start task %s.'%str(task))
                result = task.run(task.js_cmd_prefix, self.timeout)
                self.log('Finished task.')
                self.sink.push(result)
                self.log('Pushed result.')
        except Empty:
            pass

def run_all_tests(tests, results, options):
    pipeline = Source(tests, results, options.timeout, False)
    return pipeline.start(options.worker_count)

