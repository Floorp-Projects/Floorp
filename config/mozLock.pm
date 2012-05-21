#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
