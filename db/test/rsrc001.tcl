# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)rsrc001.tcl	8.9 (Sleepycat) 5/26/98
#
# Recno backing file test.
# Try different patterns of adding records and making sure that the
# corresponding file matches
proc rsrc001 { } {
	puts "Rsrc001: Basic recno backing file writeback tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile rsrc001.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir

	# Create the starting files
	set oid1 [open $testdir/rsrc.txt w]
	set oid2 [open $testdir/check.txt w]
	puts $oid1 "This is record 1"
	puts $oid2 "This is record 1"
	puts $oid1 "This is record 2 This is record 2"
	puts $oid2 "This is record 2 This is record 2"
	puts $oid1 "This is record 3 This is record 3 This is record 3"
	puts $oid2 "This is record 3 This is record 3 This is record 3"
	close $oid1
	close $oid2

	sanitize_textfile $testdir/rsrc.txt
	sanitize_textfile $testdir/check.txt

	puts "Rsrc001.a: Read file, rewrite last record; write it out and diff"
	set db [dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 DB_RECNO\
		-recsrc rsrc.txt]
	error_check_bad dbopen $db NULL
	error_check_good dbopen [is_substr $db db] 1

	# Read the last record; replace it (but we won't change it).
	# Then close the file and diff the two files.
	set txn 0
	set dbc [$db cursor $txn]
	error_check_good db_cursor [is_substr $dbc $db] 1

	set rec [$dbc get 0 $DB_LAST]
	error_check_good get_last [llength $rec] 2
	set key [lindex $rec 0]
	set data [lindex $rec 1]

	# Get the last record from the text file
	set oid [open $testdir/rsrc.txt]
	set laststr ""
	while { [gets $oid str] != -1 } {
		set laststr $str
	}
	close $oid
	error_check_good getlast $data $laststr

	set ret [$db put0 $txn $key $data 0]
	error_check_good replace_last $ret 0

	error_check_good curs_close [$dbc close] 0
	error_check_good db_sync [$db sync 0] 0
	error_check_good Rsrc001:diff($testdir/rsrc.txt,$testdir/check.txt) \
	    [catch { exec $DIFF $testdir/rsrc.txt $testdir/check.txt } res] 0

	puts "Rsrc001.b: Append some records in tree and verify in file."
	set oid [open $testdir/check.txt a]
	for {set i 1} {$i < 10} {incr i} {
		set rec [replicate "New Record $i" $i]
		puts $oid $rec
		incr key
		set ret [$db put0 $txn 0 $rec $DB_APPEND]
		error_check_good put_append $ret $key
	}
	error_check_good db_sync [$db sync 0] 0
	close $oid
	sanitize_textfile $testdir/check.txt
	set ret [catch { exec $DIFF $testdir/rsrc.txt $testdir/check.txt } res]
	error_check_good Rsrc001:diff($testdir/{rsrc.txt,check.txt}) $ret 0

	puts "Rsrc001.c: Append by record number"
	set oid [open $testdir/check.txt a]
	for {set i 1} {$i < 10} {incr i} {
		set rec [replicate "New Record (set 2) $i" $i]
		puts $oid $rec
		incr key
		set ret [$db put0 $txn $key $rec 0]
		error_check_good put_byno $ret 0
	}

	error_check_good db_sync [$db sync 0] 0
	close $oid
	sanitize_textfile $testdir/check.txt
	set ret [catch { exec $DIFF $testdir/rsrc.txt $testdir/check.txt } res]
	error_check_good Rsrc001:diff($testdir/{rsrc.txt,check.txt}) $ret 0

	puts "Rsrc001.d: Put beyond end of file."
	set oid [open $testdir/check.txt a]
	for {set i 1} {$i < 10} {incr i} {
		puts $oid ""
		incr key
	}
	set rec "Last Record"
	puts $oid $rec
	incr key
	set ret [$db put0 $txn $key $rec 0]
	error_check_good put_byno $ret 0

	error_check_good db_sync [$db sync 0] 0
	close $oid
	sanitize_textfile $testdir/check.txt
	set ret [catch { exec $DIFF $testdir/rsrc.txt $testdir/check.txt } res]
	error_check_good Rsrc001:diff($testdir/{rsrc.txt,check.txt}) $ret 0
}

# convert CR/LF to just LF.
# Needed on Windows when a file is created as text but read as binary.
proc sanitize_textfile { filename } {
	global is_windows_test
	source ./include.tcl

	if { $is_windows_test == 1 } {
		set TR "c:/mksnt/tr.exe"
		catch { exec $TR -d '\015' <$filename > $testdir/nonl.tmp } res
		catch { exec $MV $testdir/nonl.tmp $filename } res
	}
}
