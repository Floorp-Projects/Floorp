# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test001.tcl	10.7 (Sleepycat) 4/26/98
#
# DB Test 1 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and data; retrieve each.
# After all are entered, retrieve all; compare output to original.
# Close file, reopen, do retrieve and re-verify.
proc test001 { method {nentries 10000} args } {
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test001: $method ($args) $nentries equal key/data pairs"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test001.db
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
		set checkfunc test001_recno.check
		set put putn
	} else {
		set checkfunc test001.check
		set put put
	}
	puts "\tTest001.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			global kvals

			set key [expr $count + 1]
			set kvals($key) $str
		} else {
			set key $str
		}
		set ret [$db $put $txn $key $str $flags]
		error_check_good put $ret 0

		set ret [$db get $txn $key $flags]
		error_check_good get $ret $str
		incr count
	}
	close $did
	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest001.b: dump file"
	dump_file $db $txn $t1 $checkfunc
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary (or ints)
	if { [string compare $method DB_RECNO] == 0 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $nentries} {set i [incr i]} {
			puts $oid $i
		}
		close $oid
		exec $MV $t1 $t3
	} else {
		set q q
		exec $SED $nentries$q $dict > $t2
		exec $SORT $t1 > $t3
	}

	error_check_good Test001:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	puts "\tTest001.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_file_direction $DB_FIRST $DB_NEXT
	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test001:diff($t2,$t3) \
	    [catch { exec $DIFF $t2 $t3 } res] 0

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest001.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test001:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test001; keys and data are identical
proc test001.check { key data } {
	error_check_good "key/data mismatch" $data $key
}

proc test001_recno.check { key data } {
global dict
global kvals
	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "key/data mismatch, key $key" $data $kvals($key)
}
