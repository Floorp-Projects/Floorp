# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)bug001.tcl	10.3 (Sleepycat) 4/10/98
#
# Bug Test001:
# This series of tests are designed to test for known problems or trouble
# areas in the distribution.
# This test checks for cursor maintenance in the presence of deletes.
# There are N different scenarios to tests:
# 1. No duplicates.  Cursor A deletes a key, do a  GET for the key.
# 2. No duplicates.  Cursor is positioned right before key K, Delete K,
#    do a next on the cursor.
# 3. No duplicates.  Cursor is positioned on key K, do a regular delete of K.
#    do a current get on K.
# 4. Repeat 3 but do a next instead of current.
#
# 5. Duplicates. Cursor A is on the first item of a duplicate set, A
#    does a delete.  Then we do a non-cursor get.
# 6. Duplicates.  Cursor A is in a duplicate set and deletes the item.
#    do a delete of the entire Key. Test cursor current.
# 7. Continue last test and try cursor next.
# 8. Duplicates.  Cursor A is in a duplicate set and deletes the item.
#    Cursor B is in the same duplicate set and deletes a different item.
#    Verify that the cursor is in the right place.
# 9. Cursors A and B are in the place in the same duplicate set.  A deletes
#    its item.  Do current on B.
# 10. Continue 8 and do a next on B.
proc bug001 { method args } {
	set method [convert_method $method]
	puts "Bug001: $method interspersed cursor and normal operations"
	if { $method == "DB_RECNO" } {
		puts "Bug001 skipping for method RECNO"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile bug001.db
	cleanup $testdir

	set flags 0
	set txn 0

	puts "\tBug001.a: No Duplicate Tests"
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
	puts "\tBug001.a1: Delete w/cursor, regular get"

	# Now set the cursor on the middle on.
	set r [$curs get $key_set(2) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(2)
	error_check_good curs_get:DB_SET:data $d datum$key_set(2)

	# Now do the delete
	set r [$curs del 0]
	error_check_good curs_del $r 0

	# Now do the get
	set r [$db get $txn $key_set(2) 0]
	error_check_good get_after_del [is_substr $r "not found"] 1

	# Free up the cursor.
	error_check_good cursor_close [$curs close] 0

	# TEST CASE 2
	puts "\tBug001.a2: Cursor before K, delete K, cursor next"

	# Replace key 2
	set r [$db put $txn $key_set(2) datum$key_set(2) 0]
	error_check_good put $r 0

	# Open and position cursor on first item.
	set curs [$db cursor $txn]
	error_check_good curs_open:nodup [is_substr $curs $db] 1

	# Retrieve keys sequentially so we can figure out their order
	set i 1
	for {set d [$curs get 0 $DB_FIRST] } { [string length $d] != 0 } {
	    set d [$curs get 0 $DB_NEXT] } {
		set key_set($i) [lindex $d 0]
		incr i
	}

	set r [$curs get $key_set(1) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(1)
	error_check_good curs_get:DB_SET:data $d datum$key_set(1)

	# Now delete (next item) $key_set(2)
	error_check_good db_del:$key_set(2) [$db del $txn $key_set(2) 0] 0

	# Now do next on cursor
	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(3)
	error_check_good curs_get:DB_NEXT:data $d datum$key_set(3)

	# TEST CASE 3
	puts "\tBug001.a3: Cursor on K, delete K, cursor current"

	# delete item 3
	error_check_good db_del:$key_set(3) [$db del $txn $key_set(3) 0] 0
	set r [$curs get 0 $DB_CURRENT]
	error_check_good current_after_del [llength $r] 0
	error_check_good cursor_close [$curs close] 0

	puts "\tBug001.a4: Cursor on K, delete K, cursor next"

	# Restore keys 2 and 3
	set r [$db put $txn $key_set(2) datum$key_set(2) 0]
	error_check_good put $r 0
	set r [$db put $txn $key_set(3) datum$key_set(3) 0]
	error_check_good put $r 0

	# Create the new cursor and put it on 1
	set curs [$db cursor $txn]
	error_check_good curs_open:nodup [is_substr $curs $db] 1
	set r [$curs get $key_set(1) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(1)
	error_check_good curs_get:DB_SET:data $d datum$key_set(1)

	# Delete 2
	error_check_good db_del:$key_set(2) [$db del $txn $key_set(2) 0] 0

	# Now do next on cursor
	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(3)
	error_check_good curs_get:DB_NEXT:data $d datum$key_set(3)

	# Close cursor
	error_check_good curs_close [$curs close] 0
	error_check_good db_close [$db close] 0

	# Now get ready for duplicate tests


	puts "\tBug001.b: Duplicate Tests"
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method \
	    -flags $DB_DUP $args]]
	error_check_good db_open:dup [is_substr $db db] 1

	set curs [$db cursor $txn]
	error_check_good curs_open:dup [is_substr $curs $db] 1

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

	# TEST CASE 5
	puts "\tBug001.b1: Delete dup w/cursor on first item.  Get on key."

	# Now set the cursor on the first of the duplicate set.
	set r [$curs get $key_set(2) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(2)
	error_check_good curs_get:DB_SET:data $d datum$key_set(2)

	# Now do the delete
	set r [$curs del 0]
	error_check_good curs_del $r 0

	# Now do the get
	set r [$db get $txn $key_set(2) 0]
	error_check_good get_after_del $r dup_1

	# TEST CASE 6
	puts "\tBug001.b2: Now get the next duplicate from the cursor."

	# Now do next on cursor
	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_1

	# TEST CASE 3
	puts "\tBug001.b3: Two cursors in set; each delete different items"

	# Open a new cursor.
	set curs2 [$db cursor $txn]
	error_check_good curs_open [is_substr $curs2 $db] 1

	# Set on last of duplicate set.
	set r [$curs2 get $key_set(3) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(3)
	error_check_good curs_get:DB_SET:data $d datum$key_set(3)

	set r [$curs2 get 0 $DB_PREV]
	error_check_bad cursor_get:DB_PREV [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_PREV:key $k $key_set(2)
	error_check_good curs_get:DB_PREV:data $d dup_5

	# Delete the item at cursor 1 (dup_1)
	error_check_good curs1_del [$curs del 0] 0

	# Verify curs1 and curs2
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_del [llength $r] 0

	set r [$curs2 get 0 $DB_CURRENT]
	error_check_bad curs2_get [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_CURRENT:key $k $key_set(2)
	error_check_good curs_get:DB_CURRENT:data $d dup_5

	# Now delete the item at cursor 2 (dup_5)
	error_check_good curs2_del [$curs2 del 0] 0

	# Verify curs1 and curs2
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_del2 [llength $r] 0
	set r [$curs2 get 0 $DB_CURRENT]
	error_check_good curs2_get_after_del2 [llength $r] 0

	# Now verify that next and prev work.

	set r [$curs2 get 0 $DB_PREV]
	error_check_bad cursor_get:DB_PREV [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_PREV:key $k $key_set(2)
	error_check_good curs_get:DB_PREV:data $d dup_4

	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_2

	puts "\tBug001.b4: Two cursors same item, one delete, one get"

	# Move curs2 onto dup_2
	set r [$curs2 get 0 $DB_PREV]
	error_check_bad cursor_get:DB_PREV [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_PREV:key $k $key_set(2)
	error_check_good curs_get:DB_PREV:data $d dup_3

	set r [$curs2 get 0 $DB_PREV]
	error_check_bad cursor_get:DB_PREV [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_PREV:key $k $key_set(2)
	error_check_good curs_get:DB_PREV:data $d dup_2

	# delete on curs 1
	error_check_good curs1_del [$curs del 0] 0

	# Verify gets on both 1 and 2
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_del [llength $r] 0
	set r [$curs2 get 0 $DB_CURRENT]
	error_check_good curs2_get_after_del [llength $r] 0

	puts "\tBug001.b5: Now do a next on both cursors"

	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_3

	set r [$curs2 get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_3

	# Close cursor
	error_check_good curs_close [$curs close] 0
	error_check_good curs2_close [$curs2 close] 0
	error_check_good db_close [$db close] 0
}
