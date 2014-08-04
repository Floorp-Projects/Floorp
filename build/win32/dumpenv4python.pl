# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# See build/autoconf/hooks.m4

use Data::Dumper;

$Data::Dumper::Terse = 1;
$Data::Dumper::Indent = 0;

# We can't use perl hashes because Mozilla-Build's perl is 5.6, and perl
# 5.6's Data::Dumper doesn't have Pair to change ' => ' into ' : '.
@data = (
  ['env', [map { [$_, $ENV{$_}] } keys %ENV]],
  ['args', \@ARGV],
);

print Dumper \@data;
