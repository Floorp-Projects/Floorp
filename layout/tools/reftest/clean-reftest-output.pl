#!/usr/bin/perl
# vim: set shiftwidth=4 tabstop=8 autoindent expandtab:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is intended to be run over the standard output of a
# reftest run.  It will extract the parts of the output run relevant to
# reftest and HTML-ize the URLs.

use strict;

print <<EOM
<html>
<head>
<title>reftest output</title>
</head>
<body>
<pre>
EOM
;

while (<>) {
    next unless /REFTEST/;
    chomp;
    chop if /\r$/;
    s,(TEST-)([^\|]*) \| ([^\|]*) \|(.*),\1\2: <a href="\3">\3</a>\4,;
    s,(IMAGE[^:]*): (data:.*),<a href="\2">\1</a>,;
    print;
    print "\n";
}

print <<EOM
</pre>
</body>
</html>
EOM
;
