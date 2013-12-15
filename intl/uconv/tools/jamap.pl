#!/usr/local/bin/perl
use strict;

my @source_files;

my @sjis_h;
$sjis_h[0] = -1;
@sjis_h[0x81..0x9f] = map { 0x2100 + $_ * 0x200 } (0 .. 30);
@sjis_h[0xe0..0xef] = map { 0x5F00 + $_ * 0x200 } (0 .. 15);
@sjis_h[0xf0..0xf9] = (-2) x 10;
my @sjis_l;
@sjis_l[0x40..0x7e] = (0x21..0x5f);
@sjis_l[0x80..0xfc] = (0x60..0x7e, 0x121..0x17e);

sub sjis_to_jis {
  my ($s) = @_;
  my $j;
  my $type;

  my $h = $sjis_h[($s>>8)&0xff];

  if ( $h > 0 ) { # jis0208

    my $l = $sjis_l[$s&0xff];
    if ( $l == 0 ) {
      $j = $s;
      $type = 'sjis2undef';
    } else {
      $j = $h + $l;
      if ( $j >= 0x3000 && $j < 0x7500 ) { # jis0208 kanji
        $type = 'jis0208';
      } elsif ( $j < 0x2900 ) { # jis0208
        $type = 'jis0208';
      } else {
        $type = 'jis0208undef';
      }
    }

  } elsif ( $h == -1 ) { # single byte

    $j = $s;
    if ( $s <= 0x7f ) { # jis0201 roman
      $type = 'jis0201';
    } elsif ( $s >= 0xa1 && $s <= 0xdf ) { # jis0201 kana
      $type = 'jis0201';
    } else { # sjis single byte undefined
      $type = 'sjis1undef';
    }

  } elsif ( $h == -2 ) { # private use
    $j = $s;
    $type = 'private';

  } else { # sjis undefined
    $j = $s;
    $type = 'sjis2undef';
  }

  return ($j, $type);
}


sub read_sjis_map {
  my ($filename, $s_col, $u_col) = @_;
  my %map;
  open MAP, $filename or die $!;
  while (<MAP>) {
    my @cols = split /\s+/;
    my ($s, $u) = @cols[$s_col, $u_col];
    $s =~ /^0x[0-9A-Fa-f]+$/ && $u =~ /^0x[0-9A-Fa-f]+$/ or next;

    $s = oct($s);
    $u = oct($u);

    my ($j, $type) = sjis_to_jis($s);
    push @{$map{$type}}, [$j, $s, $u];

  }
  close MAP or warn $!;
  push @source_files, $filename;
  return %map;
}

sub read_0212_map {
  my ($filename, $j_col, $u_col) = @_;
  my $map;
  open MAP, $filename or die $!;
  while (<MAP>) {
    my @cols = split /\s+/;
    my ($j, $u) = @cols[$j_col, $u_col];
    $j =~ /^0x[0-9A-Fa-f]+$/ && $u =~ /^0x[0-9A-Fa-f]+$/ or next;

    $j = oct($j);
    $u = oct($u);
    $u = 0xff5e if $u == 0x007e;

    push @$map, [$j, 0, $u];
  }
  close MAP or warn $!;
  push @source_files, $filename;
  return $map;
}


my %printed;
sub write_fromu_map {
  my ($filename, $code, @maps) = @_;
  open MAP, ">$filename" or die $!;
  foreach my $map (@maps) {
    foreach my $pair (@$map) {
      my ($j, $s, $u) = @$pair;
      if ( $code eq 'sjis' ) {
        $j = $s;
      }
      if ( defined($printed{$u}) ) {
        if ( $printed{$u} ne $j ) {
          printf "conflict 0x%04x to 0x%04x, 0x%04x\n", $u, $printed{$u}, $j;
        }
      } else {
        if ( $j < 0x100 ) {
          printf MAP "0x%02X\t0x%04X\n", $j, $u;
        } else {
          printf MAP "0x%04X\t0x%04X\n", $j, $u;
        }
        $printed{$u} = $j;
      }
    }
  }
  close MAP or warn $!;
}

my @table;
my %table;
my $table_next_count = 0;

sub get_94table_index {
  my ($map_table) = @_;
  my $key = join ',', map {int($map_table->[$_])} (0 .. 93);
  my $table_index = $table{$key};
  if ( !defined($table_index) ) {
    $table_index = $table_next_count;
    $table_next_count += 94;
    $table[$table_index] = $map_table;
    $table{$key} = $table_index;
  }
  return $table_index;
}

sub get_188table_index {
  my ($map_table) = @_;
  my $map_table1 = [ @{$map_table}[0 .. 93] ];
  my $map_table2 = [ @{$map_table}[94 .. 187] ];
  my $key = join ',', map {int($map_table->[$_])} (0 .. 187);
  my $key1 = join ',', map {int($map_table1->[$_])} (0 .. 93);
  my $key2 = join ',', map {int($map_table2->[$_])} (0 .. 93);
  my $table_index = $table{$key};
  if ( !defined($table_index) ) {
    $table_index = $table_next_count;
    $table_next_count += 188;
    $table[$table_index] = $map_table1;
    $table[$table_index + 94] = $map_table2;
    $table{$key} = $table_index;
    $table{$key1} = $table_index unless defined($table{$key1});
    $table{$key2} = $table_index + 94 unless defined($table{$key2});
  }
  return $table_index;
}

get_188table_index([]);

