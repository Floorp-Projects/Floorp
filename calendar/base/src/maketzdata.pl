#!/usr/bin/perl
#
# This script takes a zones.tab file and directory, such as that
# produced from an Olson-format tz database via vzic.  Note that it
# should be used on vzic output WITHOUT the --pure option.  The
# libical distribution includes ics files generated with --pure,
# which results in both huge files and various incompatability
# problems.
#
# vzic - http://dialspace.dial.pipex.com/prod/dialspace/town/pipexdsl/s/asbm26/vzic/
# olson db - ftp://elsie.nci.nih.gov/pub/
#
use File::Basename;

$TZ_PREFIX = "/mozilla.org/20050126_1";

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

typedef struct {
    /* the tzid  for this */
    char *tzid;

    /* latitude and longitude, in +/-HHMMSS format */
    char *latitude;
    char *longitude;

    /* the ics VTIMEZONE component */
    char *icstimezone;
} ical_timezone_data_struct;

static ical_timezone_data_struct ical_timezone_data[] = {
EOF
;

$first = 1;

foreach my $tz (@zonetab) {
  chop $tz;
  if ($first != 1) {
    print TZDATA ",\n";
  }
  ($lat, $long, $name) = split(/ /, $tz);
  my $tzdata = "";
  {
    local $/ = undef;
    open TZFILE, "$zonetabdir/$name.ics" || die "Can't open timezone file $zonetabdir/$name";
    $tzdata = <TZFILE>;
    close TZFILE;
  }

  # escape quotes
  $tzdata =~ s/\"/\\\"/sg;
  $tzdata =~ s/\n/\\n/sg;

  print TZDATA <<EOF
{
	"$TZ_PREFIX/$name",
	"$lat", "$long",
	"$tzdata"
}
EOF
;

  $first = 0;
}

print TZDATA <<EOF
,
{ NULL, NULL, NULL, NULL }
};
EOF
;

close TZDATA;
