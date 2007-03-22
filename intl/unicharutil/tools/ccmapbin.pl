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
# Portions created by the Initial Developer are Copyright (C) 2002, 2003
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
#
# Revised to support extended CCMaps for non-BMP characters : 2003-09-19 (bug 205387)  
# Revised to support the automatic generation of a macro defining the size
#   of a CCMap in terms of PRUint16 : 2003-12-11 (bug 224337)

use strict;


use vars qw($fill_fmt $fu_sz);
use vars qw($e_mid_offset $e_pg_offset);

(@ARGV < 1 ) and usage();

my $ifn = $ARGV[0];

my ($ifh, $variable, $class);
open $ifh , "< $ifn" or die "Cannot open $ifn";

if (@ARGV >= 2) {
	$variable = $ARGV[1];
	printf STDERR 
  "$0:\n\t VARIABLE $variable is specified in the command line.\n" .
  "\t The variable name spec. in the input file will be ignored.\n";
}

if (@ARGV >= 3) {
	$class = $ARGV[2];
	printf STDERR 
  "$0:\n\t CLASS $class is specified in the command line.\n" .
  "\t The class spec. in the input file will be ignored.\n";
}

use constant N_PLANES => 17; # BMP + 16 non-BMP planes
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

my @ccmap = ();
my %pg_flags = ();
my @fillinfo = ();
my %comments = ();

my $planes = &read_input(\@fillinfo,$ifh,\%comments);

if (!defined($variable) && !defined($comments{'VARIABLE'})) 
{
  printf STDERR "Variable name is not specified in the cmd line. " .
    "Neither is it found in the input file.\n\n" ;
  usage();
}

$variable = $comments{'VARIABLE'} if (! defined($variable));

if (!defined($class) && !defined($comments{'CLASS'})) 
{
  printf STDERR "Class name is not specified in the cmd line. " .
    "Neither is it found in the input file.\n\n" ;
  usage();
}

$class = $comments{'CLASS'} if (! defined($class));

my $have_non_bmp = 0;

# add the non_bmp flag and the bmp ccmap size (default to 0)  
# at the very beginning if there are non-bmp characters.
if ($planes & 0x1fe) {
  push @ccmap, (1, 0); 
  $have_non_bmp = 1; 
}

my $plane_idx_offset;
foreach my $plane (0 .. ($have_non_bmp ? 16 : 0))
{
  my @plane_ccmap = add_plane(\@ccmap, \@fillinfo, $plane); 
  my $size = @plane_ccmap;
  push @ccmap, @plane_ccmap;
  if ($plane == 0 && $have_non_bmp) {
    $ccmap[1] = $size;
    # add 2 for non-BMP flag and BMP plane size 
    # that have negative indices in C++.
    $plane_idx_offset = $size + 2; 

    # 'Flag' the offset as holding the plane indices  (any negative
    # number would do)
    $pg_flags{$plane_idx_offset} =  -1; 
    $pg_flags{$plane_idx_offset + 16} =  -1; 

    # plane indices are  16 PRUint32's(not 16 PRUint16's). 
    # In Perl, we assign each PRUint32 two slots in @ccmap (in BE order) 
    my $e_plane_offset = $size + 16 * 2;

    # set  plane indices to the empty plane by default
    foreach my $i (1 .. 16) {
      # split PRUint32 into two PRUint16's in BE
      push @ccmap, $e_plane_offset >> 16; 
      push @ccmap, $e_plane_offset  & 0xffff;
    }
    # add 'the' empty plane;   
    push @ccmap, (0) x 16;  
  }
  if ($plane > 0) {
    if ($size > 0) {
      # split PRUint32 into two PRUint16's in BE.
      # subtract 2 for non-BMP flag and BMP plane size 
      # that have negative indices in C++.
      $ccmap[$plane_idx_offset + ($plane - 1) * 2] = (@ccmap - $size - 2) >> 16;
      $ccmap[$plane_idx_offset + ($plane - 1) * 2 + 1] = (@ccmap - $size -2) & 0xffff;
    }
  }
}

