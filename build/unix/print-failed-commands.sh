#!/bin/sh
#
# Usage from makefile:
#   ELOG = . $(topdir)/build/autoconf/print-failed-commands.sh
#   $(ELOG) $(CC) $CFLAGS -o $@ $<
#
# This shell script is used by the build system to print out commands that fail
# to execute properly.  It is designed to make the "make -s" command more
# useful.
#
# Note that in the example we are sourcing rather than execing the script.
# Since make already started a shell for us, we might as well use it rather
# than starting a new one.

( exec "$@" ) || {
    echo
    echo "In the directory " `pwd`
    echo "The following command failed to execute properly:"
    echo "$@"
    exit 1;
}
