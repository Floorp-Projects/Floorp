# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test021.tcl	8.8 (Sleepycat) 4/26/98
#
# DB Test 21 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self, reversed as key and self as data.
# After all are entered, retrieve each using a cursor SET_RANGE, and getting
# about 20 keys sequentially after it (in some cases we'll run out towards
# the end of the file).
proc test021 { method {nentries 10000} args } {
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test021: $method ($args) $nentries equal key/data pairs"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test021.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]

	set flags 0
	set txn 0
	set count 0

	if { [string compare $method DB_RECNO] == 0 } {
		set checkfunc test021_recno.check
		set put putn
	} else {
		set checkfunc test021.check
		set put put
	}
	puts "\tTest021.a: put loop"
	# Here is the loop where we put each key/data pair
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			global kvals

			set key [expr $count + 1]
			set kvals($key) $str
		} else {
			set key [reverse $str]
		}

		set r [$db $put $txn $key $str $flags]
		error_check_good db_put $r 0
		incr count
	}
	close $did

	# Now we will get each key from the DB and retrieve about 20
	# records after it.
	error_check_good db_close [$db close] 0

	puts "\tTest021.b: test ranges"
	set db [dbopen $testfile $DB_RDONLY 0 $method]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Open a cursor
	set dbc [$db cursor 0]
	error_check_good db_cursor [is_substr $dbc $db] 1

	set did [open $dict]
	set i 0
	while { [gets $did str] != -1 && $i < $count } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $i + 1]
		} else {
			set key [reverse $str]
		}

		set r [$dbc get $key $DB_SET_RANGE]
		error_check_bad dbc_get:$key [string length $r] 0
		set k [lindex $r 0]
		set d [lindex $r 1]
		$checkfunc $k $d

		for { set nrecs 0 } { $nrecs < 20 } { incr nrecs } {
			set r [$dbc get 0 $DB_NEXT]
			# no error checking because we may run off the end
			# of the database
			if { [llength $r] == 0 } {
				continue;
			}
			set k [lindex $r 0]
			set d [lindex $r 1]
			$checkfunc $k $d
		}
		incr i
	}
	error_check_good db_close [$db close] 0

}

# Check function for test021; keys and data are reversed
proc test021.check { key data } {
	error_check_good "key/data mismatch for $key" $data [reverse $key]
}

proc test021_recno.check { key data } {
global dict
global kvals
	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "data mismatch: key $key" $data $kvals($key)
}
