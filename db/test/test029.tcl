# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test029.tcl	10.7 (Sleepycat) 4/23/98
#
# DB Test 29 {method nentries}
# Test the Btree and Record number renumbering.

proc test029 { method {nentries 10000} args} {
	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set method [convert_method $method]
	set args [number_btree $method $args]
	puts "Test029: $method ($args)"

	if { [string compare $method DB_HASH] == 0 } {
		puts "Test029 skipping for method HASH"
		return
	}
	if { [string compare $method DB_RECNO] == 0 && $do_renumber != 1 } {
		puts "Test029 skipping for method RECNO (w/out renumbering)"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test029.db
	cleanup $testdir

	# Read the first nentries dictionary elements and reverse them.
	# Keep a list of these (these will be the keys).
	puts "\tTest029.a: initialization"
	set keys ""
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		lappend keys [reverse $str]
		incr count
	}
	close $did

	# Generate sorted order for the keys
	set sorted_keys [lsort $keys]

	# Save the first and last keys
	set last_key [lindex $sorted_keys end]
	set last_keynum [llength $sorted_keys]

	set first_key [lindex $sorted_keys 0]
	set first_keynum 1

	# Create the database
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set flags 0
	set txn 0
	puts "\tTest029.b: put/get loop"
	foreach k $keys {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [lsearch $sorted_keys $k]
			incr key
		} else {
			set key $k
		}
		set ret [$db put $txn $key $k $flags]
		error_check_good dbput $ret 0

		set ret [$db get $txn $key $flags]
		if { [string compare $ret $k] != 0 } {
			puts "Test029: put key-data $key $k got $ret"
			return
		}
	}

	# Now delete the first key in the database
	puts "\tTest029.c: delete and verify renumber"

	# Delete the first key in the file
	if { [string compare $method DB_RECNO] == 0 } {
		set key $first_keynum
	} else {
		set key $first_key
	}

	set ret [$db del $txn $key $flags]
	error_check_good db_del $ret 0

	# Verify that the keynumber of the last key changed
	if { [string compare $method DB_BTREE] == 0 } {
		set flags $DB_SET_RECNO
	}

	# First try to get the old last key (shouldn't exist)
	set ret [$db getn $txn $last_keynum $flags]
	error_check_good get_after_del [is_substr $ret "not found"] 1

	# Now try to get what we think should be the last key
	set ret [$db getn $txn [expr $last_keynum - 1] $flags]
	error_check_good getn_last_after_del $ret $last_key

	# Create a cursor; we need it for the next test and we
	# need it for recno here.

	set dbc [$db cursor $txn]
	error_check_good cursor [is_valid_widget $dbc $db.cursor] TRUE

	# OK, now re-put the first key and make sure that we
	# renumber the last key appropriately.
	if { [string compare $method DB_BTREE] == 0 } {
		set ret [$db put $txn $key $first_key 0]
		error_check_good db_put $ret 0
	} else {
		# Recno
		set ret [$dbc get 0 $DB_FIRST]
		set ret [$dbc put $key $first_key $DB_BEFORE]
		error_check_good dbc_put:DB_BEFORE $ret 0
	}

	# Now check that the last record matches the last record number
	set ret [$db getn $txn $last_keynum $flags]
	error_check_good getn_last_after_put $ret $last_key

	# Now delete the first key in the database using a cursor
	puts "\tTest029.d: delete with cursor and verify renumber"

	set ret [$dbc get 0 $DB_FIRST]
	error_check_good dbc_first [lindex $ret 0] $key
	error_check_good dbc_first [lindex $ret 1] $first_key

	# Now delete at the cursor
	set ret [$dbc del 0]
	error_check_good dbc_del $ret 0

	# Now check the record numbers of the last keys again.
	# First try to get the old last key (shouldn't exist)
	set ret [$db getn $txn $last_keynum $flags]
	error_check_good get_last_after_cursor_del:$ret \
	    [is_substr $ret "not found"] 1

	# Now try to get what we think should be the last key
	set ret [$db getn $txn [expr $last_keynum - 1] $flags]
	error_check_good getn_after_cursor_del $ret $last_key

	# OK, now re-put the first key and make sure that we
	# renumber the last key appropriately.
	if { [string compare $method DB_BTREE] == 0 } {
		set ret [$dbc put $key $first_key $DB_CURRENT]
		error_check_good dbc_put:DB_CURRENT $ret 0
	} else {
		set ret [$dbc put $key $first_key $DB_BEFORE]
		error_check_good dbc_put:DB_BEFORE $ret 0
	}

	# Now check that the last record matches the last record number
	set ret [$db getn $txn $last_keynum $flags]
	error_check_good get_after_cursor_reput $ret $last_key

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
}

