#!/usr/bin/perl -w
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is Christopher Blizzard.
# Portions created by Christopher Blizzard are Copyright (C)
# Christopher Blizzard.  All Rights Reserved.
#
# Contributor(s):

# This script will read one of the mozilla packages- file on unix and
# copy it to a target directory.  It's for unix only and is really
# designed for use in building rpms or other packages.

use Getopt::Long;
use File::Find;

use strict;

# global vars
my $install_dir  = "";
my $install_root = "";
my $package_name = "";
my $package_file = "";
my $output_file  = "";
my $exclude_file = "";
my $retval;

# std return val

$retval = GetOptions('install-dir=s',  \$install_dir,
		     'install-root=s', \$install_root,
		     'package=s',      \$package_name,
		     'package-file=s', \$package_file,
		     'output-file=s',  \$output_file,
                     'exclude-file=s', \$exclude_file);

# make sure that all of the values are specific on the command line
if (!$retval || !$install_dir || !$install_root || !$package_name || 
    !$package_file || !$output_file) {
    print_usage();
    exit 1;
}

# try to open the packages file

open (PACKAGE_FILE, $package_file) || die("$0: Failed to open file $package_file for reading.");

print "chdir to $install_dir\n";
chdir($install_dir);

my @file_list;
my @exclude_list;
my @final_file_list;
my $reading_package = 0;

LINE: while (<PACKAGE_FILE>) {
    s/\;.*//;			# it's a comment, kill it.
    s/^\s+//;			# nuke leading whitespace
    s/\s+$//;			# nuke trailing whitespace
    
    # it's a blank line, skip it.
    if (/^$/) {
	next LINE;
    }

    # it's a new component
    if (/^\[/) {
	my $this_package;
	( $this_package ) = /^\[(.+)\]$/;
	if ($this_package eq $package_name) {
	    $reading_package = 1;
	}
	else {
	    $reading_package = 0;
	}
	next LINE;
    }

    # read this line
    if ($reading_package) {
	# see if it's a deletion
	if (/^-/) {
	    my $this_file;
	    ( $this_file ) = /^-(.+)$/;
	    push (@exclude_list, $this_file);
	}
	else {
	    push (@file_list, $_);
	}
    }
}

close PACKAGE_FILE;

# check if we have an exclude file

if ($exclude_file) {
    print "reading exclude file $exclude_file\n";

    open (EXCLUDE_FILE, $exclude_file) || die("$0: Failed to open exclude file $exclude_file for reading.");

    while (<EXCLUDE_FILE>) {
	chomp;
	print "Ignoring $_\n";
	push (@exclude_list, $_);
    }

    close EXCLUDE_FILE;
}

# Expand our file list

expand_file_list(\@file_list, \@exclude_list, \@final_file_list);

print "final file list\n";
foreach (@final_file_list) {
    print $_ . "\n";
}

open (OUTPUT_FILE, ">>$output_file") || die("Failed to open output file\n");
foreach (@final_file_list) {
    # strip off the bin/
    s/^bin\///;

    if ( ! -f $_ ) {
	print("Skipping $_ because it doesn't exist\n");
    }
    else {
	print ("Adding $_\n");
	print (OUTPUT_FILE $install_root . "/" . $_ . "\n");
    }
}
close OUTPUT_FILE;

#print "\nexlude list\n";
#foreach (@exclude_list) {
#    print $_ . "\n";
#}

# this function expands a list of files

sub expand_file_list {
    my $file_list_ref = shift;
    my $exclude_list_ref = shift;
    my $final_file_list_ref = shift;
    my $this_file;
    foreach $this_file (@{$file_list_ref}) {
	# strip off the bin/
	$this_file =~ s/^bin\///;

	# is it a wild card?
	if ($this_file =~ /\*/) {
	    print "Wild card $this_file\n";
	    # expand that wild card, removing anything in the exclude
	    # list
	    my @temp_list;
	    printf ("Expanding $this_file\n");
	    @temp_list = glob($this_file);
	    foreach $this_file (@temp_list) {
		if (!in_exclude_list($this_file, $exclude_list_ref)) {
		    push (@{$final_file_list_ref}, $this_file);
		}
	    }
	}
	else {
	    if (!in_exclude_list($this_file, $exclude_list_ref)) {
		push (@{$final_file_list_ref}, $this_file);
	    }
	}
    }
}

# is this file in the exlude list?

sub in_exclude_list {
    my $file = shift;
    my $exclude_list_ref = shift;
    my $this_file;
    foreach $this_file (@{$exclude_list_ref}) {
	if ($file eq $this_file) {
	    return 1;
	}
    }
    return 0;
}

# print out a usage message

sub print_usage {
    print ("$0: --install-dir dir --install-root dir --package name --package-file file --output-file file\n");
    print ("\t install-dir is the directory where the files are installed.\n");
    print ("\t install-root is the directory that should prefix files in the package file.\n");
    print ("\t package is the name of the package to list\n");
    print ("\t package-file is the file that contains the list of packages\n");
    print ("\t output-file is the file which will contain the list of files\n");
}
