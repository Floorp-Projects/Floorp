# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)dead001.tcl	10.3 (Sleepycat) 4/10/98
#
# Deadlock Test 1.
# We create various deadlock scenarios for different numbers of lockers
# and see if we can get the world cleaned up suitably.
proc dead001 { { procs "2 4 10" } {tests "ring clump" } } {
	puts "Dead001: Deadlock detector tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl
	cleanup $testdir

	# Create the environment.
	puts "\tDead001.a: creating environment"
	set lm [lock_open "" $DB_CREATE 0644]
	error_check_bad lock_open $lm NULL
	error_check_good lock_open [is_substr $lm lockmgr] 1
	error_check_good lock_close [$lm close] 0

	set dpid [exec ./db_deadlock -vw -h $testdir \
	    >& $testdir/dd.out &]

	foreach t $tests {
		set pidlist ""
		foreach n $procs {
			# Fire off the tests
			puts "\tDead001: $n procs of test $t"
			for { set i 0 } { $i < $n } { incr i } {
#				puts "./dbtest ../test/ddscript.tcl $testdir \
#				    $t $i $i $n >& $testdir/dead001.log.$i"
				set p [ exec ./dbtest ../test/ddscript.tcl \
				    $testdir $t $i $i $n >& \
				    $testdir/dead001.log.$i &]
				lappend pidlist $p
			}
			watch_procs $pidlist 5

			# Now check output
			set dead 0
			set clean 0
			set other 0
			for { set i 0 } { $i < $n } { incr i } {
				set did [open $testdir/dead001.log.$i]
				while { [gets $did val] != -1 } {
					switch $val {
						DEADLOCK { incr dead }
						1 { incr clean }
						default { incr other }
					}
				}
				close $did
			}
			dead_check $t $n $dead $clean $other
		}
	}

	exec $KILL $dpid
	exec $RM -f $testdir/dd.out
	# Remove log files
	for { set i 0 } { $i < $n } { incr i } {
		exec $RM -f $testdir/dead001.log.$i
	}
}

