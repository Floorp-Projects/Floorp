# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)mutexscript.tcl	10.2 (Sleepycat) 4/10/98
#
# Random mutex tester.
# Usage: mutexscript dir numiters mlocks sleepint degree seed
# dir: dir in which all the mutexes live.
# numiters: Total number of iterations.
# nmutex: Total number of mutexes.
# sleepint: Maximum sleep interval.
# degree: Maximum number of locks to acquire at once
# seed: Seed for random number generator.  If -1, use pid.
source ../test/testutils.tcl
source ./include.tcl

set usage "mutexscript dir numiters nmutex sleepint degree seed"

# Verify usage
if { $argc != 6 } {
	puts stderr $usage
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set numiters [ lindex $argv 1 ]
set nmutex [ lindex $argv 2 ]
set sleepint [ lindex $argv 3 ]
set degree [ lindex $argv 4 ]
set seed [ lindex $argv 5 ]
set locker [pid]

# Initialize seed
if { $seed == -1 } {
	srand $locker
} else {
	srand $seed
}

puts -nonewline "Mutexscript: Beginning execution for $locker:"
puts " $numiters $nmutex $sleepint $degree $seed"
flush stdout

set m [mutex_init $dir $nmutex 0 0]
error_check_bad mutex_init $m NULL
error_check_good mutex_init [is_substr $m mutex] 1

for { set iter 0 } { $iter < $numiters } { incr iter } {
	set nlocks [random_int 1 $degree]
	# We will always lock objects in ascending order to avoid
	# deadlocks.
	set lastobj 1
	set mlist {}
	for { set lnum 0 } { $lnum < $nlocks } { incr lnum } {
		# Pick lock parameters
		set obj [random_int $lastobj [expr $nmutex - 1]]
		set lastobj [expr $obj + 1]
		puts "[timestamp] $locker $lnum: $obj"

		# Do get, set its val to own pid, and then add to list
		error_check_good mutex_get:$obj [$m get $obj] 0
		error_check_good mutex_setval:$obj [$m setval $obj [pid]] 0
		lappend mlist $obj
		if {$lastobj >= $nmutex} {
			break
		}
	}

	# Pick sleep interval
	exec $SLEEP [ random_int 1 $sleepint ]

	# Now release locks
	foreach i $mlist {
		error_check_good mutex_getval:$i [$m getval $i] [pid]
		error_check_good mutex_setval:$i \
		    [$m setval $i [expr 0 - [pid]]] 0
		error_check_good mutex_release:$i [$m release $i] 0
	}
	puts "[timestamp] $locker released mutexes"
	flush stdout
}

puts "[timestamp] $locker Complete"
flush stdout
