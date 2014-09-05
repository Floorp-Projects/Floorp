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
#       - EastAsianWidth.txt
#       - BidiMirroring.txt
#       - HangulSyllableType.txt
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
#     We also require the latest data file for UTR50, currently revision-12:
#        http://www.unicode.org/Public/vertical/revision-12/VerticalOrientation-12.txt
#     This file should be in a sub-directory "vertical" immediately below the
#        directory containing the other Unicode data files.
#
#
# (2) Run this tool using a command line of the form
#
#         perl genUnicodePropertyData.pl \
#                 /path/to/harfbuzz/src  \
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

if ($#ARGV != 1) {
    print <<__EOT;
# Run this tool using a command line of the form
#
#     perl genUnicodePropertyData.pl \
#             /path/to/harfbuzz/src  \
#             /path/to/UCD-directory
#
# where harfbuzz/src is the directory containing harfbuzz .cc and .hh files,
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

# CHECK that this matches Pango source (as found for example at 
# http://git.gnome.org/browse/pango/tree/pango/pango-script.h)
# for as many codes as that defines (currently up through Unicode 5.1)
# and the GLib enumeration
# http://developer.gnome.org/glib/2.30/glib-Unicode-Manipulation.html#GUnicodeScript
# (currently defined up through Unicode 6.0).
# Constants beyond these may be regarded as unstable for now, but we don't actually
# depend on the specific values.
my %scriptCode = (
  INVALID => -1,
  COMMON => 0,
  INHERITED => 1,
  ARABIC => 2,
  ARMENIAN => 3,
  BENGALI => 4,
  BOPOMOFO => 5,
  CHEROKEE => 6,
  COPTIC => 7,
  CYRILLIC => 8,
  DESERET => 9,
  DEVANAGARI => 10,
  ETHIOPIC => 11,
  GEORGIAN => 12,
  GOTHIC => 13,
  GREEK => 14,
  GUJARATI => 15,
  GURMUKHI => 16,
  HAN => 17,
  HANGUL => 18,
  HEBREW => 19,
  HIRAGANA => 20,
  KANNADA => 21,
  KATAKANA => 22,
  KHMER => 23,
  LAO => 24,
  LATIN => 25,
  MALAYALAM => 26,
  MONGOLIAN => 27,
  MYANMAR => 28,
  OGHAM => 29,
  OLD_ITALIC => 30,
  ORIYA => 31,
  RUNIC => 32,
  SINHALA => 33,
  SYRIAC => 34,
  TAMIL => 35,
  TELUGU => 36,
  THAANA => 37,
  THAI => 38,
  TIBETAN => 39,
  CANADIAN_ABORIGINAL => 40,
  YI => 41,
  TAGALOG => 42,
  HANUNOO => 43,
  BUHID => 44,
  TAGBANWA => 45,
# unicode 4.0 additions
  BRAILLE => 46,
  CYPRIOT => 47,
  LIMBU => 48,
  OSMANYA => 49,
  SHAVIAN => 50,
  LINEAR_B => 51,
  TAI_LE => 52,
  UGARITIC => 53,
# unicode 4.1 additions
  NEW_TAI_LUE => 54,
  BUGINESE => 55,
  GLAGOLITIC => 56,
  TIFINAGH => 57,
  SYLOTI_NAGRI => 58,
  OLD_PERSIAN => 59,
  KHAROSHTHI => 60,
# unicode 5.0 additions
  UNKNOWN => 61,
  BALINESE => 62,
  CUNEIFORM => 63,
  PHOENICIAN => 64,
  PHAGS_PA => 65,
  NKO => 66,
# unicode 5.1 additions
  KAYAH_LI => 67,
  LEPCHA => 68,
  REJANG => 69,
  SUNDANESE => 70,
  SAURASHTRA => 71,
  CHAM => 72,
  OL_CHIKI => 73,
  VAI => 74,
  CARIAN => 75,
  LYCIAN => 76,
  LYDIAN => 77,
# unicode 5.2 additions
  AVESTAN => 78,
  BAMUM => 79,
  EGYPTIAN_HIEROGLYPHS => 80,
  IMPERIAL_ARAMAIC => 81,
  INSCRIPTIONAL_PAHLAVI => 82,
  INSCRIPTIONAL_PARTHIAN => 83,
  JAVANESE => 84,
  KAITHI => 85,
  LISU => 86,
  MEETEI_MAYEK => 87,
  OLD_SOUTH_ARABIAN => 88,
  OLD_TURKIC => 89,
  SAMARITAN => 90,
  TAI_THAM => 91,
  TAI_VIET => 92,
# unicode 6.0 additions
  BATAK => 93,
  BRAHMI => 94,
  MANDAIC => 95,
# unicode 6.1 additions
  CHAKMA => 96,
  MEROITIC_CURSIVE => 97,
  MEROITIC_HIEROGLYPHS => 98,
  MIAO => 99,
  SHARADA => 100,
  SORA_SOMPENG => 101,
  TAKRI => 102,
# unicode 7.0 additions
  BASSA_VAH => 103,
  CAUCASIAN_ALBANIAN => 104,
  DUPLOYAN => 105,
  ELBASAN => 106,
  GRANTHA => 107,
  KHOJKI => 108,
  KHUDAWADI => 109,
  LINEAR_A => 110,
  MAHAJANI => 111,
  MANICHAEAN => 112,
  MENDE_KIKAKUI => 113,
  MODI => 114,
  MRO => 115,
  NABATAEAN => 116,
  OLD_NORTH_ARABIAN => 117,
  OLD_PERMIC => 118,
  PAHAWH_HMONG => 119,
  PALMYRENE => 120,
  PAU_CIN_HAU => 121,
  PSALTER_PAHLAVI => 122,
  SIDDHAM => 123,
  TIRHUTA => 124,
  WARANG_CITI => 125,

# additional "script" code, not from Unicode (but matches ISO 15924's Zmth tag)
  MATHEMATICAL_NOTATION => 126,
);

my $sc = -1;
my $cc = -1;
my %catCode;
my @scriptCodeToTag;
my @scriptCodeToName;

sub readHarfBuzzHeader
{
    my $file = shift;
    open FH, "< $ARGV[0]/$file" or die "can't open harfbuzz header $ARGV[0]/$file\n";
    while (<FH>) {
        s/CANADIAN_SYLLABICS/CANADIAN_ABORIGINAL/; # harfbuzz and unicode disagree on this name :(
        if (m/HB_SCRIPT_([A-Z_]+)\s*=\s*HB_TAG\s*\(('.','.','.','.')\)\s*,/) {
            unless (exists $scriptCode{$1}) {
                warn "unknown script name $1 found in $file\n";
                next;
            }
            $sc = $scriptCode{$1};
            $scriptCodeToTag[$sc] = $2;
            $scriptCodeToName[$sc] = $1;
        }
        if (m/HB_UNICODE_GENERAL_CATEGORY_([A-Z_]+)/) {
            $cc++;
            $catCode{$1} = $cc;
        }
    }
    close FH;
}

&readHarfBuzzHeader("hb-common.h");
&readHarfBuzzHeader("hb-unicode.h");

die "didn't find HarfBuzz script codes\n" if $sc == -1;
die "didn't find HarfBuzz category codes\n" if $cc == -1;

# Additional code not present in HarfBuzz headers:
$sc = $scriptCode{"MATHEMATICAL_NOTATION"};
$scriptCodeToTag[$sc] = "'Z','m','t','h'";
$scriptCodeToName[$sc] = "MATHEMATICAL_NOTATION";

my %xidmodCode = (
'inclusion'         => 0,
'recommended'       => 1,
'default-ignorable' => 2,
'historic'          => 3,
'limited-use'       => 4,
'not-NFKC'          => 5,
'not-xid'           => 6,
'obsolete'          => 7,
'technical'         => 8,
'not-chars'         => 9
);

my %bidicategoryCode = (
  "L"   =>  "0", # Left-to-Right
  "R"   =>  "1", # Right-to-Left
  "EN"  =>  "2", # European Number
  "ES"  =>  "3", # European Number Separator
  "ET"  =>  "4", # European Number Terminator
  "AN"  =>  "5", # Arabic Number
  "CS"  =>  "6", # Common Number Separator
  "B"   =>  "7", # Paragraph Separator
  "S"   =>  "8", # Segment Separator
  "WS"  =>  "9", # Whitespace
  "ON"  => "10", # Other Neutrals
  "LRE" => "11", # Left-to-Right Embedding
  "LRO" => "12", # Left-to-Right Override
  "AL"  => "13", # Right-to-Left Arabic
  "RLE" => "14", # Right-to-Left Embedding
  "RLO" => "15", # Right-to-Left Override
  "PDF" => "16", # Pop Directional Format
  "NSM" => "17", # Non-Spacing Mark
  "BN"  => "18"  # Boundary Neutral
);

my %verticalOrientationCode = (
  'U' => 0,  #   U - Upright, the same orientation as in the code charts
  'R' => 1,  #   R - Rotated 90 degrees clockwise compared to the code charts
  'Tu' => 2, #   Tu - Transformed typographically, with fallback to Upright
  'Tr' => 3  #   Tr - Transformed typographically, with fallback to Rotated
);

# initialize default properties
my @script;
my @category;
my @combining;
my @eaw;
my @mirror;
my @hangul;
my @casemap;
my @xidmod;
my @numericvalue;
my @hanVariant;
my @bidicategory;
my @fullWidth;
my @verticalOrientation;
for (my $i = 0; $i < 0x110000; ++$i) {
    $script[$i] = $scriptCode{"UNKNOWN"};
    $category[$i] = $catCode{"UNASSIGNED"};
    $combining[$i] = 0;
    $casemap[$i] = 0;
    $xidmod[$i] = $xidmodCode{"not-chars"};
    $numericvalue[$i] = -1;
    $hanVariant[$i] = 0;
    $bidicategory[$i] = $bidicategoryCode{"L"};
    $fullWidth[$i] = 0;
    $verticalOrientation[$i] = 1; # default for unlisted codepoints is 'R'
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
        }
        elsif ($fields[5] =~ /^<wide>/) {
          my $narrowChar = hex(substr($fields[5], 7));
          die "didn't expect supplementary-plane values here" if $usv > 0xffff || $narrowChar > 0xffff;
          $fullWidth[$narrowChar] = $usv;
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
my @offsets = ();
push @offsets, 0;

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

# read xidmodifications.txt
open FH, "< $ARGV[1]/security/xidmodifications.txt" or die "can't open UCD file xidmodifications.txt\n";
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
# special case U+30FB KATAKANA MIDDLE DOT -- see bug 857490
$xidmod[0x30FB] = 1;

open FH, "< $ARGV[1]/Unihan_Variants.txt" or die "can't open UCD file Unihan_Variants.txt (from Unihan.zip)\n";
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

# read VerticalOrientation-12.txt
open FH, "< $ARGV[1]/vertical/VerticalOrientation-12.txt" or die "can't open UTR50 data file VerticalOrientation-12.txt\n";
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

print DATA_TABLES "static const uint32_t sScriptCodeToTag[] = {\n";
for (my $i = 0; $i < scalar @scriptCodeToTag; ++$i) {
  printf DATA_TABLES "  HB_TAG(%s)", $scriptCodeToTag[$i];
  print DATA_TABLES $i < $#scriptCodeToTag ? ",\n" : "\n";
}
print DATA_TABLES "};\n\n";

our $totalData = 0;

print DATA_TABLES "static const int16_t sMirrorOffsets[] = {\n";
for (my $i = 0; $i < scalar @offsets; ++$i) {
    printf DATA_TABLES "  $offsets[$i]";
    print DATA_TABLES $i < $#offsets ? ",\n" : "\n";
}
print DATA_TABLES "};\n\n";

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
&genTables("CharProp1", $type, "nsCharProps1", 11, 5, \&sprintCharProps1, 1, 2, 1);

sub sprintCharProps2
{
  my $usv = shift;
  return sprintf("{%d,%d,%d,%d,%d,%d,%d},",
                 $script[$usv], $eaw[$usv], $category[$usv],
                 $bidicategory[$usv], $xidmod[$usv], $numericvalue[$usv],
                 $verticalOrientation[$usv]);
}
$type = q/
struct nsCharProps2 {
  unsigned char mScriptCode:8;
  unsigned char mEAW:3;
  unsigned char mCategory:5;
  unsigned char mBidiCategory:5;
  unsigned char mXidmod:4;
  signed char   mNumericValue:5;
  unsigned char mVertOrient:2;
};
/;
&genTables("CharProp2", $type, "nsCharProps2", 11, 5, \&sprintCharProps2, 16, 4, 1);

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
&genTables("HanVariant", "", "uint8_t", 9, 7, \&sprintHanVariants, 2, 1, 4);

sub sprintFullWidth
{
  my $usv = shift;
  return sprintf("0x%04x,", $fullWidth[$usv]);
}
&genTables("FullWidth", "", "uint16_t", 10, 6, \&sprintFullWidth, 0, 2, 1);

sub sprintCasemap
{
  my $usv = shift;
  return sprintf("0x%08x,", $casemap[$usv]);
}
&genTables("CaseMap", "", "uint32_t", 11, 5, \&sprintCasemap, 1, 4, 1);

print STDERR "Total data = $totalData\n";

printf DATA_TABLES "const uint32_t kTitleToUpper = 0x%08x;\n", $kTitleToUpper;
printf DATA_TABLES "const uint32_t kUpperToLower = 0x%08x;\n", $kUpperToLower;
printf DATA_TABLES "const uint32_t kLowerToTitle = 0x%08x;\n", $kLowerToTitle;
printf DATA_TABLES "const uint32_t kLowerToUpper = 0x%08x;\n", $kLowerToUpper;
printf DATA_TABLES "const uint32_t kCaseMapCharMask = 0x%08x;\n\n", $kCaseMapCharMask;

sub genTables
{
  my ($prefix, $typedef, $type, $indexBits, $charBits, $func, $maxPlane, $bytesPerEntry, $charsPerEntry) = @_;

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

  print HEADER "$typedef\n\n" if $typedef ne '';

  my $pageLen = $charsPerPage / $charsPerEntry;
  print DATA_TABLES "static const $type s${prefix}Values[$chCount][$pageLen] = {\n";
  for (my $i = 0; $i < scalar @char; ++$i) {
    print DATA_TABLES "  {";
    print DATA_TABLES $char[$i];
    print DATA_TABLES $i < $#char ? "},\n" : "}\n";
  }
  print DATA_TABLES "};\n\n";

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

print HEADER "enum {\n";
for (my $i = 0; $i < scalar @scriptCodeToName; ++$i) {
  print HEADER "  MOZ_SCRIPT_", $scriptCodeToName[$i], " = ", $i, ",\n";
}
print HEADER "\n  MOZ_NUM_SCRIPT_CODES = ", scalar @scriptCodeToName, ",\n";
print HEADER "\n  MOZ_SCRIPT_INVALID = -1\n";
print HEADER "};\n\n";

print HEADER <<__END;
#endif
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END

close HEADER;

