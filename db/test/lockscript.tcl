# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)lockscript.tcl	10.2 (Sleepycat) 4/10/98
#
# Random lock tester.
# Usage: lockscript dir numiters numobjs sleepint degree readratio seed
# dir: lock directory.
# numiters: Total number of iterations.
# numobjs: Number of objects on which to lock.
# sleepint: Maximum sleep interval.
# degree: Maximum number of locks to acquire at once
# readratio: Percent of locks that should be reads.
# seed: Seed for random number generator.  If -1, use pid.
source ../test/testutils.tcl
source ./include.tcl

set usage "lockscript dir numiters numobjs sleepint degree readratio seed"

# Verify usage
if { $argc != 7 } {
	puts stderr $usage
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set numiters [ lindex $argv 1 ]
set numobjs [ lindex $argv 2 ]
set sleepint [ lindex $argv 3 ]
set degree [ lindex $argv 4 ]
set readratio [ lindex $argv 5 ]
set seed [ lindex $argv 6 ]
set locker [pid]

# Initialize seed
if { $seed == -1 } {
	srand $locker
} else {
	srand $seed
}

puts -nonewline "Beginning execution for $locker: $numiters $numobjs "
puts "$sleepint $degree $readratio $seed"
flush stdout

set lp [lock_open "" 0 0 -dbhome $dir]
error_check_bad lock_open $lp NULL
error_check_good lock_open [is_substr $lp lockmgr] 1

for { set iter 0 } { $iter < $numiters } { incr iter } {
	set nlocks [random_int 1 $degree]
	# We will always lock objects in ascending order to avoid
	# deadlocks.
	set lastobj 1
	set locklist {}
	for { set lnum 0 } { $lnum < $nlocks } { incr lnum } {
		# Pick lock parameters
		set obj [random_int $lastobj $numobjs]
		set lastobj [expr $obj + 1]
		set x [random_int 1 100 ]
		if { $x <= $readratio } {
			set rw $DB_LOCK_READ
		} else {
			set rw $DB_LOCK_WRITE
		}
		puts "[timestamp] $locker $lnum: $rw $obj"

		# Do get; add to list
		set lockp [$lp get $locker $obj $rw 0]
		lappend locklist $lockp
		if {$lastobj > $numobjs} {
			break
		}
	}

	# Pick sleep interval
	exec $SLEEP [ random_int 1 $sleepint ]

	# Now release locks
	puts "[timestamp] $locker released locks"
	release_list $locklist
	flush stdout
}

puts "[timestamp] $locker Complete"
flush stdout
