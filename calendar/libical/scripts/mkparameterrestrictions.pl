#!/usr/bin/perl 

# Version: 1.0
# Script last updated: 30May1999 GMD
# Change log:
# <none>


# usually open params-in-prop.txt
open(F,"$ARGV[0]") || die "Can't open restriction file $ARGV[0]:$!";

print <<EOM;
/*
  ======================================================================
  File: parameterrestrictions.c
    
 (C) COPYRIGHT 1999 Graham Davison
 mailto:g.m.davison\@computer.org

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 ======================================================================*/

/*
 * THIS FILE IS MACHINE GENERATED DO NOT EDIT
 */


int icalrestriction_is_parameter_allowed(icalproperty_kind prop,icalparameter_kind param)
{
	switch (prop)
	{
EOM

while(<F>)
{
	chop;
	
	# split line by whitespace
	my @v = split(/\s+/,$_);
	# property is first item on line
	my $prop = shift @v;
	my $prop_name = $prop;
	if (substr($prop,0,1) eq "X") { $prop = "X"; }
	$prop = join("",split(/-/,$prop));
	
print <<EOM;

		/* ${prop_name} */
		case ICAL_${prop}_PROPERTY:
			switch (param)
			{
EOM

	foreach $param (@v)
	{
		$param = join("",split(/-/,$param));
		print "\t\t\t\tcase ICAL_${param}_PARAMETER:\n";
	}
	
print <<EOM;
					return 1;
				default:
					return 0;
			}
			
EOM

}

print <<EOM;
	}
	
	return 0;
}	
EOM
