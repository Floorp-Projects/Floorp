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

package BuildMod;
use Exporter;
use Cwd;

@ISA = qw(Exporter);
@EXPORT = qw(checkArgs);

#
# build
# 
# Builds all modules specified in the build file.
#
sub build {

    local @args = @_;

    # Global variables.
    $buildFile = "";
    $buildTitle = "";
    $buildTarget = "";		# Dbg or Opt.
    $buildPlatform = "";	# WIN32, UNIX, WIN16...
    $buildName = "";		# $buildPlatform + $buildTarget

    $buildLog = "";		# $buildPlatform + $buildTarget + "Log.txt";
    $buildStatus = 0;           # Build flag.
    $buildStatusStr = "success";
    $startTime = "";
    
    &parseArgs(@args);
   
    # Open the build file.
    open BUILDFILE, $buildFile || die ("Cannot open build file.\n");

    # Read in the build headers.
    $buildTitle = <BUILDFILE>;
    $buildPlatform = <BUILDFILE>;
    $buildTarget = <BUILDFILE>;

    # Remove \n, if any,
    $buildTitle =~ s/\n$//;
    $buildPlatform =~ s/\n$//;
    $buildTarget =~ s/\n$//;

    # Init variables.
    $buildName = "${buildPlatform} ${buildTarget}";
    $buildLog = "${buildPlatform}_${buildTarget}Log.txt";
    $startTime = time - 60 * 10;

    # Inform Tinderbox that we are starting a build.
    &beginTinderbox;
    
    # Open the build log.
    open(LOG, ">${buildLog}") || die "Cannot open build log.\n";

    # Read from the build file and do build.
    while ($strBuf = <BUILDFILE>) {

	#Remove any \n.
	$strBuf =~ s/\n//;

	# Check if the command is to change to a directory.
	$loweredBuf = lc $strBuf;
	if ($loweredBuf =~ /^cd /) {

	    $dir = $strBuf;
	    $dir =~ s/^cd\s+//;
	    print "Changing to $dir\n";
	    if (!chdir("$dir")) {
		print LOG "Cannot change to directory $dir\n";
		$buildStatus = 1;
	    }
	    
	} else {      

	    print "$strBuf 2>&1 |\n";
	    print LOG "$strBuf 2>&1 |\n";
	    open(COMMAND,"$strBuf 2>&1 |");

	    # Tee the output
	    while( <COMMAND> ){
		print $_;
		print LOG $_;
	    }
	    close(COMMAND);
	    $buildStatus |= $?;

	}

    }	    

    # Check if the build is busted or sucessful.
    $buildStatusStr = ($buildStatus ? 'busted' : 'success');
    print("Build Status: $buildStatusStr\n");

    # Finish the log file and send it to tinderbox.
    &endTinderbox(LOG);

    # Rename the log file.
    rename("${buildLog}", "${buildPlatform}${buildTarget}.last");

    # Return 1 if build was sucessful, else return 0;
    if ($buildStatusStr == 'success') {
	return 1;
    } else {
	return 0;
    }
    
}


#
# checkArgs
#
# Checks to see if there are command line arguments.
# Returns true(1) or false(0).
#
sub checkArgs {

    local @args = @_;

    # print("Number of args: $#args\n");

    if ($#args == -1) {
	return 0;
    }

    return 1;

}

#
# parseArgs
#
# Go through the argument list and set the matching global
# variables.
#
sub parseArgs {

    local @args = @_;

    # The first argument should the build file.
    $buildFile = $args[0];

    # Check if the file exits.
    if (!(-e $buildFile)) {
	die "Build file does not exist in the current directory.\n";
    }

    # Go through the rest of the arguments.
    print("Args: ");
    $i = 0;
    while( $i < @args ){

	print("$args[$i]", "\n");

	$i++;

	# Ignore the rest of the arguments.

    }
    print("\n");

    return;
    
}

#
# beginTinderbox
#
# Sends mail to the Tinderbox daemon,
# that a build is about to start.
#
sub beginTinderbox {

    print "Telling Tinderbox that we are starting a build.\n";

    open( LOG, ">>logfile" );
    print LOG "\n";
    print LOG "tinderbox: tree: $buildTitle\n";
    print LOG "tinderbox: builddate: $startTime\n";
    print LOG "tinderbox: status: building\n";
    print LOG "tinderbox: build: $buildName\n";

    if ($buildPlatform =~ /WIN32/) {
	print LOG "tinderbox: errorparser: windows\n";
	print LOG "tinderbox: buildfamily: windows\n";
    } else {
	
	# ***** Need to add other platform specific info. *****
	print LOG "tinderbox: errorparser: unknown\n";
	print LOG "tinderbox: buildfamily: unknown\n";

    }

    print LOG "\n";
    close( LOG );

    # Send the logfile to Tinderbox.
    if ($buildPlatform =~ /WIN32/) {
	print "Sending logfile.\n";
	system("$nstools\\bin\\blat logfile -t tinderbox-daemon\@warp" );
    } else {

	# ***** Need to add other platform specific mailing methods. *****
	die "Don't know how to send on $buildPlatform\n";
    }

    return;

}

#
# endTinderbox
#
# Sends mail to the Tinderbox daemon,
# that a build is done.
#
sub endTinderbox {

    my($LOG) = @_;

    print "Telling Tinderbox that the build is done.\n";
    print LOG "tinderbox: tree: $buildTitle\n";
    print LOG "tinderbox: builddate: $startTime\n";
    print LOG "tinderbox: status: $buildStatusStr\n";
    print LOG "tinderbox: build: $buildName\n";
    print LOG "tinderbox: errorparser: windows\n";
    print LOG "tinderbox: buildfamily: windows\n";

    close LOG;

    # Inform Tinderbox that the build is done.
    system("$nstools\\bin\\blat $buildLog -t tinderbox-daemon\@warp" );

    return;

}


