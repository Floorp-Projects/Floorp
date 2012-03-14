#!/usr/bin/env perl

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
# The Original Code is Mozilla Foundation code.
#
# The Initial Developer of the Original Code is Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009-2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Kew <jfkthame@gmail.com>
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

# This tool is used to prepare lookup tables of Unicode character properties
# needed by gfx code to support text shaping operations. The properties are
# read from the Unicode Character Database and compiled into multi-level arrays
# for efficient lookup.
#
# To regenerate the tables in nsUnicodePropertyData.cpp:
#
# (1) Download the current Unicode data files from
#
#         http://www.unicode.org/Public/UNIDATA/
#
#     NB: not all the files are actually needed; currently, we require
#       - UnicodeData.txt
#       - Scripts.txt
#       - EastAsianWidth.txt
#       - BidiMirroring.txt
#       - HangulSyllableType.txt
#       - ReadMe.txt (to record version/date of the UCD)
#     though this may change if we find a need for additional properties.
#
#     The Unicode data files should be together in a single directory.
#
# (2) Run this tool using a command line of the form
#
#         perl genUnicodePropertyData.pl \
#                 /path/to/hb-common.h   \
#                 /path/to/UCD-directory
#
#     (where hb-common.h is found in the gfx/harfbuzz/src directory).
#
#     This will generate (or overwrite!) the files
#
#         nsUnicodePropertyData.cpp
#         nsUnicodeScriptCodes.h
#
#     in the current directory.

use strict;

if ($#ARGV != 1) {
    print <<__EOT;
# Run this tool using a command line of the form
#
#     perl genUnicodePropertyData.pl \
#             /path/to/hb-common.h   \
#             /path/to/UCD-directory
#
# where hb-common.h is currently found in the gfx/harfbuzz/src directory,
# and UCD-directory is a directory containing the current Unicode Character
# Database files (UnicodeData.txt, etc), available from
# http://www.unicode.org/Public/UNIDATA/
#
# This will generate (or overwrite!) the files
#
#     nsUnicodePropertyData.cpp
#     nsUnicodeScriptCodes.h
#
# in the current directory.
__EOT
    exit 0;
}

# load HB_Script and HB_Category constants

# NOTE that HB_SCRIPT_* constants are now "tag" values, NOT sequentially-allocated
# script codes as used by Glib/Pango/etc.
# We therefore define a set of MOZ_SCRIPT_* constants that are script _codes_
# compatible with those libraries, and map these to HB_SCRIPT_* _tags_ as needed.

my $sc = -1;
my $cc = -1;
my %scriptCode;
my %catCode;
my @scriptCodeToTag;
my @scriptCodeToName;

open FH, "< $ARGV[0]" or die "can't open $ARGV[0] (should be header file hb-common.h)\n";
while (<FH>) {
    if (m/HB_SCRIPT_([A-Z_]+)\s*=\s*HB_TAG\s*\(('.','.','.','.')\)\s*,/) {
        $scriptCodeToTag[++$sc] = $2;
        $scriptCodeToName[$sc] = $1;
        $scriptCode{$1} = $sc;
    }
    if (m/HB_UNICODE_GENERAL_CATEGORY_([A-Z_]+)/) {
        $cc++;
        $catCode{$1} = $cc;
    }
}
close FH;

# initialize default properties
my @script;
my @category;
my @combining;
my @eaw;
my @mirror;
my @hangul;
my @casemap;
for (my $i = 0; $i < 0x110000; ++$i) {
    $script[$i] = $scriptCode{"UNKNOWN"};
    $category[$i] = $catCode{"UNASSIGNED"};
    $combining[$i] = 0;
    $casemap[$i] = 0;
}

my %ucd2hb = (
'Cc' => 'CONTROL',
'Cf' => 'FORMAT',
'Cn' => 'UNASSIGNED',
'Co' => 'PRIVATE_USE',
'Cs' => 'SURROGATE',
'Ll' => 'LOWERCASE_LETTER',
'Lm' => 'MODIFIER_LETTER',
'Lo' => 'OTHER_LETTER',
'Lt' => 'TITLECASE_LETTER',
'Lu' => 'UPPERCASE_LETTER',
'Mc' => 'SPACING_MARK',
'Me' => 'ENCLOSING_MARK',
'Mn' => 'NON_SPACING_MARK',
'Nd' => 'DECIMAL_NUMBER',
'Nl' => 'LETTER_NUMBER',
'No' => 'OTHER_NUMBER',
'Pc' => 'CONNECT_PUNCTUATION',
'Pd' => 'DASH_PUNCTUATION',
'Pe' => 'CLOSE_PUNCTUATION',
'Pf' => 'FINAL_PUNCTUATION',
'Pi' => 'INITIAL_PUNCTUATION',
'Po' => 'OTHER_PUNCTUATION',
'Ps' => 'OPEN_PUNCTUATION',
'Sc' => 'CURRENCY_SYMBOL',
'Sk' => 'MODIFIER_SYMBOL',
'Sm' => 'MATH_SYMBOL',
'So' => 'OTHER_SYMBOL',
'Zl' => 'LINE_SEPARATOR',
'Zp' => 'PARAGRAPH_SEPARATOR',
'Zs' => 'SPACE_SEPARATOR'
);

