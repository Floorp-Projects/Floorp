# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test006.tcl	10.4 (Sleepycat) 4/10/98
#
# DB Test 6 {access method}
# Keyed delete test.
# Create database.
# Go through database, deleting all entries by key.
proc test006 { method {nentries 10000} {reopen 6} args} {
	set tnum Test00$reopen
	set args [convert_args $method $args]
	set do_renumber [is_rrecno $method]
	set method [convert_method $method]
	puts -nonewline "$tnum: $method ($args) $nentries equal small key; medium data pairs"
	if {$reopen == 7} {
		puts "(with close)"
	} else {
		puts ""
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile $tnum.db

	set flags 0
	set txn 0
	set count 0

	# Here is the loop where we put and get each key/data pair

	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1 ]
			set put putn
		} else {
			set key $str
			set put put
		}

		set datastr [ make_data_str $str ]

		$db $put $txn $key $datastr $flags
		set ret [$db get $txn $key $flags]
		if { [string compare $ret $datastr] != 0 } {
			puts "$tnum: put $datastr got $ret"
			return
		}
		incr count
	}
	close $did

	if { $reopen == 7 } {
		error_check_good db_close [$db close] 0
		set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
		error_check_good dbopen [is_valid_db $db] TRUE
	}

	# Now we will get each key from the DB and compare the results
	# to the original, then delete it.
	set count 0
	set did [open $dict]
	set key 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { $do_renumber == 1 } {
			set key 1
		} elseif { [string compare $method DB_RECNO] == 0 } {
			incr key
		} else {
			set key $str
		}

		set datastr [ make_data_str $str ]

		set ret [ $db del $txn $key $flags ]
		error_check_good db_del:$key $ret 0
		incr count
	}
	close $did

	error_check_good db_close [$db close] 0
}
