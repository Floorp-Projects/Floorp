# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test026.tcl	8.5 (Sleepycat) 4/25/98
#
# DB Test 26 {access method}
# Keyed delete test through cursor.
# If ndups is small; this will test on-page dups; if it's large, it
# will test off-page dups.
proc test026 { method {nentries 2000} {ndups 5} {tnum 26} args} {
	set omethod $method
	set method [convert_method $method]
	set args [convert_args $method $args]
	if { [string compare $method DB_RECNO] == 0 || \
	    [is_rbtree $omethod] == 1 } {
		puts "Test0$tnum skipping for method $omethod"
		return
	}
	puts "Test0$tnum: $method ($args) $nentries keys with $ndups dups; cursor delete test"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test0$tnum.db

	set flags 0
	set txn 0
	set count 0

	# Here is the loop where we put and get each key/data pair

	puts "Test0$tnum.a: Put loop"
	cleanup $testdir
	set args [add_to_args $DB_DUP $args]
	set db [eval [concat dbopen $testfile \
	    [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]
	while { [gets $did str] != -1 && $count < [expr $nentries * $ndups] } {
		set datastr [ make_data_str $str ]
		for { set j 1 } { $j <= $ndups} {incr j} {
			set ret [$db put $txn $str $j$datastr $flags]
			error_check_good db_put $ret 0
			incr count
		}
	}
	close $did

	error_check_good db_close [$db close] 0
	set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Now we will sequentially traverse the database getting each
	# item and deleting it.
	set count 0
	set dbc [$db cursor $txn]
	error_check_good db_cursor [is_substr $dbc $db] 1

	puts "Test0$tnum.b: Get/delete loop"
	set i 1
	for { set ret [$dbc get 0 $DB_FIRST] } {
	    [string length $ret] != 0 } {
	    set ret [$dbc get 0 $DB_NEXT] } {

		set key [lindex $ret 0]
		set data [lindex $ret 1]
		if { $i == 1 } {
			set curkey $key
		}
		error_check_good seq_get:key $key $curkey
		error_check_good seq_get:data $data $i[make_data_str $key]

		if { $i == $ndups } {
			set i 1
		} else {
			incr i
		}

		# Now delete the key
		set ret [ $dbc del $flags ]
		error_check_good db_del:$key $ret 0
	}
	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

	puts "Test0$tnum.c: Verify empty file"
	# Double check that file is now empty
	set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
	error_check_good dbopen [is_valid_db $db] TRUE
	set dbc [$db cursor $txn]
	error_check_good db_cursor [is_substr $dbc $db] 1
	set ret [$dbc get 0 $DB_FIRST]
	error_check_good get_on_empty [string length $ret] 0
	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
}