# read ReadMe.txt
my @versionInfo;
open FH, "< $ARGV[1]/ReadMe.txt" or die "can't open Unicode ReadMe.txt file\n";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
}
close FH;

my $kTitleToUpper = 0x80000000;
my $kUpperToLower = 0x40000000;
my $kLowerToTitle = 0x20000000;
my $kLowerToUpper = 0x10000000;
my $kCaseMapCharMask = 0x001fffff;

# read UnicodeData.txt
open FH, "< $ARGV[1]/UnicodeData.txt" or die "can't open UCD file UnicodeData.txt\n";
while (<FH>) {
    chomp;
    my @fields = split /;/;
    if ($fields[1] =~ /First/) {
        my $first = hex "0x$fields[0]";
        $_ = <FH>;
        @fields = split /;/;
        if ($fields[1] =~ /Last/) {
            my $last = hex "0x$fields[0]";
            do {
                $category[$first] = $catCode{$ucd2hb{$fields[2]}};
                $combining[$first] = $fields[3];
                $first++;
            } while ($first <= $last);
        } else {
            die "didn't find Last code for range!\n";
        }
    } else {
        my $usv = hex "0x$fields[0]";
        $category[$usv] = $catCode{$ucd2hb{$fields[2]}};
        $combining[$usv] = $fields[3];
        my $upper = hex $fields[12];
        my $lower = hex $fields[13];
        my $title = hex $fields[14];
        # we only store one mapping for each character,
        # but also record what kind of mapping it is
        if ($upper && $lower) {
            $casemap[$usv] |= $kTitleToUpper;
            $casemap[$usv] |= ($usv ^ $upper);
        }
        elsif ($lower) {
            $casemap[$usv] |= $kUpperToLower;
            $casemap[$usv] |= ($usv ^ $lower);
        }
        elsif ($title && ($title != $upper)) {
            $casemap[$usv] |= $kLowerToTitle;
            $casemap[$usv] |= ($usv ^ $title);
        }
        elsif ($upper) {
            $casemap[$usv] |= $kLowerToUpper;
            $casemap[$usv] |= ($usv ^ $upper);
        }
    }
}
close FH;

# read Scripts.txt
open FH, "< $ARGV[1]/Scripts.txt" or die "can't open UCD file Scripts.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s+;\s+([^ ]+)/) {
        my $script = uc($3);
        warn "unknown script $script" unless exists $scriptCode{$script};
        $script = $scriptCode{$script};
        my $start = hex "0x$1";
        my $end = (defined $2) ? hex "0x$2" : $start;
        for (my $i = $start; $i <= $end; ++$i) {
            $script[$i] = $script;
        }
    }
}
close FH;

# read EastAsianWidth.txt
my %eawCode = (
  'A' => 0, #         ; Ambiguous
  'F' => 1, #         ; Fullwidth
  'H' => 2, #         ; Halfwidth
  'N' => 3, #         ; Neutral
  'NA'=> 4, #         ; Narrow
  'W' => 5  #         ; Wide 
);
open FH, "< $ARGV[1]/EastAsianWidth.txt" or die "can't open UCD file EastAsianWidth.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    s/#.*//;
    if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s*;\s*([^ ]+)/) {
        my $eaw = uc($3);
        warn "unknown EAW code $eaw" unless exists $eawCode{$eaw};
        $eaw = $eawCode{$eaw};
        my $start = hex "0x$1";
        my $end = (defined $2) ? hex "0x$2" : $start;
        for (my $i = $start; $i <= $end; ++$i) {
            $eaw[$i] = $eaw;
        }
    }
}
close FH;

