#!/usr/bin/env perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This tool is used to prepare a list of valid language subtags from the
# IANA registry (http://www.iana.org/assignments/language-subtag-registry).

# Run as
#
#   perl genLanguageTagList.pl language-subtag-registry > gfxLanguageTagList.cpp
#
# where language-subtag-registry is a copy of the IANA registry file.

use strict;

my $timestamp = gmtime();
print <<__END;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Derived from the IANA language subtag registry by genLanguageTagList.pl.
 *
 * Created on $timestamp.
 *
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */

__END

my $isLanguage = 0;

while (<>) {
  # strip leading/trailing whitespace, if any
  chomp;
  s/^\s+//;
  s/\s+$//;

  # assume File-Date precedes the actual list;
  # record the date, and begin assignment to an array of valid tags
  if (m/^File-Date:\s*(.+)$/) {
    print "// Based on IANA registry dated $1\n\n";
    print "static const uint32_t sLanguageTagList[] = {";
    next;
  }

  if (m/^%%/) {
    $isLanguage = 0;
    next;
  }

  # we only care about records of type 'language'
  if (m/^Type:\s*(.+)$/) {
    $isLanguage = ($1 eq 'language');
    next;
  }

  # append the tag to our string, with ";" as a delimiter
  if ($isLanguage && m/^Subtag:\s*([a-z]{2,3})\s*$/) {
    my $tagstr = $1;
    print "\n  TRUETYPE_TAG(",
      join(",", map { $_ eq " " ? " 0 " : "'" . $_ . "'" } split(//, substr($tagstr . "  ", 0, 4))),
      "), // ", $tagstr;
    next;
  }

  if ($isLanguage && m/^Description:\s*(.+)$/) {
    print " = $1";
    $isLanguage = 0; # only print first Description field
    next;
  }
}

# at end of file, terminate our assignment to the array of tags
print <<__END;

  0x0 // end of language code list
};

/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END
