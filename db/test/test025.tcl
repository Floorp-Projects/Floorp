# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test025.tcl	8.4 (Sleepycat) 4/26/98
#
# DB Test 25 {method nentries}
# Test the DB_APPEND flag.

proc test025 { method {nentries 10000} args} {
global kvals
	set args [convert_args $method $args]
	set method [convert_method $method]
	set args [convert_args $method $args]
	puts "Test025: $method ($args)"

	if { [string compare $method DB_BTREE] == 0 } {
		puts "Test025 skipping for method BTREE"
		return
	}
	if { [string compare $method DB_HASH] == 0 } {
		puts "Test025 skipping for method HASH"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test025.db
	set t1 $testdir/t1

	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	puts "\tTest025.a: put/get loop"
	set txn 0
	set flags $DB_APPEND
	set checkfunc test025_check


	# Here is the loop where we put and get each key/data pair
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		set k [expr $count + 1]
		set kvals($k) $str
		set recno [$db put $txn 0 $str $flags]
		error_check_good db_put $recno [expr $count + 1]

		set ret [$db get $txn $recno 0]
		error_check_good "get $recno" $ret $str
		incr count
	}
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest025.b: dump file"
	dump_file $db $txn $t1 $checkfunc
	error_check_good db_close [$db close] 0

	puts "\tTest025.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_file_direction $DB_FIRST $DB_NEXT

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest001.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_file_direction $DB_LAST $DB_PREV

}

proc test025_check { key data } {
global kvals
	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good " key/data mismatch for |$key|" $data $kvals($key)
}
