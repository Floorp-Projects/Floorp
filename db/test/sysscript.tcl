# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)sysscript.tcl	10.3 (Sleepycat) 4/10/98
#
# System integration test script.
# This script runs a single process that tests the full functionality of
# the system.  The database under test contains nfiles files.  Each process
# randomly generates a key and some data.  Both keys and data are bimodally
# distributed between small keys (1-10 characters) and large keys (the avg
# length is indicated via the command line parameter.
# The process then decides on a replication factor between 1 and nfiles.
# It writes the key and data to that many files and tacks on the file ids
# of the files it writes to the data string.  For example, let's say that
# I randomly generate the key dog and data cat.  Then I pick a replication
# factor of 3.  I pick 3 files from the set of n (say 1, 3, and 5).  I then
# rewrite the data as 1:3:5:cat.  I begin a transaction, add the key/data
# pair to each file and then commit.  Notice that I may generate replication
# of the form 1:3:3:cat in which case I simply add a duplicate to file 3.
#
# Usage: sysscript dir nfiles key_avg data_avg seed
#
# dir: DB_HOME directory
# nfiles: number of files in the set
# key_avg: average big key size
# data_avg: average big data size
# seed: Random number generator seed (-1 means use pid)
source ./include.tcl
source ../test/testutils.tcl
set alphabet "abcdefghijklmnopqrstuvwxyz"
set mypid [pid]

set usage "sysscript dir nfiles key_avg data_avg seed"

# Verify usage
if { $argc != 5 } {
	puts stderr $usage
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set nfiles [ lindex $argv 1 ]
set key_avg [ lindex $argv 2 ]
set data_avg [ lindex $argv 3 ]
set seed [ lindex $argv 4 ]

# Initialize seed
if { $seed == -1 } {
	set seed $mypid
}
srand $seed

puts "Beginning execution for $mypid"
puts "$dir DB_HOME"
puts "$nfiles files"
puts "$key_avg average key length"
puts "$data_avg average data length"
puts "$seed seed"

flush stdout

# Create local environment
set flags [expr $DB_INIT_MPOOL | $DB_INIT_LOCK | $DB_INIT_LOG | $DB_INIT_TXN]
set dbenv [record dbenv -dbflags $flags -dbhome $dir]
error_check_bad $mypid:dbenv $dbenv NULL
error_check_good $mypid:dbenv [is_substr $dbenv env] 1
set tm [record txn "" 0 0 -dbenv $dbenv]

# Now open the files
for { set i 0 } { $i < $nfiles } { incr i } {
	set file test024.$i.db
	set db($i) [record dbopen $file 0 0 DB_UNKNOWN -dbenv $dbenv]
	error_check_bad $mypid:dbopen $db($i) NULL
	error_check_bad $mypid:dbopen [is_substr $db($i) error] 1
}

while { 1 } {
	# Decide if we're going to create a big key or a small key
	# We give small keys a 70% chance.
	if { [random_int 1 10] < 8 } {
		set k [random_data 5 0 0 ]
	} else {
		set k [random_data $key_avg 0 0 ]
	}
	set data [random_data $data_avg 0 0]

	set txn [record $tm begin]
	error_check_bad $mypid:txn_begin $txn NULL
	error_check_good $mypid:txn_begin [is_substr $txn $tm] 1

	# Open cursors
	for { set f 0 } {$f < $nfiles} {incr f} {
		set cursors($f) [record $db($f) cursor $txn]
		error_check_bad $mypid:cursor_open $cursors($f) NULL
		error_check_good $mypid:cursor_open \
		    [is_substr $cursors($f) $db($f)] 1
	}
	set aborted 0

	# Check to see if key is already in database
	set found 0
	for { set i 0 } { $i < $nfiles } { incr i } {
		set r [record $db($i) get $txn $k 0]
		if { $r == "-1" } {
			for {set f 0 } {$f < $nfiles} {incr f} {
				error_check_good $mypid:cursor_close \
				    [$cursors($f) close] 0
			}
			error_check_good $mypid:txn_abort [record $txn abort] 0
			set aborted 1
			set found 2
			break
		} elseif { $r != "Key $k not found." } {
			set found 1
			break
		}
	}
	switch $found {
	2 {
		# Transaction aborted, no need to do anything.
	}
	0 {
		# Key was not found, decide how much to replicate
		# and then create a list of that many file IDs.
		set repl [random_int 1 $nfiles]
		set fset ""
		for { set i 0 } { $i < $repl } {incr i} {
			set f [random_int 0 [expr $nfiles - 1]]
			lappend fset $f
			set data $f:$data
		}

		foreach i $fset {
			set r [record $db($i) put $txn $k $data 0]
			if {$r == "-1"} {
				for {set f 0 } {$f < $nfiles} {incr f} {
					error_check_good $mypid:cursor_close \
					    [$cursors($f) close] 0
				}
				error_check_good $mypid:txn_abort \
				    [record $txn abort] 0
				set aborted 1
				break
			}
		}
	}
	1 {
		# Key was found.  Make sure that all the data values
		# look good.
		set f [zero_list $nfiles]
		set data $r
		while { [set ndx [string first : $r]] != -1 } {
			set fnum [string range $r 0 [expr $ndx - 1]]
			if { [lindex $f $fnum] == 0 } {
				set flag $DB_SET
			} else {
				set flag $DB_NEXT
			}
			set full [record $cursors($fnum) get $k $flag]
			if {$full == "-1"} {
				for {set f 0 } {$f < $nfiles} {incr f} {
					error_check_good $mypid:cursor_close \
					    [$cursors($f) close] 0
				}
				error_check_good $mypid:txn_abort \
				    [record $txn abort] 0
				set aborted 1
				break
			}
			error_check_bad $mypid:curs_get($k,$data,$fnum,$flag) \
			    [string length $full] 0
			set key [lindex $full 0]
			set rec [lindex $full 1]
			error_check_good $mypid:dbget_$fnum:key $key $k
			error_check_good \
			    $mypid:dbget_$fnum:data($k) $rec $data
			set f [lreplace $f $fnum $fnum 1]
			incr ndx
			set r [string range $r $ndx end]
		}
	}
	}
	if { $aborted == 0 } {
		for {set f 0 } {$f < $nfiles} {incr f} {
			error_check_good $mypid:cursor_close \
			    [$cursors($f) close] 0
		}
		error_check_good $mypid:commit [record $txn commit] 0
	}
}

# Close files
for { set i 0 } { $i < $nfiles} { incr i } {
	set r [record $db($i) close]
	error_check_good $mypid:db_close:$i $r 0
}

# Close tm and environment
error_check_good $mypid:txn_close [record $tm close] 0
rename $dbenv {}

puts "[timestamp] [pid] Complete"
flush stdout

filecheck $file 0

