#!/usr/bin/env perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
#       - BidiMirroring.txt
#       - BidiBrackets.txt
#       - HangulSyllableType.txt
#       - LineBreak.txt
#       - ReadMe.txt (to record version/date of the UCD)
#       - Unihan_Variants.txt (from Unihan.zip)
#     though this may change if we find a need for additional properties.
#
#     The Unicode data files listed above should be together in one directory.
#
#     We also require the file
#        http://www.unicode.org/Public/security/latest/xidmodifications.txt
#     This file should be in a sub-directory "security" immediately below the
#        directory containing the other Unicode data files.
#
#     We also require the latest data file for UTR50, currently revision-13:
#        http://www.unicode.org/Public/vertical/revision-13/VerticalOrientation-13.txt
#     This file should be in a sub-directory "vertical" immediately below the
#        directory containing the other Unicode data files.
#
#
# (2) Run this tool using a command line of the form
#
#         perl genUnicodePropertyData.pl      \
#                 /path/to/harfbuzz/src       \
#                 /path/to/icu/common/unicode \
#                 /path/to/UCD-directory
#
#     This will generate (or overwrite!) the files
#
#         nsUnicodePropertyData.cpp
#         nsUnicodeScriptCodes.h
#
#     in the current directory.

use strict;
use List::Util qw(first);