sub print_sjis_table_index {
  my @maps = @_;
  my %map_table;
  foreach my $map (@maps) {
    foreach my $pair (@$map) {
      my ($j, $s, $u) = @$pair;
      my $row = $s >> 8;
      my $cell = $s&0xff;
      if ( $cell >= 0x40 && $cell <= 0x7e ) {
        $cell -= 0x40;
      } elsif ( $cell >= 0x80 && $cell <= 0xfc ) {
        $cell -= 0x41;
      } else {
        next;
      }
      if ( defined($map_table{$row}->[$cell]) && $map_table{$row}->[$cell] != $u ) {
         print "conflict!\n";
      }
      $map_table{$row}->[$cell] = $u;
    }
  }

  for ( my $i = 0x80; $i < 0x100; $i++ ) {
    if ( ($i & 0x7) == 0 ) {
      print MAP "\n ";
    }
    if ( $i >= 0xa1 && $i <= 0xdf ) {
      printf MAP " 0x%04X,", $i + 0xfec0;
    } elsif ( $i >= 0xf0 && $i <= 0xf9 ) {
      printf MAP " 0x%04X,", 0xe000 + ($i - 0xf0) * 188;
    } elsif ( $i == 0x80 ) {
      print MAP " 0xFFFD,";
    } elsif ( $i == 0xa0 ) {
      print MAP " 0xF8F0,";
    } elsif ( $i >= 0xfd ) {
      printf MAP " 0x%04X,", $i + (0xf8f1 - 0xfd);
    } else {
      my $table_index = get_188table_index($map_table{$i});
      printf MAP " %6d,", $table_index;
    }
  }
}

sub print_jis_table_index {
  my @maps = @_;
  my %map_table;
  foreach my $map (@maps) {
    foreach my $pair (@$map) {
      my ($j, $s, $u) = @$pair;
      my $row = $j >> 8;
      my $cell = ($j&0xff) - 0x21;
      if ( defined($map_table{$row}->[$cell]) && $map_table{$row}->[$cell] != $u ) {
         print "conflict!\n";
      }
      $map_table{$row}->[$cell] = $u;
    }
  }

  for ( my $i = 0; $i < 0x80; $i++ ) {
    if ( ($i & 0x7) == 0 ) {
      print MAP "\n ";
    }
    if ( $i >= 0x21 && $i <= 0x7e ) {
      my $table_index = get_94table_index($map_table{$i});
      printf MAP " %6d,", $table_index;
    } else {
      print MAP " 0xFFFD,";
    }
  }
}

sub print_table_index {
  my ($map_name, @maps) = @_;
  print MAP "static const uint16_t g${map_name}IndexShiftJis[] = {";
  print_sjis_table_index(@maps);
  print MAP "\n};\n";
  print MAP "static const uint16_t g${map_name}IndexJis0208[] = {";
  print_jis_table_index(@maps);
  print MAP "\n};\n";
  print MAP "static const uint16_t * const g${map_name}Index[] = {";
  print MAP "\n  g${map_name}IndexShiftJis, g${map_name}IndexJis0208";
  print MAP "\n};\n\n";
}

sub print_0212_table_index {
  my ($map_name, @maps) = @_;
  print MAP "static const uint16_t g${map_name}Index[] = {";
  print_jis_table_index(@maps);
  print MAP "\n};\n\n";
}


sub print_table {
  print MAP "static const uint16_t gJapaneseMap[] = {";
  for ( my $i = 0; $i < $table_next_count; $i += 94 ) {
    my $index = $i;
    print MAP "\n  /* index $index */\n         ";
    my $map_table = $table[$i];
    my $print_count = 1;
    for ( my $j = 0; $j < 94; $j++ ) {
      my $u = $map_table->[$j];
      if ( $u == 0 ) { $u = 0xfffd; }
      printf MAP " 0x%04X,", $u;
      if ( ++$print_count == 8 ) {
        print MAP "\n ";
        $print_count = 0;
      }
    }
  }
  print MAP "\n};\n";
}


my %cp932 = read_sjis_map('CP932.TXT', 0, 1);
my %ibm = read_sjis_map('IBM943.TXT', 0, 1);
my $jis0212 = read_0212_map('JIS0212.TXT', 0, 1);

%printed = ();
write_fromu_map('jis0201-uf-unify', 'jis',
  $cp932{jis0201},
  $ibm{jis0201}
);
write_fromu_map('jis0208-uf-unify', 'jis',
  $cp932{jis0208},
  $ibm{jis0208}
);

%printed = ();
write_fromu_map('jis0208ext-uf-unify', 'jis',
  $cp932{jis0208undef},
  $ibm{jis0208undef}
);

%printed = ();
write_fromu_map('sjis-uf-unify', 'sjis',
  @cp932{'jis0201', 'jis0208', 'jis0208undef', 'sjis1undef', 'sjis2undef'},
  @ibm{'jis0201', 'jis0208', 'jis0208undef', 'sjis1undef', 'sjis2undef'}
);

open MAP, ">japanese.map" or die $!;
binmode MAP;

while (<DATA>) {
  if ( /^!/ ) { last; }
  print MAP;
}
print MAP "/* generated by jamap.pl @source_files */\n\n";
print MAP <<EOM;
// IE-compatible handling of undefined codepoints:
// 0x80 --> U+0080
// 0xa0 --> U+F8F0
// 0xfd --> U+F8F1
// 0xfe --> U+F8F2
// 0xff --> U+F8F3
EOM

print_table_index('CP932', @cp932{'jis0208', 'jis0208undef', 'sjis2undef'});
print_table_index('IBM943', @ibm{'jis0208', 'jis0208undef', 'sjis2undef'});
print_0212_table_index('JIS0212', $jis0212);
print_table();

close MAP or warn $!;

__DATA__
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

!
