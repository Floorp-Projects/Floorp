#! /usr/local/bin/perl
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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

require('coreconf.pl');

#######-- read in variables on command line into %var

$var{ZIP} = "zip";

&parse_argv;

 
######-- Do the packaging of jars.

foreach $jarfile (split(/ /,$var{FILES}) ) {
    print STDERR "---------------------------------------------\n";
    print STDERR "Packaging jar file $jarfile....\n";

    $jarinfo = $var{$jarfile};

    ($jardir,$jaropts) = split(/\|/,$jarinfo);

    $zipoptions = "";
    if ($jaropts =~ /a/) {
	if ($var{OS_ARCH} eq 'WINNT') {
	    $zipoptions = '-ll';
	}
    }


# just in case the directory ends in a /, remove it
    if ($jardir =~ /\/$/) {
	chop $jardir;
    }

    $dirdepth --;
    
    print STDERR "jardir = $jardir\n";
    system("ls $jardir");

    if (-d $jardir) {


# count the number of slashes

	$slashes =0;
	
	foreach $i (split(//,$jardir)) {
	    if ($i =~ /\//) {
		$slashes++;
	    }
	}

	$dotdots =0;
	
	foreach $i (split(m|/|,$jardir)) {
	    if ($i eq '..') {
		$dotdots ++;
	    }
	}

	$dirdepth = ($slashes +1) - (2*$dotdots);

	print STDERR "changing dir $jardir\n";
	chdir($jardir);
	print STDERR "making dir META-INF\n";
	mkdir("META-INF",0755);

	$filelist = "";
	opendir(DIR,".");
	while ($_ = readdir(DIR)) {
	    if (! ( ($_ eq '.') || ($_ eq '..'))) {
		if ( $jaropts =~ /i/) {
		    if (! /^include$/) {
			$filelist .= "$_ ";
		    }
		}
		else {
		    $filelist .= "$_ ";
		}
	    }
	}
	closedir(DIR);	

	print STDERR "zip $zipoptions -r $jarfile $filelist\n";
	system("zip $zipoptions -r $jarfile $filelist");
	rmdir("META-INF");
	    for $i (1 .. $dirdepth) {
	    chdir("..");
	    print STDERR "chdir ..\n";
	}
    }
    else {
        print STDERR "Directory $jardir doesn't exist\n";
    }

}