&print_ccmap(\@ccmap,  \%pg_flags, $variable, $class, \%comments, $have_non_bmp);

exit 0;

# END of Main

sub usage
{
  print STDERR <<USAGE;
Usage: $0 input_file [variable [class]]

The output file "class.ccmap" will be generated with 
all three cases LE(16/32/64bit)/BE(16bit), BE(32bit), and BE(64bit)
put together. 'variable' will be used to name two macros, one for
dimensioning the size of a PRUin16[]  and the other for the array
initializer.

When 'variable' is omitted, it has to be specified in the input file with
the following syntax.

VARIABLE:: variable
   
When 'class' is omitted, it has to be specified in the input file with
the following syntax.

CLASS:: class_name
   
USAGE

  exit 1;
}

sub read_input 
{
  my($fillinfo_p, $input, $comments_p) = @_;
  @$fillinfo_p = (0) x (FILL_SZ * N_PLANES);

  # init bitfield for plane flags (17bits : BMP + 16 non-BMP planes)
  my $planes = 0; 
  my($lc)=0;
  while (<$input>)
  {
    $lc++;
    chomp;
    /^\s*VARIABLE::\s*([a-zA-Z][a-zA-Z0-9_]*)$/ and 
      $comments_p->{'VARIABLE'} = $1,
      next;
    /^\s*CLASS::/ and 
      ($comments_p->{'CLASS'} = $_) =~ s/^\s*CLASS::\s*([a-zA-Z0-9_]+).*$/$1/,
      next;
    /^\s*DESCRIPTION::/ and 
      ($comments_p->{'DESC'} = $_) =~ s/^\s*DESCRIPTION::\s*//, next;
    /^\s*FILE::/ and 
      ($comments_p->{'FILE'} = $_) =~ s/^\s*FILE::\s*//, next;

    next unless /^\s*0[Xx][0-9A-Fa-f]{4}/; 

    /^\s*(.*)\s*$/;
    my ($u, $comment) = split /\s+/, $1, 2;
    $u =~ s/,//g;
    $u =~ tr/A-Z/a-z/;
    next if /^0x.*[^0-9a-f]+.*/;

    my $usv = oct $u;
    if ( 0xd800 <= $usv && $usv <= 0xdfff || # surrogate code points
         $usv > 0x10ffff ) {
      printf STDERR "Invalid input $u at %4d\n", $lc;
      next;
    }
    $fillinfo_p->[($usv >> 4)] |= (1 << ($usv & 0x0f));
#   printf STDERR "input %s(%04x) \@line %d : put %04x @ %04x\n",
#     $u,$usv, $lc, (1 << ($usv & 0x0f)), ($usv >> 4) & 0xfff;

    # turn on plane flags
    $planes |= (1 << ($usv >> 16)); 

    my $key = sprintf("0X%06X", $usv); 
    $comments_p->{$key} = "";

# Remove '/*' and '*/' (C style comment) or '//' (C++ style comment)
# or ':' and store only the textual content of the comment.
    if (defined($comment)) {
      ($comments_p->{$key} = $comment) 
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

  return $planes;
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

sub add_plane
{
  my ($full_ccmap_p, $fillinfo_p, $plane) = @_; 
# my @ccmap = @$ccmap_p;
  my @ccmap = (); # plane ccmap
  my(@fillinfo) =   splice @$fillinfo_p, 0, FILL_SZ;
  # convert 4096(FILL_SZ) 16bit integers to a string of 4096 * $fu_sz
  # characters.
  my($plane_str) = pack $fill_fmt x FILL_SZ, @fillinfo;

  #  empty plane
  if ($plane_str eq "\0" x ($fu_sz * FILL_SZ)) {
    # for non-BMP plane, the default empty plane ccmap would work.
    # for BMP, we need 'self-referring' folded CCMap (the smallest CCMap)
    push @ccmap, (0) x 16 if (!$plane);
    return @ccmap;
  }
    
  #get all upper pointers to point at empty mid pointers
  push @ccmap, ($e_mid_offset) x 16;
  #get all mid-pointers to point at empty page.
  push @ccmap, ($e_pg_offset) x 16; 
  push @ccmap, (0) x 16;  # empty pg

  my $f_mid_offset = 0; 
  my $f_pg_offset;
  
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
        $ccmap[$mid_offset + $pg] = $f_pg_offset;
        next;
      }
  
      $ccmap[$mid_offset + $pg] = @ccmap;
  
  # 'Flag' the offset as the beginning of a page with actual data as 
  # opposed to pointer sections.
      $pg_flags{(scalar @$full_ccmap_p) + (scalar @ccmap)} =  @ccmap;
      
      push @ccmap, @pg_fill;
    }
  }
  return @ccmap; 
}

