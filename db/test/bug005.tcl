proc bug005 { method } {
	source ./include.tcl
	set method [convert_method $method]
	set flags [expr $DB_CREATE | $DB_INIT_LOCK | \
		$DB_INIT_LOG | $DB_INIT_MPOOL | $DB_INIT_TXN]
	set env [dbenv -dbhome $testdir -dbflags $flags]
	error_check_good env [is_substr $env env] 1

	set db [dbopen foo.db \
	    [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method -flags $DB_DUP \
	    -dbenv $env]
	error_check_good dbopen [is_substr $db db] 1

	set tmgr [txn "" 0 0 -dbenv $env]
	error_check_good txnopen [is_substr $tmgr mgr] 1

	set t [$tmgr begin]
	error_check_good txn_begin [is_substr $t $tmgr] 1

	puts "Bug005.a: Adding 10 duplicates"
	# Add a bunch of dups
	for { set i 0 } { $i < 10 } {incr i} {
		set ret [$db put $t doghouse $i"DUPLICATE_DATA_VALUE" 0]
		error_check_good db_put $ret 0
	}

	puts "Bug005.b: Adding key after duplicates"
	# Now add one more key/data AFTER the dup set.
	set ret [$db put $t zebrahouse NOT_A_DUP 0]
	error_check_good db_put $ret 0

	error_check_good txn_commit [$t commit] 0

	set t [$tmgr begin]
	error_check_good txn_begin [is_substr $t $tmgr] 1

	# Now delete everything
	puts "Bug005.c: Deleting duplicated key"
	set ret [$db del $t doghouse 0]
	error_check_good del $ret 0

	# Now reput everything
	set pad \
	    abcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuv

	puts "Bug005.d: Reputting duplicates with big data vals"
	for { set i 0 } { $i < 10 } {incr i} {
		set ret [$db put $t doghouse $i"DUPLICATE_DATA_VALUE"$pad 0]
		error_check_good db_put $ret 0
	}
	error_check_good txn_commit [$t commit] 0

	# Check duplicates for order
	set t [$tmgr begin]
	error_check_good txn_begin [is_substr $t $tmgr] 1
	set dbc [$db cursor $t]
	error_check_good dbcursor [is_substr $dbc $db] 1


	puts "Bug005.e: Verifying that duplicates are in order."
	set i 0
	for { set ret [$dbc get doghouse $DB_SET] } {$i < 10 && \
	    [llength $ret] != 0} { set ret [$dbc get 0 $DB_NEXT] } {
		set data [lindex $ret 1]
		error_check_good duplicate_value $data \
		    $i"DUPLICATE_DATA_VALUE"$pad
		incr i
	}

	$db close
	$tmgr close
	rename $env ""
	exec $RM $testdir/foo.db
}
