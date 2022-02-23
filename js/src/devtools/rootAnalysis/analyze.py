#!/usr/bin/env python3

#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the static rooting analysis
"""

from subprocess import Popen
import argparse
import os
import subprocess
import sys

try:
    from shlex import quote
except ImportError:
    from pipes import quote


def execfile(thefile, globals):
    exec(compile(open(thefile).read(), filename=thefile, mode="exec"), globals)


# Label a string as an output.
class Output(str):
    pass


# Label a string as a pattern for multiple inputs.
class MultiInput(str):
    pass


def env(config):
    e = dict(os.environ)
    e["PATH"] = ":".join(
        p for p in (config.get("gcc_bin"), config.get("sixgill_bin"), e["PATH"]) if p
    )
    e["XDB"] = "%(sixgill_bin)s/xdb.so" % config
    e["SOURCE"] = config["source"]
    e["ANALYZED_OBJDIR"] = config["objdir"]
    return e


def fill(command, config):
    filled = []
    for s in command:
        try:
            rep = s.format(**config)
        except KeyError:
            print("Substitution failed: %s" % s)
            filled = None
            break

        if isinstance(s, Output):
            filled.append(Output(rep))
        elif isinstance(s, MultiInput):
            N = int(config["jobs"])
            for i in range(1, N + 1):
                filled.append(rep.format(i=i, n=N))
        else:
            filled.append(rep)

    if filled is None:
        raise Exception("substitution failure")

    return tuple(filled)


def print_command(command, outfile=None, env=None):
    output = " ".join(quote(s) for s in command)
    if outfile:
        output += " > " + outfile
    if env:
        changed = {}
        e = os.environ
        for key, value in env.items():
            if (key not in e) or (e[key] != value):
                changed[key] = value
        if changed:
            outputs = []
            for key, value in changed.items():
                if key in e and e[key] in value:
                    start = value.index(e[key])
                    end = start + len(e[key])
                    outputs.append(
                        '%s="%s${%s}%s"' % (key, value[:start], key, value[end:])
                    )
                else:
                    outputs.append("%s='%s'" % (key, value))
            output = " ".join(outputs) + " " + output

    print(output)


JOBS = {
    "dbs": {
        "command": [
            "{analysis_scriptdir}/run_complete",
            "--foreground",
            "--no-logs",
            "--build-root={objdir}",
            "--wrap-dir={sixgill}/scripts/wrap_gcc",
            "--work-dir=work",
            "-b",
            "{sixgill_bin}",
            "--buildcommand={buildcommand}",
            ".",
        ],
        "outputs": [],
    },
    "list-dbs": {"command": ["ls", "-l"]},
    "rawcalls": {
        "command": [
            "{js}",
            "{analysis_scriptdir}/computeCallgraph.js",
            "{typeInfo}",
            Output("rawcalls"),
            Output("rawEdges"),
            "{i}",
            "{n}",
        ],
        "multi-output": True,
        "outputs": ["rawcalls.{i}.of.{n}", "gcEdges.{i}.of.{n}"],
    },
    "mergeJSON": {
        "command": [
            "{js}",
            "{analysis_scriptdir}/mergeJSON.js",
            MultiInput("{rawEdges}"),
            Output("gcEdges"),
        ],
        "outputs": ["gcEdges.json"],
    },
    "gcFunctions": {
        "command": [
            "{js}",
            "{analysis_scriptdir}/computeGCFunctions.js",
            MultiInput("{rawcalls}"),
            "--outputs",
            Output("callgraph"),
            Output("gcFunctions"),
            Output("gcFunctions_list"),
            Output("limitedFunctions_list"),
        ],
        "outputs": [
            "callgraph.txt",
            "gcFunctions.txt",
            "gcFunctions.lst",
            "limitedFunctions.lst",
        ],
    },
    "gcTypes": {
        "command": [
            "{js}",
            "{analysis_scriptdir}/computeGCTypes.js",
            Output("gcTypes"),
            Output("typeInfo"),
        ],
        "outputs": ["gcTypes.txt", "typeInfo.txt"],
    },
    "allFunctions": {
        "command": ["{sixgill_bin}/xdbkeys", "src_body.xdb"],
        "redirect-output": "allFunctions.txt",
    },
    "hazards": {
        "command": [
            "{js}",
            "{analysis_scriptdir}/analyzeRoots.js",
            "{gcFunctions_list}",
            "{gcEdges}",
            "{limitedFunctions_list}",
            "{gcTypes}",
            "{typeInfo}",
            "{i}",
            "{n}",
            "tmp.{i}.of.{n}",
        ],
        "multi-output": True,
        "redirect-output": "rootingHazards.{i}.of.{n}",
    },
    "gather-hazards": {
        "command": ["cat", MultiInput("{hazards}")],
        "redirect-output": "rootingHazards.txt",
    },
    "explain": {
        "command": [
            sys.executable,
            "{analysis_scriptdir}/explain.py",
            "{gather-hazards}",
            "{gcFunctions}",
            Output("explained_hazards"),
            Output("unnecessary"),
            Output("refs"),
        ],
        "outputs": ["hazards.txt", "unnecessary.txt", "refs.txt"],
    },
    "heapwrites": {
        "command": ["{js}", "{analysis_scriptdir}/analyzeHeapWrites.js"],
        "redirect-output": "heapWriteHazards.txt",
    },
}


# Generator of (i, j, item) tuples:
#  - i is just the index of the yielded tuple (a la enumerate())
#  - j is the index of the item in the command list
#  - item is command[j]
def out_indexes(command):
    i = 0
    for (j, fragment) in enumerate(command):
        if isinstance(fragment, Output):
            yield (i, j, fragment)
            i += 1


def run_job(name, config):
    job = JOBS[name]
    outs = job.get("outputs") or job.get("redirect-output")
    print("Running " + name + " to generate " + str(outs))
    if "function" in job:
        job["function"](config, job["redirect-output"])
        return

    N = int(config["jobs"]) if job.get("multi-output") else 1
    config["n"] = N
    jobs = {}
    for i in range(1, N + 1):
        config["i"] = i
        cmd = fill(job["command"], config)
        info = spawn_command(cmd, job, name, config)
        jobs[info["proc"].pid] = info

    final_status = 0
    while jobs:
        pid, status = os.wait()
        final_status = final_status or status
        info = jobs[pid]
        del jobs[pid]
        if "redirect" in info:
            info["redirect"].close()

        # Rename the temporary files to their final names.
        for (temp, final) in info["rename_map"].items():
            try:
                if config["verbose"]:
                    print("Renaming %s -> %s" % (temp, final))
                os.rename(temp, final)
            except OSError:
                print("Error renaming %s -> %s" % (temp, final))
                raise

    if final_status != 0:
        raise Exception("job {} returned status {}".format(name, final_status))


def spawn_command(cmdspec, job, name, config):
    rename_map = {}

    if "redirect-output" in job:
        stdout_filename = "{}.tmp{}".format(name, config.get("i", ""))
        final_outfile = job["redirect-output"].format(**config)
        rename_map[stdout_filename] = final_outfile
        command = cmdspec
        if config["verbose"]:
            print_command(cmdspec, outfile=final_outfile, env=env(config))
    else:
        outfiles = job["outputs"]
        outfiles = fill(outfiles, config)
        stdout_filename = None

        # To print the supposedly-executed command, replace the Outputs in the
        # command with final output file names. (The actual command will be
        # using temporary files that get renamed at the end.)
        if config["verbose"]:
            pc = list(cmdspec)
            for (i, j, name) in out_indexes(cmdspec):
                pc[j] = outfiles[i]
            print_command(pc, env=env(config))

        # Replace the Outputs with temporary filenames, and record a mapping
        # from those temp names to their actual final names that will be used
        # if the command succeeds.
        command = list(cmdspec)
        for (i, j, name) in out_indexes(cmdspec):
            command[j] = "{}.tmp{}".format(name, config.get("i", ""))
            rename_map[command[j]] = outfiles[i]

    sys.stdout.flush()
    info = {"rename_map": rename_map}
    if stdout_filename:
        info["redirect"] = open(stdout_filename, "w")
        info["proc"] = Popen(command, stdout=info["redirect"], env=env(config))
    else:
        info["proc"] = Popen(command, env=env(config))

    if config["verbose"]:
        print("Spawned process {}".format(info["proc"].pid))

    return info


# Default to conservatively assuming 4GB/job.
def max_parallel_jobs(job_size=4 * 2 ** 30):
    """Return the max number of parallel jobs we can run without overfilling
    memory, assuming heavyweight jobs."""
    from_cores = int(subprocess.check_output(["nproc", "--ignore=1"]).strip())
    mem_bytes = os.sysconf("SC_PAGE_SIZE") * os.sysconf("SC_PHYS_PAGES")
    from_mem = round(mem_bytes / job_size)
    return min(from_cores, from_mem)


config = {"analysis_scriptdir": os.path.dirname(__file__)}

defaults = [
    "%s/defaults.py" % config["analysis_scriptdir"],
    "%s/defaults.py" % os.getcwd(),
]

parser = argparse.ArgumentParser(
    description="Statically analyze build tree for rooting hazards."
)
parser.add_argument(
    "step", metavar="STEP", type=str, nargs="?", help="run only step STEP"
)
parser.add_argument(
    "--source", metavar="SOURCE", type=str, nargs="?", help="source code to analyze"
)
parser.add_argument(
    "--objdir",
    metavar="DIR",
    type=str,
    nargs="?",
    help="object directory of compiled files",
)
parser.add_argument(
    "--js",
    metavar="JSSHELL",
    type=str,
    nargs="?",
    help="full path to ctypes-capable JS shell",
)
parser.add_argument(
    "--first",
    metavar="STEP",
    type=str,
    nargs="?",
    help="execute all jobs starting with STEP",
)
parser.add_argument(
    "--last", metavar="STEP", type=str, nargs="?", help="stop at step STEP"
)
parser.add_argument(
    "--jobs",
    "-j",
    default=None,
    metavar="JOBS",
    type=int,
    help="number of simultaneous analyzeRoots.js jobs",
)
parser.add_argument(
    "--list", const=True, nargs="?", type=bool, help="display available steps"
)
parser.add_argument(
    "--buildcommand",
    "--build",
    "-b",
    type=str,
    nargs="?",
    help="command to build the tree being analyzed",
)
parser.add_argument(
    "--tag",
    "-t",
    type=str,
    nargs="?",
    help='name of job, also sets build command to "build.<tag>"',
)
parser.add_argument(
    "--expect-file",
    type=str,
    nargs="?",
    help="deprecated option, temporarily still present for backwards " "compatibility",
)
parser.add_argument(
    "--verbose",
    "-v",
    action="count",
    default=1,
    help="Display cut & paste commands to run individual steps",
)
parser.add_argument("--quiet", "-q", action="count", default=0, help="Suppress output")

args = parser.parse_args()
args.verbose = max(0, args.verbose - args.quiet)

for default in defaults:
    try:
        execfile(default, config)
        if args.verbose:
            print("Loaded %s" % default)
    except Exception:
        pass

data = config.copy()

for k, v in vars(args).items():
    if v is not None:
        data[k] = v

if args.tag and not args.buildcommand:
    args.buildcommand = "build.%s" % args.tag

if args.jobs is not None:
    data["jobs"] = args.jobs
if not data.get("jobs"):
    data["jobs"] = max_parallel_jobs()

if args.buildcommand:
    data["buildcommand"] = args.buildcommand
elif "BUILD" in os.environ:
    data["buildcommand"] = os.environ["BUILD"]
else:
    data["buildcommand"] = "make -j{} -s".format(data["jobs"])

if "ANALYZED_OBJDIR" in os.environ:
    data["objdir"] = os.environ["ANALYZED_OBJDIR"]

if "GECKO_PATH" in os.environ:
    data["source"] = os.environ["GECKO_PATH"]
if "SOURCE" in os.environ:
    data["source"] = os.environ["SOURCE"]

steps = [
    "dbs",
    "gcTypes",
    "rawcalls",
    "gcFunctions",
    "mergeJSON",
    "allFunctions",
    "hazards",
    "gather-hazards",
    "explain",
    "heapwrites",
]

if args.list:
    for step in steps:
        job = JOBS[step]
        outfiles = job.get("outputs") or job.get("redirect-output")
        if outfiles:
            print(
                "%s\n    ->%s %s"
                % (step, "*" if job.get("multi-output") else "", outfiles)
            )
        else:
            print(step)
    sys.exit(0)

for step in steps:
    job = JOBS[step]
    if "redirect-output" in job:
        data[step] = job["redirect-output"]
    elif "outputs" in job and "command" in job:
        outfiles = job["outputs"]
        for (i, j, name) in out_indexes(job["command"]):
            data[name] = outfiles[i]
        num_outputs = len(list(out_indexes(job["command"])))
        assert (
            len(outfiles) == num_outputs
        ), 'step "%s": mismatched number of output files (%d) and params (%d)' % (
            step,
            num_outputs,
            len(outfiles),
        )  # NOQA: E501

if args.step:
    if args.first or args.last:
        raise Exception(
            "--first and --last cannot be used when a step argument is given"
        )
    steps = [args.step]
else:
    if args.first:
        steps = steps[steps.index(args.first) :]
    if args.last:
        steps = steps[: steps.index(args.last) + 1]

for step in steps:
    run_job(step, data)
