import fcntl, os, select, time
from subprocess import Popen, PIPE

# Run a series of subprocesses. Try to keep up to a certain number going in
# parallel at any given time. Enforce time limits.
#
# This is implemented using non-blocking I/O, and so is Unix-specific.
#
# We assume that, if a task closes its standard error, then it's safe to
# wait for it to terminate. So an ill-behaved task that closes its standard
# output and then hangs will hang us, as well. However, as it takes special
# effort to close one's standard output, this seems unlikely to be a
# problem in practice.
class TaskPool(object):

    # A task we should run in a subprocess. Users should subclass this and
    # fill in the methods as given.
    class Task(object):
        def __init__(self):
            self.pipe = None
            self.start_time = None

        # Record that this task is running, with |pipe| as its Popen object,
        # and should time out at |deadline|.
        def start(self, pipe, deadline):
            self.pipe = pipe
            self.deadline = deadline

        # Return a shell command (a string or sequence of arguments) to be
        # passed to Popen to run the task. The command will be given
        # /dev/null as its standard input, and pipes as its standard output
        # and error.
        def cmd(self):
            raise NotImplementedError

        # TaskPool calls this method to report that the process wrote
        # |string| to its standard output.
        def onStdout(self, string):
            raise NotImplementedError

        # TaskPool calls this method to report that the process wrote
        # |string| to its standard error.
        def onStderr(self, string):
            raise NotImplementedError

        # TaskPool calls this method to report that the process terminated,
        # yielding |returncode|.
        def onFinished(self, returncode):
            raise NotImplementedError

        # TaskPool calls this method to report that the process timed out and
        # was killed.
        def onTimeout(self):
            raise NotImplementedError

    # If a task output handler (onStdout, onStderr) throws this, we terminate
    # the task.
    class TerminateTask(Exception):
        pass

    def __init__(self, tasks, cwd='.', job_limit=4, timeout=150):
        self.pending = iter(tasks)
        self.cwd = cwd
        self.job_limit = job_limit
        self.timeout = timeout
        self.next_pending = self.get_next_pending()

    # Set self.next_pending to the next task that has not yet been executed.
    def get_next_pending(self):
        try:
            return self.pending.next()
        except StopIteration:
            return None

    def run_all(self):
        # The currently running tasks: a set of Task instances.
        running = set()
        with open(os.devnull, 'r') as devnull:
            while True:
                while len(running) < self.job_limit and self.next_pending:
                    t = self.next_pending
                    p = Popen(t.cmd(), bufsize=16384,
                              stdin=devnull, stdout=PIPE, stderr=PIPE,
                              cwd=self.cwd)

                    # Put the stdout and stderr pipes in non-blocking mode. See
                    # the post-'select' code below for details.
                    flags = fcntl.fcntl(p.stdout, fcntl.F_GETFL)
                    fcntl.fcntl(p.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)
                    flags = fcntl.fcntl(p.stderr, fcntl.F_GETFL)
                    fcntl.fcntl(p.stderr, fcntl.F_SETFL, flags | os.O_NONBLOCK)

                    t.start(p, time.time() + self.timeout)
                    running.add(t)
                    self.next_pending = self.get_next_pending()

                # If we have no tasks running, and the above wasn't able to
                # start any new ones, then we must be done!
                if not running:
                    break

                # How many seconds do we have until the earliest deadline?
                now = time.time()
                secs_to_next_deadline = max(min([t.deadline for t in running]) - now, 0)

                # Wait for output or a timeout.
                stdouts_and_stderrs = ([t.pipe.stdout for t in running]
                                     + [t.pipe.stderr for t in running])
                (readable,w,x) = select.select(stdouts_and_stderrs, [], [], secs_to_next_deadline)
                finished = set()
                terminate = set()
                for t in running:
                    # Since we've placed the pipes in non-blocking mode, these
                    # 'read's will simply return as many bytes as are available,
                    # rather than blocking until they have accumulated the full
                    # amount requested (or reached EOF). The 'read's should
                    # never throw, since 'select' has told us there was
                    # something available.
                    if t.pipe.stdout in readable:
                        output = t.pipe.stdout.read(16384)
                        if output != "":
                            try:
                                t.onStdout(output)
                            except TerminateTask:
                                terminate.add(t)
                    if t.pipe.stderr in readable:
                        output = t.pipe.stderr.read(16384)
                        if output != "":
                            try:
                                t.onStderr(output)
                            except TerminateTask:
                                terminate.add(t)
                        else:
                            # We assume that, once a task has closed its stderr,
                            # it will soon terminate. If a task closes its
                            # stderr and then hangs, we'll hang too, here.
                            t.pipe.wait()
                            t.onFinished(t.pipe.returncode)
                            finished.add(t)
                # Remove the finished tasks from the running set. (Do this here
                # to avoid mutating the set while iterating over it.)
                running -= finished

                # Terminate any tasks whose handlers have asked us to do so.
                for t in terminate:
                    t.pipe.terminate()
                    t.pipe.wait()
                    running.remove(t)

                # Terminate any tasks which have missed their deadline.
                finished = set()
                for t in running:
                    if now >= t.deadline:
                        t.pipe.terminate()
                        t.pipe.wait()
                        t.onTimeout()
                        finished.add(t)
                # Remove the finished tasks from the running set. (Do this here
                # to avoid mutating the set while iterating over it.)
                running -= finished
        return None

def get_cpu_count():
    """
    Guess at a reasonable parallelism count to set as the default for the
    current machine and run.
    """
    # Python 2.6+
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except (ImportError,NotImplementedError):
        pass

    # POSIX
    try:
        res = int(os.sysconf('SC_NPROCESSORS_ONLN'))
        if res > 0:
            return res
    except (AttributeError,ValueError):
        pass

    # Windows
    try:
        res = int(os.environ['NUMBER_OF_PROCESSORS'])
        if res > 0:
            return res
    except (KeyError, ValueError):
        pass

    return 1

if __name__ == '__main__':
    # Test TaskPool by using it to implement the unique 'sleep sort' algorithm.
    def sleep_sort(ns, timeout):
        sorted=[]
        class SortableTask(TaskPool.Task):
            def __init__(self, n):
                super(SortableTask, self).__init__()
                self.n = n
            def start(self, pipe, deadline):
                super(SortableTask, self).start(pipe, deadline)
            def cmd(self):
                return ['sh', '-c', 'echo out; sleep %d; echo err>&2' % (self.n,)]
            def onStdout(self, text):
                print '%d stdout: %r' % (self.n, text)
            def onStderr(self, text):
                print '%d stderr: %r' % (self.n, text)
            def onFinished(self, returncode):
                print '%d (rc=%d)' % (self.n, returncode)
                sorted.append(self.n)
            def onTimeout(self):
                print '%d timed out' % (self.n,)

        p = TaskPool([SortableTask(_) for _ in ns], job_limit=len(ns), timeout=timeout)
        p.run_all()
        return sorted

    print repr(sleep_sort([1,1,2,3,5,8,13,21,34], 15))
