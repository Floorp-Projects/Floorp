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
# The Original Code is Mozilla Corportation code.
#
# The Initial Developer of the Original Code is Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert O'Callahan <robert@ocallahan.org>
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

# This script is a bunch of utilities for computing statistics about the textruns
# created during a Gecko run.
#
# Usage:
# 1) Uncomment #define DUMP_TEXT_RUNS in gfxAtsuiFonts.cpp
# 2) Build
# 3) Run over some test set, redirecting stdout to a file
# 4) Pipe that file through this script

# --exclude-spaces-only: ignore all textruns that consistent of zero or more
# spaces
my $exclude_spaces = grep(/^--exclude-spaces-only$/, @ARGV);

# --dump-runs: process textruns into a format that can be used by
# gfxTextRunPerfTest, print that on standard output, and do nothing else
my $dump_runs = grep(/^--dump-runs$/, @ARGV);

# --obfuscate: ROTL13 the textrun text
my $obfuscate = grep(/^--obfuscate$/, @ARGV);

my @textruns = ();

while (<STDIN>) {
  if (/^0x(\w+)\((.*)\) TEXTRUN "(.*)" ENDTEXTRUN$/) {
    my %tr = ( fontgroup => $1,
               families => $2,
               text => $3 );
    push(@textruns, \%tr);
  } elsif (/^0x(\w+)\((.*)\) TEXTRUN "(.*)$/) {
    my %tr = ( fontgroup => $1,
               families => $2 );
    my $text = $3."\n";
    while (<STDIN>) {
      if (/^(.*)" ENDTEXTRUN$/) {
        $text .= $1;
        last;
      }
      $text .= $_;
    }
    $tr{text} = $text;
    push(@textruns, \%tr);
  }
}

my %quote = ( "\\" => 1, "\"" => 1 );

sub quote_str {
    my ($text) = @_;
    my @chars = split(//, $text);
    my @strs = ();
    foreach my $c (@chars) {
      if (ord($c) >= 0x80) {
        $c = "\\x".sprintf("%x",ord($c)).'""';
      } elsif ($quote{$c}) {
        $c = "\\$c";
      } elsif ($c eq "\n") {
        $c = " ";
      }
      push(@strs, $c);
    }
    return '"'.join("", @strs).'"';
}

if ($dump_runs) {
  foreach my $tr (@textruns) {
    print "{ ", &quote_str($tr->{families}), ",\n";
    my $text = $tr->{text};
    if ($obfuscate) {
        $text =~ tr/a-mA-Mn-zN-Z/n-zN-Za-mA-M/;
    }
    print "  ", &quote_str($text), " },\n";
  }
  exit(0);
}

my %trs_by_text = ();
my %trs_by_text_and_fontgroup = ();
my %trs_by_trimmed_text_and_fontgroup = ();
my @tr_lengths = ();

$trs_by_text{" "} = [];
$trs_by_text{""} = [];

sub trim {
  my ($s) = @_;
  $s =~ s/^ *//g;
  $s =~ s/ *$//g;
  return $s;
}

my $total_textruns = 0;

foreach my $tr (@textruns) {
  if ($exclude_spaces && $tr->{text} =~ /^ *$/) {
    next;
  }
  ++$total_textruns;
  push(@{$trs_by_text{$tr->{text}}}, $tr);
  push(@{$trs_by_text_and_fontgroup{$tr->{fontgroup}.$tr->{text}}}, $tr);
  push(@{$trs_by_trimmed_text_and_fontgroup{$tr->{fontgroup}.&trim($tr->{text})}}, $tr);
  if (1 < scalar(@{$trs_by_trimmed_text_and_fontgroup{$tr->{fontgroup}.&trim($tr->{text})}})) {
    $tr_lengths[length($tr->{text})]++;
  }
}

print "Number of textruns:\t$total_textruns\n";
print "Number of textruns which are one space:\t", scalar(@{$trs_by_text{" "}}), "\n";
print "Number of textruns which are empty:\t", scalar(@{$trs_by_text{""}}), "\n";

my $count = 0;
foreach my $k (keys(%trs_by_text)) {
  if ($k =~ /^ *$/) {
    $count += @{$trs_by_text{$k}};
  }
}
print "Number of textruns which are zero or more spaces:\t$count\n";

print "Number of unique textruns by text and fontgroup:\t", scalar(keys(%trs_by_text_and_fontgroup)), "\n";
print "Number of unique textruns by trimmed text and fontgroup:\t", scalar(keys(%trs_by_trimmed_text_and_fontgroup)), "\n";

my $sum = 0;
my $weighted_sum = 0;
if (1) {
  print "Textrun length distribution:\n";
  for my $i (0..(scalar(@tr_lengths)-1)) {
    my $amount = defined($tr_lengths[$i])?$tr_lengths[$i]:0;
    $sum += $amount;
    $weighted_sum += $i*$amount;
    print "$i\t$sum\t$weighted_sum\n";
  }
}
