# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test004.tcl	10.5 (Sleepycat) 4/10/98
#
# DB Test 4 {access method}
# Check that cursor operations work.  Create a database.
# Read through the database sequentially using cursors and
# delete each element.
proc test004 { method {nentries 10000} {reopen 4} {build_only 0} args} {
	set tnum Test00$reopen
	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts -nonewline "$tnum: $method ($args) $nentries delete small key; medium data pairs"
	if {$reopen == 5} {
		puts "(with close)"
	} else {
		puts ""
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test004.db
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

	# Here is the loop where we put and get each key/data pair

	set kvals ""
	puts "\tTest00$reopen.a: put/get loop"
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set put putn
			lappend kvals $str
		} else {
			set key $str
			set put put
		}

		set datastr [ make_data_str $str ]

		$db $put $txn $key $datastr $flags
		set ret [$db get $txn $key $flags]
		error_check_good "$tnum:put" $ret $datastr
		incr count
	}
	close $did
	if { $build_only == 1 } {
		return $db
	}
	if { $reopen == 5 } {
		error_check_good db_close [$db close] 0
		set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
		error_check_good dbopen [is_valid_db $db] TRUE
	}
	puts "\tTest00$reopen.b: get/delete loop"
	# Now we will get each key from the DB and compare the results
	# to the original, then delete it.
	set outf [open $t1 w]
	set c [$db cursor $txn]

	set count 0
	for {set d [$c get 0 $DB_FIRST] } { [string length $d] != 0 } {
	    set d [$c get 0 $DB_NEXT] } {
		set k [lindex $d 0]
		set d2 [lindex $d 1]
		if { [string compare $method DB_RECNO] == 0 } {
			set datastr \
			    [make_data_str [lindex $kvals [expr $k - 1]]]
		} else {
			set datastr [make_data_str $k]
		}
		error_check_good $tnum:$k $d2 $datastr
		puts $outf $k
		$c del 0
		if { [string compare $method DB_RECNO] == 0 && \
			$do_renumber == 1 } {
			set kvals [lreplace $kvals 0 0]
		}
		incr count
	}
	close $outf
	error_check_good curs_close [$c close] 0

	# Now compare the keys to see if they match the dictionary
	if { [string compare $method DB_RECNO] == 0 } {
		error_check_good test00$reopen:keys_deleted $count $nentries
	} else {
		set q q
		exec $SED $nentries$q $dict > $t2
		exec $SORT $t1 > $t3
		error_check_good Test00$reopen:diff($t3,$t2) \
		    [catch { exec $DIFF $t3 $t2 } res] 0
	}

	error_check_good db_close [$db close] 0
}

