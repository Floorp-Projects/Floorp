#!perl
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

package ReportMod;
use Exporter;
use Socket;

@ISA = qw(Exporter);
@EXPORT = qw(sendReport);

#
# sendReport
#
# Connect to the remote host and give the results of the test run.
#
#
sub sendReport {

    local ($localhost,$remotehost, $port, $resultfile) = @_;

    openConnection(F, $remotehost, $port) || die ("Error connecting to the server.\n");
    
    $buffer = "b\t$localhost\n";
    syswrite(F, $buffer, length($buffer));
    
    $testRunID = <F>;
    print ("Test Run ID: $testRunID");
    
    open(REPORT,$resultfile) || die "Could not open file.\n";
    while (<REPORT>) {
	
	$testID = $_;
	$testClass = <REPORT>;
	$testResult = <REPORT>;
	$testLog = "";
	$logBuffer = "";

	$testBuffer = "p\t$testRunID\t$testID\t";

	# Parse the result string. 
	$loweredResult = lc $testResult;
	if ($loweredResult =~ /passed/) {
	    $testBuffer .= "p\n";
	} 
	
	elsif ($loweredResult =~ /failed/) {
	    $testBuffer .= "f\n";
	} 
	
	elsif ($loweredResult =~ /exception/) {
	    $testBuffer .= "u\n";
	}
	
	elsif ($loweredResult =~ /assertion/) {
	    $testBuffer .= "a\n";
	}
	
	else {
	    $testBuffer .= "c\n";
	}
    
	# Write the result to the socket.
	syswrite(F, $testBuffer, length($testBuffer));

	# Read the transaction ID from the server.
	$transID = <F>;
	print "Transaciton ID: $transID\n";

	# If the test didn't pass, get the result log.
	if (!($loweredResult =~ /passed/)) {
	    
	    $logBuffer = "n\t$transID\n";

	    $beginBuf = <REPORT>;
	    # Check if the log is correct.
	    if (!($beginBuf =~ /<BEGIN TEST LOG>/)) {
		die "Log is incorrect.\n";
	    }

	    while (1) {
		$endBuf = <REPORT>;
		if ($endBuf =~ /<END TEST LOG>/) {
		    last;
		} else {
		    $testLog .= $endBuf;
		}
	    }
	    
	    # Print the log into the socket.
	    $logBuffer .= "$testLog<ETB>\n";
	    syswrite(F, $logBuffer, length($logBuffer));
	    
	}

    }
    
    $buffer = "e\t$testRunID\n";
    syswrite(F, $buffer, length($buffer));
    
    # Give the server time to finish up before closing the socket.
    sleep(3);
    close(F);

}

#
# open Connection
#
# Open a socket to the remote host.
#
sub openConnection {

    my ($FS, $dest, $port) = @_;

    $AF_INET = 2;
    $SOCK_STREAM = 1;
    
    $sockaddr = 'S n a4 x8';
    
    ($name,$aliases,$proto) = getprotobyname('tcp');
    ($name,$aliases,$port) = getservbyname($port,'tcp')
	unless $port =~ /^\d+$/;
    ($name,$aliases,$type,$len,$thisaddr) =
	gethostbyname($hostname);
    ($name,$aliases,$type,$len,$thataddr) = gethostbyname($dest);
    
    $this = pack($sockaddr, $AF_INET, 0, $thisaddr);
    $that = pack($sockaddr, $AF_INET, $port, $thataddr);
    
    if (socket($FS, $AF_INET, $SOCK_STREAM, $proto)) { 
	print "socket ok\n";
    }
    else {
	die $!;
    }
    
    if (connect($FS,$that)) {
	print "connect ok\n";
    }
    else {
	die $!;
    }

    return 1;

}

1;


