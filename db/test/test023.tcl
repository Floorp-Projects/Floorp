# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test023.tcl	8.7 (Sleepycat) 4/26/98
#
# Duplicate delete test.
# Add a key with duplicates (first time on-page, second time off-page)
# Number the dups.
# Delete dups and make sure that CURRENT/NEXT/PREV work correctly.
proc test023 { method args } {
	global dupnum
	global dupstr
	global alphabet
	set omethod $method
	set method [convert_method $method]
	puts "Test023: $method delete duplicates/check cursor operations"
	if { [string compare $method DB_RECNO] == 0 || \
	    [is_rbtree $omethod] == 1 } {
		puts "Test023: skipping for method $omethod"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test023.db
	set t1 $testdir/t1
	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method -flags \
	    $DB_DUP $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set flags 0
	set txn 0

	set dbc [$db cursor $txn]
	error_check_good db_cursor [is_substr $dbc $db] 1

	foreach i { onpage offpage } {
		if { $i == "onpage" } {
			set dupstr DUP
		} else {
			set dupstr [repeat $alphabet 50]
		}
		puts "\tTest023.a: Insert key w/$i dups"
		set key "duplicate_val_test"
		for { set count 0 } { $count < 20 } { incr count } {
			set ret [$db put $txn $key $count$dupstr $flags]
			error_check_good db_put $ret 0
		}

		# Now let's get all the items and make sure they look OK.
		puts "\tTest023.b: Check initial duplicates"
		set dupnum 0
		dump_file $db $txn $t1 test023.check

		# Now delete a couple of random items (FIRST, LAST one in middle)
		# Make sure that current returns an error and that NEXT and PREV
		# do the right things

		set ret [$dbc get $key $DB_SET]
		error_check_bad dbc_get:SET [llength $ret] 0

		puts "\tTest023.c: Delete first and try gets"
		# This should be the first duplicate
		error_check_good dbc_get:SET [lindex $ret 0] duplicate_val_test
		error_check_good dbc_get:SET [lindex $ret 1] 0$dupstr

		# Now delete it.
		set ret [$dbc del 0]
		error_check_good dbc_del:FIRST $ret 0

		# Now current should fail
		set ret [$dbc get $key $DB_CURRENT]
		error_check_good dbc_get:deleted [llength $ret] 0

		# Now Prev should fail
		set ret [$dbc get $key $DB_PREV]
		error_check_good dbc_get:prev0 [llength $ret] 0

		# Now 10 nexts should work to get us in the middle
		for { set j 1 } { $j <= 10 } { incr j } {
			set ret [$dbc get $key $DB_NEXT]
			error_check_good dbc_get:next [llength $ret] 2
			error_check_good dbc_get:next [lindex $ret 1] $j$dupstr
		}

		puts "\tTest023.d: Delete middle and try gets"
		# Now do the delete on the current key.
		set ret [$dbc del 0]
		error_check_good dbc_del:10 $ret 0

		# Now current should fail
		set ret [$dbc get $key $DB_CURRENT]
		error_check_good dbc_get:deleted [llength $ret] 0

		# Prev and Next should work
		set ret [$dbc get $key $DB_NEXT]
		error_check_good dbc_get:next [llength $ret] 2
		error_check_good dbc_get:next [lindex $ret 1] 11$dupstr

		set ret [$dbc get $key $DB_PREV]
		error_check_good dbc_get:next [llength $ret] 2
		error_check_good dbc_get:next [lindex $ret 1] 9$dupstr

		# Now go to the last one
		for { set j 11 } { $j <= 19 } { incr j } {
			set ret [$dbc get $key $DB_NEXT]
			error_check_good dbc_get:next [llength $ret] 2
			error_check_good dbc_get:next [lindex $ret 1] $j$dupstr
		}

		puts "\tTest023.e: Delete last and try gets"
		# Now do the delete on the current key.
		set ret [$dbc del 0]
		error_check_good dbc_del:LAST $ret 0

		# Now current should fail
		set ret [$dbc get $key $DB_CURRENT]
		error_check_good dbc_get:deleted [llength $ret] 0

		# Next should fail
		set ret [$dbc get $key $DB_NEXT]
		error_check_good dbc_get:next19 [llength $ret] 0

		# Prev should work
		set ret [$dbc get $key $DB_PREV]
		error_check_good dbc_get:next [llength $ret] 2
		error_check_good dbc_get:next [lindex $ret 1] 18$dupstr


		# Now overwrite the current one, then count the number
		# of data items to make sure that we have the right number.

		puts "\tTest023.f: Count keys, overwrite current, count again"
		# At this point we should have 17 keys the (initial 20 minus
		# 3 deletes)
		set dbc2 [$db cursor 0]
		error_check_good db_cursor:2 [is_substr $dbc2 $db] 1

		set count_check 0
		for { set rec [$dbc2 get 0 $DB_FIRST] } {
		    [llength $rec] != 0 } { set rec [$dbc2 get 0 $DB_NEXT] } {
			incr count_check
		}
		error_check_good numdups $count_check 17

		set ret [$dbc put $key OVERWRITE $DB_CURRENT]
		error_check_good dbc_put:current $ret 0

		set count_check 0
		for { set rec [$dbc2 get 0 $DB_FIRST] } {
		    [llength $rec] != 0 } { set rec [$dbc2 get 0 $DB_NEXT] } {
			incr count_check
		}
		error_check_good numdups $count_check 17

		# Done, delete all the keys for next iteration
		error_check_good db_delete [$db del $txn $key 0] 0

		# database should be empty

		set ret [$dbc get 0 $DB_FIRST]
		error_check_good first_after_empty [llength $ret] 0
	}

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

}

# Check function for test023; keys and data are identical
proc test023.check { key data } {
	global dupnum
	global dupstr
	error_check_good "bad key" $key duplicate_val_test
	error_check_good "data mismatch for $key" $data $dupnum$dupstr
	incr dupnum
}

proc repeat { str n } {
	set ret ""
	while { $n > 0 } {
		set ret $str$ret
		incr n -1
	}
	return $ret
}
