#!/usr/local/bin/perl 

$pr = 1;
$ignore = 0;


print<<EOM;
/* -*- Mode: C -*-*/
/*======================================================================
  FILE: ical.i

  (C) COPYRIGHT 1999 Eric Busboom
  http://www.softwarestudio.org

  The contents of this file are subject to the Mozilla Public License
  Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
  the License for the specific language governing rights and
  limitations under the License.

  The original author is Eric Busboom

  Contributions from:
  Graham Davison (g.m.davison\@computer.org)

  ======================================================================*/  

%module Net__ICal

%{
#include "ical.h"

#include <sys/types.h> /* for size_t */
#include <time.h>

%}

EOM

foreach $f (@ARGV)
{
  @f = split(/\//,$f);
  $fn = pop(@f);

  $fn=~/pvl/ && next;
  $fn=~/sspm/ && next;
  $fn=~/cstp/ && next;
   $fn=~/csdb/ && next;
   $fn=~/vcal/ && next;
   $fn=~/yacc/ && next;
   $fn=~/lex/ && next;

  print "\n/**********************************************************************\n";

  print "\t$fn\n";
  print "**********************************************************************/\n\n";


  open F, $f;

  while(<F>){
    
    s/^#.*//;

    s/\/\*.*\*\///;
    
    /^\/\// && next;

    next if /^$/;
    
    if(/\/\*/){ $pr = 0;}
    
    /icalparser_parse\(/ and $ignore = 1;
    /vanew/ and $ignore = 1; 
    /_stub/ and $ignore = 1; 
    /_response/ and $ignore = 1; 
    /line_gen_func/ and $ignore = 1; 
    /extern/  and $ignore = 1; 
    /sspm/  and $ignore = 1; 
    /icalrecur/  and $ignore = 1; 

    if ($pr == 1 && $ignore == 0){
      print ;
    }
    
    
    if(/\*\//){ $pr = 1;}

    if (/\;/){ $ignore = 0; }

  }

}


