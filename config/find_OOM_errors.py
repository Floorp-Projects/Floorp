#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function

usage = """%prog: A test for OOM conditions in the shell.

%prog finds segfaults and other errors caused by incorrect handling of
allocation during OOM (out-of-memory) conditions.
"""

help = """Check for regressions only. This runs a set of files with a known
number of OOM errors (specified by REGRESSION_COUNT), and exits with a non-zero
result if more or less errors are found. See js/src/Makefile.in for invocation.
"""


import re
import shlex
import subprocess
import sys
import threading

from optparse import OptionParser

#####################################################################
# Utility functions
#####################################################################


def run(args, stdin=None):
    class ThreadWorker(threading.Thread):
        def __init__(self, pipe):
            super(ThreadWorker, self).__init__()
            self.all = ""
            self.pipe = pipe
            self.setDaemon(True)

        def run(self):
            while True:
                line = self.pipe.readline()
                if line == '':
                    break
                else:
                    self.all += line

    try:
        if type(args) == str:
            args = shlex.split(args)

        args = [str(a) for a in args]  # convert to strs

        stdin_pipe = subprocess.PIPE if stdin else None
        proc = subprocess.Popen(args, stdin=stdin_pipe,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if stdin_pipe:
            proc.stdin.write(stdin)
            proc.stdin.close()

        stdout_worker = ThreadWorker(proc.stdout)
        stderr_worker = ThreadWorker(proc.stderr)
        stdout_worker.start()
        stderr_worker.start()

        proc.wait()
        stdout_worker.join()
        stderr_worker.join()

    except KeyboardInterrupt:
        sys.exit(-1)

    stdout, stderr = stdout_worker.all, stderr_worker.all
    result = (stdout, stderr, proc.returncode)
    return result


def get_js_files():
    (out, err, exit) = run('find ../jit-test/tests -name "*.js"')
    if (err, exit) != ("", 0):
        sys.exit("Wrong directory, run from an objdir")
    return out.split()


#####################################################################
# Blacklisting
#####################################################################
def in_blacklist(sig):
    return sig in blacklist


def add_to_blacklist(sig):
    blacklist[sig] = blacklist.get(sig, 0)
    blacklist[sig] += 1

# How often is a particular lines important for this.


def count_lines():
    """Keep track of the amount of times individual lines occur, in order to
       prioritize the errors which occur most frequently."""
    counts = {}
    for string, count in blacklist.items():
        for line in string.split("\n"):
            counts[line] = counts.get(line, 0) + count

    lines = []
    for k, v in counts.items():
        lines.append("{0:6}: {1}".format(v, k))

    lines.sort()

    countlog = file("../OOM_count_log", "w")
    countlog.write("\n".join(lines))
    countlog.flush()
    countlog.close()


#####################################################################
# Output cleaning
#####################################################################
def clean_voutput(err):
    # Skip what we can't reproduce
    err = re.sub(r"^--\d+-- run: /usr/bin/dsymutil \"shell/js\"$",
                 "", err, flags=re.MULTILINE)
    err = re.sub(r"^==\d+==", "", err, flags=re.MULTILINE)
    err = re.sub(r"^\*\*\d+\*\*", "", err, flags=re.MULTILINE)
    err = re.sub(r"^\s+by 0x[0-9A-Fa-f]+: ", "by: ", err, flags=re.MULTILINE)
    err = re.sub(r"^\s+at 0x[0-9A-Fa-f]+: ", "at: ", err, flags=re.MULTILINE)
    err = re.sub(
        r"(^\s+Address 0x)[0-9A-Fa-f]+( is not stack'd)", r"\1\2", err, flags=re.MULTILINE)
    err = re.sub(r"(^\s+Invalid write of size )\d+",
                 r"\1x", err, flags=re.MULTILINE)
    err = re.sub(r"(^\s+Invalid read of size )\d+",
                 r"\1x", err, flags=re.MULTILINE)
    err = re.sub(r"(^\s+Address 0x)[0-9A-Fa-f]+( is )\d+( bytes inside a block of size )[0-9,]+( free'd)",  # NOQA: E501
                 r"\1\2\3\4", err, flags=re.MULTILINE)

    # Skip the repeating bit due to the segfault
    lines = []
    for l in err.split('\n'):
        if l == " Process terminating with default action of signal 11 (SIGSEGV)":
            break
        lines.append(l)
    err = '\n'.join(lines)

    return err


def remove_failed_allocation_backtraces(err):
    lines = []

    add = True
    for l in err.split('\n'):

        # Set start and end conditions for including text
        if l == " The site of the failed allocation is:":
            add = False
        elif l[:2] not in ['by: ', 'at:']:
            add = True

        if add:
            lines.append(l)

    err = '\n'.join(lines)

    return err


def clean_output(err):
    err = re.sub(r"^js\(\d+,0x[0-9a-f]+\) malloc: \*\*\* error for object 0x[0-9a-f]+: pointer being freed was not allocated\n\*\*\* set a breakpoint in malloc_error_break to debug\n$",  # NOQA: E501
                 "pointer being freed was not allocated", err, flags=re.MULTILINE)

    return err


#####################################################################
# Consts, etc
#####################################################################

command_template = 'shell/js' \
    + ' -m -j -p' \
    + ' -e "const platform=\'darwin\'; const libdir=\'../jit-test/lib/\';"' \
    + ' -f ../jit-test/lib/prolog.js' \
    + ' -f {0}'


# Blacklists are things we don't want to see in our logs again (though we do
# want to count them when they happen). Whitelists we do want to see in our
# logs again, principally because the information we have isn't enough.

blacklist = {}
# 1 means OOM if the shell hasn't launched yet.
add_to_blacklist(r"('', '', 1)")
add_to_blacklist(r"('', 'out of memory\n', 1)")

whitelist = set()
whitelist.add(r"('', 'out of memory\n', -11)")  # -11 means OOM
whitelist.add(r"('', 'out of memory\nout of memory\n', -11)")


#####################################################################
# Program
#####################################################################

# Options
parser = OptionParser(usage=usage)
parser.add_option("-r", "--regression", action="store", metavar="REGRESSION_COUNT", help=help,
                  type="int", dest="regression", default=None)

(OPTIONS, args) = parser.parse_args()


if OPTIONS.regression is not None:
    # TODO: This should be expanded as we get a better hang of the OOM problems.
    # For now, we'll just check that the number of OOMs in one short file does not
    # increase.
    files = ["../jit-test/tests/arguments/args-createontrace.js"]
else:
    files = get_js_files()

    # Use a command-line arg to reduce the set of files
    if len(args):
        files = [f for f in files if f.find(args[0]) != -1]


if OPTIONS.regression is None:
    # Don't use a logfile, this is automated for tinderbox.
    log = file("../OOM_log", "w")


num_failures = 0
for f in files:

    # Run it once to establish boundaries
    command = (command_template + ' -O').format(f)
    out, err, exit = run(command)
    max = re.match(".*OOM max count: (\d+).*", out,
                   flags=re.DOTALL).groups()[0]
    max = int(max)

    # OOMs don't recover well for the first 20 allocations or so.
    # TODO: revisit this.
    for i in range(20, max):

        if OPTIONS.regression is None:
            print("Testing allocation {0}/{1} in {2}".format(i, max, f))
        else:
            # something short for tinderbox, no space or \n
            sys.stdout.write('.')

        command = (command_template + ' -A {0}').format(f, i)
        out, err, exit = run(command)

        # Success (5 is SM's exit code for controlled errors)
        if exit == 5 and err.find("out of memory") != -1:
            continue

        # Failure
        else:

            if OPTIONS.regression is not None:
                # Just count them
                num_failures += 1
                continue

            #########################################################################
            # The regression tests ends above. The rest of this is for running  the
            # script manually.
            #########################################################################

            problem = str((out, err, exit))
            if in_blacklist(problem) and problem not in whitelist:
                add_to_blacklist(problem)
                continue

            add_to_blacklist(problem)

            # Get valgrind output for a good stack trace
            vcommand = "valgrind --dsymutil=yes -q --log-file=OOM_valgrind_log_file " + command
            run(vcommand)
            vout = file("OOM_valgrind_log_file").read()
            vout = clean_voutput(vout)
            sans_alloc_sites = remove_failed_allocation_backtraces(vout)

            # Don't print duplicate information
            if in_blacklist(sans_alloc_sites):
                add_to_blacklist(sans_alloc_sites)
                continue

            add_to_blacklist(sans_alloc_sites)

            log.write("\n")
            log.write("\n")
            log.write(
                "=========================================================================")
            log.write("\n")
            log.write("An allocation failure at\n\tallocation {0}/{1} in {2}\n\t"
                      "causes problems (detected using bug 624094)"
                      .format(i, max, f))
            log.write("\n")
            log.write("\n")

            log.write(
                "Command (from obj directory, using patch from bug 624094):\n  " + command)
            log.write("\n")
            log.write("\n")
            log.write("stdout, stderr, exitcode:\n  " + problem)
            log.write("\n")
            log.write("\n")

            double_free = err.find(
                "pointer being freed was not allocated") != -1
            oom_detected = err.find("out of memory") != -1
            multiple_oom_detected = err.find(
                "out of memory\nout of memory") != -1
            segfault_detected = exit == -11

            log.write("Diagnosis: ")
            log.write("\n")
            if multiple_oom_detected:
                log.write("  - Multiple OOMs reported")
                log.write("\n")
            if segfault_detected:
                log.write("  - segfault")
                log.write("\n")
            if not oom_detected:
                log.write("  - No OOM checking")
                log.write("\n")
            if double_free:
                log.write("  - Double free")
                log.write("\n")

            log.write("\n")

            log.write("Valgrind info:\n" + vout)
            log.write("\n")
            log.write("\n")
            log.flush()

    if OPTIONS.regression is None:
        count_lines()

print()

# Do the actual regression check
if OPTIONS.regression is not None:
    expected_num_failures = OPTIONS.regression

    if num_failures != expected_num_failures:

        print("TEST-UNEXPECTED-FAIL |", end='')
        if num_failures > expected_num_failures:
            print("More out-of-memory errors were found ({0}) than expected ({1}). "
                  "This probably means an allocation site has been added without a "
                  "NULL-check. If this is unavoidable, you can account for it by "
                  "updating Makefile.in.".format(
                      num_failures, expected_num_failures),
                  end='')
        else:
            print("Congratulations, you have removed {0} out-of-memory error(s) "
                  "({1} remain)! Please account for it by updating Makefile.in."
                  .format(expected_num_failures - num_failures, num_failures),
                  end='')
        sys.exit(-1)
    else:
        print('TEST-PASS | find_OOM_errors | Found the expected number of OOM '
              'errors ({0})'.format(expected_num_failures))
