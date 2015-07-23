#!/bin/bash
set -e
set -x

srcdir=$PWD/src
objdir=${MOZ_OBJDIR-objdir}

# If the objdir is a relative path, it is relative to the srcdir.
case "$objdir" in
    /*)
	;;
    *)
        objdir="$srcdir/$objdir"
	;;
esac

if [ ! -d $objdir ]; then
    mkdir $objdir
fi
cd $objdir

if [ "`uname -m`" = "x86_64" ]; then
    export LD_LIBRARY_PATH=/tools/gcc-4.5-0moz3/installed/lib64
    _arch=64
else
    export LD_LIBRARY_PATH=/tools/gcc-4.5-0moz3/installed/lib
    _arch=32
fi

# Note: an exit code of 2 turns the job red on TBPL.
MOZCONFIG=$srcdir/browser/config/mozconfigs/linux${_arch}/valgrind make -f $srcdir/client.mk configure || exit 2
make -j4 || exit 2
make package || exit 2

# We need to set MOZBUILD_STATE_PATH so that |mach| skips its first-run
# initialization step and actually runs the |valgrind-test| command.
export MOZBUILD_STATE_PATH=.

# |mach valgrind-test|'s exit code will be 1 (which turns the job orange on
# TBPL) if Valgrind finds errors, and 2 (which turns the job red) if something
# else goes wrong, such as Valgrind crashing.
python2.7 $srcdir/mach valgrind-test
exit $?