if ($#ARGV != 2) {
    print <<__EOT;
# Run this tool using a command line of the form
#
#     perl genUnicodePropertyData.pl      \\
#             /path/to/harfbuzz/src       \\
#             /path/to/icu/common/unicode \\
#             /path/to/UCD-directory
#
# where harfbuzz/src is the directory containing harfbuzz .cc and .hh files,
# icu/common/unicode is the directory containing ICU 'common' public headers,
# and UCD-directory is a directory containing the current Unicode Character
# Database files (UnicodeData.txt, etc), available from
# http://www.unicode.org/Public/UNIDATA/, with additional resources as
# detailed in the source comments.
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

my $HARFBUZZ = $ARGV[0];
my $ICU = $ARGV[1];
my $UNICODE = $ARGV[2];

# load HB_Category constants

my $cc = -1;
my %catCode;

sub readHarfBuzzHeader
{
    my $file = shift;
    open FH, "< $HARFBUZZ/$file" or die "can't open harfbuzz header $HARFBUZZ/$file\n";
    while (<FH>) {
        if (m/HB_UNICODE_GENERAL_CATEGORY_([A-Z_]+)/) {
            $cc++;
            $catCode{$1} = $cc;
        }
    }
    close FH;
}

&readHarfBuzzHeader("hb-unicode.h");

die "didn't find HarfBuzz category codes\n" if $cc == -1;

my %scriptCode;
my @scriptCodeToTag;
my @scriptCodeToName;

my $sc = -1;

sub readIcuHeader
{
    my $file = shift;
    open FH, "< $ICU/$file" or die "can't open ICU header $ICU/$file\n";
    while (<FH>) {
        # adjust for ICU vs UCD naming discrepancies
        s/LANNA/TAI_THAM/;
        s/MEITEI_MAYEK/MEETEI_MAYEK/;
        s/ORKHON/OLD_TURKIC/;
        s/MENDE/MENDE_KIKAKUI/;
        s/SIGN_WRITING/SIGNWRITING/;
        if (m|USCRIPT_([A-Z_]+)\s*=\s*([0-9]+),\s*/\*\s*([A-Z][a-z]{3})\s*\*/|) {
            $sc = $2;
            $scriptCode{$1} = $sc;
            $scriptCodeToTag[$sc] = $3;
            $scriptCodeToName[$sc] = $1;
        }
    }
    close FH;
}

&readIcuHeader("uscript.h");

die "didn't find ICU script codes\n" if $sc == -1;

my %xidmodCode = (
'Recommended'       => 0,
'Inclusion'         => 1,
'Uncommon_Use'      => 2,
'Technical'         => 3,
'Obsolete'          => 4,
'Aspirational'      => 5,
'Limited_Use'       => 6,
'Exclusion'         => 7,
'Not_XID'           => 8,
'Not_NFKC'          => 9,
'Default_Ignorable' => 10,
'Deprecated'        => 11,
'not-chars'         => 12
);

my %bidicategoryCode = (
  "L"   =>  0, # Left-to-Right
  "R"   =>  1, # Right-to-Left
  "EN"  =>  2, # European Number
  "ES"  =>  3, # European Number Separator
  "ET"  =>  4, # European Number Terminator
  "AN"  =>  5, # Arabic Number
  "CS"  =>  6, # Common Number Separator
  "B"   =>  7, # Paragraph Separator
  "S"   =>  8, # Segment Separator
  "WS"  =>  9, # Whitespace
  "ON"  => 10, # Other Neutrals
  "LRE" => 11, # Left-to-Right Embedding
  "LRO" => 12, # Left-to-Right Override
  "AL"  => 13, # Right-to-Left Arabic
  "RLE" => 14, # Right-to-Left Embedding
  "RLO" => 15, # Right-to-Left Override
  "PDF" => 16, # Pop Directional Format
  "NSM" => 17, # Non-Spacing Mark
  "BN"  => 18, # Boundary Neutral
  "FSI" => 19, # First Strong Isolate
  "LRI" => 20, # Left-to-Right Isolate
  "RLI" => 21, # Right-to-left Isolate
  "PDI" => 22  # Pop Direcitonal Isolate
);

my %verticalOrientationCode = (
  'U' => 0,  #   U - Upright, the same orientation as in the code charts
  'R' => 1,  #   R - Rotated 90 degrees clockwise compared to the code charts
  'Tu' => 2, #   Tu - Transformed typographically, with fallback to Upright
  'Tr' => 3  #   Tr - Transformed typographically, with fallback to Rotated
);

my %lineBreakCode = ( # ordering matches ICU's ULineBreak enum
  "XX" => 0,
  "AI" => 1,
  "AL" => 2,
  "B2" => 3,
  "BA" => 4,
  "BB" => 5,
  "BK" => 6,
  "CB" => 7,
  "CL" => 8,
  "CM" => 9,
  "CR" => 10,
  "EX" => 11,
  "GL" => 12,
  "HY" => 13,
  "ID" => 14,
  "IN" => 15,
  "IS" => 16,
  "LF" => 17,
  "NS" => 18,
  "NU" => 19,
  "OP" => 20,
  "PO" => 21,
  "PR" => 22,
  "QU" => 23,
  "SA" => 24,
  "SG" => 25,
  "SP" => 26,
  "SY" => 27,
  "ZW" => 28,
  "NL" => 29,
  "WJ" => 30,
  "H2" => 31,
  "H3" => 32,
  "JL" => 33,
  "JT" => 34,
  "JV" => 35,
  "CP" => 36,
  "CJ" => 37,
  "HL" => 38,
  "RI" => 39
);

# initialize default properties
my @script;
my @category;
my @combining;
my @mirror;
my @pairedBracketType;
my @hangul;
my @casemap;
my @xidmod;
my @numericvalue;
my @hanVariant;
my @bidicategory;
my @fullWidth;
my @fullWidthInverse;
my @verticalOrientation;
my @lineBreak;
for (my $i = 0; $i < 0x110000; ++$i) {
    $script[$i] = $scriptCode{"UNKNOWN"};
    $category[$i] = $catCode{"UNASSIGNED"};
    $combining[$i] = 0;
    $pairedBracketType[$i] = 0;
    $casemap[$i] = 0;
    $xidmod[$i] = $xidmodCode{"not-chars"};
    $numericvalue[$i] = -1;
    $hanVariant[$i] = 0;
    $bidicategory[$i] = $bidicategoryCode{"L"};
    $fullWidth[$i] = 0;
    $fullWidthInverse[$i] = 0;
    $verticalOrientation[$i] = 1; # default for unlisted codepoints is 'R'
    $lineBreak[$i] = $lineBreakCode{"XX"};
}

# blocks where the default for bidi category is not L
for my $i (0x0600..0x07BF, 0x08A0..0x08FF, 0xFB50..0xFDCF, 0xFDF0..0xFDFF, 0xFE70..0xFEFF, 0x1EE00..0x0001EEFF) {
  $bidicategory[$i] = $bidicategoryCode{"AL"};
}
for my $i (0x0590..0x05FF, 0x07C0..0x089F, 0xFB1D..0xFB4F, 0x00010800..0x00010FFF, 0x0001E800..0x0001EDFF, 0x0001EF00..0x0001EFFF) {
  $bidicategory[$i] = $bidicategoryCode{"R"};
}
for my $i (0x20A0..0x20CF) {
  $bidicategory[$i] = $bidicategoryCode{"ET"};
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
open FH, "< $UNICODE/ReadMe.txt" or die "can't open Unicode ReadMe.txt file\n";
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
open FH, "< $UNICODE/UnicodeData.txt" or die "can't open UCD file UnicodeData.txt\n";
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
                $bidicategory[$first] = $bidicategoryCode{$fields[4]};
                unless (length($fields[7]) == 0) {
                  $numericvalue[$first] = $fields[7];
                }
                if ($fields[1] =~ /CJK/) {
                  @hanVariant[$first] = 3;
                }
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
        $bidicategory[$usv] = $bidicategoryCode{$fields[4]};
        unless (length($fields[7]) == 0) {
          $numericvalue[$usv] = $fields[7];
        }
        if ($fields[1] =~ /CJK/) {
          @hanVariant[$usv] = 3;
        }
        if ($fields[5] =~ /^<narrow>/) {
          my $wideChar = hex(substr($fields[5], 9));
          die "didn't expect supplementary-plane values here" if $usv > 0xffff || $wideChar > 0xffff;
          $fullWidth[$usv] = $wideChar;
          $fullWidthInverse[$wideChar] = $usv;
        }
        elsif ($fields[5] =~ /^<wide>/) {
          my $narrowChar = hex(substr($fields[5], 7));
          die "didn't expect supplementary-plane values here" if $usv > 0xffff || $narrowChar > 0xffff;
          $fullWidth[$narrowChar] = $usv;
          $fullWidthInverse[$usv] = $narrowChar;
        }
    }
}
close FH;

