# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)randomlock.tcl	10.3 (Sleepycat) 4/10/98
#
# Exercise multi-process aspects of lock.  Generate a bunch of parallel
# testers that try to randomly obtain locks.
proc lock005 { dir {iter 500} {max 1000} {procs 5} {ldegree 5} {objs 75} \
    {reads 65} {wait 1} {conflicts { 0 0 0 0 0 1 0 1 1}} {seeds {}} } {
source ./include.tcl
	puts "Lock005: Multi-process random lock test"

# Clean up after previous runs
	lock_cleanup $dir

	# Open/create the lock region
	set lp [lock_open "" $DB_CREATE 0644 -maxlocks $max]
	error_check_bad lock_open $lp NULL
	error_check_good lock_open [is_substr $lp lockmgr] 1

	# Now spawn off processes
	set pidlist {}
	for { set i 0 } {$i < $procs} {incr i} {
		set s -1
		if { [llength $seeds] == $procs } {
			set s [lindex $seeds $i]
		}
		puts "exec ./dbtest ../test/lockscript.tcl $dir $iter \
		    $objs $wait $ldegree $reads $s > $dir/$i.lockout &"
		set p [exec ./dbtest ../test/lockscript.tcl $dir $iter \
		    $objs $wait $ldegree $reads $s > $dir/$i.lockout & ]
		lappend pidlist $p
	}
	puts "Lock005: $procs independent processes now running"
	watch_procs $pidlist
	# Remove log files
	for { set i 0 } {$i < $procs} {incr i} {
		exec $RM -f $dir/$i.lockout
	}
}
