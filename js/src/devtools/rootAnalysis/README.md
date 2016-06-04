# Spidermonkey JSAPI rooting analysis

This directory contains scripts for running Brian Hackett's static GC rooting
analysis on a JS source directory.

To use it on SpiderMonkey:

1.  Be on Fedora/CentOS/RedHat Linux x86_64, or a Docker image of one of those.

    Specifically, the prebuilt GCC **won't work on Ubuntu**
    without the `CFLAGS` and `CXXFLAGS` settings from
    <http://trac.wildfiregames.com/wiki/StaticRootingAnalysis>.

2.  Have the Gecko build prerequisites installed.

3.  Install taskcluster-vcs, eg by doing

        npm install taskcluster-vcs
        export PATH="$PATH:$(pwd)/node_modules/.bin"

4. In some directory, using $SRCDIR as the top of your Gecko source checkout,
    run these commands:

        mkdir work
        cd work
        GECKO_DIR=$SRCDIR $SRCDIR/taskcluster/scripts/builder/build-haz-linux.sh $(pwd) --dep

The `--dep` is optional, and will avoid rebuilding the JS shell used to run the
analysis later. Output goes to `analysis/hazards.txt`. This will run the
analysis on the js/src tree only; if you wish to analyze the full browser, use

    GECKO_DIR=$SRCDIR $SRCDIR/taskcluster/scripts/builder/build-haz-linux.sh --project browser $(pwd)

After running the analysis once, you can reuse the `*.xdb` database files
generated, using modified analysis scripts, by running
`analysis/run-analysis.sh` (or pass `--list` to see ways to select even more
restrictive parts of the overall analysis; the default is `gcTypes` which will
do everything but regenerate the xdb files).

Also, you can pass `-v` to get exact command lines to cut & paste for running the
various stages, which is helpful for running under a debugger.


## Overview of what is going on here

So what does this actually do?

1.  It downloads a GCC compiler and plugin ("sixgill") from Mozilla servers, using
    "tooltool" (a binary archive tool).

2. It runs `run_complete`, a script that builds the target codebase with the
    downloaded GCC, generating a few database files containing control flow
    graphs of the full compile, along with type information etc.

3.  Then it runs `analyze.py`, a Python script, which runs all the scripts
    which actually perform the analysis -- the tricky parts.
    (Those scripts are written in JS.)
