# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test019.tcl	10.5 (Sleepycat) 4/10/98
#
# Test019 { access_method nentries }
# Test the partial get functionality.
proc test019 { method {nentries 10000} args } {
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test019: $method ($args) $nentries partial get test"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test019.db
	cleanup $testdir

	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]
	srand [pid]

	set flags 0
	set txn 0
	set count 0

	puts "\tTest019.a: put/get loop"
	for { set i 0 } {  [gets $did str] != -1 && $i < $nentries } \
	    { incr i } {

		if { $method == "DB_RECNO" } {
			set key [expr $i + 1]
		} else {
			set key $str
		}
		set repl [random_int 1 100]
		set data [replicate $str $repl]
		set ret [$db put $txn $key $data $DB_NOOVERWRITE]
		error_check_good dbput:$key $ret 0

		set ret [$db get $txn $key 0]
		error_check_good dbget:$key $ret $data
		set kvals($key) $repl

	}
	close $did

	puts "\tTest019.b partial get loop"
	set did [open $dict]
	for { set i 0 } {  [gets $did str] != -1 && $i < $nentries } \
	    { incr i } {
		if { $method == "DB_RECNO" } {
			set key [expr $i + 1]
		} else {
			set key $str
		}
		set realdata [replicate $str $kvals($key)]
		set maxndx [expr [string length $realdata] - 1]
		set beg [random_int 0 [expr $maxndx - 1]]
		set len [random_int 1 [expr $maxndx - $beg]]

		set ret [$db get $txn $key $DB_DBT_PARTIAL $beg $len]
		# In order for tcl to handle this, we have to overwrite the
		# last character with a NULL.  That makes the length one
		# less than we expect
		error_check_good dbget:$key $ret \
		    [string range $realdata $beg [expr $beg + $len - 2]]
	}
	error_check_good db_close [$db close] 0
}
