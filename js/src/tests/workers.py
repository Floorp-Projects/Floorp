# Multiprocess activities with a push-driven divide-process-collect model.

import os, sys, time
from threading import Thread, Lock
from Queue import Queue, Empty
from datetime import datetime

class Source:
    def __init__(self, task_list, results, verbose = False):
        self.tasks = Queue()
        for task in task_list:
            self.tasks.put_nowait(task)

        self.results = results
        self.verbose = verbose
    
    def start(self, worker_count):
        t0 = datetime.now()

        sink = Sink(self.results)
        self.workers = [ Worker(_+1, self.tasks, sink, self.verbose) for _ in range(worker_count) ]
        if self.verbose: print '[P] Starting workers.'
        for w in self.workers:
            w.t0 = t0
            w.start()
        ans = self.join_workers()
        if self.verbose: print '[P] Finished.'

        t1 = datetime.now()
        dt = t1-t0

        return ans

    def join_workers(self):
        try:
            for w in self.workers:
                w.thread.join(20000)
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

class Worker(object):
    def __init__(self, id, tasks, sink, verbose):
        self.id = id
        self.tasks = tasks
        self.sink = sink
        self.verbose = verbose

        self.thread = None
        self.stop = False

    def log(self, msg):
        dd = datetime.now() - self.t0
        dt = dd.seconds + 1e-6 * dd.microseconds
        
        if self.verbose:
            print '[W%d %.3f] %s' % (self.id, dt, msg)

    def start(self):
        self.thread = Thread(target=self.run)
        self.thread.setDaemon(True)
        self.thread.start()

    def run(self):
        try:
            while True:
                if self.stop:
                    break
                self.log('Get next task.')
                task = self.tasks.get(False)
                self.log('Start task %s.'%str(task))
                result = task()
                self.log('Finished task.')
                self.sink.push(result)
                self.log('Pushed result.')
        except Empty:
            pass
