# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test015.tcl	10.6 (Sleepycat) 4/26/98
#
# DB Test 15 {access method}
# Partial put test when item does not exist.
proc test015 { method {nentries 7500} { start 0 } args } {
global rvar
	set t_table {
		{ 1 { 1 1 1 } }
		{ 2 { 1 1 5 } }
		{ 3 { 1 1 50 } }
		{ 4 { 1 100 1 } }
		{ 5 { 100 1000 5 } }
		{ 6 { 1 100 50 } }
	}
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test015: $method ($args) $nentries equal key/data pairs, partial put test"
	test015_init
	if { $start == 0 } {
		set start { 1 2 3 4 5 6 }
	}
	foreach entry $t_table {
		set this [lindex $entry 0]
		if { [lsearch $start $this] == -1 } {
			continue
		}
		puts -nonewline "$this: "
		eval [concat test015_body $method [lindex $entry 1] \
		    $nentries $args]
	}
}

proc test015_init { } {
global rvar
	srand 0xf0f0f0f0
}

proc test015_body { method off_low off_hi rcount {nentries 10000} args } {
global rvar
global dvals
	set method [convert_method $method]
	puts "Put $rcount strings random offsets between $off_low and $off_hi"
	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test015.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE] \
	    0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set flags 0
	set txn 0
	set count 0

	puts "\tTest015.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	# Each put is a partial put of a record that does not exist.
	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set put putn
		} else {
			set key $str
			set put put
		}
		set data [replicate $str $rcount]
		# This is a hack.  In DB we will store the records with
		# some padding, but these will get lost if we just return
		# them in TCL.  As a result, we're going to have to hack
		# get to check for 0 padding and return a list consisting
		# of the number of 0's and the actual data.
		set off [ random_int $off_low $off_hi ]
		set dvals($key) [list $off $data]
		set ret [$db $put $txn $key $data $DB_DBT_PARTIAL $off 0]
		error_check_good put $ret 0
		incr count
	}
	close $did

	# Now make sure that everything looks OK
	puts "\tTest015.b: check entire file contents"
	dump_file $db $txn $t1 test015.check
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

	error_check_good Test015:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	puts "\tTest015.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile NULL $txn $t1 \
	    test015.check dump_file_direction $DB_FIRST $DB_NEXT

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test015:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest015.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 \
	    test015.check dump_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test015:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	unset dvals
}

# Check function for test015; keys and data are identical
proc test015.check { key data } {
global dvals
	error_check_good key"$key"_exists [info exists dvals($key)] 1
	error_check_good "mismatch on padding for key $key" $data $dvals($key)
	if { [llength $data] == 2 } {
		if { [lindex $data 0] != [lindex $dvals($key) 0] } {
			error "FAIL: Test015: padding mismatch for key $key \
			    Expected [lindex $dvals($key) 0] \
			    Got [lindex $data 0]"
		}
		if { [string compare [lindex $data 1] [lindex $dvals($key) 1] ]\
		    != 0 } {
			error "FAIL: Test015: data mismatch for key $key\
			    Expected [lindex $dvals($key) 1]\
			    Got [lindex $data 1]"
		}
	} elseif { [string compare $dvals($key) $data] != 0 } {
		if { [lindex $data 0] != [lindex $dvals($key) 0] } {
			error \
			    "FAIL: Test015: mismatch: expected |$dvals($keys)|\
			    \tGot |$data|"
		}
	}
}

