# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)mlock.tcl	10.4 (Sleepycat) 4/10/98
#
# Exercise multi-process aspects of lock.
proc lock004 { {maxlocks 1000} {conflicts {0 0 0 0 0 1 0 1 1} } } {
source ./include.tcl
	puts "Lock004: Basic multi-process lock tests."

	lock_cleanup $testdir

	set nmodes [isqrt [llength $conflicts]]

	# Open the lock
	mlock_open $maxlocks $nmodes $conflicts
	mlock_wait
	set r [lock_unlink $testdir 0]
	error_check_good lock_unlink $r 0
}

# Make sure that we can create a region; destroy it, attach to it,
# detach from it, etc.

proc mlock_open { maxl nmodes conf } {
source ./include.tcl

	puts "Lock004.a multi-process open/close test"
	# Open/Create region here.  Then close it and try to open from
	# other test process.

	set lp [lock_open "" $DB_CREATE 0644 \
	    -maxlocks $maxl -nmodes $nmodes -conflicts $conf]
	error_check_bad lock_open $lp NULL
	set ret [ $lp close ]
	error_check_good lock_close $ret 0
	set f1 [open |./dbtest r+]
	puts $f1 "set lp \[lock_open \"\" 0 0 \]"
	puts $f1 "puts \$lp"
	puts $f1 "flush stdout"
	flush $f1

	set r [gets $f1 result]
	error_check_bad remote:lock_open $r -1
	error_check_bad remote:lock_open $result NULL

	# Now make sure that we can reopen the region.
	set lp [lock_open "" 0 0]
	error_check_bad lock_open $lp NULL
	error_check_good lock_open [is_substr $lp lockmgr] 1

	# Try closing the remote region
	puts $f1 "set ret \[\$lp close\]"
	puts $f1 "puts \$ret"
	puts $f1 "flush stdout"
	flush $f1
	set r [gets $f1 result]
	error_check_good remote:lock_close $result 0
	# Try opening for create.  Will succeed because region exists.
	puts $f1 "set lp \[lock_open \"\" $DB_CREATE 0644 -maxlocks $maxl -nmodes $nmodes -conflicts $conf\]"
	puts $f1 "puts \$lp"
	puts $f1 "flush stdout"
	flush $f1

	set r [gets $f1 result]
	error_check_bad remote:lock_open $result NULL
	error_check_good remote:lock_open [is_substr $result lockmgr] 1

	# close locally
	set r [$lp close]
	error_check_good lock_close $r 0

	# Close and exit remote
	puts $f1 "set r \[\$lp close\]"
	puts $f1 "puts \$r"
	puts $f1 "flush stdout"
	flush $f1

	set r [gets $f1 result]
	error_check_good remote_lock_close $result 0
	catch { close $f1 } result
	puts "Exiting mlock_open"
}

proc mlock_wait { } {
	source ./include.tcl

	puts "Entering wait test"
	# Open region locally

	set lp [lock_open "" 0 0]
	error_check_bad lock_open $lp NULL

	# Open region remotely
	set f1 [open |./dbtest r+]
	puts $f1 "set lp \[lock_open \"\" 0 0 \]"
	puts $f1 "puts \$lp"
	puts $f1 "flush stdout"
	flush $f1

	debug_check
	exec $SLEEP 5

	set r [gets $f1 result]
	error_check_bad remote:lock_open $r -1
	error_check_bad remote:lock_open $result NULL

	# Get a write lock locally; try for the read lock
	# remotely.  We hold the locks for several seconds
	# so that we can use timestamps to figure out if the
	# other process waited.

	set locker 1
	set l1 [$lp get $locker object1 $DB_LOCK_WRITE 0]
	error_check_bad lock_get $l1 NULL
	error_check_good lock_get [is_substr $l1 $lp] 1

	# Now request a lock that we expect to hang; generate
	# timestamps so we can tell if it actually hangs.

	set locker 2
	puts $f1 "set start \[timestamp -r\]"
	puts $f1 "set l \[\$lp get $locker object1 $DB_LOCK_READ 0\]"
	puts $f1 "puts \[expr \[timestamp -r\] - \$start\]"
	puts $f1 "flush stdout"
	flush $f1

	# Now sleep before releasing lock
	exec $SLEEP 5
	set result [$l1 put]
	error_check_good lock_put $result 0

	# Now get the result from the other script
	set r [gets $f1 result]
	error_check_good lock_get:remote_time [expr $result > 4] 1

	# Now make the other guy wait 5 second and then release his
	# lock while we try to get a write lock on it
	puts $f1 "exec $SLEEP 5"
	puts $f1 "set r \[\$l put\]"
	puts $f1 "puts \$r"
	puts $f1 "flush stdout"
	flush $f1

	set locker 1
	set start [timestamp -r]
	set l [$lp get $locker object1 $DB_LOCK_WRITE 0]
	error_check_good lock_get:time \
	    [expr [expr [timestamp -r] - $start] > 4] 1
	error_check_bad lock_get:local $l NULL
	error_check_good lock_get:local [is_substr $l $lp] 1

	# Now check remote's result
	set r [gets $f1 result]
	error_check_good lock_put:remote $result 0

	# Clean up remote
	puts $f1 "set r \[\$lp close\]"
	puts $f1 "puts \$r"
	puts $f1 "flush stdout"
	flush $f1

	set r [gets $f1 result]
	error_check_good lock_close:remote $result 0
	close $f1

	# Now close up locally
	set r [$l put]
	error_check_good lock_put $r 0

	set r [$lp close]
	error_check_good lock_close $r 0
}
