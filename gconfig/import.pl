#! /usr/local/bin/perl
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

print STDERR "import.pl\n";

require('gconfig.pl');


$returncode =0;


#######-- read in variables on command line into %var

$var{ZIP} = "zip";
$var{UNZIP} = "unzip -o";

&parse_argv;

if (! ($var{IMPORTS} =~ /\w/)) {
    print STDERR "nothing to import\n";
}

######-- Do the import!

foreach $import (split(/ /,$var{IMPORTS}) ) {

    print STDERR "\n\nIMPORTING .... $import\n-----------------------------\n";


# if a specific version specified in IMPORT variable
# (if $import has a slash in it)

    if ($import =~ /\//) {
        # $component=everything before the first slash of $import

	$import =~ m|^([^/]*)/|; 
	$component = $1;

	$import =~ m|^(.*)/([^/]*)$|;

	# $path=everything before the last slash of $import
	$path = $1;

	# $version=everything after the last slash of $import
	$version = $2;

	if ($var{VERSION} ne "current") {
	    $version = $var{VERSION};
	}
    }
    else {
	$component = $import;
	$path = $import;
	$version = $var{VERSION};
    }

    $releasejardir = "$var{RELEASE_TREE}/$path";
    if ($version eq "current") {
	print STDERR "Current version specified. Reading 'current' file ... \n";
	
	open(CURRENT,"$releasejardir/current") || die "NO CURRENT FILE\n";
	$version = <CURRENT>;
	$version =~ s/(\r?\n)*$//; # remove any trailing [CR/]LF's
	close(CURRENT);
	print STDERR "Using version $version\n";
	if ( $version eq "") {
	    die "Current version file empty. Stopping\n";
	}
    }
    
    $releasejardir = "$releasejardir/$version";
    if ( ! -d $releasejardir) {
	die "$releasejardir doesn't exist (Invalid Version?)\n";
    }
    foreach $jarfile (split(/ /,$var{FILES})) {

	($relpath,$distpath,$options) = split(/\|/, $var{$jarfile});

	if ($var{'OVERRIDE_IMPORT_CHECK'} eq 'YES') {
	    $options =~ s/v//g;
	}

	if ( $relpath ne "") { $releasejarpathname = "$releasejardir/$relpath";}
	else { $releasejarpathname = $releasejardir; }
	
	if (-e "$releasejarpathname/$jarfile") {
	    print STDERR "\nWorking on jarfile: $jarfile\n";
	    
	    if ($distpath =~ m|/$|) {
		$distpathname = "$distpath$component";
	    }
	    else {
		$distpathname = "$distpath"; 
	    }
	  
	  
#the block below is used to determine whether or not the xp headers have
#already been imported for this component

	    $doimport = 1;
	  if ($options =~ /v/) {   # if we should check the imported version
	      print STDERR "Checking if version file exists $distpathname/version\n";
	      if (-e "$distpathname/version") {
		  open( VFILE, "<$distpathname/version") ||
		      die "Cannot open $distpathname/version for reading. Permissions?\n";
		  $importversion = <VFILE>;
		  close (VFILE);
		  $importversion =~ s/\r?\n$//;   # Strip off any trailing CR/LF
		  if ($version eq $importversion) {
		      print STDERR "$distpathname version '$importversion' already imported. Skipping...\n";
		      $doimport =0;
		  }
	      }
	  }
	  
	  if ($doimport == 1) {
	      if (! -d "$distpathname") {
		  &rec_mkdir("$distpathname");
	      }
	      # delete the stuff in there already.
	      # (this should really be recursive delete.)
	      
	      if ($options =~ /v/) {
		  $remheader = "\nREMOVING files in '$distpathname/' :";
		  opendir(DIR,"$distpathname") ||
		      die ("Cannot read directory $distpathname\n");
		  @filelist = readdir(DIR);
		  closedir(DIR);
		  foreach $file ( @filelist ) {
		      if (! ($file =~ m!/.?.$!) ) {
			  if (! (-d $file)) {
			      $file =~ m!([^/]*)$!;
			      print STDERR "$remheader $1";
			      $remheader = " ";
			      unlink "$distpathname/$file";
			  }
		      }
		  }
	      }


	      print STDERR "\n\n";

	      print STDERR "\nExtracting jarfile '$jarfile' to local directory $distpathname/\n";
	      
	      print STDERR "$var{UNZIP} $releasejarpathname/$jarfile -d $distpathname\n";
	      system("$var{UNZIP} $releasejarpathname/$jarfile -d $distpathname");
	      
	      $r = $?;

	      if ($options =~ /v/) {
		  if ($r == 0) {
		      unlink ("$distpathname/version");
		      if (open(VFILE,">$distpathname/version")) {
			  print VFILE "$version\n";
			  close(VFILE);
		      }
		  }
		  else {
		      print STDERR "Could not create '$distpathname/version'. Permissions?\n";
		      $returncode ++;
		  }
	      }
	  }  # if (doimport)
	} # if (-e releasejarpathname/jarfile)
    } # foreach jarfile)
} # foreach IMPORT



exit($returncode);





