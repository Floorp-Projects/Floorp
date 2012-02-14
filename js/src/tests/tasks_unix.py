# A unix-oriented process dispatcher.  Uses a single thread with select and
# waitpid to dispatch tasks.  This avoids several deadlocks that are possible
# with fork/exec + threads + Python.

import errno, os, sys, select
from datetime import datetime, timedelta
from results import TestOutput

PROGRESS_BAR_GRANULARITY = 0.1 #sec

class Task(object):
    def __init__(self, test, pid, stdout, stderr):
        self.test = test
        self.cmd = test.get_command(test.js_cmd_prefix)
        self.pid = pid
        self.stdout = stdout
        self.stderr = stderr
        self.start = datetime.now()
        self.out = []
        self.err = []

def spawn_test(test):
    """Spawn one child, return a task struct."""
    (rout, wout) = os.pipe()
    (rerr, werr) = os.pipe()

    rv = os.fork()

    # Parent.
    if rv:
        os.close(wout)
        os.close(werr)
        return Task(test, rv, rout, rerr)

    # Child.
    os.close(rout)
    os.close(rerr)

    os.dup2(wout, 1)
    os.dup2(werr, 2)

    cmd = test.get_command(test.js_cmd_prefix)
    os.execvp(cmd[0], cmd)

def get_max_wait(tasks, results, timeout):
    """
    Return the maximum time we can wait before any task should time out.
    """
    now = datetime.now()
    wait = timedelta(0)
    for task in tasks:
        remaining = timedelta(seconds=timeout) - (now - task.start)
        if remaining > wait:
            wait = remaining
    wait = wait.total_seconds()

    # The test harness uses a timeout of 0 to indicate we should wait forever,
    # but for select(), a timeout of 0 indicates a zero-length wait.  Instead,
    # translate the timeout into None to tell select to wait forever.
    if wait == 0:
        return None

    # If we have a progress-meter, we need to wake up to update it frequently.
    if results.pb is not None:
        wait = min(wait, PROGRESS_BAR_GRANULARITY)

    return wait

def flush_input(fd, frags):
    """
    Read any pages sitting in the file descriptor 'fd' into the list 'frags'.
    """
    rv = os.read(fd, 4096)
    frags.append(rv)
    while len(rv) == 4096:
        # If read() returns a full buffer, it may indicate there was 1 buffer
        # worth of data, or that there is more data to read.  Poll the socket
        # before we read again to ensure that we will not block indefinitly.
        readable, _, _ = select.select([fd], [], [], 0)
        if not readable:
            return

        rv = os.read(fd, 4096)
        frags.append(rv)

def read_input(tasks, timeout):
    """
    Select on input or errors from the given task list for a max of timeout
    seconds.
    """
    rlist = []
    exlist = []
    outmap = {} # Fast access to fragment list given fd.
    for t in tasks:
        rlist.append(t.stdout)
        rlist.append(t.stderr)
        outmap[t.stdout] = t.out
        outmap[t.stderr] = t.err
        # This will trigger with a close event when the child dies, allowing
        # us to respond immediately and not leave cores idle.
        exlist.append(t.stdout)

    readable, _, _ = select.select(rlist, [], exlist, timeout)
    for fd in readable:
        flush_input(fd, outmap[fd])

def remove_task(tasks, pid):
    """
    Return a pair with the removed task and the new, modified tasks list.
    """
    index = None
    for i, t in enumerate(tasks):
        if t.pid == pid:
            index = i
            break
    else:
        raise KeyError("No such pid: %s" % pid)

    out = tasks[index]
    tasks.pop(index)
    return out

def timed_out(task, timeout):
    """
    Return True if the given task has been running for longer than |timeout|.
    |timeout| may be falsy, indicating an infinite timeout (in which case
    timed_out always returns False).
    """
    if timeout:
        now = datetime.now()
        return (now - task.start) > timedelta(seconds=timeout)
    return False

def reap_zombies(tasks, results, timeout):
    """
    Search for children of this process that have finished.  If they are tasks,
    then this routine will clean up the child and send a TestOutput to the
    results channel.  This method returns a new task list that has had the ended
    tasks removed.
    """
    while True:
        try:
            pid, status = os.waitpid(0, os.WNOHANG)
            if pid == 0:
                break
        except OSError, e:
            if e.errno == errno.ECHILD:
                break
            raise e

        ended = remove_task(tasks, pid)
        flush_input(ended.stdout, ended.out)
        flush_input(ended.stderr, ended.err)
        os.close(ended.stdout)
        os.close(ended.stderr)

        out = TestOutput(
                   ended.test,
                   ended.cmd,
                   ''.join(ended.out),
                   ''.join(ended.err),
                   os.WEXITSTATUS(status),
                   (datetime.now() - ended.start).total_seconds(),
                   timed_out(ended, timeout))
        results.push(out)
    return tasks

def kill_undead(tasks, results, timeout):
    """
    Signal all children that are over the given timeout.
    """
    for task in tasks:
        if timed_out(task, timeout):
            os.kill(task.pid, 9)

def run_all_tests(tests, results, options):
    # Copy and reverse for fast pop off end.
    tests = tests[:]
    tests.reverse()

    # The set of currently running tests.
    tasks = []

    while len(tests) or len(tasks):
        while len(tests) and len(tasks) < options.worker_count:
            tasks.append(spawn_test(tests.pop()))

        timeout = get_max_wait(tasks, results, options.timeout)
        read_input(tasks, timeout)

        # We attempt to reap once before forcibly killing timed out tasks so
        # that anything that died during our sleep is not marked as timed out
        # in the test results.
        tasks = reap_zombies(tasks, results, False)
        if kill_undead(tasks, results, options.timeout):
            tasks = reap_zombies(tasks, results, options.timeout)

        if results.pb:
            results.pb.update(results.n)

    return True

