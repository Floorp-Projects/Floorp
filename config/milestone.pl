#!/usr/bin/perl -w
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
# The Original Code is the Win32 Version System.
#
# The Initial Developer of the Original Code is Netscape Communications Corporation
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

use Getopt::Long;

use strict;
use vars qw(
            $TOPSRCDIR
            $MILESTONE_FILE
            $MILESTONE
            $MILESTONE_NUM
            @MILESTONE_PARTS
            $MILESTONE_BUILD
            $MILESTONE_QUALIFIER
            $opt_getms
            $opt_debug
            $opt_help
            );

&GetOptions('topsrcdir=s' => \$TOPSRCDIR, 'getms', 'debug', 'help');

if (defined($opt_help)) {
    &usage();
    exit;
}

$MILESTONE_FILE  = "$TOPSRCDIR/config/milestone.txt";
@MILESTONE_PARTS = (0, 0, 0, 0);
$MILESTONE_QUALIFIER = "";

#
# Grab milestone (top line of $MILESTONE_FILE that starts with a digit)
#
open(FILE,"$MILESTONE_FILE") ||
  die ("Can't open $MILESTONE_FILE for reading!");
$MILESTONE = <FILE>;
while($MILESTONE =~ /^\s*#/ || $MILESTONE !~ /^\d/) {
    $MILESTONE = <FILE>;
}
close(FILE);
chomp($MILESTONE);
$MILESTONE_NUM = $MILESTONE;

#
# Split the milestone into parts (major, minor, minor2, minor3, ...)
#
if ($MILESTONE =~ /\D$/) {    # for things like 0.9.9+, strip off the +
    $MILESTONE_QUALIFIER = $MILESTONE;
    $MILESTONE_QUALIFIER =~ s/.*\d//;
    $MILESTONE_NUM =~ s/\D*$//;
}
if ($MILESTONE_NUM =~ /[^0-9\.]/) {
    die(<<END
Illegal milestone $MILESTONE in $MILESTONE_FILE!
A proper milestone would look like

        x.x.x
        x.x.x.x
        x.x.x+
END
    );
}
@MILESTONE_PARTS = split(/\./, $MILESTONE_NUM);

if ($opt_debug) {
    print ("MS $MILESTONE MSNUM $MILESTONE_NUM MSQ $MILESTONE_QUALIFIER MS_PARTS @MILESTONE_PARTS\n");
}

if ($opt_getms && !$MILESTONE_QUALIFIER) {
    print "$MILESTONE";
    exit;
}

# TODO
# Later on I'll add options to update *all* hardcoded versions in
# the source tree...
# probably have a file listing all files that need to be changed,
# and replace them with templates that have __MOZ_MAJOR_VERSION__
# or whatever in place of 0, __MOZ_MINOR_VERSION__ instead of 9,
# etc., given the right options.

sub usage() {
    print <<END
`milestone.pl [--topsrcdir TOPSRCDIR] --getms`  # will output \$MILESTONE if no '+'
END
    ;
}
