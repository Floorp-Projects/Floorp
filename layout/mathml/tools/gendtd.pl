#!/bin/perl

# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla MathML Project.
# 
# The Initial Developer of the Original Code is The University Of
# Queensland.  Portions created by The University Of Queensland are
# Copyright (C) 1999 The University Of Queensland.  All
# Rights Reserved.
# 
# Contributor(s):
#   Roger B. Sidje <rbs@maths.uq.edu.au>
#

# Purpose:
#   This script processes unicode points and generates a file that
#   contains the MathML DTD, i.e., the XML definitions of
#   MathML entities.
#   RBS - Aug 27, 1999.

$usage = <<USAGE;

Usage: gendtd.pl unicode_file [entity_file]

- unicode_file is the input file. It must either be: byalpha.txt
  or bycodes.txt.
    
  The files byalpha.txt and bycodes.txt were simply obtained by visiting
  the links of the W3C MathML REC (see below) and using the Navigator's 
  command "Save as text":
  byalpha.txt: http://www.w3.org/TR/REC-MathML/chap6/byalpha.html
  bycodes.txt: http://www.w3.org/TR/REC-MathML/chap6/bycodes.html

  It is recommended to use byalpha.txt because it includes aliases (i.e., 
  all the entity names corresponding to the same unicode point)

- entity_file is the output file. It will contain the XML definitions
  of MathML entities. If entity_file is not provided, the default
  output file will be: mathml.dtd
USAGE

$badusage = 0;
$badusage = 1 if $#ARGV < 0;
$badusage = 1 if $ARGV[0] ne "byalpha.txt" && $ARGV[0] ne "bycodes.txt";
if ($badusage) {
  print $usage;
  exit(0);
}

$unicode_file = $ARGV[0];
$entity_file  = $ARGV[1];
$entity_file  = "mathml.dtd" if !$entity_file;

print "\nINPUT: $unicode_file";
print "\nOUTPUT: $entity_file\n";

# Output: the file $entity_file contains the definitions of XML
# entities for all of MathML or for the TeX symbols and/or MathML operators.

# Note: other features are commented in the code.



@IGNORE = ("amp",            # these default XML entities will not be processed 
           "lt",
           "gt");

&getTeX();                   # Get the converted TeX entities as returned by TeX2MML.
                             # Accuracy depends on the implementation of TeX2MML!
                             
&getMathMLOperators();       # Get the MathML Operators - exact *copy-paste* from
                             # http://www.w3.org/TR/REC-MathML/appendixC.html
                             
&getUnicode($unicode_file);  # $unicode_file = "byalpha.txt" or "bycodes.txt"
                             # byalpha.txt, bycodes.txt are simply the Save as text of
                             # http://www.w3.org/TR/REC-MathML/chap6/byalpha.html
                             # http://www.w3.org/TR/REC-MathML/chap6/bycodes.html                             


