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
# The Original Code is Mozilla Communicator.
#
# The Initial Developer of the Original Code is
# Jungshik Shin <jshin@mailaps.org>.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

# This script is used to generate precompiled CCMap files.
# See bug 180266 for details.

use strict;

use vars qw($fill_fmt $fu_sz);
use vars qw($e_mid_offset $e_pg_offset $f_mid_offset $f_pg_offset);

(@ARGV < 1 ) and usage();

my $ifn = $ARGV[0];

my ($ifh, $class);
open $ifh , "< $ifn" or die "Cannot open $ifn";

if (@ARGV >= 2) {
	$class = $ARGV[1];
	printf STDERR 
  "$0:\n\t CLASS $class is specified in the command line.\n" .
  "\t The class spec. in the input file will be ignored.\n";
}

use constant PLANE_SZ => 0x10000;
use constant MID_SZ   => PLANE_SZ / 16;
use constant PG_SZ    => MID_SZ / 16;

# Unlike FillInfo() method in Mozilla, let's use 16bit integer
# to pack the character coverage/representability. This way,
# we can just copy fillinfo to fill up page maps later.
use constant {
    FILL_SZ      => PLANE_SZ / 16,
    MID_FILL_SZ  => MID_SZ / 16,
    PG_FILL_SZ   => PG_SZ / 16
};



# network byte order short. actually, byte order doesn't matter.
$fill_fmt = "n1";  
$fu_sz = length(pack $fill_fmt, 0); # fillinfo unit size in byte (size of short)

$e_mid_offset = 16; 
$e_pg_offset = 32;  
$f_pg_offset = 0;
$f_mid_offset = 0;

my @ccmap = ();
my %pg_offsets = ();
my @fillinfo = ();
my %comments = ();

#get all upper pointers to point at empty mid pointers
push @ccmap, ($e_mid_offset) x 16;
#get all mid-pointers to point at empty page.
push @ccmap, ($e_pg_offset) x 16; 
push @ccmap, (0) x 16;  # empty pg


&read_input(\@fillinfo,$ifh,\%comments);

if (!defined($class) && !defined($comments{'CLASS'})) 
{
  printf STDERR "Class name is not specified in the cmd line. " .
    "Neither is it found in the input file.\n\n" ;
  usage();
}

$class = $comments{'CLASS'} if (! defined($class));

foreach my $mid (0 .. 15)
{
  my(@mid_fill) =   splice @fillinfo, 0, MID_FILL_SZ;
  # convert 256(MID_FILL_SZ) 16bit integers to a string of 256 * $fu_sz
  # characters.
  my($mid_str) = pack $fill_fmt x MID_FILL_SZ, @mid_fill;

  # for an empty mid, upper-pointer is already pointing to the empty mid.
  next if ($mid_str eq "\0" x ($fu_sz * MID_FILL_SZ));

  # for a full mid, add full mid if necessary.
  if ($mid_str eq "\xff" x  ($fu_sz * MID_FILL_SZ)) {
    ($f_mid_offset, $f_pg_offset) =
    add_full_mid(\@ccmap, $f_pg_offset) unless ($f_mid_offset);
    $ccmap[$mid] = $f_mid_offset;
    next;
  }

  my $mid_offset = add_new_mid(\@ccmap,$mid);
  
  foreach my $pg (0 .. 15) {
    my(@pg_fill) = splice @mid_fill, 0, PG_FILL_SZ;
    my($pg_str)  = pack $fill_fmt x PG_FILL_SZ, @pg_fill;

    # for an empty pg, mid-pointer is already pointing to the empty page.
    next if ($pg_str eq "\x0" x  ($fu_sz * PG_FILL_SZ)); 
      
    # for a full pg, add the full pg if necessary.
    # and set the mid-pointer to the full pg offset.
    if ($pg_str eq "\xff" x  ($fu_sz * PG_FILL_SZ)) {
      if (! $f_pg_offset) {
        $f_pg_offset = @ccmap;
        #for the full pg, endianess and ALU size are immaterial.
        push @ccmap, (0xffff) x 16; 
      }
      $ccmap[$mid_offset + $pg ] = $f_pg_offset;
      next;
    }

    $ccmap[$mid_offset + $pg] = @ccmap;

# 'Flag' the offset as the beginning of a page with actual data as 
# opposed to pointer sections.
    $pg_offsets{scalar @ccmap} =  @ccmap;
    
    push @ccmap, @pg_fill;
  }
}

