# Make sure that if we do PREVs on a log, but the beginning of the
# log has been truncated, we do the right thing.
proc bug006 { } {
source ./include.tcl
	puts "Bug006: Prev on log when beginning of log has been truncated."
	# Use archive test to populate log
	cleanup $testdir
	archive

	# Delete all log files under 50
	set ret [catch { glob $testdir/log.000* } result]
	if { $ret == 0 } {
		eval exec $RM -rf $result
	}

	# Now open the log and get the first record and try a prev
	set lp [log "" 0 0]
	error_check_good log_open [is_substr $lp log] 1

	set ret [$lp get "0 0" $DB_FIRST]
	error_check_bad log_get [string length $ret] 0

	# This should give  DB_NOTFOUND which is a ret of length 0
	set ret [$lp get "0 0" $DB_PREV]
	error_check_good log_get_prev [string length $ret] 0

	error_check_good log_close [$lp close] 0
}
