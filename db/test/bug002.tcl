# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)bug002.tcl	10.2 (Sleepycat) 4/10/98
#
# Bug Test002:
# This test checks basic cursor operations.
# There are N different scenarios to tests:
# 1. (no dups) Set cursor, retrieve current.
# 2. (no dups) Set cursor, retrieve next.
# 3. (no dups) Set cursor, retrieve prev.
proc bug002 { method args } {
	set method [convert_method $method]
	puts "Bug002: $method interspersed cursor and normal operations"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile bug002.db
	cleanup $testdir

	set flags 0
	set txn 0

	puts "\tBug002.a: No duplicates"
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good db_open:nodup [is_substr $db db] 1

	set curs [$db cursor $txn]
	error_check_good curs_open:nodup [is_substr $curs $db] 1

	# Put three keys in the database
	for { set key 1 } { $key <= 3 } {incr key} {
		set r [$db put $txn $key datum$key $flags]
		error_check_good put $r 0
	}

	# Retrieve keys sequentially so we can figure out their order
	set i 1
	for {set d [$curs get 0 $DB_FIRST] } { [string length $d] != 0 } {
	    set d [$curs get 0 $DB_NEXT] } {
		set key_set($i) [lindex $d 0]
		incr i
	}

	# TEST CASE 1
	puts "\tBug002.a1: Set cursor, retrieve current"

	# Now set the cursor on the middle on.
	set r [$curs get $key_set(2) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(2)
	error_check_good curs_get:DB_SET:data $d datum$key_set(2)

	# Now retrieve current
	set r [$curs get 0 $DB_CURRENT]
	error_check_bad cursor_get:DB_CURRENT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_CURRENT:key $k $key_set(2)
	error_check_good curs_get:DB_CURRENT:data $d datum$key_set(2)

	# TEST CASE 2
	puts "\tBug002.a2: Set cursor, retrieve previous"
	set r [$curs get 0 $DB_PREV]
	error_check_bad cursor_get:DB_PREV [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_PREV:key $k $key_set(1)
	error_check_good curs_get:DB_PREV:data $d datum$key_set(1)

	#TEST CASE 3
	puts "\tBug002.a2: Set cursor, retrieve next"

	# Now set the cursor on the middle on.
	set r [$curs get $key_set(2) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(2)
	error_check_good curs_get:DB_SET:data $d datum$key_set(2)

	# Now retrieve next
	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(3)
	error_check_good curs_get:DB_NEXT:data $d datum$key_set(3)

	# Close cursor and database.
	error_check_good curs_close [$curs close] 0
	error_check_good db_close [$db close] 0
}
