#!/usr/sbin/perl
#
# stresstest1.pl
#
# Usage: > stresstest1.pl [-r]
#
#   -r  Turn on recurrence option.
# 
# Calls the randical.cgi to generate a random iCal file
# Then call the itiprig to run the itiprig/stressTest.script that
# loads this file.
#
# The itiprig/stressTest.script output is put in itiprig/stressTest.tmp.
# Then this perl scripts examines itiprig/stressTest.tmp.
# It checks for a line "Loaded [0-9]+ events" in the tmp file.
# If the number in that line is 0, (the event did not load properly)
# then the script writes to a file stressTest.log noting what event caused
# the failure.  Otherwise it writes OK and the number of events loaded in
# stressTest.log.
#
# This scripts makes sure the iCal Parser never breaks.
#

if ($#ARGV + 1 > 0) {
    if ($ARGV[0] eq "-r")
    {
	print STDOUT "Turning on recurrence option\n";
	$recurrenceOption = $ARGV[0];
    }
}

print STDOUT "Removing old ../itiprig/stressTest.log\n";
`rm -f ../itiprig/stressTest.log`;
while (1)
{
    #if WINDOWS
    # Remove old stressTestFile.txt
    `rm -f ../itiprig/stressTestFile.txt`;
    
    # Run the randical.cgi script and write the output to stressTestFile.txt
    open(OUT, "perl randical.cgi $recurrenceOption |") || die "Can't run perl randical.cgi\n";
    open(STTSTFILE, ">>../itiprig/stressTestFile.txt") || die "Can't write to ../itiprig/stressFile.txt\n";
    $eventBuf = "";
    while($line = <OUT>)
    {
	print STTSTFILE $line;
	$eventBuf .= $line;
    }
    close(OUT);
    close(STTSTFILE);

    # Call the itiprig to run the stressTest.script which will try to
    # load stressTestFile.txt and print out the event.
    # The script output is written to stressTest.tmp.
    chdir("../itiprig/winfe");
    `Debug/itiprig.exe ../stressTest.script`;
    print STDOUT "Checking if event loaded correctly ...";

    # Print the event to the log file if it failed to load properly
    # Hardcoded to look for events for now.  Do for looking for todos, journals
    # later.
    open(STTSTMPFILE, "../stressTest.tmp") || die "Can't open itiprig/stressTest.tmp\n";
    open(LOG, ">> ../stressTest.log") || die "Can't open itiprig/stressTest.log\n";

    $loadedEvents = -1;
    $events = -1;
    while($line = <STTSTMPFILE>)
    {
	$loadedEvents = index($line, ": Loaded ");
	$events = index($line, " events,");
	if ($loadedEvents > 0 && $events > 0)
	{
	    $startIndex = $loadedEvents + length(": Loaded ");
	    $lengthNum = $events - $startIndex;
	    $num = substr($line, $startIndex, $lengthNum);

	    if ($num eq "0")
	    {
		print LOG "> ($num) FAILED TO LOAD EVENT\n$eventBuf\n ---------------------------\n";
		break;
		print STDOUT " FAILED.\n";
	    }
	    else
	    {
		print LOG "> ($num) OK\n";
		print STDOUT " passed.\n";
	    }
	}
    }
    
    chdir("../../tools");
    #endif #if WINDOWS
}


