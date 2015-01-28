# A unix-oriented process dispatcher.  Uses a single thread with select and
# waitpid to dispatch tasks.  This avoids several deadlocks that are possible
# with fork/exec + threads + Python.

import errno, os, select
from datetime import datetime, timedelta
from results import TestOutput

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

def spawn_test(test, passthrough=False):
    """Spawn one child, return a task struct."""
    if not passthrough:
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

def total_seconds(td):
    """
    Return the total number of seconds contained in the duration as a float
    """
    return (float(td.microseconds) \
            + (td.seconds + td.days * 24 * 3600) * 10**6) / 10**6

def get_max_wait(tasks, results, timeout):
    """
    Return the maximum time we can wait before any task should time out.
    """

    # If we have a progress-meter, we need to wake up to update it frequently.
    wait = results.pb.update_granularity()

    # If a timeout is supplied, we need to wake up for the first task to
    # timeout if that is sooner.
    if timeout:
        now = datetime.now()
        timeout_delta = timedelta(seconds=timeout)
        for task in tasks:
            remaining = task.start + timeout_delta - now
            if remaining < wait:
                wait = remaining

    # Return the wait time in seconds, clamped to zero.
    return max(total_seconds(wait), 0)

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
        raise KeyError("No such pid: {}".format(pid))

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
        except OSError as e:
            if e.errno == errno.ECHILD:
                break
            raise e

        ended = remove_task(tasks, pid)
        flush_input(ended.stdout, ended.out)
        flush_input(ended.stderr, ended.err)
        os.close(ended.stdout)
        os.close(ended.stderr)

        returncode = os.WEXITSTATUS(status)
        if os.WIFSIGNALED(status):
            returncode = -os.WTERMSIG(status)

        out = TestOutput(
            ended.test,
            ended.cmd,
            ''.join(ended.out),
            ''.join(ended.err),
            returncode,
            total_seconds(datetime.now() - ended.start),
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
            tasks.append(spawn_test(tests.pop(), options.passthrough))

        timeout = get_max_wait(tasks, results, options.timeout)
        read_input(tasks, timeout)

        kill_undead(tasks, results, options.timeout)
        tasks = reap_zombies(tasks, results, options.timeout)

        results.pb.poke()

    return True
