# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)mutex.tcl	10.5 (Sleepycat) 4/20/98
#
# Exercise mutex functionality.
# Options are:
# -dir <directory in which to store mpool>
# -iter <iterations>
# -mdegree <number of mutexes per iteration>
# -nmutex <number of mutexes>
# -procs <number of processes to run>
# -seeds <list of seed values for processes>
# -wait <wait interval after getting locks>
proc mutex_usage {} {
	puts stderr "mutex\n\t-dir <dir>\n\t-iter <iterations>"
	puts stderr "\t-mdegree <locks per iteration>\n\t-nmutex <n>"
	puts stderr "\t-procs <nprocs>"
	puts stderr "\t-seeds <list of seeds>\n\t-wait <max wait interval>"
	return
}

proc mutex { args } {
	source ./include.tcl

	set dir db
	set iter 500
	set mdegree 3
	set nmutex 20
	set procs 5
	set seeds {}
	set wait 2

	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-d.* { incr i; set testdir [lindex $args $i] }
			-i.* { incr i; set iter [lindex $args $i] }
			-m.* { incr i; set mdegree [lindex $args $i] }
			-n.* { incr i; set nmutex [lindex $args $i] }
			-p.* { incr i; set procs [lindex $args $i] }
			-s.* { incr i; set seeds [lindex $args $i] }
			-w.* { incr i; set wait [lindex $args $i] }
			default {
				mutex_usage
				return
			}
		}
	}

	if { [file exists $testdir/$dir] != 1 } {
		exec $MKDIR $testdir/$dir
	} elseif { [file isdirectory $testdir/$dir ] != 1 } {
		error "$testdir/$dir is not a directory"
	}

	# Basic sanity tests
	mutex001 $dir $nmutex

	# Basic synchronization tests
	mutex002 $dir $nmutex

	# Multiprocess tests
	mutex003 $dir $iter $nmutex $procs $mdegree $wait $seeds
}

proc mutex001 { dir nlocks } {
	source ./include.tcl

	puts "Mutex001: Basic functionality"
	mutex_cleanup $testdir/$dir

	# Test open w/out create; should fail
	set m [ mutex_init $dir $nlocks 0 0 ]
	error_check_good mutex_init $m NULL

	# Now open for real
	set m [ mutex_init $dir $nlocks $DB_CREATE 0644 ]
	error_check_bad mutex_init $m NULL
	error_check_good mutex_init [is_substr $m mutex] 1

	# Get, set each mutex; sleep, then get Release
	for { set i 0 } { $i < $nlocks } { incr i } {
		set r [ $m get $i ]
		error_check_good mutex_get $r 0

		set r [$m setval $i $i]
		error_check_good mutex_setval $r 0
	}
	exec $SLEEP 5
	for { set i 0 } { $i < $nlocks } { incr i } {
		set r [$m getval $i $i]
		error_check_good mutex_getval $r $i

		set r [ $m release $i ]
		error_check_good mutex_get $r 0
	}

	error_check_good mutex_close [$m close] 0
	puts "Mutex001: completed successfully."
}

# Test basic synchronization
proc mutex002 { dir nlocks } {
	source ./include.tcl

	puts "Mutex002: Basic synchronization"
	mutex_cleanup $testdir/$dir

	# Now open the region we'll use for multiprocess testing.
	set mr [mutex_init $dir $nlocks $DB_CREATE 0644]
	error_check_bad mutex_init $mr NULL
	error_check_good mutex_init [is_substr $mr mutex] 1

	set f1 [open |./dbtest r+]
	puts $f1 "set m \[mutex_init $dir $nlocks 0 0 \]"
	puts $f1 "puts \$m"
	puts $f1 "flush stdout"
	flush $f1

	set r [gets $f1 result]
	error_check_bad remote:mutex_init $r -1
	error_check_bad remote:mutex_init $result NULL

	# Do a get here, then set the value to be pid.
	# On the remote side fire off a get and getval.

	set r [$mr get 1]
	error_check_good lock_get $r 0

	set r [$mr setval 1 [pid]]
	error_check_good lock_get $r 0

	# Now have the remote side request the lock and check its
	# value. Then wait 5 seconds, release the mutex and see
	# what the remote side returned.

	puts $f1 "set start \[timestamp -r\]"
	puts $f1 "set r \[\$m get 1]"
	puts $f1 "puts \[expr \[timestamp -r\] - \$start\]"
	puts $f1 "set r \[\$m getval 1\]"
	puts $f1 "puts \$r"
	puts $f1 "flush stdout"
	flush $f1

	# Now sleep before resetting and releasing lock
	exec $SLEEP 5
	set newv [expr [pid] - 1]
	set r [$mr setval 1 $newv]
	error_check_good mutex_setval $r 0

	set r [$mr release 1]
	error_check_good mutex_release $r 0

	# Now get the result from the other script
	set r [gets $f1 result]
	error_check_bad remote:gets $r -1
	error_check_good lock_get:remote_time [expr $result > 4] 1

	set r [gets $f1 result]
	error_check_bad remote:gets $r -1
	error_check_good lock_get:remote_getval $result $newv

	# Close down the remote
	puts $f1 "\$m close"
	flush $f1
	catch { close $f1 } result

	set r [$mr close]
	error_check_good mutex_close $r 0
	puts "Mutex002: completed successfully."
}

# Generate a bunch of parallel
# testers that try to randomly obtain locks.
proc mutex003 { dir iter nmutex procs mdegree wait seeds } {
	source ./include.tcl

	puts "Mutex003: Multi-process random mutex test ($procs processes)"

	mutex_cleanup $testdir/$dir

	# Now open the region we'll use for multiprocess testing.
	set mr [mutex_init $dir $nmutex $DB_CREATE 0644]
	error_check_bad mutex_init $mr NULL
	error_check_good mutex_init [is_substr $mr mutex] 1
	set r [$mr close]
	error_check_good mutex_close $r 0

	# Now spawn off processes
	set proclist {}
	for { set i 0 } {$i < $procs} {incr i} {
		set s -1
		if { [llength $seeds] == $procs } {
			set s [lindex $seeds $i]
		}
		puts "exec ./dbtest ../test/mutexscript.tcl $testdir/$dir \
		    $iter $nmutex $wait $mdegree $s > $testdir/$i.mutexout &"
		set p [exec ./dbtest ../test/mutexscript.tcl $testdir/$dir \
		    $iter $nmutex $wait $mdegree $s > $testdir/$i.mutexout &]
		lappend proclist $p
	}
	puts "Mutex003: $procs independent processes now running"
	watch_procs $proclist
	# Remove output files
	for { set i 0 } {$i < $procs} {incr i} {
		exec $RM -f $testdir/$i.mutexout
	}
}

proc mutex_cleanup { dir } {
	source ./include.tcl

	exec $RM -rf $dir/__mutex.share
}
