#!/usr/bin/perl
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
# The Original Code is Mozilla Calendar Code.
#
# The Initial Developer of the Original Code is
# Christopher S. Charabaruk (ccharabaruk@meldstar.com).
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

# create a build id for calendar
@timevars = localtime(time);
$buildid  = sprintf("%4.4d%.2d%.2d%.2d%s", ($timevars[5] + 1900) , ($timevars[4] + 1) , $timevars[3] , $timevars[2] , "-cal");

#print our build id
print $buildid . "\n";

# for each filename in @ARGV...
foreach $file (@ARGV)
{
    # print filename
    print "Working on " . $file . "\n";

    # look for XX_DATE_XX and replace with $buildid
    # look for gDateMade=* and replace * with $buildid
    open(IN, $file) or die "cannot open $file for read\n";
    open(OUT,">" . $file . "-temp") or die "cannot open " . $file . "-temp for write\n";
    while(<IN>)
    {
        $line = $_;
        $line =~ s/\d{10}-cal/$buildid/;
        print OUT $line;
    }
    close(IN); close (OUT);

    # delete the old file, rename the new one to the old one
    if (unlink($file) == 1) { rename($file . "-temp", $file); }
    else { print "delete $file and rename " . $file . "-temp to $file\n"; }
}

print "All done!\n";
