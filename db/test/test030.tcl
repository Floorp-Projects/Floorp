# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test030.tcl	10.6 (Sleepycat) 4/10/98
#
# DB Test 30 Run the random db tester on the specified access method.
# Options are:
# -adds <maximum number of keys before you disable adds>
# -cursors <number of cursors>
# -dataavg <average data size>
# -dups <allow duplicates in file>
# -delete <minimum number of keys before you disable deletes>
# -errpct <Induce errors errpct of the time>
# -init <initial number of entries in database>
# -keyavg <average key size>
# -seed <Use the specified seed>

proc test030 { method {nops 10000} args } {
source ./include.tcl
set usage "\t-adds <maximum number of keys before you disable adds>\
\t-cursors <number of cursors>\
\t-dataavg <average data size>\
\t-dups <allow duplicates in file>\
\t-delete <minimum number of keys before you disable deletes>\
\t-errpct <Induce errors errpct of the time>\
\t-init <initial number of entries in database>\
\t-keyavg <average key size>\
\t-seed <Use the specified seed>"

	set method [convert_method $method]
	if { [string compare $method DB_RECNO] == 0 } {
		puts "Test$reopen skipping for method RECNO"
		return
	}
	puts "Test030: Random tester on $method for $nops operations"
	# Set initial parameters
	set adds 100000
	set cursors 5
	set dataavg 40
	set delete 10000
	set dups 0
	set errpct 0
	set init 0
	set keyavg 25
	set seed -1

	# Process parameters
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-a.*  { incr i; set adds [lindex $args $i] }
			-c.*  { incr i; set cursors [lindex $args $i] }
			-da.* { incr i; set dataavg [lindex $args $i] }
			-de.* { incr i; set delete [lindex $args $i] }
			-du.* { incr i; set dups [lindex $args $i] }
			-e.*  { incr i; set errpct [lindex $args $i] }
			-i.*  { incr i; set init [lindex $args $i] }
			-k.*  { incr i; set keyavg [lindex $args $i] }
			-s.*  { incr i; set seed [lindex $args $i] }
			default {
				puts $usage
				return
			}
		}
	}

	# Create the database and and initialize it.
	set root $testdir/test030
	set f $root.db
	cleanup $testdir

	# Run the script with 3 times the number of initial elements to
	# set it up.
	set db [dbopen $f [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method]
	error_check_good dbopen:$f [is_valid_db $db] TRUE

	set r [$db close]
	error_check_good dbclose:$f $r 0

	# We redirect standard out, but leave standard error here so we
	# can see errors.

	puts "\tTest030.a: Initializing database"
	if { $init != 0 } {
		set n [expr 3 * $init]
		exec ./dbtest ../test/dbscript.tcl $f $n 1 $init $n $keyavg \
		    $dataavg $dups 0 -1 > $testdir/test030.init
	}

	puts "\tTest030.b: Now firing off random dbscript, running: "
	# Now the database is initialized, run a test
	puts "./dbtest ../test/dbscript.tcl $f $nops $cursors $delete $adds \
	    $keyavg $dataavg $dups $errpct $seed > $testdir/test030.log"
	exec ./dbtest ../test/dbscript.tcl $f $nops $cursors $delete $adds \
	    $keyavg $dataavg $dups $errpct $seed > $testdir/test030.log
}