# read Scripts.txt
open FH, "< $UNICODE/Scripts.txt" or die "can't open UCD file Scripts.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s+;\s+([^ ]+)/) {
        my $script = uc($3);
        warn "unknown ICU script $script" unless exists $scriptCode{$script};
        my $script = $scriptCode{$script};
        my $start = hex "0x$1";
        my $end = (defined $2) ? hex "0x$2" : $start;
        for (my $i = $start; $i <= $end; ++$i) {
            $script[$i] = $script;
        }
    }
}
close FH;

# read BidiMirroring.txt
my @offsets = ();
push @offsets, 0;

open FH, "< $UNICODE/BidiMirroring.txt" or die "can't open UCD file BidiMirroring.txt\n";
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
        my $offsetIndex = first { $offsets[$_] eq $mirrorOffset } 0..$#offsets;
        if ($offsetIndex == undef) {
            die "too many offset codes\n" if scalar @offsets == 31;
            push @offsets, $mirrorOffset;
            $offsetIndex = $#offsets;
        }
        $mirror[hex "0x$1"] = $offsetIndex;
    }
}
close FH;

# read BidiBrackets.txt
my %pairedBracketTypeCode = (
  'N' => 0,
  'O' => 1,
  'C' => 2
);
open FH, "< $UNICODE/BidiBrackets.txt" or die "can't open UCD file BidiBrackets.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    s/#.*//;
    if (m/([0-9A-F]{4,6});\s*([0-9A-F]{4,6});\s*(.)/) {
        my $mirroredChar = $offsets[$mirror[hex "0x$1"]] + hex "0x$1";
        die "bidi bracket does not match mirrored char\n" unless $mirroredChar == hex "0x$2";
        my $pbt = uc($3);
        warn "unknown Bidi Bracket type" unless exists $pairedBracketTypeCode{$pbt};
        $pairedBracketType[hex "0x$1"] = $pairedBracketTypeCode{$pbt};
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
open FH, "< $UNICODE/HangulSyllableType.txt" or die "can't open UCD file HangulSyllableType.txt\n";
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

# read LineBreak.txt
open FH, "< $UNICODE/LineBreak.txt" or die "can't open UCD file LineBreak.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    s/#.*//;
    if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s*;\s*([^ ]+)/) {
        my $lb = uc($3);
        warn "unknown LineBreak class" unless exists $lineBreakCode{$lb};
        $lb = $lineBreakCode{$lb};
        my $start = hex "0x$1";
        my $end = (defined $2) ? hex "0x$2" : $start;
        for (my $i = $start; $i <= $end; ++$i) {
            $lineBreak[$i] = $lb;
        }
    }
}
close FH;

