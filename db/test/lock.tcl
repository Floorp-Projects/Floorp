# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)lock.tcl	10.6 (Sleepycat) 4/10/98
#
# Test driver for lock tests.
#						General	Multi	Random
# Options are:
# -dir <directory in which to store mpool>	Y	Y	Y
# -iterations <iterations>			Y	N	Y
# -ldegree <number of locks per iteration>	N	N	Y
# -maxlocks <locks in table>			Y	Y	Y
# -objs <number of objects>			N	N	Y
# -procs <number of processes to run>		N	N	Y
# -reads <read ratio>				N	N	Y
# -seeds <list of seed values for processes>	N	N	Y
# -wait <wait interval after getting locks>	N	N	Y
# -conflicts <conflict matrix; a list of lists>	Y	Y	Y
proc lock_usage {} {
	puts stderr "randomlock\n\t-dir <dir>\n\t-iterations <iterations>"
	puts stderr "\t-conflicts <conflict matrix>"
	puts stderr "\t-ldegree <locks per iteration>\n\t-maxlocks <n>"
	puts stderr "\t-objs <objects>\n\t-procs <nprocs>\n\t-reads <%reads>"
	puts stderr "\t-seeds <list of seeds>\n\t-wait <max wait interval>"
	return
}

proc locktest { args } {
source ./include.tcl


# Set defaults
	set conflicts { 0 0 0 0 0 1 0 1 1}
	set iterations 1000
	set ldegree 5
	set maxlocks 1000
	set objs 75
	set procs 5
	set reads 65
	set seeds {}
	set wait 5
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-c.* { incr i; set conflicts [linkdex $args $i] }
			-d.* { incr i; set testdir [lindex $args $i] }
			-i.* { incr i; set iterations [lindex $args $i] }
			-l.* { incr i; set ldegree [lindex $args $i] }
			-m.* { incr i; set maxlocks [lindex $args $i] }
			-o.* { incr i; set objs [lindex $args $i] }
			-p.* { incr i; set procs [lindex $args $i] }
			-r.* { incr i; set reads [lindex $args $i] }
			-s.* { incr i; set seeds [lindex $args $i] }
			-w.* { incr i; set wait [lindex $args $i] }
			default {
				lock_usage
				return
			}
		}
	}
	set nmodes [isqrt [llength $conflicts]]

	if { [file exists $testdir] != 1 } {
		exec $MKDIR $testdir
	} elseif { [file isdirectory $testdir ] != 1 } {
		error "$testdir is not a directory"
	}

	# Cleanup
	lock_cleanup $testdir

	# Open the lock
	lock001 $maxlocks $nmodes $conflicts
	# Now open the region we'll use for testing.
	set lp [lock_open "" $DB_CREATE 0644 \
	    -maxlocks $maxlocks -nmodes $nmodes -conflicts $conflicts]
	lock002 $lp $iterations $nmodes
	error_check_good lock_close [$lp close] 0
	set ret [ lock_unlink $testdir 0 ]
	error_check_good lock_unlink $ret 0

	lock003 $nmodes $conflicts
	lock004 $maxlocks $conflicts
	lock005 $testdir $iterations $maxlocks $procs $ldegree $objs \
	    $reads $wait $conflicts $seeds
}

# Make sure that we can create a region; destroy it, attach to it,
# detach from it, etc.

proc lock001 { maxl nmodes conf } {
source ./include.tcl
	puts "Lock001: open/close test"

	# Lock without create should fail.
	set lp [lock_open "" 0 0644 -maxlocks $maxl -nmodes $nmodes \
	    -conflicts $conf]
	error_check_good lock_open $lp NULL

	# Now try to do open and create the region
	set lp [lock_open "" $DB_CREATE 0644 \
	    -maxlocks $maxl -nmodes $nmodes -conflicts $conf]
	error_check_bad lock_open $lp NULL

	# Now try closing the region
	set ret [ $lp close ]
	error_check_good lock_close $ret 0

	# Try reopening the region
	set lp [lock_open "" 0 0644 -maxlocks $maxl -nmodes $nmodes \
	    -conflicts $conf]
	error_check_bad lock_open $lp NULL

	# Try unlinking without force (should fail)
	set ret [ lock_unlink $testdir 0 ]
	error_check_good lock_unlink $ret -1

	# Close the region
	error_check_good lock_close [$lp close] 0

	# Create/open a new region
	set lp [lock_open "" $DB_CREATE 0644 \
	    -maxlocks $maxl -nmodes $nmodes -conflicts $conf]
	error_check_bad lock_open $lp NULL

	# Close the region
	set ret [$lp close]
	error_check_good close $ret 0

	# Finally, try unlinking the region without force and no live processes
	set ret [ lock_unlink $testdir 0 ]
	error_check_good lock_unlink $ret 0

	# Now try creating the region in a different directory
	if { [file exists $testdir/MYLOCK] != 1 } {
		exec $MKDIR $testdir/MYLOCK
	}
	set lp [lock_open MYLOCK $DB_CREATE 0644 \
	    -maxlocks $maxl -nmodes $nmodes -conflicts $conf]
	error_check_bad lock_open $lp NULL
	error_check_good lock_close [$lp close] 0
	error_check_good lock_unlink [lock_unlink $testdir/MYLOCK 0] 0
}