# read BidiMirroring.txt
my @distantMirrors = ();
my $smallMirrorOffset = 64;
open FH, "< $ARGV[1]/BidiMirroring.txt" or die "can't open UCD file BidiMirroring.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    s/#.*//;
    if (m/([0-9A-F]{4,6});\s*([0-9A-F]{4,6})/) {
        my $mirrorOffset = hex("0x$2") - hex("0x$1");
        if ($mirrorOffset < $smallMirrorOffset && $mirrorOffset >= -128) {
            $mirror[hex "0x$1"] = $mirrorOffset;
        } else {
            die "too many distant mirror codes\n" if scalar @distantMirrors == 128 - $smallMirrorOffset;
            $mirror[hex "0x$1"] = $smallMirrorOffset + scalar @distantMirrors;
            push @distantMirrors, hex("0x$2");
        }
    }
}
close FH;

# read HangulSyllableType.txt
my %hangulType = (
  'L'   => 0x01,
  'V'   => 0x02,
  'T'   => 0x04,
  'LV'  => 0x03,
  'LVT' => 0x07
);
open FH, "< $ARGV[1]/HangulSyllableType.txt" or die "can't open UCD file HangulSyllableType.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    s/#.*//;
    if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s*;\s*([^ ]+)/) {
        my $hangul = uc($3);
        warn "unknown Hangul syllable type" unless exists $hangulType{$hangul};
        $hangul = $hangulType{$hangul};
        my $start = hex "0x$1";
        my $end = (defined $2) ? hex "0x$2" : $start;
        for (my $i = $start; $i <= $end; ++$i) {
            $hangul[$i] = $hangul;
        }
    }
}
close FH;

my $timestamp = gmtime();

open DATA_TABLES, "> nsUnicodePropertyData.cpp" or die "unable to open nsUnicodePropertyData.cpp for output";

my $licenseBlock = q[
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * Derived from the Unicode Character Database by genUnicodePropertyData.pl
 *
 * For Unicode terms of use, see http://www.unicode.org/terms_of_use.html
 *
 * ***** END LICENSE BLOCK ***** */
];

my $versionInfo = join("\n", @versionInfo);

print DATA_TABLES <<__END;
$licenseBlock
/*
 * Created on $timestamp from UCD data files with version info:
 *

$versionInfo

 *
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */

#include "mozilla/StandardInteger.h"
#include "harfbuzz/hb-common.h"

__END

print DATA_TABLES "static const PRUint32 sScriptCodeToTag[] = {\n";
for (my $i = 0; $i < scalar @scriptCodeToTag; ++$i) {
  printf DATA_TABLES "  HB_TAG(%s)", $scriptCodeToTag[$i];
  print DATA_TABLES $i < $#scriptCodeToTag ? ",\n" : "\n";
}
print DATA_TABLES "};\n\n";

sub sprintScript
{
  my $usv = shift;
  return sprintf("%d,", $script[$usv]);
}
&genTables("Script", "PRUint8", 10, 6, \&sprintScript, 16);

sub sprintCC
{
  my $usv = shift;
  return sprintf("%d,", $combining[$usv]);
}
&genTables("CClass", "PRUint8", 10, 6, \&sprintCC, 1);

print DATA_TABLES "static const PRInt32 kSmallMirrorOffset = $smallMirrorOffset;\n";
print DATA_TABLES "static const PRUint16 sDistantMirrors[] = {\n";
for (my $i = 0; $i < scalar @distantMirrors; ++$i) {
    printf DATA_TABLES "  0x%04X", $distantMirrors[$i];
    print DATA_TABLES $i < $#distantMirrors ? ",\n" : "\n";
}
print DATA_TABLES "};\n\n";

sub sprintMirror
{
  my $usv = shift;
  return sprintf("%d,", $mirror[$usv]);
}
&genTables("Mirror", "PRInt8", 9, 7, \&sprintMirror, 0);

sub sprintCatEAW
{
  my $usv = shift;
  return sprintf("{%d,%d},", $eaw[$usv], $category[$usv]);
}
&genTables("CatEAW", "struct {\n  unsigned char mEAW:3;\n  unsigned char mCategory:5;\n}",
           9, 7, \&sprintCatEAW, 16);

sub sprintHangulType
{
  my $usv = shift;
  return sprintf("%d,", $hangul[$usv]);
}
&genTables("Hangul", "PRUint8", 10, 6, \&sprintHangulType, 0);

sub sprintCasemap
{
  my $usv = shift;
  return sprintf("0x%08x,", $casemap[$usv]);
}
&genTables("CaseMap", "PRUint32", 11, 5, \&sprintCasemap, 1);

