# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test028.tcl	8.7 (Sleepycat) 6/2/98
#
# Put after cursor delete test.
proc test028 { method args } {
	global dupnum
	global dupstr
	global alphabet
	set omethod $method
	set method [convert_method $method]
	puts "Test028: $method put after cursor delete test"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	if { [is_rbtree $omethod] == 1 } {
		puts "Test0028 skipping for method $omethod"
		return
	}
	if { [string compare $method DB_RECNO] == 0 } {
		set args [add_to_args 0 $args]
		set key 10
	} else {
		set args [add_to_args $DB_DUP $args]
		set key "put_after_cursor_del"
	}


	# Create the database and open the dictionary
	set testfile test028.db
	set t1 $testdir/t1
	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set ndups 20
	set txn 0
	set flags 0

	set dbc [$db cursor $txn]
	error_check_good db_cursor [is_substr $dbc $db] 1

	foreach i { offpage onpage } {
		foreach b { bigitem smallitem } {
			if { $i == "onpage" } {
				if { $b == "bigitem" } {
					set dupstr [repeat $alphabet 100]
				} else {
					set dupstr DUP
				}
			} else {
				if { $b == "bigitem" } {
					set dupstr [repeat $alphabet 100]
				} else {
					set dupstr [repeat $alphabet 50]
				}
			}

			if { $b == "bigitem" } {
				set dupstr [repeat $dupstr 10]
			}
			puts "\tTest028: $i/$b"

			puts "\tTest028.a: Insert key with single data item"
			set ret [$db put $txn $key $dupstr $flags]
			error_check_good db_put $ret 0

			# Now let's get the item and make sure its OK.
			puts "\tTest028.b: Check initial entry"
			set ret [$db get $txn $key $flags]
			error_check_good db_get $ret $dupstr

			# Now try a put with NOOVERWRITE SET (should be error)
			puts "\tTest028.c: No_overwrite test"
			set ret [$db put $txn $key $dupstr $DB_NOOVERWRITE]
			error_check_bad db_put $ret 0

			# Now delete the item with a cursor
			puts "\tTest028.d: Delete test"
			set ret [$dbc get $key $DB_SET]
			error_check_bad dbc_get:SET [llength $ret] 0

			set ret [$dbc del $flags]
			error_check_good dbc_del $ret 0

			puts "\tTest028.e: Reput the item"
			set ret [$db put $txn $key $dupstr $DB_NOOVERWRITE]
			error_check_good db_put $ret 0

			puts "\tTest028.f: Retrieve the item"
			set ret [$db get $txn $key $flags]
			error_check_good db_get $ret $dupstr

			# Delete the key to set up for next test
			set ret [$db del $txn $key $flags]
			error_check_good db_del $ret 0

			# Now repeat the above set of tests with
			# duplicates (if not RECNO).
			if { [string compare $method DB_RECNO] == 0 } {
				continue;
			}

			puts "\tTest028.g: Insert key with duplicates"
			for { set count 0 } { $count < $ndups } { incr count } {
				set ret [$db put $txn $key $count$dupstr $flags]
				error_check_good db_put $ret 0
			}

			puts "\tTest028.h: Check dups"
			set dupnum 0
			dump_file $db $txn $t1 test028.check

			# Try no_overwrite
			puts "\tTest028.i: No_overwrite test"
			set ret [$db put $txn $key $dupstr $DB_NOOVERWRITE]
			error_check_bad db_put $ret 0

			# Now delete all the elements with a cursor
			puts "\tTest028.j: Cursor Deletes"
			set count 0
			for { set ret [$dbc get $key $DB_SET] } {
			    [string length $ret] != 0 } {
			    set ret [$dbc get 0 $DB_NEXT] } {
				set k [lindex $ret 0]
				set d [lindex $ret 1]
				error_check_good db_seq(key) $k $key
				error_check_good db_seq(data) $d $count$dupstr
				set ret [$dbc del 0]
				error_check_good dbc_del $ret 0
				incr count
				if { $count == [expr $ndups - 1] } {
					puts "\tTest028.k:\
						Duplicate No_Overwrite test"
					set ret [$db put $txn $key $dupstr \
					    $DB_NOOVERWRITE]
					error_check_bad db_put $ret 0
				}
			}

			# Make sure all the items are gone
			puts "\tTest028.l: Get after delete"
			set ret [$dbc get $key $DB_SET]
			error_check_good get_after_del [string length $ret] 0

			puts "\tTest028.m: Reput the item"
			set ret [$db put $txn $key 0$dupstr $DB_NOOVERWRITE]
			error_check_good db_put $ret 0
			for { set count 1 } { $count < $ndups } { incr count } {
				set ret [$db put $txn $key $count$dupstr $flags]
				error_check_good db_put $ret 0
			}

			puts "\tTest028.n: Retrieve the item"
			set dupnum 0
			dump_file $db $txn $t1 test028.check

			# Clean out in prep for next test
			set ret [$db del $txn $key 0]
			error_check_good db_del $ret 0
		}
	}

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

}

# Check function for test028; keys and data are identical
proc test028.check { key data } {
	global dupnum
	global dupstr
	error_check_good "Bad key" $key put_after_cursor_del
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
