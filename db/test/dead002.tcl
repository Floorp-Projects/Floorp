# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)dead002.tcl	8.3 (Sleepycat) 4/10/98
#
# Deadlock Test 2.
# Identical to Test 1 except that instead of running a standalone deadlock
# detector, we create the region with "detect on every wait"
proc dead002 { { procs "2 4 10" } {tests "ring clump" } } {
	puts "Dead002: Deadlock detector tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl
	cleanup $testdir

	# Create the environment.
	puts "\tDead002.a: creating environment"
	set lm [lock_open "" $DB_CREATE 0644 -detect 1]
	error_check_bad lock_open $lm NULL
	error_check_good lock_open [is_substr $lm lockmgr] 1
	error_check_good lock_close [$lm close] 0

	foreach t $tests {
		set pidlist ""
		foreach n $procs {
			# Fire off the tests
			puts "\tDead002: $n procs of test $t"
			for { set i 0 } { $i < $n } { incr i } {
#				puts "./dbtest ../test/ddscript.tcl $testdir \
#				    $t $i $i $n >& $testdir/dead002.log.$i"
				set p [ exec ./dbtest ../test/ddscript.tcl \
				    $testdir $t $i $i $n >& \
				    $testdir/dead002.log.$i &]
				lappend pidlist $p
			}
			watch_procs $pidlist 5

			# Now check output
			set dead 0
			set clean 0
			set other 0
			for { set i 0 } { $i < $n } { incr i } {
				set did [open $testdir/dead002.log.$i]
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

	exec $RM -f $testdir/dd.out
	# Remove log files
	for { set i 0 } { $i < $n } { incr i } {
		exec $RM -f $testdir/dead002.log.$i
	}
}