&print_ccmap(\@ccmap,  \%pg_offsets, $class . ".ccmap", \%comments);
#&print_ccmap(\@ccmap, "32BE", \%pg_offsets, $class . ".32be", \%comments);
#&print_ccmap(\@ccmap, "64BE", \%pg_offsets, $class . ".64be", \%comments);

exit 0;

# END of Main

sub usage
{
  print STDERR <<USAGE;
Usage: $0 input_file [class]

The output file "class.ccmap" will be generated with 
all three cases LE(16/32/64bit)/BE(16bit), BE(32bit), and BE(64bit)
put together.

When 'class' is omitted, it has to be specified in the input file with
the following syntax

CLASS:: class_name
   
USAGE

  exit 1;
}

sub read_input 
{
  my($fillinfo_p, $input, $comments_p) = @_;
  @$fillinfo_p = (0) x FILL_SZ;
  my($lc)=0;
  while (<$input>)
  {
    $lc++;
    chomp;
    /^CLASS::/ and 
      ($comments_p->{'CLASS'} = $_) =~ s/^CLASS::\s*([a-zA-Z0-9_]+).*$/$1/,
      next;
    /^DESCRIPTION::/ and 
      ($comments_p->{'DESC'} = $_) =~ s/^DESCRIPTION::\s*//, next;
    /^FILE::/ and 
      ($comments_p->{'FILE'} = $_) =~ s/^FILE::\s*//, next;

    next unless /^\s*0[Xx][0-9A-Fa-f]{4}/; 

    /^\s*(.*)\s*$/;
    my ($u, $comment) = split /\s+/, $1, 2;
    $u =~ s/,//g;
    $u =~ tr/A-Z/a-z/;
    next if /^0x.*[^0-9a-f]+.*/;

    my $usv = oct $u;
    if ( 0xd800 <= $usv && $usv <= 0xdfff) { #surrogate pair
      printf STDERR "Invalid input $u at %4d\n", $lc;
    }
    $fillinfo_p->[($usv >> 4) & 0xfff] |= (1 << ($usv & 0x0f));
#   printf STDERR "input %s(%04x) \@line %d : put %04x @ %04x\n",
#     $u,$usv, $lc, (1 << ($usv & 0x0f)), ($usv >> 4) & 0xfff;

    $comments_p->{$usv} = "";

# Remove '/*' and '*/' (C style comment) or '//' (C++ style comment)
# or ':' and store only the textual content of the comment.
    if (defined($comment)) {
      ($comments_p->{$usv} = $comment) 
             =~ s !
                    (?:/\*|//|:)?  # '/*', '//' or ':' or NULL. Do not store. 
                    \s*            # zero or more of white space(s)
                    ([^*]+)        # one or more of non-white space(s).Store it
                                   # in $1 for the reference in replace part.
                    \s*            # zero or more of white space(s)
                    (?:\*/)?       # '*/' or NONE. Do not store 
                  !$1!sx           # replace the whole match with $1 stored above.
    }
  }
}

sub add_full_mid
{
  my($ccmap_p, $f_pg_offset) = @_;
  # add a full page if not yet added.
  if (! $f_pg_offset) {
    $f_pg_offset = @$ccmap_p;
    push @$ccmap_p, (0xffff) x 16; 
  }
# add the full mid-pointer array with all the pointers pointing to the full page.
  my $f_mid_offset = @$ccmap_p;
  push @$ccmap_p, ($f_pg_offset) x 16; 
  return ($f_mid_offset, $f_pg_offset);
}

sub add_new_mid
{
  my($ccmap_p, $mid) = @_;
  my $mid_offset =  @$ccmap_p;
  $ccmap_p->[$mid] = $mid_offset;
  #by default, all mid-pointers  point to the empty page.
  push @$ccmap_p, ($e_pg_offset) x 16; 
  return $mid_offset;
}

sub print_ccmap
{
  my($ccmap_p,$pg_offset_p, $ofn, $comments_p) = @_;

  my(@idxlist);

  open OUT, "> $ofn" or 
  die "cannot open $ofn for output\n";

  print OUT print_preamble($class);

  print OUT "\n/*\n"; 
  defined ($comments_p->{'CLASS'}) and 
    print OUT "   CLASS:: $comments_p->{'CLASS'}\n";
  defined ($comments_p->{'DESC'}) and 
    print OUT "   DESCRIPTION:: $comments_p->{'DESC'}\n";
  defined ($comments_p->{'FILE'}) and 
    print OUT "   FILE:: $comments_p->{'FILE'}\n";

  print OUT "\n";

  for my $usv (sort  keys %$comments_p) {
    next if ($usv =~ /CLASS/);
    printf OUT "   0X%04X : %s\n", $usv, $comments_p->{$usv};
  }

  printf OUT "*/\n\n";



# When CCMap is accessed, (PRUint16 *) is casted to 
# the pointer type of the ALU of a machine.  
# For little endian machines, the size of the ALU
# doesn't matter (16, 32, 64). For Big endian
# machines with 32/64 bit ALU, two/four 16bit words 
# have to be rearranged to be interpreted correctly
# as 32bit or 64bit integers with the 16bit word
# at the lowest addr. taking the highest place value.
# This shuffling is NOT necessary for the upper pointer section
# and mid-pointer sections.

  foreach my $fmt ("LE", "32BE", "64BE")
  { 
    for ($fmt) {
      /LE/ and do {
        @idxlist = (0, 1, 2, 3);
        print OUT "#if (defined(IS_LITTLE_ENDIAN) || ALU_SIZE == 16)\n" .
                  "// Precompiled CCMap for Big Endian(16bit)/Little" .
                  "  Endian(16/32/64bit) \n";
        last;
      };
      /32BE/ and do {
        @idxlist = (1, 0, 3, 2);
        print OUT "#elif (ALU_SIZE == 32)\n" .
                  "// Precompiled CCMap for  Big Endian(32bit)\n";
        last;
      };
      /64BE/ and do {
        @idxlist = (3, 2, 1, 0);
        print OUT "#elif (ALU_SIZE == 64)\n" .
                  "// Precompiled CCMap for Big Endian(64bit)\n";
        last;
      };
    }

    my($offset) = 0;
    while ($offset < @$ccmap_p)  {
      printf OUT "/* %04x */ ", $offset;
      for my $i (0 .. 3) {
        for my $j (defined($pg_offset_p->{$offset}) ? @idxlist : (0,1,2,3)) {
          printf OUT "0x%04X,", $ccmap_p->[$offset + $i * 4 + $j];
        }
        print OUT "\n           " if $i==1; 
      }
      print OUT "\n";
      $offset += 16;
    }
  } 

  print OUT <<END;
#else
#error "We don't support this architecture."
#endif

END

  close OUT;
}

sub print_preamble
{

 my($class) = @_;

 sprintf  <<PREAMBLE;
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * Contributor(s): Jungshik Shin <jshin\@mailaps.org> (Original developer)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /*========================================================
  This file contains  a precompiled CCMap for a class of Unicode
  characters ($class) to be identified quickly by Mozilla. 
  It was generated  by  ccmapbin.pl which you can find  under 
  mozilla/intl/unicharutil/tools.

  Enumerated below are characters included in the precompiled CCMap
  which is human-readable but not so human-friendly.  If you 
  needs to modify the list of characters belonging to "$class",
  you have to make a new file (with the name of your choice)
  listing characters (one character per line) you want to put 
  into "$class" in the format

         0xuuuu // comment

  In addition, the input file can have the following optional lines that
  read

      CLASS::$class
      DESCRIPTION:: description of a character class 
      FILE:: mozilla source file to include output files
      

  Then, run the following in the current directory.

    perl ccmapbin.pl input_file [$class] 

  which will generate $class.ccmap.

  (see bug 180266 and bug 167136)

 */

PREAMBLE

}
 
