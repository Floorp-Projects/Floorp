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
#   Christopher Seawood <cls@seawood.org>
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

package mozLock;

use strict;
use IO::File;
use Cwd;

BEGIN {
    use Exporter ();
    use vars qw ($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

    $VERSION = 1.00;
    @ISA = qw(Exporter);
    @EXPORT = qw(&mozLock &mozUnlock);
    %EXPORT_TAGS = ( );
    @EXPORT_OK = qw();
}

my $lockcounter = 0;
my $locklimit = 100;
my $locksleep = 0.1;
my %lockhash;

# File::Spec->rel2abs appears to be broken in ActiveState Perl 5.22
# so roll our own
sub priv_abspath($) {
    my ($file) = @_;
    my ($dir, $out);
    my (@inlist, @outlist);

    # Force files to have unix paths.
    $file =~ s/\\/\//g;

    # Check if file is already absolute
    if ($file =~ m/^\// || substr($file, 1, 1) eq ':') {
	return $file;
    }
    $out = cwd . "/$file";

    # Do what File::Spec->canonpath should do
    @inlist = split(/\//, $out);
    foreach $dir (@inlist) {
	if ($dir eq '..') {
	    pop @outlist;
	} else {
	    push @outlist, $dir;
	}
    }
    $out = join '/',@outlist;
    return $out;
}

sub mozLock($) {
    my ($inlockfile) = @_;
    my ($lockhandle, $lockfile);
    $lockfile = priv_abspath($inlockfile);
    #print "LOCK: $lockfile\n";
    $lockcounter = 0;
    $lockhandle = new IO::File || die "Could not create filehandle for $lockfile: $!\n";    
    while ($lockcounter < $locklimit) {
	if (! -e $lockfile) {
	    open($lockhandle, ">$lockfile") || die "$lockfile: $!\n";
	    $lockhash{$lockfile} = $lockhandle;
	    last;
	}
	$lockcounter++;
	select(undef,undef,undef, $locksleep);
    }
    if ($lockcounter >= $locklimit) {
	undef $lockhandle;
	die "$0: Could not get lockfile $lockfile.\nRemove $lockfile to clear up\n";
    }
}

sub mozUnlock($) {
    my ($inlockfile) = @_;
    my ($lockhandle, $lockfile);
    #$lockfile = File::Spec->rel2abs($inlockfile);
    $lockfile = priv_abspath($inlockfile);
    #print "UNLOCK: $lockfile\n";
    $lockhandle = $lockhash{$lockfile};
    if (defined($lockhandle)) {
	close($lockhandle);
	$lockhash{$lockfile} = undef;
	unlink($lockfile);
    } else {
	print "WARNING: $0: lockhandle for $lockfile not defined.  Lock may not be removed.\n";
    }
}

END {};

1;
