#! /usr/local/bin/perl
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

# 
open(PA_TAGS, "<pa_tags.h");
open(HASH, "|/usr/local/bin/gperf -T -t -l -Npa_LookupTag -p -k1,\$,2,3 > gperf.out.$$");
print HASH 'struct pa_TagTable { char *name; int id; };
%%
';

open(RMAP, ">pa_hash.rmap");
$nextval = 0;

while (<PA_TAGS>) {
  if (/^#[ 	]*define[ 	]([A-Z_][A-Z0-9_]+)[ 	]*(.*)/) {
    $var = $1;
    $val = $2;
    $val =~ s/"//g;
    $pre = $var;
    $pre =~ s/_.*//;
    $post = $var;
    $post =~ s/$pre//;
    $post =~ s/_//;
    if ($pre eq "PT") {
      $strings{$post} = $val;
    } elsif ($pre eq "P") {
      if ($strings{$post} ne "") {
        print HASH $strings{$post} . ", $var\n";
      }
      if ($var ne "P_UNKNOWN" && $var ne "P_MAX") {
	while ($nextval < $val) {
	  print RMAP "/* $nextval */\t\"\",\n";
	  $nextval++;
	}
	$newval = $strings{$post};
	$newval =~ tr/a-z/A-Z/;
        print RMAP "/*$val*/\t\"$newval\",\n";
	$nextval = $val + 1;
      }
    }
  }
}
close(PA_TAGS);
close(HASH);
close(RMAP);
open(C, "<gperf.out.$$");
unlink("gperf.out.$$");
open(T, "<pa_hash.template");

while (<T>) {
  if (/^@begin/) {
    ($name, $start, $end) =
      m#@begin[ 	]*([A-Za-z0-9_]+)[ 	]*/([^/]*)/[ 	]*/([^/]*)/#;
    $line = <C> until (eof(C) || $line =~ /$start/);
    if ($line =~ /$start/) {
      $template{$name} .= $line;
      do {
	$line = <C>;
	$template{$name} .= $line;
      } until ($line =~ /$end/ || eof(C));
    }
  } elsif (/^@include/) {
    ($name) = /@include[ 	]*(.*)$/;
    print $template{$name};
  } elsif (/^@sub/) {
    ($name, $old, $new) =
      m#@sub[ 	]*([A-Za-z0-9_]*)[ 	]/([^/]*)/([^/]*)/#;
    $template{$name} =~ s/$old/$new/g;
  } elsif (/^@/) {
    ;
  } else {
    print $_;
  }
}
