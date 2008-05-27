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
# The Original Code is clean-reftest-report.pl.
#
# The Initial Developer of the Original Code is the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Vladimir Vukicevic <vladimir@pobox.com>
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

print <<EOD
<html>
<head>
<title>reftest output</title>
<style type="text/css">
/* must be in this order */
.PASS { background-color: green; }
.FAIL { background-color: red; }
.XFAIL { background-color: #999300; }
.WEIRDPASS { background-color: #00FFED; }
.PASSRANDOM { background-color: #598930; }
.FAILRANDOM, td.XFAILRANDOM { background-color: #99402A; }

.FAILIMAGES { }
img { margin: 5px; width: 80px; height: 100px; }
img.testresult { border: 2px solid red; }
img.testref { border: 2px solid green; }
a { color: inherit; }
.always { display: inline ! important; }
</style>
</head>
<body>
<p>
<span class="PASS always"><input type="checkbox" checked="true" onclick="var s = document.styleSheets[0].cssRules[0].style; if (s.display == 'none') s.display = null; else s.display = 'none';">PASS</span>&nbsp;
<span class="FAIL always"><input type="checkbox" checked="true" onclick="var s = document.styleSheets[0].cssRules[1].style; if (s.display == 'none') s.display = null; else s.display = 'none';">UNEXPECTED FAIL</span>&nbsp;
<span class="XFAIL always"><input type="checkbox" checked="true" onclick="var s = document.styleSheets[0].cssRules[2].style; if (s.display == 'none') s.display = null; else s.display = 'none';">KNOWN FAIL</span>&nbsp;
<span class="WEIRDPASS always"><input type="checkbox" checked="true" onclick="var s = document.styleSheets[0].cssRules[3].style; if (s.display == 'none') s.display = null; else s.display = 'none';">UNEXPECTED PASS</span>&nbsp;
<span class="PASSRANDOM always"><input type="checkbox" checked="true" onclick="var s = document.styleSheets[0].cssRules[4].style; if (s.display == 'none') s.display = null; else s.display = 'none';">PASS (Random)</span>&nbsp;
<span class="FAILRANDOM always"><input type="checkbox" checked="true" onclick="var s = document.styleSheets[0].cssRules[5].style; if (s.display == 'none') s.display = null; else s.display = 'none';">FAIL (Random)</span>&nbsp;
</p>
<table>
EOD
;

sub readcleanline {
  my $l = <>;
  chomp $l;
  chop $l if ($l =~ /\r$/);
  return $l;
}

sub do_html {
  my ($l) = @_;

  $l =~ s,(file:[^ ]*),<a href="\1">\1</a>,g;
  $l =~ s,(data:[^ ]*),<a href="\1">\1</a>,g;

  return $l;
}

$l = 0;

while (<>) {
  $l++;
  next unless /^REFTEST/;

  chomp;
  chop if /\r$/;

  s/^REFTEST *//;

  my $randomresult = 0;
  if (/RESULT EXPECTED TO BE RANDOM/) {
    s/\(RESULT EXPECTED TO BE RANDOM\) //;
    $randomresult = 1;
  }

  if (/^PASS(.*)$/) {
    my $class = $randomresult ? "PASSRANDOM" : "PASS";
    print '<tr><td class="' . $class . '">' . do_html($1) . "</td></tr>\n";
  } elsif (/^UNEXPECTED (....): (.*)$/) {
    if ($randomresult) {
      die "Error on line $l: UNEXPECTED with test marked random?!";
    }
    my $class = ($1 eq "PASS") ? "WEIRDPASS" : "FAIL";
    print '<tr><td class="' . $class . '">' . do_html($2) . "</td></tr>\n";

    # UNEXPECTED results can be followed by one or two images
    $testline = &readcleanline;

    print '<tr><td class="FAILIMAGES">';

    if ($testline =~ /REFTEST   IMAGE: (data:.*)$/) {
      print '<a href="' . $1 . '"><img class="testresult" src="' . $1 . '"></a>';
    } elsif ($testline =~ /REFTEST   IMAGE 1 \(TEST\): (data:.*)$/) {
      $refline = &readcleanline;
      print '<a href="' . $1 . '"><img class="testresult" src="' . $1 . '"></a>';
      {
        die "Error on line $l" unless $refline =~ /REFTEST   IMAGE 2 \(REFERENCE\): (data:.*)$/;
        print '<a href="' . $1 . '"><img class="testref" src="' . $1 . '"></a>';
      }

    } else {
      die "Error on line $l";
    }

    print "</td></tr>\n";
  } elsif (/^KNOWN FAIL(.*$)/) {
    my $class = $randomresult ? "XFAILRANDOM" : "XFAIL";
    print '<tr><td class="' . $class . '">' . do_html($1) . "</td></tr>\n";
  } else {
    print STDERR "Unknown Line: " . $_ . "\n";
    print "<tr><td><pre>" . $_ . "</pre></td></tr>\n";
  }
}

print <<EOD
</table>
</body>
</html>
EOD
;
