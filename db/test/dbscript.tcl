# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)dbscript.tcl	8.12 (Sleepycat) 4/10/98
#
# Random db tester.
# Usage: dbscript file numops min_del max_add key_avg data_avgdups
# file: db file on which to operate
# numops: number of operations to do
# ncurs: number of cursors
# min_del: minimum number of keys before you disable deletes.
# max_add: maximum number of keys before you disable adds.
# key_avg: average key size
# data_avg: average data size
# dups: 1 indicates dups allowed, 0 indicates no dups
# errpct: What percent of operations should generate errors
# seed: Random number generator seed (-1 means use pid)
source ./include.tcl
source ../test/testutils.tcl
set alphabet "abcdefghijklmnopqrstuvwxyz"

set usage "dbscript file numops ncurs min_del max_add key_avg data_avg dups errpcnt seed"

# Verify usage
if { $argc != 10 } {
	puts stderr $usage
	exit
}

# Initialize arguments
set file [lindex $argv 0]
set numops [ lindex $argv 1 ]
set ncurs [ lindex $argv 2 ]
set min_del [ lindex $argv 3 ]
set max_add [ lindex $argv 4 ]
set key_avg [ lindex $argv 5 ]
set data_avg [ lindex $argv 6 ]
set dups [ lindex $argv 7 ]
set errpct [ lindex $argv 8 ]
set seed [ lindex $argv 9 ]

# Initialize seed
if { $seed == -1 } {
	set seed [pid]
}
srand $seed

puts "Beginning execution for [pid]"
puts "$file database"
puts "$numops Operations"
puts "$ncurs cursors"
puts "$min_del keys before deletes allowed"
puts "$max_add or fewer keys to add"
puts "$key_avg average key length"
puts "$data_avg average data length"
if { $dups != 1 } {
	puts "No dups"
} else {
	puts "Dups allowed"
}
puts "$errpct % Errors"
puts "$seed seed"

flush stdout

set db [record dbopen $file 0 0 DB_UNKNOWN]
error_check_bad dbopen $db NULL
error_check_good dbopen [is_substr $db db] 1

# Initialize globals including data
set nkeys [db_init $db 1]
puts "Initial number of keys: $nkeys"

set txn 0

# Open the cursors
set curslist {}
for { set i 0 } { $i < $ncurs } { incr i } {
	set dbc [record $db cursor]
	error_check_bad cursor_create $dbc NULL
	lappend curslist $dbc

}