# Make sure that the basic lock tests work.  Do some simple gets and puts for
# a single locker.
proc lock002 {lp iter nmodes} {
	source ./include.tcl
	puts "Lock002: test basic lock operations"
	set locker 999

	# Get and release each type of lock
	for {set i 0} { $i < $nmodes } {incr i} {
		set obj obj$i
		set lockp [$lp get $locker $obj $i 0]
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
		set ret [ $lockp put ]
		error_check_good lock_put $ret 0
	}

	# Get a bunch of locks for the same locker; these should work
	set obj OBJECT
	for {set i 0} { $i < $nmodes } {incr i} {
		set lockp [$lp get $locker $obj $i 0]
		lappend locklist $lockp
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
	}
	release_list $locklist

	set locklist {}
	# Check that reference counted locks work
	for {set i 0} { $i < 10 } {incr i} {
		set lockp [$lp get $locker $obj $DB_LOCK_WRITE 1]
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
		lappend locklist $lockp
	}
	release_list $locklist

	# Finally try some failing locks
	set locklist {}
	for {set i 0} { $i < $nmodes } {incr i} {
		set lockp [$lp get $locker $obj $i 0]
		lappend locklist $lockp
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
	}

	# Change the locker
	set locker [incr locker]
	set blocklist {}
	# Skip NO_LOCK lock.
	for {set i 1} { $i < $nmodes } {incr i} {
		set lockp [$lp get $locker $obj $i $DB_LOCK_NOWAIT ]
		error_check_good lock_get [is_substr $lockp $lp] 0
		error_check_good lock_get [is_blocked $lockp] 1
	}

	# Now release original locks
	release_list $locklist

	# Now re-acquire blocking locks
	set locklist {}
	for {set i 1} { $i < $nmodes } {incr i} {
		set lockp [$lp get $locker $obj $i $DB_LOCK_NOWAIT ]
		error_check_good lock_get [is_substr $lockp $lp] 1
		error_check_good lock_get [is_blocked $lockp] 0
		lappend locklist $lockp
	}

	# Now release new locks
	release_list $locklist
}

proc lock003 { nmodes conflicts } {
source ./include.tcl
	puts "Lock003: lock region grow test"
	set lp [lock_open "" $DB_CREATE 0644 \
	    -maxlocks 10 -nmodes $nmodes -conflicts $conflicts ]
	error_check_bad lock_open $lp NULL

# First, cause to grow because of locks
	set locker 12345
	set obj too_many_locks
	set locklist {}
	for {set i 0} { $i < 15 } { incr i } {
		set lockp [$lp get $locker $obj $DB_LOCK_READ 0]
		lappend locklist $lockp
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
	}
	release_list $locklist

# Next, cause to grow because of objects
	set locklist {}
	for {set i 0} { $i < 30 } { incr i } {
		set lockp [$lp get $locker $obj$i $DB_LOCK_WRITE 0]
		lappend locklist $lockp
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
	}
	release_list $locklist

# Finally, cause to grow because of string space; that means we need
# to lock objects with very long names
#
# XXX
# The loop end points, 50, and immediately below, 10, are for HPUX 9.05.
# If we grow the region more than 4 times on those systems, they crash.
	set locker 12345
	set longobj ""
	for {set i 0} { $i < 50} {incr i} {
		set longobj $longobj$obj
	}

	set locklist {}
	for {set i 0} { $i < 10 } { incr i } {
		set lockp [$lp get $locker $longobj$i $DB_LOCK_WRITE 0]
		lappend locklist $lockp
		error_check_good lock_get [is_blocked $lockp] 0
		error_check_good lock_get [is_substr $lockp $lp] 1
	}
	release_list $locklist

	error_check_good lock_close [$lp close] 0
}


# Blocked locks appear as lockmgrN.lockM\nBLOCKED
proc is_blocked { l } {
	if { [string compare $l BLOCKED ] == 0 } {
		return 1
	} else {
		return 0
	}
}
