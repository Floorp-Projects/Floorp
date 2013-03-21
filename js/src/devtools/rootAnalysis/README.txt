This directory contains scripts and a makefile for running Brian Hackett's
static GC rooting analysis on a JS source directory.

To use it:

1. Download and compile sixgill. Make sure the gcc plugin is enabled. (The
   configure output will tell you.)

  - [sfink] I needed a couple of patches to get it work on Fedora 16/17/18.
    Ask me if you need them.

2. Compile an optimized JS shell that includes the patch at
   <http://people.mozilla.org/~sfink/data/bug-835552-cwd-snarf>. This does not
   need to be in the same source tree as you are running these scripts from.
   Remember the full path to the resulting JS binary; we'll call it $JS_SHELL
   below.

3. |make clean| in the objdir of the JS source tree that you're going to be
   analyzing. (These analysis scripts will default to the tree they are within,
   but you can point them at another tree.)

4. in $objdir/js/src/devtools/analysis, |make JS=$JS_SHELL
   SIXGILL=.../path/to/sixgill...|. You may need one or more of the following
   additional settings in addition to the |JS| already given:

   - JSOBJDIR: if you are analyzing a different source tree, set this to the
     objdir of the tree you want to analyze.

   - ANALYSIS_SCRIPT_DIR: by default, the *.js files within the directory
     containing this README will be used, but you can point to a different
     directory full. At the time of this writing, there are some changes not in
     bhackett's git repo that are necessary, and you'll also need the
     gen-hazards.sh shell script.

   - JOBS: set this to the number of parallel jobs you'd like to run the final
     analysis pass with. This defaults to 6, somewhat randomly, which gave me a
     large speedup even on a machine with only 2 cores.

The results will be in rootingHazards.txt
