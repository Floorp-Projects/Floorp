#!/usr/bin/perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
  if (/EXPECTED RANDOM/) {
    s/\(EXPECTED RANDOM\)//;
    $randomresult = 1;
  }

  if (/^TEST-PASS \| (.*)$/) {
    my $class = $randomresult ? "PASSRANDOM" : "PASS";
    print '<tr><td class="' . $class . '">' . do_html($1) . "</td></tr>\n";
  } elsif (/^TEST-UNEXPECTED-(....) \| (.*)$/) {
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
  } elsif (/^TEST-KNOWN-FAIL \| (.*$)/) {
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
