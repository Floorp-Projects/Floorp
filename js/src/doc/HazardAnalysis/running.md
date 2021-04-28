# Running the Rooting Hazard Analysis

The `js/src/devtools/rootAnalysis` directory contains scripts for running Brian
Hackett's static GC rooting and thread heap write safety analyses on a JS
source directory.

To run the analysis on SpiderMonkey:

1. Install prerequisites.

        mach hazards bootstrap

2. Build the shell to run the analysis.

        mach hazards build-shell

3. Compile all the code to gather info.

        mach hazards gather --application=js

4. Analyze the gathered info.

        mach hazards analyze --application=js

Output goes to `haz-js/hazards.txt`. This will run the analysis on the js/src
tree only; if you wish to analyze the full browser, use

        --application=browser

(or leave it off; `--application=browser` is the default)

After running the analysis once, you can reuse the `*.xdb` database files
generated, using modified analysis scripts, by running either the `mach hazards
analyze` command above, or with `haz-js/run-analysis.sh` (pass `--list` to see
ways to select even more restrictive parts of the overall analysis; the default
is `gcTypes` which will do everything but regenerate the xdb files).

Also, you can pass `-v` to get exact command lines to cut & paste for running
the various stages, which is helpful for running under a debugger.

## Overview of what is going on here

So what does this actually do?

1.  It downloads a GCC compiler and plugin ("sixgill") from Mozilla servers.

2. It runs `run_complete`, a script that builds the target codebase with the
    downloaded GCC, generating a few database files containing control flow
    graphs of the full compile, along with type information etc.

3.  Then it runs `analyze.py`, a Python script, which runs all the scripts
    which actually perform the analysis -- the tricky parts.
    (Those scripts are written in JS.)

The easiest way to get this running is to not try to do the instrumented
compilation locally. Instead, grab the relevant files from a try server push
and analyze them locally.

## Local Analysis of Downloaded Intermediate Files

Another useful path is to let the continuous integration system do the hard
work of generating the intermediate files and analyze them locally. This is
particularly useful if you are working on the analysis itself.

* Do a try push with "--upload-xdbs" appended to the try: ..." line.

        mach try fuzzy -q "'haz" --upload-xdbs


* Create an empty directory to run the analysis.

* When the try job is complete, download the resulting `src_body.xdb.bz2`,
`src_comp.xdb.bz2`, and `file_source.xdb.bz2` files into your directory.

* Fetch a compiler and sixgill plugin to use:

        mach hazards bootstrap

If you are on osx, these will not be available. Instead, build sixgill manually
(these directions are a little stale):

        hg clone https://hg.mozilla.org/users/sfink_mozilla.com/sixgill
        cd sixgill
        CC=$HOME/.mozbuild/hazard-tools/gcc/bin/gcc ./release.sh --build # This will fail horribly.
        make bin/xdb.so CXX=clang++

* Build an optimized JS shell with ctypes. Note that this does not need to
match the source you are analyzing in any way; in fact, you pretty much never
need to update this once you've built it. (Though I reserve the right to use
any new JS features implemented in Spidermonkey in the future...)

        mach hazards build-shell


The shell will be placed by default in `$topsrcdir/obj-haz-shell`.

* Make a defaults.py file containing the following, with your own paths filled in:

        js = "<objdir>/dist/bin/js"
        sixgill_bin = "<sixgill-dir>/bin"

* For the rooting analysis, run

        python <srcdir>/js/src/devtools/rootAnalysis/analyze.py gcTypes

* For the heap write analysis, run

        python <srcdir>/js/src/devtools/rootAnalysis/analyze.py heapwrites

Also, you may wish to run with -v (aka --verbose) to see the exact commands
executed that you can cut & paste if needed. (I use them to run under the JS
debugger when I'm working on the analysis.)
