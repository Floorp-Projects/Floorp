# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)bug003.tcl	10.3 (Sleepycat) 4/10/98
#
# Bug Test003:
# Check if deleting a key when a cursor is on a duplicate of that key works.
proc bug003 { method args } {
	set method [convert_method $method]
	if { $method == "DB_RECNO" } {
		puts "Bug003 skipping for method RECNO"
		return
	}
	puts "Bug003: $method delete of key in presence of cursor"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile bug003.db
	cleanup $testdir

	set flags 0
	set txn 0

	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method \
	    -flags $DB_DUP $args]]
	error_check_good db_open:dup [is_substr $db db] 1

	set curs [$db cursor $txn]
	error_check_good curs_open:dup [is_substr $curs $db] 1

	puts "\tBug003.a: Key delete with cursor on duplicate."
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

	# Now put in a bunch of duplicates for key 2
	for { set d 1 } { $d <= 5 } {incr d} {
		set r [$db put $txn $key_set(2) dup_$d $flags]
		error_check_good dup:put $r 0
	}

	# Now put the cursor on a duplicate of key 2

	# Now set the cursor on the first of the duplicate set.
	set r [$curs get $key_set(2) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(2)
	error_check_good curs_get:DB_SET:data $d datum$key_set(2)

	# Now do two nexts
	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_1

	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_2

	# Now do the delete
	set r [$db del $txn $key_set(2) 0]
	error_check_good delete $r 0

	# Now check the get current on the cursor.
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs_after_del [llength $r] 0

	# Now check that the rest of the database looks intact.  There
	# should be only two keys, 1 and 3.

	set r [$curs get 0 $DB_FIRST]
	error_check_bad cursor_get:DB_FIRST [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_FIRST:key $k $key_set(1)
	error_check_good curs_get:DB_FIRST:data $d datum$key_set(1)

	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(3)
	error_check_good curs_get:DB_NEXT:data $d datum$key_set(3)

	set r [$curs get 0 $DB_NEXT]
	error_check_good cursor_get:DB_NEXT [llength $r] 0

	puts "\tBug003.b: Cursor delete of first item, followed by cursor FIRST"
	# Set to beginning
	set r [$curs get 0 $DB_FIRST]
	error_check_bad cursor_get:DB_FIRST [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_FIRST:key $k $key_set(1)
	error_check_good curs_get:DB_FIRST:data $d datum$key_set(1)

	# Now do delete
	error_check_good curs_del [$curs del 0] 0

	# Now do DB_FIRST
	set r [$curs get 0 $DB_FIRST]
	error_check_bad cursor_get:DB_FIRST [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_FIRST:key $k $key_set(3)
	error_check_good curs_get:DB_FIRST:data $d datum$key_set(3)

	error_check_good curs_close [$curs close] 0
	error_check_good db_close [$db close] 0
}
