# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test016.tcl	10.6 (Sleepycat) 4/26/98
#
# DB Test 16 {access method}
# Partial put test where partial puts make the record smaller.
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and a fixed, medium length data string;
# retrieve each. After all are entered, go back and do partial puts,
# replacing a random-length string with the key value.
# Then verify.

set datastr abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz

proc test016 { method {nentries 10000} args } {
global datastr
global dvals
	srand 0xf0f0f0f0
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test016: $method ($args) $nentries partial put shorten"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test016.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set flags 0
	set txn 0
	set count 0

	# Here is the loop where we put and get each key/data pair

	puts "\tTest016.a: put/get loop"
	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set put putn
		} else {
			set key $str
			set put put
		}
		$db $put $txn $key $datastr $flags
		set ret [$db get $txn $key $flags]
		if { [string compare $ret $datastr] != 0 } {
			puts "Test016: put $datastr got $ret"
			return
		}
		incr count
	}
	close $did

	# Next we will do a partial put replacement, making the data
	# shorter
	puts "\tTest016.b: partial put loop"
	set did [open $dict]
	set count 0
	set len [string length $datastr]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}

		set repl_len [random_int [string length $key] $len]
		set repl_off [random_int 0 [expr $len - $repl_len] ]
		set s1 [string range $datastr 0 [ expr $repl_off - 1] ]
		set s2 [string toupper $key]
		set s3 [string range $datastr [expr $repl_off + $repl_len] end ]
		set dvals($key) $s1$s2$s3
		set ret [ $db $put $txn $key $s2 $DB_DBT_PARTIAL \
		    $repl_off $repl_len ]
		error_check_good put $ret 0
		set ret [$db get $txn $key $flags]
		error_check_good put $ret $s1$s2$s3
		incr count
	}
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest016.c: dump file"
	dump_file $db $txn $t1 test016.check
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary
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

	error_check_good Test016:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again.
	puts "\tTest016.d: close, open, and dump file"
	open_and_dump_file $testfile NULL $txn $t1 test016.check \
	    dump_file_direction $DB_FIRST $DB_NEXT

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}
	error_check_good Test016:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest016.e: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 test016.check \
	    dump_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}
	error_check_good Test016:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test016; data should be whatever is set in dvals
proc test016.check { key data } {
global datastr
global dvals
	error_check_good key"$key"_exists [info exists dvals($key)] 1
	error_check_good "data mismatch for key $key" $data $dvals($key)
}