sub print_ccmap
{
  my($ccmap_p,$pg_flags_p, $variable, $class, $comments_p, $is_ext) = @_;


  my $ofn = $class . ($is_ext ? ".x-ccmap" : ".ccmap"); 

  open OUT, "> $ofn" or 
  die "cannot open $ofn for output\n";

  print OUT print_preamble($variable, $class);

  print OUT "\n/*\n"; 
# defined ($comments_p->{'CLASS'}) and 
#   print OUT "   CLASS:: $comments_p->{'CLASS'}\n";
  print OUT "   VARIABLE:: $variable\n";
  print OUT "   CLASS:: $class\n";
  defined ($comments_p->{'DESC'}) and 
    print OUT "   DESCRIPTION:: $comments_p->{'DESC'}\n";
  defined ($comments_p->{'FILE'}) and 
    print OUT "   FILE:: $comments_p->{'FILE'}\n";

  print OUT "\n";

  for my $key (sort keys %$comments_p) {
    next if ($key !~ /^0X/);
    printf OUT "   %s : %s\n", $key, $comments_p->{$key};
  }

  printf OUT "*/\n\n";


  my(@idxlist, @int16toint32);

# When CCMap is accessed, (PRUint16 *) is cast to 
# the pointer type of the ALU of a machine.  
# For little endian machines, the size of the ALU
# doesn't matter (16, 32, 64). For Big endian
# machines with 32/64 bit ALU, two/four 16bit words 
# have to be rearranged to be interpreted correctly
# as 32bit or 64bit integers with the 16bit word
# at the lowest address taking the highest place value.
# This shuffling is NOT necessary for the upper pointer section
# and mid-pointer sections.

# If non-BMP characters are presente, 16 plane indices 
# (32bit integers stored in two 16bit shorts in
# BE order) have to be treated differently based on the
# the endianness as well.

# For BMP-only CCMap, 16BE CCMap is identical to LE CCMaps.
# With non-BMP characters present, to avoid the misalignment on 64bit
# machines, we have to store the ccmap flag (indicating whether the map 
# is extended or not) and the BMP map size in two 32bit integers instead of
# two 16bit integers (bug 225340)
  my @fmts = $is_ext ? ("64LE", "LE", "16BE", "32BE", "64BE") : ("LE", "32BE", "64BE") ;
  foreach my $fmt (@fmts)
  { 

    my($offset) = 0;
    for ($fmt) {
      /64LE/ and do {
        @idxlist = (0, 1, 2, 3);
        @int16toint32 = (1, 0, 3, 2);
        print OUT "#if (defined(IS_LITTLE_ENDIAN) && ALU_SIZE == 64)\n" .
		          "// Precompiled CCMap for Little Endian(64bit)\n"; 
        printf OUT "#define ${variable}_SIZE %d\n", scalar @$ccmap_p + 2;
        printf OUT "#define ${variable}_INITIALIZER    \\\n";
        printf OUT "/* EXTFLG */ 0x%04X,0x0000,0x%04X,0x0000,    \\\n", 
	               $ccmap_p->[0], $ccmap_p->[1];
        last;
	    };
      /LE/ and do {
        @idxlist = (0, 1, 2, 3);
        @int16toint32 = (1, 0, 3, 2);
        print OUT $is_ext ? 
                  "#elif defined(IS_LITTLE_ENDIAN)\n" . 
                  "// Precompiled CCMap for Little Endian(16/32bit) \n" :
                  "#if (defined(IS_LITTLE_ENDIAN) || ALU_SIZE == 16)\n" . 
                  "// Precompiled CCMap for Little Endian(16/32/64bit)\n" .
                  "// and Big Endian(16bit)\n";
        printf OUT "#define ${variable}_SIZE %d\n", scalar @$ccmap_p;
        printf OUT "#define ${variable}_INITIALIZER    \\\n";
        if ($is_ext) {
             printf OUT "/* EXTFLG */ 0x%04X,0x%04X,    \\\n", 
	                   $ccmap_p->[0], $ccmap_p->[1];
        }
        last;
      };
      /16BE/ and do {
        @idxlist = (0, 1, 2, 3);
        @int16toint32 = (0, 1, 2, 3);
        print OUT "#elif (ALU_SIZE == 16)\n" .
                  "// Precompiled CCMap for Big Endian(16bit)\n";
        printf OUT "#define ${variable}_SIZE %d\n", scalar @$ccmap_p;
        printf OUT "#define ${variable}_INITIALIZER    \\\n";
        printf OUT "/* EXTFLG */ 0x%04X,0x%04X,    \\\n", 
	               $ccmap_p->[0], $ccmap_p->[1];
        last;
      };
      /32BE/ and do {
        @idxlist = (1, 0, 3, 2);
        @int16toint32 = (0, 1, 2, 3);
        print OUT "#elif (ALU_SIZE == 32)\n" .
                  "// Precompiled CCMap for  Big Endian(32bit)\n";
        printf OUT "#define ${variable}_SIZE %d\n", scalar @$ccmap_p;
        printf OUT "#define ${variable}_INITIALIZER    \\\n";
        if ($is_ext) {
             printf OUT "/* EXTFLG */ 0x%04X,0x%04X,    \\\n", 
	                   $ccmap_p->[0], $ccmap_p->[1];
        }
        last;
      };
      /64BE/ and do {
        @idxlist = (3, 2, 1, 0);
        @int16toint32 = (0, 1, 2, 3);
        print OUT "#elif (ALU_SIZE == 64)\n" .
                  "// Precompiled CCMap for Big Endian(64bit)\n";
        printf OUT "#define ${variable}_SIZE %d\n", scalar @$ccmap_p + 
                   ($is_ext ? 2 : 0);
        printf OUT "#define ${variable}_INITIALIZER    \\\n";
        if ($is_ext) {
             printf OUT "/* EXTFLG */ 0x0000,0x%04X,0x0000,0x%04X,    \\\n", 
	                   $ccmap_p->[0], $ccmap_p->[1];
        }
        last;
      };
    }

    $offset = $is_ext ? 2 : 0; 

    while ($offset < @$ccmap_p)  {
      printf OUT "/* %06x */ ", $offset - ($is_ext ? 2 : 0);
      for my $i (0 .. 3) {
        for my $j (defined($pg_flags_p->{$offset}) ? 
                   ($pg_flags_p->{$offset} > 0 ? 
                   @idxlist : @int16toint32)  : (0,1,2,3)) {
          printf OUT "0x%04X,", $ccmap_p->[$offset + $i * 4 + $j];
        }
        print OUT "    \\\n             " if $i==1; 
      }
      if ($offset + 16 < @$ccmap_p) {print OUT "    \\\n"; }
      $offset += 16;
    }
    print OUT "\n";
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

 my($variable, $class) = @_;
 sprintf <<PREAMBLE;
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Jungshik Shin <jshin\@mailaps.org>
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

      VARIABLE::$variable
      CLASS::$class
      DESCRIPTION:: description of a character class 
      FILE:: mozilla source file to include the output file
      

  Then, run the following in the current directory.

    perl ccmapbin.pl input_file [$variable [$class]] 

  which will generate $class.ccmap (or $class.x-ccmap if the ccmap
  includes non-BMP characters.). $variable is used as the prefix
  in macros for the array initializer and the array size. 

  (see bug 180266, bug 167136, and bug 224337)

 */

PREAMBLE

}
 
