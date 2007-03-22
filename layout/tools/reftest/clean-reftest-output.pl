#!/usr/bin/perl
# vim: set shiftwidth=4 tabstop=8 autoindent expandtab:
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
# The Original Code is clean-reftest-report.pl.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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
    s,(PASS|FAIL):( \(!=\))? (.*),\1:\2 <a href="\3">\3</a>,;
    s,(IMAGE [^:]*): (data:.*),<a href="\2">\1</a>,;
    print;
    print "\n";
}

print <<EOM
</pre>
</body>
</html>
EOM
;
