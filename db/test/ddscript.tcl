# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)ddscript.tcl	10.2 (Sleepycat) 4/10/98
#
# Deadlock detector script tester.
# Usage: ddscript dir test lockerid objid numprocs
# dir: DBHOME directory
# test: Which test to run
# lockerid: Lock id for this locker
# objid: Object id to lock.
# numprocs: Total number of processes running
source ../test/testutils.tcl
source ./include.tcl

set usage "ddscript dir test lockerid objid numprocs"

# Verify usage
if { $argc != 5 } {
	puts stderr $usage
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set tnum [ lindex $argv 1 ]
set lockerid [ lindex $argv 2 ]
set objid [ lindex $argv 3 ]
set numprocs [ lindex $argv 4 ]

set lm [lock_open "" 0 0 -dbhome $dir]
error_check_bad lock_open $lm NULL
error_check_good lock_open [is_substr $lm lockmgr] 1

puts [eval $tnum $lm $lockerid $objid $numprocs]
