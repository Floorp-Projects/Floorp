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
    _arch=64
else
    _arch=32
fi

TOOLTOOL_MANIFEST=browser/config/tooltool-manifests/linux${_arch}/releng.manifest
TOOLTOOL_SERVER=https://api.pub.build.mozilla.org/tooltool/
(cd $srcdir; ./mach artifact toolchain -v --tooltool-url $TOOLTOOL_SERVER --tooltool-manifest $TOOLTOOL_MANIFEST ${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}}) || exit 2

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