# On each iteration we're going to generate random keys and
# data.  We'll select either a get/put/delete operation unless
# we have fewer than min_del keys in which case, delete is not
# an option or more than max_add in which case, add is not
# an option.  The tcl global arrays a_keys and l_keys keep track
# of key-data pairs indexed by key and a list of keys, accessed
# by integer.
set adds 0
set puts 0
set gets 0
set dels 0
set bad_adds 0
set bad_puts 0
set bad_gets 0
set bad_dels 0
for { set iter 0 } { $iter < $numops } { incr iter } {
	set op [pick_op $min_del $max_add $nkeys]
	set err [is_err $errpct]

	# The op0's indicate that there aren't any duplicates, so we
	# exercise regular operations.  If dups is 1, then we'll use
	# cursor ops.
	switch $op$dups$err {
		add00 {
			incr adds
			set k [random_data $key_avg 1 a_keys ]
			set data [random_data $data_avg 0 0]
			set ret [record $db put $txn $k $data $DB_NOOVERWRITE ]
			newpair $k $data
		}
		add01 {
			incr bad_adds
			set k [random_key]
			set data [random_data $data_avg 0 0]
			set ret [record $db put $txn $k $data $DB_NOOVERWRITE ]
			# Error case so no change to data state
		}
		add10 {
			incr adds
			set dbcinfo [random_cursor $curslist]
			set dbc [lindex $dbcinfo 0]
			if { [random_int 1 2] == 1 } {
				# Add a new key
				set k [random_data $key_avg 1 a_keys ]
				set data [random_data $data_avg 0 0]
				set ret [record $dbc put $txn $k $data \
				    $DB_KEYFIRST ]
				newpair $k $data
			} else {
				# Add a new duplicate
				set dbc [lindex $dbcinfo 0]
				set k [lindex $dbcinfo 1]
				set data [random_data $data_avg 0 0]

				set op [pick_cursput]
				set ret [record $dbc put $txn $k $data $op]
				adddup $k [lindex $dbcinfo 2] $data
			}
		}
		add11 {
			# TODO
			incr bad_adds
			set ret 1
		}
		put00 {
			incr puts
			set k [random_key]
			set data [random_data $data_avg 0 0]
			set ret [record $db put $txn $k $data 0]
			changepair $k $data
		}
		put01 {
			incr bad_puts
			set k [random_key]
			set data [random_data $data_avg 0 0]
			set ret [record $db put $txn $k $data $DB_NOOVERWRITE]
			# Error case so no change to data state
		}
		put10 {
			incr puts
			set dbcinfo [random_cursor $curslist]
			set dbc [lindex $dbcinfo 0]
			set k [lindex $dbcinfo 1]
			set data [random_data $data_avg 0 0]

			set ret [record $dbc put $txn $k $data $DB_CURRENT]
			changedup $k [lindex $dbcinfo 2] $data
		}
		put11 {
			incr bad_puts
			set k [random_key]
			set data [random_data $data_avg 0 0]
			set dbc [record $db cursor]
			set ret [record $dbc put $txn $k $data $DB_CURRENT]
			error_check_good curs_close [record $dbc close] 0
			# Error case so no change to data state
		}
		get00 {
			incr gets
			set k [random_key]
			set val [record $db get $txn $k 0]
			if { $val == $a_keys($k) } {
				set ret 0
			} else {
				set ret "Error got |$val| expected |$a_keys($k)|"
			}
			# Get command requires no state change
		}
		get01 {
			incr bad_gets
			set k [random_data $key_avg 1 a_keys ]
			set ret [record $db get $txn $k 0]
			# Error case so no change to data state
		}
		get10 {
			incr gets
			set dbcinfo [random_cursor $curslist]
			if { [llength $dbcinfo] == 3 } {
				set ret 0
			else
				set ret 0
			}
			# Get command requires no state change
		}
		get11 {
			incr bad_gets
			set k [random_key]
			set dbc [record $db cursor]
			if { [random_int 1 2] == 1 } {
				set dir $DB_NEXT
			} else {
				set dir $DB_PREV
			}
			set ret [record $dbc get $txn $k $DB_NEXT]
			error_check_good curs_close [record $dbc close] 0
			# Error and get case so no change to data state
		}
		del00 {
			incr dels
			set k [random_key]
			set ret [record $db del $txn $k 0]
			rempair $k
		}
		del01 {
			incr bad_dels
			set k [random_data $key_avg 1 a_keys ]
			set ret [record $db del $txn $k 0]
			# Error case so no change to data state
		}
		del10 {
			incr dels
			set dbcinfo [random_cursor $curslist]
			set dbc [lindex $dbcinfo 0]
			set ret [record $dbc del $txn 0]
			remdup [lindex dbcinfo 1] [lindex dbcinfo 2]
		}
		del11 {
			incr bad_dels
			set c [record $db cursor]
			set ret [record $c del $txn 0]
			error_check_good curs_close [record $c close] 0
			# Error case so no change to data state
		}
	}
	if { $err == 1 } {
		# Verify failure.
		error_check_good $op$dups$err:$k [is_substr Error $ret] 1
	} else {
		# Verify success
		error_check_good $op$dups$err:$k $ret 0
	}

	flush stdout
}

# Close cursors and file
foreach i $curslist {
	set r [record $i close]
	error_check_good cursor_close:$i $r 0
}
set r [record $db close]
error_check_good db_close:$db $r 0

puts "[timestamp] [pid] Complete"
puts "Successful ops: $adds adds $gets gets $puts puts $dels dels"
puts "Error ops: $bad_adds adds $bad_gets gets $bad_puts puts $bad_dels dels"
flush stdout

filecheck $file $txn
