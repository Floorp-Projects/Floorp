#!perl
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Seawood <cls@seawood.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# Usage: $0 <includedir>
#
# Purpose:
#	Remove old header files by comparing list of headers generated 
#	on the fly against headers already existing under <includedir>
# 
# Assumptions:
# 	All directories under (and including) <includedir> have a list
#	of headers in the file .headerlist-<nameofsubdir>.
#

use strict;
use IO::Handle;

my $includedir = shift;

if (! -d $includedir) {
    exit(1);
}

purge("$includedir");

exit(0);

sub purge($) {
    my ($dirname) = @_;
    my ($file, $dir, $tmp, $listfile);
    my (@dirlist, @filelist, @masterlist, @removelist);
    my $ignoredir = 0;

    $listfile = "$dirname/.headerlist";

    #print "Inside purge: $dirname, $listfile\n";

    # Ignore files and just process subdirs if there's no master filelist
    $ignoredir = 1 if (! -e "$listfile" );

    # Create lists of current files and subdirectories
    my $SDIR = new IO::Handle;
    opendir($SDIR, "$dirname") || die "opendir($dirname): $!\n";
    while ($file = readdir($SDIR)) {
	next if ($file eq "." || $file eq ".." || $file =~ m/.headerlist/);
	if ( -d "$dirname/$file" ) {
	    push @dirlist, "$file";
	} else {
	    push @filelist, "$file" if (!$ignoredir);
	}
    }
    closedir($SDIR);

    if (!$ignoredir) {
	# Read in "master" file list
	undef @masterlist;
	my $MLIST = new IO::Handle;
	open($MLIST, "$listfile") || die "$listfile: $!\n";
	while ($tmp = <$MLIST>) {
	    chomp($tmp);
	    push @masterlist, "$tmp";
	}
	close($MLIST);

	# compare master list with read list
	undef @removelist;
	foreach $file (@filelist) {
	    push @removelist, $file if (!grep(/$file/, @masterlist));
	}
    }

    # Call purge recursively
    foreach $dir (@dirlist) {
	#print "purge(\"$dirname/$dir\") \n";
	purge("$dirname/$dir");
    }

    if (!$ignoredir) {
	# Remove files
	foreach $file (@removelist) {
	    print "purge old header: $dirname/$file\n";
	    unlink("$dirname/$file");
	}
    }

    #Unlink listfile now that we're done processing this dir
    unlink("$listfile") if (!$ignoredir);
    
}

