# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)bug004.tcl	10.4 (Sleepycat) 4/10/98
#
# Bug Test004:
# Check if we handle the case where we delete a key with the cursor on it
# and then add the same key.  The cursor should not get the new item
# returned, but the item shouldn't disappear.
# Run test tests, one where the overwriting put is done with a put and
# one where it's done with a cursor put.
proc bug004 { method args } {
	set method [convert_method $method]
	if { $method == "DB_RECNO" } {
		puts "Bug004 skipping for method RECNO"
		return
	}
	puts "Bug004: $method delete and replace in presence of cursor."

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile bug004.db
	cleanup $testdir

	set flags 0
	set txn 0

	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method \
	    -flags $DB_DUP $args]]
	error_check_good db_open:dup [is_substr $db db] 1

	set curs [$db cursor $txn]
	error_check_good curs_open:dup [is_substr $curs $db] 1

	puts "\tBug004.a: Set cursor, delete cursor, put with key."
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

	# Now put the cursor on key 1

	# Now set the cursor on the first of the duplicate set.
	set r [$curs get $key_set(1) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(1)
	error_check_good curs_get:DB_SET:data $d datum$key_set(1)

	# Now do the delete
	set r [$curs del 0]
	error_check_good delete $r 0

	# Now check the get current on the cursor.
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs_after_del [llength $r] 0

	# Now do a put on the key
	set r [$db put $txn $key_set(1) new_datum$key_set(1) $flags]
	error_check_good put $r 0

	# Do a get
	set r [$db get $txn $key_set(1) 0]
	error_check_good get $r new_datum$key_set(1)

	# Recheck cursor
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs_after_del [llength $r] 0

	# Move cursor and see if we get the key.
	set r [$curs get 0 $DB_FIRST]
	error_check_bad cursor_get:DB_FIRST [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_FIRST:key $k $key_set(1)
	error_check_good curs_get:DB_FIRST:data $d new_datum$key_set(1)

	puts "\tBug004.b: Set two cursor on a key, delete one, overwrite other"
	set curs2 [$db cursor $txn]
	error_check_good curs2_open [is_substr $curs2 $db] 1

	# Set both cursors on the 4rd key
	set r [$curs get $key_set(3) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(3)
	error_check_good curs_get:DB_SET:data $d datum$key_set(3)

	set r [$curs2 get $key_set(3) $DB_SET]
	error_check_bad cursor2_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs2_get:DB_SET:key $k $key_set(3)
	error_check_good curs2_get:DB_SET:data $d datum$key_set(3)

	# Now delete through cursor 1
	error_check_good curs1_del [$curs del 0] 0

	# Verify gets on both 1 and 2
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_del [llength $r] 0
	set r [$curs2 get 0 $DB_CURRENT]
	error_check_good curs2_get_after_del [llength $r] 0

	# Now do a replace through cursor 2
	set r [$curs2 put 0 new_datum$key_set(3) $DB_CURRENT]
	error_check_bad curs_replace $r 0

	# Gets fail
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_replace [llength $r] 0
	set r [$curs2 get 0 $DB_CURRENT]
	error_check_good curs2_get_after_replace [llength $r] 0

	puts "\tBug004.c: Set two cursors on a dup, delete one, overwrite other"

	# Set both cursors on the 2nd duplicate of key 2
	set r [$curs get $key_set(2) $DB_SET]
	error_check_bad cursor_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_SET:key $k $key_set(2)
	error_check_good curs_get:DB_SET:data $d datum$key_set(2)

	set r [$curs get 0 $DB_NEXT]
	error_check_bad cursor_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs_get:DB_NEXT:data $d dup_1

	set r [$curs2 get $key_set(2) $DB_SET]
	error_check_bad cursor2_get:DB_SET [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs2_get:DB_SET:key $k $key_set(2)
	error_check_good curs2_get:DB_SET:data $d datum$key_set(2)

	set r [$curs2 get 0 $DB_NEXT]
	error_check_bad cursor2_get:DB_NEXT [llength $r] 0
	set k [lindex $r 0]
	set d [lindex $r 1]
	error_check_good curs2_get:DB_NEXT:key $k $key_set(2)
	error_check_good curs2_get:DB_NEXT:data $d dup_1

	# Now delete through cursor 1
	error_check_good curs1_del [$curs del 0] 0

	# Verify gets on both 1 and 2
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_del [llength $r] 0
	set r [$curs2 get 0 $DB_CURRENT]
	error_check_good curs2_get_after_del [llength $r] 0

	# Now do a replace through cursor 2
	set r [$curs2 put 0 new_dup_1 $DB_CURRENT]
	error_check_bad curs_replace $r 0

	# Both gets should fail
	set r [$curs get 0 $DB_CURRENT]
	error_check_good curs1_get_after_replace [llength $r] 0
	set r [$curs2 get 0 $DB_CURRENT]
	error_check_good curs2_get_after_replace [llength $r] 0

	error_check_good curs2_close [$curs2 close] 0
	error_check_good curs_close [$curs close] 0
	error_check_good db_close [$db close] 0
}
