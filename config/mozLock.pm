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
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2001 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#	Christopher Seawood <cls@seawood.org>

package mozLock;

use strict;

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
my $locklimit = 60;
my $locksleep = 1;

sub mozLock($) {
    my ($lockfile) = @_;
    $lockcounter = 0;
    while ( -e $lockfile && $lockcounter < $locklimit) {
	$lockcounter++;
	sleep(1);
    }
    die "$0: Could not get lockfile $lockfile.\nRemove $lockfile to clear up\n" if ($lockcounter >= $locklimit);
    #print "LOCK: $lockfile\n";
    open(LOCK, ">$lockfile") || die "$lockfile: $!\n";
}

sub mozUnlock($) {
    my ($lockfile) = @_;
    #print "UNLOCK: $lockfile\n";
    unlink($lockfile);
}

END {};

1;
