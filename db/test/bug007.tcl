# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)bug007.tcl	8.2 (Sleepycat) 4/10/98
#
# Bug Test001:
# Make sure that we handle retrieves of zero-length data items correctly.
# The following ops, should allow a partial data retrieve of 0-length.
#	db_get
#	db_cget FIRST, NEXT, LAST, PREV, CURRENT, SET, SET_RANGE
proc bug007 { method args } {
	set method [convert_method $method]
	puts "Bug007: $method 0-length partial data retrieval"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile bug007.db
	cleanup $testdir

	set flags 0
	set txn 0

	puts "\tBug007.a: Populate a database"
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good db_create [is_substr $db db] 1

	# Put ten keys in the database
	for { set key 1 } { $key <= 10 } {incr key} {
		set r [$db put $txn $key datum$key $flags]
		error_check_good put $r 0
	}

	# Retrieve keys sequentially so we can figure out their order
	set i 1
	set curs [$db cursor 0]
	error_check_good db_curs [is_substr $curs $db] 1
	for {set d [$curs get 0 $DB_FIRST] } { [string length $d] != 0 } {
	    set d [$curs get 0 $DB_NEXT] } {
		set key_set($i) [lindex $d 0]
		incr i
	}

	puts "\tBug007.a: db get with 0 partial length retrieve"

	# Now set the cursor on the middle on.
	set ret [$db get 0 $key_set(5) $DB_DBT_PARTIAL 0 0]
	error_check_good db_get_0 [string length $ret] 0

	puts "\tBug007.a: db cget FIRST with 0 partial length retrieve"
	set ret [$curs get 0 [expr $DB_FIRST|$DB_DBT_PARTIAL] 0 0]
	set data [lindex $ret 1]
	set key [lindex $ret 0]
	error_check_good key_check_first $key $key_set(1)
	error_check_good db_cget_first [string length $data] 0

	puts "\tBug007.b: db cget NEXT with 0 partial length retrieve"
	set ret [$curs get 0 [expr $DB_NEXT|$DB_DBT_PARTIAL] 0 0]
	set data [lindex $ret 1]
	set key [lindex $ret 0]
	error_check_good key_check_next $key $key_set(2)
	error_check_good db_cget_next [string length $data] 0

	puts "\tBug007.c: db cget LAST with 0 partial length retrieve"
	set ret [$curs get 0 [expr $DB_LAST|$DB_DBT_PARTIAL] 0 0]
	set data [lindex $ret 1]
	set key [lindex $ret 0]
	error_check_good key_check_last $key $key_set(10)
	error_check_good db_cget_last [string length $data] 0

	puts "\tBug007.d: db cget PREV with 0 partial length retrieve"
	set ret [$curs get 0 [expr $DB_PREV|$DB_DBT_PARTIAL] 0 0]
	set data [lindex $ret 1]
	set key [lindex $ret 0]
	error_check_good key_check_prev $key $key_set(9)
	error_check_good db_cget_prev [string length $data] 0

	puts "\tBug007.e: db cget CURRENT with 0 partial length retrieve"
	set ret [$curs get 0 [expr $DB_CURRENT|$DB_DBT_PARTIAL] 0 0]
	set data [lindex $ret 1]
	set key [lindex $ret 0]
	error_check_good key_check_current $key $key_set(9)
	error_check_good db_cget_current [string length $data] 0

	puts "\tBug007.f: db cget SET with 0 partial length retrieve"
	set ret [$curs get $key_set(7) [expr $DB_SET|$DB_DBT_PARTIAL] 0 0]
	set data [lindex $ret 1]
	set key [lindex $ret 0]
	error_check_good key_check_set $key $key_set(7)
	error_check_good db_cget_set [string length $data] 0

	if {$method == "DB_BTREE"} {
		puts "\tBug007.g: db cget SET_RANGE with 0 partial length \
		    retrieve"
		set ret [$curs get $key_set(5) \
		    [expr $DB_SET_RANGE|$DB_DBT_PARTIAL] 0 0]
		set data [lindex $ret 1]
		set key [lindex $ret 0]
		error_check_good key_check_set $key $key_set(5)
		error_check_good db_cget_set [string length $data] 0
	}

	error_check_good curs_close [$curs close] 0
	error_check_good db_close [$db close] 0

}