# read xidmodifications.txt
open FH, "< $UNICODE/security/xidmodifications.txt" or die "can't open UCD file xidmodifications.txt\n";
push @versionInfo, "";
while (<FH>) {
  chomp;
  unless (/\xef\xbb\xbf/) {
    push @versionInfo, $_;
  }
  last if /Generated:/;
}
while (<FH>) {
  if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s+;\s+[^ ]+\s+;\s+([^ ]+)/) {
    my $xidmod = $3;
    warn "unknown Identifier Modification $xidmod" unless exists $xidmodCode{$xidmod};
    $xidmod = $xidmodCode{$xidmod};
    my $start = hex "0x$1";
    my $end = (defined $2) ? hex "0x$2" : $start;
    for (my $i = $start; $i <= $end; ++$i) {
      $xidmod[$i] = $xidmod;
    }
  }
}
close FH;

open FH, "< $UNICODE/Unihan_Variants.txt" or die "can't open UCD file Unihan_Variants.txt (from Unihan.zip)\n";
push @versionInfo, "";
while (<FH>) {
  chomp;
  push @versionInfo, $_;
  last if /Date:/;
}
my $savedusv = 0;
my $hasTC = 0;
my $hasSC = 0;
while (<FH>) {
  chomp;
  if (m/U\+([0-9A-F]{4,6})\s+k([^ ]+)Variant/) {
    my $usv = hex "0x$1";
    if ($usv != $savedusv) {
      unless ($savedusv == 0) {
        if ($hasTC && !$hasSC) {
          $hanVariant[$savedusv] = 1;
        } elsif (!$hasTC && $hasSC) {
          $hanVariant[$savedusv] = 2;
        }
      }
      $savedusv = $usv;
      $hasTC = 0;
      $hasSC = 0;
    }
    if ($2 eq "Traditional") {
      $hasTC = 1;
    }
    if ($2 eq "Simplified") {
      $hasSC = 1;
    }
  } 
}
close FH;

# read VerticalOrientation-13.txt
open FH, "< $UNICODE/vertical/VerticalOrientation-13.txt" or die "can't open UTR50 data file VerticalOrientation-13.txt\n";
push @versionInfo, "";
while (<FH>) {
    chomp;
    push @versionInfo, $_;
    last if /Date:/;
}
while (<FH>) {
    chomp;
    s/#.*//;
    if (m/([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))*\s*;\s*([^ ]+)/) {
        my $vo = $3;
        warn "unknown Vertical_Orientation code $vo"
            unless exists $verticalOrientationCode{$vo};
        $vo = $verticalOrientationCode{$vo};
        my $start = hex "0x$1";
        my $end = (defined $2) ? hex "0x$2" : $start;
        for (my $i = $start; $i <= $end; ++$i) {
            $verticalOrientation[$i] = $vo;
        }
    }
}
close FH;

my $timestamp = gmtime();

open DATA_TABLES, "> nsUnicodePropertyData.cpp" or die "unable to open nsUnicodePropertyData.cpp for output";