printf DATA_TABLES "const PRUint32 kTitleToUpper = 0x%08x;\n", $kTitleToUpper;
printf DATA_TABLES "const PRUint32 kUpperToLower = 0x%08x;\n", $kUpperToLower;
printf DATA_TABLES "const PRUint32 kLowerToTitle = 0x%08x;\n", $kLowerToTitle;
printf DATA_TABLES "const PRUint32 kLowerToUpper = 0x%08x;\n", $kLowerToUpper;
printf DATA_TABLES "const PRUint32 kCaseMapCharMask = 0x%08x;\n\n", $kCaseMapCharMask;

sub genTables
{
  my ($prefix, $type, $indexBits, $charBits, $func, $maxPlane) = @_;

  print DATA_TABLES "#define k${prefix}MaxPlane  $maxPlane\n";
  print DATA_TABLES "#define k${prefix}IndexBits $indexBits\n";
  print DATA_TABLES "#define k${prefix}CharBits  $charBits\n";

  my $indexLen = 1 << $indexBits;
  my $dataLen = 1 << $charBits;
  my %charIndex = ();
  my %pageMapIndex = ();
  my @pageMap = ();
  my @char = ();
  
  my $planeMap = "\x00" x $maxPlane;
  foreach my $plane (0 .. $maxPlane) {
    my $pageMap = "\x00" x $indexLen * 2;
    foreach my $page (0 .. $indexLen - 1) {
        my $charValues = "";
        foreach my $ch (0 .. $dataLen - 1) {
            my $usv = $plane * 0x10000 + $page * $dataLen + $ch;
            $charValues .= &$func($usv);
        }
        chop $charValues;

        unless (exists $charIndex{$charValues}) {
            $charIndex{$charValues} = scalar keys %charIndex;
            $char[$charIndex{$charValues}] = $charValues;
        }
        substr($pageMap, $page * 2, 2) = pack('S', $charIndex{$charValues});
    }
    
    unless (exists $pageMapIndex{$pageMap}) {
        $pageMapIndex{$pageMap} = scalar keys %pageMapIndex;
        $pageMap[$pageMapIndex{$pageMap}] = $pageMap;
    }
    if ($plane > 0) {
        substr($planeMap, $plane - 1, 1) = pack('C', $pageMapIndex{$pageMap});
    }
  }

  if ($maxPlane) {
    print DATA_TABLES "static const PRUint8 s${prefix}Planes[$maxPlane] = {";
    print DATA_TABLES join(',', map { sprintf("%d", $_) } unpack('C*', $planeMap));
    print DATA_TABLES "};\n\n";
  }

  my $chCount = scalar @char;
  my $pmBits = $chCount > 255 ? 16 : 8;
  my $pmCount = scalar @pageMap;
  print DATA_TABLES "static const PRUint${pmBits} s${prefix}Pages[$pmCount][$indexLen] = {\n";
  for (my $i = 0; $i < scalar @pageMap; ++$i) {
    print DATA_TABLES "  {";
    print DATA_TABLES join(',', map { sprintf("%d", $_) } unpack('S*', $pageMap[$i]));
    print DATA_TABLES $i < $#pageMap ? "},\n" : "}\n";
  }
  print DATA_TABLES "};\n\n";

  print DATA_TABLES "static const $type s${prefix}Values[$chCount][$dataLen] = {\n";
  for (my $i = 0; $i < scalar @char; ++$i) {
    print DATA_TABLES "  {";
    print DATA_TABLES $char[$i];
    print DATA_TABLES $i < $#char ? "},\n" : "}\n";
  }
  print DATA_TABLES "};\n\n";

  print STDERR "Data for $prefix = ", $pmCount*$indexLen*$pmBits/8 +
                                      $chCount*$dataLen*(($type =~ /32/) ? 4 : 1) +
                                      $maxPlane, "\n";
}

print DATA_TABLES <<__END;
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END

close DATA_TABLES;

open HEADER, "> nsUnicodeScriptCodes.h" or die "unable to open nsUnicodeScriptCodes.h for output";

print HEADER <<__END;
$licenseBlock
/*
 * Created on $timestamp from UCD data files with version info:
 *

$versionInfo

 *
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */

#ifndef NS_UNICODE_SCRIPT_CODES
#define NS_UNICODE_SCRIPT_CODES
__END

print HEADER "enum {\n";
for (my $i = 0; $i < scalar @scriptCodeToName; ++$i) {
  print HEADER "  MOZ_SCRIPT_", $scriptCodeToName[$i], " = ", $i, ",\n";
}
print HEADER "  MOZ_SCRIPT_INVALID = -1\n";
print HEADER "};\n\n";

print HEADER <<__END;
#endif
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END

close HEADER;

