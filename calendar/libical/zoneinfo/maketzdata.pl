#!/usr/bin/perl

#
# maketzdata.pl
#
# This script takes a zones.tab file in the root directory of
# the libical zoneinfo dir, and generates a tzdata.c file which
# is used by Mozilla Calendar.  tzdata.c normally lives in
# calendar/base/src/tzdata.c, and is #include'd by calICSService.cpp
#
# The reason for the ugly large char array is that MSVC can't
# handle string literals longer than 2048 chars; it can, however,
# handle multiple-megabyte array literals.  Go figure.
#
# Usage of this is: perl ./maketzdata.pl zones.tab tzdata.c
#

use File::Basename;

$tzid_prefix = "/softwarestudio.org/Olson_20010626_2/";

if ($#ARGV != 1) {
  print "Usage: maketzdata.pl zones.tab tzdata.c\n";
  die;
}

$zonetabfile = $ARGV[0];
$zonetabdir = dirname($zonetabfile);
$tzdatafile = $ARGV[1];

open ZONETAB, $zonetabfile || die "Can't open $zonetab";
@zonetab = <ZONETAB>;
close ZONETAB;

open TZDATA, ">" . $tzdatafile || die "Can't open $tzdata for writing";

print TZDATA <<EOF
/**
 ** Automagically generated via maketzdata.pl from zone.tab
 ** DO NOT EDIT BY HAND
 **/

EOF
;

if ($as_bytes) {
print TZDATA <<EOF
/**
 ** Due to string literal limit in MSVC of 2048 bytes, the tz data
 ** is represented as arrays of char literals.  This is most unfortunate.
 **/

EOF
;
}

print TZDATA <<EOF
typedef struct {
    /* the tzid  for this */
    const char *tzid;

    /* latitude and longitude, in +/-HHMMSS format */
    const char *latitude;
    const char *longitude;

    /* the ics VTIMEZONE component */
    const char *icstimezone;
} ical_timezone_data_struct;

EOF
;

print TZDATA "static const char ical_timezone_data_timezones[] = {";

# tzs will store the name/offset of the data in ical_timezone_data_timezones;
my %tzs;
$data_offset = 0;

foreach my $tz (@zonetab) {
  chop $tz;
  ($lat, $long, $name) = split(/ /, $tz);
  my $tzdata = "";
  {
    local $/ = undef;
    open TZFILE, "$zonetabdir/$name.ics" || die "Can't open timezone file $zonetabdir/$name";
    $tzdata = <TZFILE>;
    close TZFILE;
  }

  # add in the softwarestudio prefix
  $name = $tzid_prefix . $name;

  $hashdata = {
	       name => $name,
	       offset => $data_offset,
	       lat => $lat,
	       long => $long
	      };

  # put the data in the hash for later
  $tzs{$name} = $hashdata;

  $bytelen = length($tzdata);
  $clen = 0;
  while ($clen < $bytelen) {
    print TZDATA "\n/* $data_offset */ " if ($data_offset % 20 == 0);

    local $b = substr $tzdata, $clen, 1;
    if ($b eq "\n") { $b = "\\n"; }
    elsif ($b eq "\r") { $b = "\\r"; }
    elsif ($b eq "\t") { $b = "\\t"; }
    elsif ($b eq "\\") { $b = "\\\\"; }

    print TZDATA "'$b',";
    $data_offset++;

    $clen++;
  }

  print TZDATA "\n" if ($data_offset % 20 == 0);
  print TZDATA "0x00,";
  $data_offset++;
}

print TZDATA "0x00\n};\n\n";
print TZDATA "static ical_timezone_data_struct ical_timezone_data[] = {\n";

foreach my $k (keys %tzs) {
  $tz = $tzs{$k};

  $name = $tz->{name};
  $lat = $tz->{lat};
  $long = $tz->{long};
  $offt = $tz->{offset};

  print TZDATA "    { \"$name\", \"$lat\", \"$long\", ical_timezone_data_timezones + $offt },\n";
}

print TZDATA "    { NULL, NULL, NULL, NULL }\n";
print TZDATA "};\n\n";

close TZDATA;