my $licenseBlock = q[
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Derived from the Unicode Character Database by genUnicodePropertyData.pl
 *
 * For Unicode terms of use, see http://www.unicode.org/terms_of_use.html
 */
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

#include <stdint.h>
#include "harfbuzz/hb.h"

__END

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

print DATA_TABLES "#if !ENABLE_INTL_API\n";
print DATA_TABLES "static const uint32_t sScriptCodeToTag[] = {\n";
for (my $i = 0; $i < scalar @scriptCodeToTag; ++$i) {
  printf DATA_TABLES "  HB_TAG('%c','%c','%c','%c')", unpack('cccc', $scriptCodeToTag[$i]);
  print DATA_TABLES $i < $#scriptCodeToTag ? ",\n" : "\n";
}
print DATA_TABLES "};\n";
print DATA_TABLES "#endif\n\n";

our $totalData = 0;

print DATA_TABLES "#if !ENABLE_INTL_API\n";
print DATA_TABLES "static const int16_t sMirrorOffsets[] = {\n";
for (my $i = 0; $i < scalar @offsets; ++$i) {
    printf DATA_TABLES "  $offsets[$i]";
    print DATA_TABLES $i < $#offsets ? ",\n" : "\n";
}
print DATA_TABLES "};\n";
print DATA_TABLES "#endif\n\n";

print HEADER "#pragma pack(1)\n\n";

sub sprintCharProps1
{
  my $usv = shift;
  return sprintf("{%d,%d,%d}, ", $mirror[$usv], $hangul[$usv], $combining[$usv]);
}
my $type = q/
struct nsCharProps1 {
  unsigned char mMirrorOffsetIndex:5;
  unsigned char mHangulType:3;
  unsigned char mCombiningClass:8;
};
/;
&genTables("#if !ENABLE_INTL_API", "#endif",
           "CharProp1", $type, "nsCharProps1", 11, 5, \&sprintCharProps1, 1, 2, 1);

sub sprintCharProps2_short
{
  my $usv = shift;
  return sprintf("{%d,%d},",
                 $verticalOrientation[$usv], $xidmod[$usv]);
}
$type = q|
struct nsCharProps2 {
  // Currently only 6 bits are defined here, so 2 more could be added without
  // affecting the storage requirements for this struct.
  unsigned char mVertOrient:2;
  unsigned char mXidmod:4;
};
|;
&genTables("#if ENABLE_INTL_API", "#endif",
           "CharProp2", $type, "nsCharProps2", 9, 7, \&sprintCharProps2_short, 16, 1, 1);

sub sprintCharProps2_full
{
  my $usv = shift;
  return sprintf("{%d,%d,%d,%d,%d,%d,%d,%d},",
                 $script[$usv], $pairedBracketType[$usv], $category[$usv],
                 $bidicategory[$usv], $xidmod[$usv], $numericvalue[$usv],
                 $verticalOrientation[$usv], $lineBreak[$usv]);
}
$type = q|
struct nsCharProps2 {
  unsigned char mScriptCode:8;
  unsigned char mPairedBracketType:3; // only 2 bits actually needed
  unsigned char mCategory:5;
  unsigned char mBidiCategory:5;
  unsigned char mXidmod:4;
  signed char   mNumericValue:5;
  unsigned char mVertOrient:2;
  unsigned char mLineBreak; // only 6 bits actually needed
};
|;
&genTables("#if !ENABLE_INTL_API", "#endif",
           "CharProp2", $type, "nsCharProps2", 12, 4, \&sprintCharProps2_full, 16, 5, 1);

print HEADER "#pragma pack()\n\n";

sub sprintHanVariants
{
  my $baseUsv = shift;
  my $varShift = 0;
  my $val = 0;
  while ($varShift < 8) {
    $val |= $hanVariant[$baseUsv++] << $varShift;
    $varShift += 2;
  }
  return sprintf("0x%02x,", $val);
}
## Han Variant data currently unused but may be needed in future, see bug 857481
## &genTables("", "", "HanVariant", "", "uint8_t", 9, 7, \&sprintHanVariants, 2, 1, 4);

sub sprintFullWidth
{
  my $usv = shift;
  return sprintf("0x%04x,", $fullWidth[$usv]);
}
&genTables("", "", "FullWidth", "", "uint16_t", 10, 6, \&sprintFullWidth, 0, 2, 1);

sub sprintFullWidthInverse
{
  my $usv = shift;
  return sprintf("0x%04x,", $fullWidthInverse[$usv]);
}
&genTables("", "", "FullWidthInverse", "", "uint16_t", 10, 6, \&sprintFullWidthInverse, 0, 2, 1);

sub sprintCasemap
{
  my $usv = shift;
  return sprintf("0x%08x,", $casemap[$usv]);
}
&genTables("#if !ENABLE_INTL_API", "#endif",
           "CaseMap", "", "uint32_t", 11, 5, \&sprintCasemap, 1, 4, 1);

print STDERR "Total data = $totalData\n";

printf DATA_TABLES "const uint32_t kTitleToUpper = 0x%08x;\n", $kTitleToUpper;
printf DATA_TABLES "const uint32_t kUpperToLower = 0x%08x;\n", $kUpperToLower;
printf DATA_TABLES "const uint32_t kLowerToTitle = 0x%08x;\n", $kLowerToTitle;
printf DATA_TABLES "const uint32_t kLowerToUpper = 0x%08x;\n", $kLowerToUpper;
printf DATA_TABLES "const uint32_t kCaseMapCharMask = 0x%08x;\n\n", $kCaseMapCharMask;

sub genTables
{
  my ($guardBegin, $guardEnd,
      $prefix, $typedef, $type, $indexBits, $charBits, $func, $maxPlane, $bytesPerEntry, $charsPerEntry) = @_;

  if ($typedef ne '') {
    print HEADER "$guardBegin\n";
    print HEADER "$typedef\n";
    print HEADER "$guardEnd\n\n";
  }

  print DATA_TABLES "\n$guardBegin\n";
  print DATA_TABLES "#define k${prefix}MaxPlane  $maxPlane\n";
  print DATA_TABLES "#define k${prefix}IndexBits $indexBits\n";
  print DATA_TABLES "#define k${prefix}CharBits  $charBits\n";

  my $indexLen = 1 << $indexBits;
  my $charsPerPage = 1 << $charBits;
  my %charIndex = ();
  my %pageMapIndex = ();
  my @pageMap = ();
  my @char = ();
  
  my $planeMap = "\x00" x $maxPlane;
  foreach my $plane (0 .. $maxPlane) {
    my $pageMap = "\x00" x $indexLen * 2;
    foreach my $page (0 .. $indexLen - 1) {
        my $charValues = "";
        for (my $ch = 0; $ch < $charsPerPage; $ch += $charsPerEntry) {
            my $usv = $plane * 0x10000 + $page * $charsPerPage + $ch;
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
    print DATA_TABLES "static const uint8_t s${prefix}Planes[$maxPlane] = {";
    print DATA_TABLES join(',', map { sprintf("%d", $_) } unpack('C*', $planeMap));
    print DATA_TABLES "};\n\n";
  }

  my $chCount = scalar @char;
  my $pmBits = $chCount > 255 ? 16 : 8;
  my $pmCount = scalar @pageMap;
  if ($maxPlane == 0) {
    die "there should only be one pageMap entry!" if $pmCount > 1;
    print DATA_TABLES "static const uint${pmBits}_t s${prefix}Pages[$indexLen] = {\n";
  } else {
    print DATA_TABLES "static const uint${pmBits}_t s${prefix}Pages[$pmCount][$indexLen] = {\n";
  }
  for (my $i = 0; $i < scalar @pageMap; ++$i) {
    print DATA_TABLES $maxPlane > 0 ? "  {" : "  ";
    print DATA_TABLES join(',', map { sprintf("%d", $_) } unpack('S*', $pageMap[$i]));
    print DATA_TABLES $maxPlane > 0 ? ($i < $#pageMap ? "},\n" : "}\n") : "\n";
  }
  print DATA_TABLES "};\n\n";

  my $pageLen = $charsPerPage / $charsPerEntry;
  print DATA_TABLES "static const $type s${prefix}Values[$chCount][$pageLen] = {\n";
  for (my $i = 0; $i < scalar @char; ++$i) {
    print DATA_TABLES "  {";
    print DATA_TABLES $char[$i];
    print DATA_TABLES $i < $#char ? "},\n" : "}\n";
  }
  print DATA_TABLES "};\n";
  print DATA_TABLES "$guardEnd\n";

  my $dataSize = $pmCount * $indexLen * $pmBits/8 +
                 $chCount * $pageLen * $bytesPerEntry + 
                 $maxPlane;
  $totalData += $dataSize;

  print STDERR "Data for $prefix = $dataSize\n";
}

print DATA_TABLES <<__END;
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END

close DATA_TABLES;

print HEADER "namespace mozilla {\n";
print HEADER "namespace unicode {\n";
print HEADER "enum class Script {\n";
for (my $i = 0; $i < scalar @scriptCodeToName; ++$i) {
  print HEADER "  ", $scriptCodeToName[$i], " = ", $i, ",\n";
}
print HEADER "\n  NUM_SCRIPT_CODES = ", scalar @scriptCodeToName, ",\n";
print HEADER "\n  INVALID = -1\n";
print HEADER "};\n";
print HEADER "} // namespace unicode\n";
print HEADER "} // namespace mozilla\n\n";

print HEADER <<__END;
#endif
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END

close HEADER;