print "\nNumber of Symbols:  TeX = " . ($#TEX+1) .  "  MathML Operators = " . ($#OPERATOR+1);

#Check if all MathML Operators have been assigned unicode points...

print "\n\nChecking MathML Operators...";
for ($i=0; $i<=$#OPERATOR; ++$i) {
  if (!($UNICODE{$OPERATOR[$i]})) {
    print "\n$OPERATOR[$i] does not have its unicode point";
#    exit(0);
  }
  else {
#    print "\n$OPERATOR[$i] $UNICODE{$OPERATOR[$i]}";
  }
}

#Check if all TeX Symbols have been assigned unicode points...
 
print "\n\nChecking TeX Symbols...";
for ($i=0; $i<=$#TEX; ++$i) {  
  if (!($UNICODE{$TEX[$i]})) {
    print "\n$TEX[$i] does not have its unicode point";
#    exit(0);
  }
  else {
#    print "\n$TEX[$i] $UNICODE{$TEX[$i]}";
  }
}


################
# Now extract the unicode points of the entities of interest.
# Uncomment the appropriate case, and comment the others

#Case 1: Extrat the unicode points of *TeX symbols only*
#@ENTITY_SET = @TEX;

#Case 2: Extract the unicode points of *MathML Operators only*
#@ENTITY_SET = @OPERATOR;

#Case 3: Extract the unicode points of the union *TeX + MathML Operators*
#@ENTITY_SET = (@TEX,@OPERATOR);

#Case 4: Extract *all* MathML entities
#Case 4a: without aliases...
#@ENTITY_SET = @ENTITY_LAST_ALIAS{keys %ENTITY_LAST_ALIAS}; # no aliases here
#Case 4b: with aliases...
@ENTITY_SET = @ENTITY; #aliases are here if it was getUnicode("byalpha.txt").


#################
open (OUTPUT_FILE, ">$entity_file") || die("can't open $entity_file");

print "\n\nBuilding the entity list and Saving into:\n$entity_file ...\n";

# collect the aliases in an array of strings.

$count = $pua = 0;
$ignore = join(" ",@IGNORE);

for ($i=0; $i<=$#ENTITY_SET; ++$i) {
  $entity = $ENTITY_SET[$i];
  $unicode = $UNICODE{$entity};
  
  #skip if entity should be ignored
  next if $ignore =~ /$entity/;
  
  #skip if no unicode or entity is already in the table
  next if $unicode eq "" || $TABLE{$unicode} =~ /$entity/;

  $TABLE{$unicode} .= "   $entity";

  $XML_DECLARATION{$unicode} .= 
     '<!ENTITY ' . $entity . ' "&#x' . $unicode . ';"> ';
  ++$count;

  $XML_CONTENT .= '&' . $entity . ";\n";

  if ($unicode ge "E000" && $unicode le "F8FF") {
#     print "PUA\n-------------------------\n" if $pua == 0;
#     print "$unicode $entity\n";
     $TABLE_PUA{$unicode} .= "   $entity";
     ++$pua;
  }
}

foreach $unicode (sort keys %TABLE_PUA) {
  $PUA .= "$unicode $TABLE_PUA{$unicode}\n";
}

foreach $unicode (sort keys %TABLE) {
#  print OUTPUT_FILE "$unicode $TABLE{$unicode}\n";
  $XML_DOCTYPE .= $XML_DECLARATION{$unicode} . "\n";
}

print OUTPUT_FILE <<HEADER_DATA;
<!-- 
  *  The contents of this file are subject to the Mozilla Public
  *  License Version 1.1 (the "License"); you may not use this file
  *  except in compliance with the License. You may obtain a copy of
  *  the License at http://www.mozilla.org/MPL/
  *  
  *  Software distributed under the License is distributed on an "AS
  *  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
  *  implied. See the License for the specific language governing
  *  rights and limitations under the License.
  *  
  *  The Original Code is Mozilla MathML Project.
  *  
  *  The Initial Developer of the Original Code is The University of
  *  Queensland.  Portions created by The University of Queensland are
  *  Copyright (C) 1999 The University of Queensland.  All
  *  Rights Reserved.
  *  
  *  Contributor(s):
  *    Roger B. Sidje <rbs\@maths.uq.edu.au>
  --> 

<!-- MathML 1.01 entities - Auto-generated (Total in this list: $count) -->

HEADER_DATA

#print OUTPUT_FILE "<?xml version=\"1.0\"?>\n<!DOCTYPE math\n[\n";
print OUTPUT_FILE "$XML_DOCTYPE";
#print OUTPUT_FILE "]>\n";                                         


#print OUTPUT_FILE "<para>$XML_CONTENT</para>\n";
#print <<XML;
#<!--
#$PUA
#-->
#XML


close(OUTPUT_FILE);

print "Done $count unicode points. PUA: $pua\n";

exit(0);

################################






# All outputs are global variables ...

#
# extract all the symbols of the MathML REC byalpha.txt or bycodes.txt (the 
# name of the file is passed as argument) 
#  INPUT: "byalpha.txt" or "bycodes.txt"
# OUTPUT: - hash array %UNICODE holding $UNICODE{$entity} = $unicode
#         - array @ENTITY such $ENTITY[$i] is an entity name
#         - hash array %ENTITY_LAST_ALIAS such that 
#           $ENTITY_LAST_ALIAS{$unicode} = last entity with that unicode
sub getUnicode {
  local($infile) = @_[0];

  $byalpha = $infile =~ /byalpha/;

  print "\nScanning $infile ...";
  open (INFILE, $infile) || die("Can't open $infile");
 
  $count = 0;
  while (<INFILE>) {
    #pattern byalpha: entity                   isolat2       377 unicode =capital Z, acute accent
    if ($byalpha) { # byalpha -- ALIASES ARE INCLUDED	
      if ( /^([a-z\.]\S+)\s+\S+\s+\d+\s+(\S+)\s+.*/i ) {
        ($entity,$unicode) = ($1,$2);
        if ($UNICODE{$entity}) { #conflicting mapping ?
            next if $UNICODE{$entity} eq $unicode;
            print "\nWARNING! Found: $entity -> $unicode <-- $map --> $UNICODE{$entity}";
        }

        $UNICODE{$entity} = $unicode;
        $ENTITY_LAST_ALIAS{$unicode} = $entity;
        ++$count;
      }
    }
    else { # bycodes -- ALIASES ARE NOT INCLUDED 
      #pattern bycode: unicode      9  entity     mmlextra    tabulator stop; horizontal tabulation
      if ( /^(\S+)\s+\d+\s+([a-z\.]+)\s+\S+\s+.*/i ) {
        ($unicode,$entity) = ($1,$2);
#        print "\n$entity  $unicode"; 
        $UNICODE{$entity} = $unicode;
        $ENTITY_LAST_ALIAS{$unicode} = $entity;      
        ++$count;
      }
    }
  }
  @ENTITY = (keys %UNICODE);
  print "\nFound: $count unicode points, " . ($#ENTITY+1) . " entities\n";
}    

#Get entity names of TeX Symbols
#Symbols obtained by using TeX2MathML (http://hutchinson.belmont.ma.us/tth/mml/tthmmlform.html)
#The input snippets were taken from David Carlisle's symbols.tex  (ver 3.2)
#  INPUT: 
# OUTPUT: an array @TEX such that $TEX[$i] is the name of an entity
sub getTeX {
@SYMBOLS = <<TEX2MathML;
&alpha;&theta;o&tau;&beta;&thetav;&pi;&upsi;&gamma;&iota;&piv;&phi;
&delta;&kappa;&rho;&phiv;&epsi;&lambda;&rhov;&chi;&epsiv;&mu;&sigma;
&psi;&zeta;&nu;&sigmav;&omega;&eta;i&Gamma;&Lambda;&Sigma;&Psi;
&Delta;i&Upsi;&Omega;&Theta;&Pi;&Phi;&emsp;
&PlusMinus;&cap;&loz;&oplus;&mp;&cup;\bigtriangleup\ominus×\uplus
\bigtriangledown&otimes;÷\sqcap\triangleleftø&ast;\sqcup\triangleright
\odot&star;&vee;\lhd\bigcirc&circ;&and;\rhd\dagger&bullet;&setminus;\unlhd\ddagger·r\unrhd\amalg
&le;&ge;&equiv;\models\prec\succ~&bottom;\preceq\succeq&ap;\mid <<
>>\asymp&parallel;&sub;&sup;&ap;\bowtie&sube;&supe;&cong;\Join\sqsubset\sqsupset&ne;
\smile\sqsubseteq\sqsupseteq\doteq\frown&isin;&ni;&prop;=\vdash\dashv
&larr;&larr;&uarr;&lArr;&lArr;&uArr;&rarr;&rarr;&darr;&rArr;&rArr;&uArr;&lrarr;&lrarr;
&darr;&lrArr;&lrArr;&darr;&rarr;\longmapsto\nearrow\hookleftarrow\hookrightarrow\searrow
\leftharpoonup\rightharpoonup\swarrow\leftharpoondown\rightharpoondown\nwarrow\rightleftharpoons\leadsto
&mldr;&mldr;:&aleph;'&forall;&infin;&empty;&exist;&imath;&dtri;&not;\Diamond&jmath;
&radic;\flat\triangle&ell;&top;\natural&clubs;&weierp;&bot;\sharp&diams;&rfraktur;
&bsol;&hearts;&ifraktur;&angle;&part;&spades;\mho
&sum;&bigcap;\bigodot&Pi;&bigcup;&bigotimes;\coprod\bigsqcup&bigoplus;&int;&bigvee;\biguplus&oint;&bigwedge;
&Pr;
()&uarr;&uArr;[]&darr;&uArr;{}&darr;&darr;&lfloor;&rfloor;&lceil;&rceil;&lang;&rang;/&bsol;&Verbar;
TEX2MathML

# extract the relevant part
  $count = 0;
  $string = join("",@SYMBOLS);
  $string =~ s#\n##g;
  $string =~ s#[^a-zA-Z\&;]##g;
  $string =~ s#;#;\n#g;
  while ($string =~ m#\&([a-zA-z]+);#g) {
    $TEX[$count] = $1;
    ++$count;
  }
}

#Get entity names of MathML Operators
#  INPUT: 
# OUTPUT: an array @OPERATOR such that $OPERATOR[$i] is the name of an operator entity
sub getMathMLOperators {
@SYMBOLS = <<MathMLOperatorDictionary;
             "("                                  form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             ")"                                  form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "["                                  form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "]"                                  form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "{"                                  form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "}"                                  form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&CloseCurlyDoubleQuote;"            form="postfix" fence="true"  lspace="0em" rspace="0em"
             "&CloseCurlyQuote;"                  form="postfix" fence="true"  lspace="0em" rspace="0em"
             "&LeftAngleBracket;"                 form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&LeftBracketingBar;"                form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&LeftCeiling;"                      form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&LeftDoubleBracket;"                form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&LeftDoubleBracketingBar;"          form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&LeftFloor;"                        form="prefix"  fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&OpenCurlyDoubleQuote;"             form="prefix"  fence="true"  lspace="0em" rspace="0em"
             "&OpenCurlyQuote;"                   form="prefix"  fence="true"  lspace="0em" rspace="0em"
             "&RightAngleBracket;"                form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&RightBracketingBar;"               form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&RightCeiling;"                     form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&RightDoubleBracket;"               form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&RightDoubleBracketingBar;"         form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&RightFloor;"                       form="postfix" fence="true" stretchy="true"  lspace="0em" rspace="0em"
             "&LeftSkeleton;"                     form="prefix"  fence="true"  lspace="0em" rspace="0em"
             "&RightSkeleton;"                    form="postfix" fence="true"  lspace="0em" rspace="0em"

             "&InvisibleComma;"                   form="infix"   separator="true"  lspace="0em" rspace="0em"

             ","                                  form="infix"   separator="true"  lspace="0em" rspace=".33333em"

             "&HorizontalLine;"                   form="infix"   stretchy="true" minsize="0"  lspace="0em" rspace="0em"
             "&VerticalLine;"                     form="infix"   stretchy="true" minsize="0"  lspace="0em" rspace="0em"

             ";"                                  form="infix"   separator="true"  lspace="0em" rspace=".27777em"
             ";"                                  form="postfix" separator="true"  lspace="0em" rspace="0em"

             ":="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&Assign;"                           form="infix"    lspace=".27777em" rspace=".27777em"

             "&Because;"                          form="infix"    lspace=".27777em" rspace=".27777em"
             "&Therefore;"                        form="infix"    lspace=".27777em" rspace=".27777em"

             "&VerticalSeparator;"                form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"

             "//"                                 form="infix"    lspace=".27777em" rspace=".27777em"

             "&Colon;"                            form="infix"    lspace=".27777em" rspace=".27777em"

             "&amp;"                              form="prefix"   lspace="0em" rspace=".27777em"
             "&amp;"                              form="postfix"  lspace=".27777em" rspace="0em"

             "*="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "-="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "+="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "/="                                 form="infix"    lspace=".27777em" rspace=".27777em"

             "->"                                 form="infix"    lspace=".27777em" rspace=".27777em"

             ":"                                  form="infix"    lspace=".27777em" rspace=".27777em"

             ".."                                 form="postfix"  lspace=".22222em" rspace="0em"
             "..."                                form="postfix"  lspace=".22222em" rspace="0em"

             "&SuchThat;"                         form="infix"    lspace=".27777em" rspace=".27777em"

             "&DoubleLeftTee;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&DoubleRightTee;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&DownTee;"                          form="infix"    lspace=".27777em" rspace=".27777em"
             "&LeftTee;"                          form="infix"    lspace=".27777em" rspace=".27777em"
             "&RightTee;"                         form="infix"    lspace=".27777em" rspace=".27777em"

             "&Implies;"                          form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RoundImplies;"                     form="infix"    lspace=".27777em" rspace=".27777em"

             "|"                                  form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "||"                                 form="infix"    lspace=".22222em" rspace=".22222em"
             "&Or;"                               form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"

             "&amp;&amp;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&And;"                              form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"

             "&amp;"                              form="infix"    lspace=".27777em" rspace=".27777em"

             "!"                                  form="prefix"   lspace="0em" rspace=".27777em"
             "&Not;"                              form="prefix"   lspace="0em" rspace=".27777em"

             "&Exists;"                           form="prefix"   lspace="0em" rspace=".27777em"
             "&ForAll;"                           form="prefix"   lspace="0em" rspace=".27777em"
             "&NotExists;"                        form="prefix"   lspace="0em" rspace=".27777em"

             "&Element;"                          form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotElement;"                       form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotReverseElement;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSquareSubset;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSquareSubsetEqual;"             form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSquareSuperset;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSquareSupersetEqual;"           form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSubset;"                        form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSubsetEqual;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSuperset;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSupersetEqual;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&ReverseElement;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&SquareSubset;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&SquareSubsetEqual;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&SquareSuperset;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&SquareSupersetEqual;"              form="infix"    lspace=".27777em" rspace=".27777em"
             "&Subset;"                           form="infix"    lspace=".27777em" rspace=".27777em"
             "&SubsetEqual;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&Superset;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&SupersetEqual;"                    form="infix"    lspace=".27777em" rspace=".27777em"

             "&DoubleLeftArrow;"                  form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DoubleLeftRightArrow;"             form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DoubleRightArrow;"                 form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownLeftRightVector;"              form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownLeftTeeVector;"                form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownLeftVector;"                   form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownLeftVectorBar;"                form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownRightTeeVector;"               form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownRightVector;"                  form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&DownRightVectorBar;"               form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftArrow;"                        form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftArrowBar;"                     form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftArrowRightArrow;"              form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftRightArrow;"                   form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftRightVector;"                  form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftTeeArrow;"                     form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftTeeVector;"                    form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftVector;"                       form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LeftVectorBar;"                    form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LowerLeftArrow;"                   form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&LowerRightArrow;"                  form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightArrow;"                       form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightArrowBar;"                    form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightArrowLeftArrow;"              form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightTeeArrow;"                    form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightTeeVector;"                   form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightVector;"                      form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightVectorBar;"                   form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&ShortLeftArrow;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&ShortRightArrow;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&UpperLeftArrow;"                   form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&UpperRightArrow;"                  form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"

             "="                                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&lt;"                               form="infix"    lspace=".27777em" rspace=".27777em"
             ">"                                  form="infix"    lspace=".27777em" rspace=".27777em"
             "!="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "=="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&lt;="                              form="infix"    lspace=".27777em" rspace=".27777em"
             ">="                                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&Congruent;"                        form="infix"    lspace=".27777em" rspace=".27777em"
             "&CupCap;"                           form="infix"    lspace=".27777em" rspace=".27777em"
             "&DotEqual;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&DoubleVerticalBar;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&Equal;"                            form="infix"    lspace=".27777em" rspace=".27777em"
             "&EqualTilde;"                       form="infix"    lspace=".27777em" rspace=".27777em"
             "&Equilibrium;"                      form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&GreaterEqual;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&GreaterEqualLess;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&GreaterFullEqual;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&GreaterGreater;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&GreaterLess;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&GreaterSlantEqual;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&GreaterTilde;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&HumpDownHump;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&HumpEqual;"                        form="infix"    lspace=".27777em" rspace=".27777em"
             "&LeftTriangle;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&LeftTriangleBar;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&LeftTriangleEqual;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&le;"                               form="infix"    lspace=".27777em" rspace=".27777em"
             "&LessEqualGreater;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&LessFullEqual;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&LessGreater;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&LessLess;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&LessSlantEqual;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&LessTilde;"                        form="infix"    lspace=".27777em" rspace=".27777em"
             "&NestedGreaterGreater;"             form="infix"    lspace=".27777em" rspace=".27777em"
             "&NestedLessLess;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotCongruent;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotCupCap;"                        form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotDoubleVerticalBar;"             form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotEqual;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotEqualTilde;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreater;"                       form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreaterEqual;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreaterFullEqual;"              form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreaterGreater;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreaterLess;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreaterSlantEqual;"             form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotGreaterTilde;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotHumpDownHump;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotHumpEqual;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLeftTriangle;"                  form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLeftTriangleBar;"               form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLeftTriangleEqual;"             form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLess;"                          form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLessEqual;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLessFullEqual;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLessGreater;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLessLess;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLessSlantEqual;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotLessTilde;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotNestedGreaterGreater;"          form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotNestedLessLess;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotPrecedes;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotPrecedesEqual;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotPrecedesSlantEqual;"            form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotPrecedesTilde;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotRightTriangle;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotRightTriangleBar;"              form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotRightTriangleEqual;"            form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSucceeds;"                      form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSucceedsEqual;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSucceedsSlantEqual;"            form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotSucceedsTilde;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotTilde;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotTildeEqual;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotTildeFullEqual;"                form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotTildeTilde;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&NotVerticalBar;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&Precedes;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&PrecedesEqual;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&PrecedesSlantEqual;"               form="infix"    lspace=".27777em" rspace=".27777em"
             "&PrecedesTilde;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&Proportion;"                       form="infix"    lspace=".27777em" rspace=".27777em"
             "&Proportional;"                     form="infix"    lspace=".27777em" rspace=".27777em"
             "&ReverseEquilibrium;"               form="infix"   stretchy="true"  lspace=".27777em" rspace=".27777em"
             "&RightTriangle;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&RightTriangleBar;"                 form="infix"    lspace=".27777em" rspace=".27777em"
             "&RightTriangleEqual;"               form="infix"    lspace=".27777em" rspace=".27777em"
             "&Succeeds;"                         form="infix"    lspace=".27777em" rspace=".27777em"
             "&SucceedsEqual;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&SucceedsSlantEqual;"               form="infix"    lspace=".27777em" rspace=".27777em"
             "&SucceedsTilde;"                    form="infix"    lspace=".27777em" rspace=".27777em"
             "&Tilde;"                            form="infix"    lspace=".27777em" rspace=".27777em"
             "&TildeEqual;"                       form="infix"    lspace=".27777em" rspace=".27777em"
             "&TildeFullEqual;"                   form="infix"    lspace=".27777em" rspace=".27777em"
             "&TildeTilde;"                       form="infix"    lspace=".27777em" rspace=".27777em"
             "&UpTee;"                            form="infix"    lspace=".27777em" rspace=".27777em"
             "&VerticalBar;"                      form="infix"    lspace=".27777em" rspace=".27777em"

             "&SquareUnion;"                      form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"
             "&Union;"                            form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"
             "&UnionPlus;"                        form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"

             "-"                                  form="infix"    lspace=".22222em" rspace=".22222em"
             "+"                                  form="infix"    lspace=".22222em" rspace=".22222em"
             "&Intersection;"                     form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"
             "&MinusPlus;"                        form="infix"    lspace=".22222em" rspace=".22222em"
             "&PlusMinus;"                        form="infix"    lspace=".22222em" rspace=".22222em"
             "&SquareIntersection;"               form="infix"   stretchy="true"  lspace=".22222em" rspace=".22222em"

             "&Vee;"                              form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "&CircleMinus;"                      form="prefix"  largeop="true" movablelimits="true"  lspace="0em" rspace=".16666em"
             "&CirclePlus;"                       form="prefix"  largeop="true" movablelimits="true"  lspace="0em" rspace=".16666em"
             "&Sum;"                              form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "&Union;"                            form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "&UnionPlus;"                        form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "lim"                                form="prefix"  movablelimits="true"  lspace="0em" rspace=".16666em"
             "max"                                form="prefix"  movablelimits="true"  lspace="0em" rspace=".16666em"
             "min"                                form="prefix"  movablelimits="true"  lspace="0em" rspace=".16666em"

             "&CircleMinus;"                      form="infix"    lspace=".16666em" rspace=".16666em"
             "&CirclePlus;"                       form="infix"    lspace=".16666em" rspace=".16666em"

             "&ClockwiseContourIntegral;"         form="prefix"  largeop="true" stretchy="true"  lspace="0em" rspace="0em"
             "&ContourIntegral;"                  form="prefix"  largeop="true" stretchy="true"  lspace="0em" rspace="0em"
             "&CounterClockwiseContourIntegral;"  form="prefix"  largeop="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DoubleContourIntegral;"            form="prefix"  largeop="true" stretchy="true"  lspace="0em" rspace="0em"
             "&Integral;"                         form="prefix"  largeop="true" stretchy="true"  lspace="0em" rspace="0em"

             "&Cup;"                              form="infix"    lspace=".16666em" rspace=".16666em"

             "&Cap;"                              form="infix"    lspace=".16666em" rspace=".16666em"

             "&VerticalTilde;"                    form="infix"    lspace=".16666em" rspace=".16666em"

             "&Wedge;"                            form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "&CircleTimes;"                      form="prefix"  largeop="true" movablelimits="true"  lspace="0em" rspace=".16666em"
             "&Coproduct;"                        form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "&Product;"                          form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"
             "&Intersection;"                     form="prefix"  largeop="true" movablelimits="true" stretchy="true"  lspace="0em" rspace=".16666em"

             "&Coproduct;"                        form="infix"    lspace=".16666em" rspace=".16666em"

             "&Star;"                             form="infix"    lspace=".16666em" rspace=".16666em"

             "&CircleDot;"                        form="prefix"  largeop="true" movablelimits="true"  lspace="0em" rspace=".16666em"

             "*"                                  form="infix"    lspace=".16666em" rspace=".16666em"
             "&InvisibleTimes;"                   form="infix"    lspace="0em" rspace="0em"

             "&CenterDot;"                        form="infix"    lspace=".16666em" rspace=".16666em"

             "&CircleTimes;"                      form="infix"    lspace=".16666em" rspace=".16666em"

             "&Vee;"                              form="infix"    lspace=".16666em" rspace=".16666em"

             "&Wedge;"                            form="infix"    lspace=".16666em" rspace=".16666em"

             "&Diamond;"                          form="infix"    lspace=".16666em" rspace=".16666em"

             "&Backslash;"                        form="infix"   stretchy="true"  lspace=".16666em" rspace=".16666em"

             "/"                                  form="infix"   stretchy="true"  lspace=".16666em" rspace=".16666em"

             "-"                                  form="prefix"   lspace="0em" rspace=".05555em"
             "+"                                  form="prefix"   lspace="0em" rspace=".05555em"
             "&MinusPlus;"                        form="prefix"   lspace="0em" rspace=".05555em"
             "&PlusMinus;"                        form="prefix"   lspace="0em" rspace=".05555em"

             "."                                  form="infix"    lspace="0em" rspace="0em"

             "&Cross;"                            form="infix"    lspace=".11111em" rspace=".11111em"

             "**"                                 form="infix"    lspace=".11111em" rspace=".11111em"

             "&CircleDot;"                        form="infix"    lspace=".11111em" rspace=".11111em"

             "&SmallCircle;"                      form="infix"    lspace=".11111em" rspace=".11111em"

             "&Square;"                           form="prefix"   lspace="0em" rspace=".11111em"

             "&Del;"                              form="prefix"   lspace="0em" rspace=".11111em"
             "&PartialD;"                         form="prefix"   lspace="0em" rspace=".11111em"

             "&CapitalDifferentialD;"             form="prefix"   lspace="0em" rspace=".11111em"
             "&DifferentialD;"                    form="prefix"   lspace="0em" rspace=".11111em"

             "&Sqrt;"                             form="prefix"  stretchy="true"  lspace="0em" rspace=".11111em"

             "&DoubleDownArrow;"                  form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DoubleLongLeftArrow;"              form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DoubleLongLeftRightArrow;"         form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DoubleLongRightArrow;"             form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DoubleUpArrow;"                    form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DoubleUpDownArrow;"                form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DownArrow;"                        form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DownArrowBar;"                     form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DownArrowUpArrow;"                 form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&DownTeeArrow;"                     form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftDownTeeVector;"                form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftDownVector;"                   form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftDownVectorBar;"                form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftUpDownVector;"                 form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftUpTeeVector;"                  form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftUpVector;"                     form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LeftUpVectorBar;"                  form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LongLeftArrow;"                    form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LongLeftRightArrow;"               form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&LongRightArrow;"                   form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&ReverseUpEquilibrium;"             form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightDownTeeVector;"               form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightDownVector;"                  form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightDownVectorBar;"               form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightUpDownVector;"                form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightUpTeeVector;"                 form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightUpVector;"                    form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&RightUpVectorBar;"                 form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&ShortDownArrow;"                   form="infix"    lspace=".11111em" rspace=".11111em"
             "&ShortUpArrow;"                     form="infix"    lspace=".11111em" rspace=".11111em"
             "&UpArrow;"                          form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&UpArrowBar;"                       form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&UpArrowDownArrow;"                 form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&UpDownArrow;"                      form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&UpEquilibrium;"                    form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"
             "&UpTeeArrow;"                       form="infix"   stretchy="true"  lspace=".11111em" rspace=".11111em"

             "^"                                  form="infix"    lspace=".11111em" rspace=".11111em"

             "&lt;>"                              form="infix"    lspace=".11111em" rspace=".11111em"

             "'"                                  form="postfix"  lspace=".11111em" rspace="0em"

             "!"                                  form="postfix"  lspace=".11111em" rspace="0em"
             "!!"                                 form="postfix"  lspace=".11111em" rspace="0em"

             "~"                                  form="infix"    lspace=".11111em" rspace=".11111em"

             "@"                                  form="infix"    lspace=".11111em" rspace=".11111em"

             "--"                                 form="postfix"  lspace=".11111em" rspace="0em"
             "--"                                 form="prefix"   lspace="0em" rspace=".11111em"
             "++"                                 form="postfix"  lspace=".11111em" rspace="0em"
             "++"                                 form="prefix"   lspace="0em" rspace=".11111em"

             "&ApplyFunction;"                    form="infix"    lspace="0em" rspace="0em"

             "?"                                  form="infix"    lspace=".11111em" rspace=".11111em"

             "_"                                  form="infix"    lspace=".11111em" rspace=".11111em"

             "&Breve;"                            form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&Cedilla;"                          form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&DiacriticalGrave;"                 form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&DiacriticalDot;"                   form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&DiacriticalDoubleAcute;"           form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&DiacriticalLeftArrow;"             form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DiacriticalLeftRightArrow;"        form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DiacriticalLeftRightVector;"       form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DiacriticalLeftVector;"            form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DiacriticalAcute;"                 form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&DiacriticalRightArrow;"            form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DiacriticalRightVector;"           form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DiacriticalTilde;"                 form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&DoubleDot;"                        form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&DownBreve;"                        form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&Hacek;"                            form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&Hat;"                              form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&OverBar;"                          form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&OverBrace;"                        form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&OverBracket;"                      form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&OverParenthesis;"                  form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&TripleDot;"                        form="postfix" accent="true"  lspace="0em" rspace="0em"
             "&UnderBar;"                         form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&UnderBrace;"                       form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&UnderBracket;"                     form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
             "&UnderParenthesis;"                 form="postfix" accent="true" stretchy="true"  lspace="0em" rspace="0em"
MathMLOperatorDictionary

# extract the relevant part
  $count = 0;
  $string = join("",@SYMBOLS);
  $string =~ s#\n##g;
  $string =~ s#[^a-zA-Z\&;]##g;
  $string =~ s#;#;\n#g;
  while ($string =~ m#\&([a-zA-z]+);#g) {
     $OPERATOR[$count] = $1;
     ++$count;
  }
}