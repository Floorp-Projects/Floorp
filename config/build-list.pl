#!env perl

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

#
# A generic script to add entries to a file 
# if the entry does not already exist
# 
# Usage: $0 <filename> <entry>

use Fcntl qw(:DEFAULT :flock);

sub usage() {
    print "$0 <filename> <entry>\n";
    exit(1);
}

if ($#ARGV != 1) {
    usage();
}

$file = shift;
$entry = shift;

# touch the file if it doesn't exist
if ( ! -e "$file") {
    $now = time;
    utime $now, $now, $file;
}

# This needs to be atomic
open(OUT, ">>$file") || die ("$file: $!\n");
flock(OUT, LOCK_EX);
system("grep -c '^$entry\$' $file >/dev/null");
$exit_value = $? >> 8;
if ($exit_value) {
    print OUT "$entry\n";
}
flock(OUT, LOCK_UN);
close(OUT);

exit(0);
