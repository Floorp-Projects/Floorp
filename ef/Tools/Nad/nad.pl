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

#
# nad.pl
#
# Takes a nad file (Netscape Architecture Definition) and
# produces a header with the Terminals and Named rules in it
# and prints the BURG specification to stdout.
#
# Usage:
#   perl nad.pl <nad file> <PrimitiveOperations source> <PrimitiveOperations Enum FN> <DataNodeTemplates FN> <Named Rule Enum File> > <Burg File>
#

# rules in BURG must be >= 1
# terminals in BURG must be >= 1

# Fields of a $gPrimitiveInfo structure
$nameIndex = 0;
$categoryIndex = 1;
$usageIndex = 2;
$commentIndex = 3;

# DataNode flag numbers
$dnIsReal = 0;
$dnCanRaiseException = 1;
$dnIsRoot = 2;

# Special kinds
$voidKind = "vkVoid";
$shortOrigin = "aoVariable";

# grab the arguments
($NadFile, $PrimitiveOperationsSourceFN, $PrimitiveOperationsEnumFN, $DataNodeTemplateFN, $BurgHeaderFN) = @ARGV;

#print STDERR "$PrimitiveOperationsSourceFN\n";

# read PrimitiveOperation infos
open PrimitiveOperationsSourceFN or die "Couldn't open $PrimitiveOperationsSourceFN: $!\n";
$readingStage = 0;
while (<PrimitiveOperationsSourceFN>)
{
	if ($readingStage == 0) {
		if (/^\s*((?:\/\/.*)?\n)$/) {
			push @headerLines,$1;
		} else {
			$readingStage = 1;
		}
	}
	if ($readingStage == 1) {
		if (/^\s*ARG-ORIGIN\s+'(.)'\s+(\w+)\s*;\s*\n$/) {
			$argOrigins{$1} = $2;
		} elsif (/^\s*ARG-KIND\s+'(.)'\s+(\w+)\s*;\s*\n$/) {
			$argKinds{$1} = $2;
		} elsif (/^\s*SHORT-ARG\s+'(.)'\s+(\w+)\s*;\s*\n$/) {
			$shortArgs{$1} = $2;
		} elsif (!/^\s*(\/\/.*)?\n$/) {
			$readingStage = 2;
		}
	}
	if ($readingStage == 2) {
		die "Bad line: $_\n" unless m/^(?:\s*{(\w+),\s*(\w+),\s*"([^"\s]*)"})?(?:\s*(\/\/.*))?\n?$/;
		# print STDERR "--->$1***$2***$3***$4\n";
		#($name, $category, $usage, $comment) = ($1, $2, $3, $4);
		push @gPrimitiveInfo, [$1, $2, $3, $4];
	}
}
close PrimitiveOperationsSourceFN;

$argOrigins = "";
$argKinds = "";
$shortArgs = "";
foreach (keys %argOrigins) {$argOrigins .= $_;}
foreach (keys %argKinds) {$argKinds .= $_;}
foreach (keys %shortArgs) {$shortArgs .= $_;}
#print STDERR "argOrigins = \"$argOrigins\"\n";
#print STDERR "argKinds = \"$argKinds\"\n";
#print STDERR "shortArgs = \"$shortArgs\"\n";

$maxArgOriginNameLength = maxStringLength(values %argOrigins);
$maxValueKindNameLength = maxStringLength(values %argKinds, values %shortArgs);
$maxPrimitiveNameLength = maxStringLength(map $_->[$nameIndex], @gPrimitiveInfo);
$maxCategoryNameLength = maxStringLength(map $_->[$categoryIndex], @gPrimitiveInfo);

# do DataNodeTemplates
unlink ($DataNodeTemplateFN) or break;
outputNodeTemplates($DataNodeTemplateFN, \@gPrimitiveInfo);

# do PrimitiveOperations enum
unlink ($PrimitiveOperationsEnumFN) or break;
$gLastPrimEnumName = outputEnumFromListX("PrimitiveOperation", $PrimitiveOperationsEnumFN, 0, \@gPrimitiveInfo, \&primInfoToEnumDescriptor);
open FILE, ">>$PrimitiveOperationsEnumFN" or die "Couldn't open $PrimitiveOperationsEnumFN: $!\n";
print FILE "\nconst uint nPrimitiveOperations = $gLastPrimEnumName + 1;\n";
close FILE;

# now handle the BURG section of the file
open NadFile or die "Couldn't open $NadFile: $!\n";
while (<NadFile>)
{
    if (/%(.*)/)
    {
        $section = $1;

        if ($section eq "terminals")
        {
            #print STDERR "processing terminals...\n";
            while(<NadFile>)
            {
                if (/%/)
                {
                    last;
                }
                else
                {
                    chop;
                    $ts[++$#ts] = $_;
                }
            }
        }
        elsif($section eq "startsymbols")
        {
            #print STDERR "processing startsymbols...\n";
            while(<NadFile>)
            {
                if (/%/)
                {
                    last;
                }
                else
                {
                    chop;
                    $sss[++$#sss] = $_;
                }
            }            
        }
        elsif($section eq "top")
        {
            #print STDERR "processing top...\n";
            while(<NadFile>)
            {
                if (/%/)
                {
                    last;
                }
                else
                {
                    $top[++$#top] = $_;
                }
            }            
        }
        elsif($section eq "nonterm_header")
        {
           #print STDERR "processing nonterm_header...\n";
            while(<NadFile>)
            {
                if (/%/)
                {
                    last;
                }
                else
                {
                    $BurgHeaderFN[++$#header] = $_;
                }
            }            
        }
        elsif($section eq "grammar")
        {
            #print STDERR "processing grammar...\n";
            while(<NadFile>)
            {
                if (/%/)
                {
                    last;
                }
                else
                {
                    chop;
					$old = $_;
					if (!s/\/\/.*//)
					{
						$_ = $old;
						if (s/\w//)
						{
							$_ = $old;
							($rules[++$#rules], $costs[++$#costs], $rulenames[++$#rulenames])  = split(/\$/, $_);
							$costs[$#costs] =~ s/\s//g;
							$rulenames[$#rulenames] =~ s/\s//g;
						}
					}
                } 
            }            
            

        }
    }
}

# Do NamedRules enum file
unlink ($BurgHeaderFN) or break;
outputEnumFromListX("NamedRules", $BurgHeaderFN, 1, \@rulenames, \&ruleNameToEnumDescriptor);

# print out cruft at top
print "%{\n";
foreach $x (@top)
{
    print $x;
}
print "%}\n";

# print out start symbol
# we use a false one so we can have multiple start symbols
print "%start xxxGeneratedStartSymbol\n";

# print terminals
$termno = 1;
foreach $x (@primoperations)
{
	$_ = $x;
	if (s/\w+//)
	{
   		print "%term $x = $termno\n";
    	$termno++;
	}
}

# convert a primitive info into a list of terminal names
# skip first terminal which is pkNone
@ts = ();
foreach $x (@gPrimitiveInfo[1..$#gPrimitiveInfo])
{
	push @ts, $x->[$nameIndex];
}

# print out "%term BLAHBLAH = number"
foreach $x (@ts)
{
	$_ = $x;
	if (s/\w+//)
	{
   		print "%term $x = $termno\n";
    	$termno++;
	}
}

# start counting anonruleno's at the number rules + number of start symbols
$anonruleno = $#rules +1 + $#sss + 1;
print "%%\n";


# now print out the start symbols
# start symbols are anonymous rules
foreach $x (@sss)
{
    print "xxxGeneratedStartSymbol:\t$x \t = $anonruleno (0);\n";
    $anonruleno++;
}


# now print out the rules
# if it is a named rule we store it away in 
# an array of named rules so we can print the enum out later
# named rules start at 0 anonymous rules pick up where sss's left of
$namedruleno = 1;
$i = 0;
foreach $x (@rules)
{
	#print STDERR "$i rulename:  ***$rulenames[$i]*** ***$costs[$i]***\n";
	
    if ($rulenames[$i] eq "")
    {
        # this is an anonymous rule
		#print STDERR "anon\n";

        print "$x\t = $anonruleno ($costs[$i]);\n"; 
        $anonruleno++;
    }
    else
    {
        print "$x\t = $namedruleno ($costs[$i]);\n";
        $namedrules[++$#namedrules] = $rulenames[$i];
        $namedruleno++;
    }

    $i = $i + 1;
}

print "%%\n";


# primInfoToEnumDescriptor
#
# Convert a primInfo to a ($enumname, $enumcomment) list
sub primInfoToEnumDescriptor
{
	my ($primInfo) = @_;
	return ($$primInfo[$nameIndex], $$primInfo[$commentIndex]);
}

# primInfoToEnumDescriptor
#
# Convert a namedrule (a string) to a ($enumname, $enumcomment) list
sub ruleNameToEnumDescriptor
{
	my ($name) = @_;
	return ($name, "");	
}


#	outputGeneratedHeader
#
#	Output header of generated file.
sub outputGeneratedHeader
{
	my ($fh, $filename) = @_;
	
	$_ = $filename;
	
	if (!(s/.*:([^:]+)\Z/$1/))
	{
		s/(.*)/$1/;
	}
	
	$filename = $1;

	print $fh "//\n// $filename\n//\n// Generated file\n// DO NOT EDIT\n//\n\n";
}


# outputEnumFromListX
#
# in
#    name: name of this enum, will be printed as a comment above
#    fileName: file name which to append this enum
#    enumBase: value of first enum (usually either 0 or 1)
#    items: list of items
#	 convertFunc: function that transforms an item into a (enumname, enumcomment) list
#
# out
#    the file $fileName with the appended enum
#    returns the last enum output
# 
sub outputEnumFromListX
{
    my ($name, $fileName, $enumBase, $items, $convertFunc) = @_;
	my $i;
	my $lastEnum;
	my $lastName;

    for ($i = $#$items; $i >= 0; $i--)
    {
		my ($name, $comment) = &$convertFunc($$items[$i]);
		if ($name ne "")
		{
			$lastEnum = $i;
			$lastName = $name;
			last;
		}
	}
	#($hi, $crap) = &$convertFunc($$items[$lastEnum]);
	#print STDERR "----> $lastEnum, $hi <---\n";

    open FILE, ">>$fileName" or die "Couldn't open $fileName: $!\n";

	outputGeneratedHeader(\*FILE, $fileName);
    print FILE "enum $name\n";
    print FILE "{\n";

	my $enumCount = $enumBase;		# used so we don't count spaces, but still go through the whole array
    for ($i = 0; $i <= $#$items; $i++)
    {
		# print previous item's comma and comment
		# if it has a name, then we print out the enum name, else we just print the comment
		my ($name, $comment) = &$convertFunc($$items[$i]);

		if ($name ne "")
		{
			printf FILE "\t%-20s\t%s\n", "$name = $enumCount" . ($i == $lastEnum ? "" : ","), $comment;
			$enumCount++;
		}
		elsif ($comment eq "")
			{printf FILE "\n";}
		else
			{printf FILE "\t%-20s\t%s\n", "", $comment;}
    }
    print FILE "};\n";
    close FILE;

	return $lastName;
}

sub getResult
{
	my ($expression) = @_;
	
	#print STDERR "getResult: $expression\n";
	$_ = $expression;
	if (s/(.*)?->(.*)/$1 $2/)
	{
		#print STDERR "getResult -- lhs -- : $1\n";
		#print STDERR "getResult -- rhs -- : $2\n";
		return $2;
	}
}

sub getArgs
{
	my ($expression) = @_;
	my @returnVal;
	#print STDERR "return val is $#returnVal\n";
	
	$_ = $expression;
	if (s/(.*)?->(.*)//)
	{	
		$lhs = $1;
		#print STDERR "$lhs XXXX $2\n";
		$_ = $lhs;
		
		while (s/\b([A-Za-z\(\)]+)//)
		{
			#print STDERR "$1 foo \n";
			$returnVal[++$#returnVal] = $1;
		}
	}
	
	#print STDERR "return val is $#returnVal\n";
	return @returnVal;
}


#
# Given a list of strings, return the length of the longest one.
#
sub maxStringLength
{
	my $max = 0;
	foreach (@_)
		{$max = length if $max < length;}
	return $max;
}


#
# Ensure that the usage string has the proper format.
# Return the outputs and inputs strings.
#
sub verifyUsage
{
	my ($usage) = @_;
	return ("", "") if $usage eq "";
	my $flags = 0;
	die "Bad usage string: $usage\n" unless $usage =~ /^([^:]*):([^:]*)$/;
	my ($outputs, $inputs) = ($1, $2);
	#print STDERR "'$usage' -> '$outputs', '$inputs'\n";
	die "Bad usage string: $usage\n" unless
			$outputs =~ /^E?(|[Z$shortArgs]|[$argOrigins][$argKinds])$/o and
			$inputs ne "*" and $inputs =~ /^(Z|([$argOrigins][$argKinds\@]|[$shortArgs])*\*?)$/o;
	return ($outputs, $inputs);
}


#
# Decode a single argument, possibly with a wildcard.
#
# Return four values:
#   the argument's origin,
#   the argument's valueKind, and
#   true if the valueKind is a wildcard (either '@' or '_').
#
sub decodeArg
{
	my ($arg) = @_;
	my $origin = $shortOrigin;
	my $kind = $voidKind;
	my $wildcard = 0;
	if (defined($shortArgs{$arg})) {
		$kind = $shortArgs{$arg};
	} elsif ($arg =~ /^([$argOrigins])([$argKinds])$/) {
		$origin = $argOrigins{$1};
		$kind = $argKinds{$2};
	} elsif ($arg =~ /^([$argOrigins])[\@_]$/) {
		$origin = $argOrigins{$1};
		$wildcard = 1;
	} else {
		die "Internal error\n";
	}
	return ($origin, $kind, $wildcard);
}


#
# Convert an inputs string returned from verifyUsage into a string that contains the
# input constraints only and is appropriate as a C++ identifier name.
# Wildcard '@' symbols are converted into '_' symbols.
#
# Return two values:
#   the constraint string,
#   true if the last input is repeated.
#
sub inputsToConstraintString
{
	my ($inputs) = @_;
	$inputs = "" if $inputs eq "Z";
	die "Internal error\n" unless $inputs =~ /^([^\*]*)(\*?)$/;
	my $constraints = $1;
	my $repeat = $2 ne "";
	$constraints =~ tr/\@/_/;
	return ($constraints, $repeat);
}


#
# Convert a constraint string returned from inputsToConstraintString into an array
# of single-argument strings and return that array.
#
sub decodeConstraintString
{
	my ($constraints) = @_;
	my @args = ();
	while ($constraints ne "") {
		die "Internal error\n" unless $constraints =~ /^([$argOrigins][$argKinds\_]|[$shortArgs])(.*)$/o;
		push @args, $1;
		$constraints = $2;
	};
	return @args;
}


#
# Convert an outputs string returned from verifyUsage into a ValueKind constant.
# Null outputs get vkVoid.
#
sub outputsToKind
{
	my ($outputs) = @_;
	if ($outputs =~ /([$shortArgs]|[$argOrigins][$argKinds])/) {
		my ($origin, $kind, $wildcard) = decodeArg($1);
		die "Bad output: $outputs\n" if $wildcard || ($origin ne $shortOrigin);
		return $kind;
	} else {
		return $voidKind;
	}
}


#
# Convert a usage string (without quotes) to a hexadecimal string representing the desired flags value
#
sub usageToFlags
{
	my ($usage) = @_;
	my $flags = 0;
	$flags |= 1<<$dnIsReal if $usage ne "";
	$flags |= 1<<$dnCanRaiseException | 1<<$dnIsRoot if $usage =~ /E/;
	$flags |= 1<<$dnIsRoot if $usage =~ /Z/;
	return sprintf "0x%.4X", $flags;
}


#
# in
#    arrayName: name of this array
#    namesName: name of array of enum names
#    fileName: file name which to append this enum
#    names: list of names
#
# out
#    the file $fileName with the appended definitions
#
sub outputNodeTemplates
{
    my ($fileName, $primInfos) = @_;
	my $i;
	my $str;
	my $lastEnum;
	my %constraintStrings;

	# find out where to place the comma
    for ($i = $#$primInfos; $i >= 0; $i--)
    {
		if ($primInfos->[$i][$nameIndex] ne "")
		{
			$lastEnum = $i;
			last;
		}
	}

    open FILE, ">>$fileName" or die "Couldn't open $fileName: $!\n";

	outputGeneratedHeader(\*FILE, $fileName);	
	print FILE "#include \"Primitives.h\"\n\n";
	print FILE @headerLines;

    print FILE "\nconst DataNode::Template DataNode::templates[nPrimitiveOperations] = \n";
    print FILE "{\n";
    my $formatString = "\t{%-".($maxPrimitiveNameLength+2)."s".
    				   "%-".($maxCategoryNameLength+2)."s".
    				   "%-".($maxValueKindNameLength+2)."s".
    				   "%-8s\t%s\n";
    my $commentFormatString = "\t%-".(1 + $maxPrimitiveNameLength+2 + $maxCategoryNameLength+2 + $maxValueKindNameLength+2 + 8)."s\t%s\n";
    for ($i = 0; $i <= $#$primInfos; $i++)
    {
		my ($name, $category, $usage, $comment) =
			($primInfos->[$i][$nameIndex], $primInfos->[$i][$categoryIndex], $primInfos->[$i][$usageIndex], $primInfos->[$i][$commentIndex]);

		# If it has a name, then we print out the template, else we just print the comment.
		if ($name ne "") {
			my ($outputs, $inputs) = verifyUsage($usage);
			printf FILE $formatString,
						"$name,",
						"$category,",
						outputsToKind($outputs).",",
						usageToFlags($usage)."}".($i == $lastEnum ? "" : ","),
						$comment;
			my ($constraintString, $repeat) = inputsToConstraintString($inputs);
			$constraintStrings{$constraintString} = 1;
		} elsif ($comment eq "")
			{printf FILE "\n";}
		else
			{printf FILE $commentFormatString, "", $comment;}
    }
    print FILE "};\n\n\n";

    print FILE "#ifdef DEBUG\n";
    # Print definitions for all of the constraint strings, printing each unique one only once.
    my @constraintStrings = sort keys %constraintStrings;
    my $maxConstraintNameLength = length("constraint") + maxStringLength(@constraintStrings);
	foreach $str (@constraintStrings) {
		if ($str ne "") {
			my @constraints = decodeConstraintString($str);
			printf FILE "static const DataNode::InputConstraint %-".($maxConstraintNameLength+3)."s= {",
					"constraint$str\[]";
			my @constraint;
			while (defined($constraint = shift @constraints)) {
				my ($origin, $kind, $wildcard) = decodeArg($constraint);
				printf FILE "{%-".($maxValueKindNameLength+2)."s", "$kind,";
				print FILE "DataNode::";
				if (@constraints)
					{printf FILE "%-".($maxArgOriginNameLength+3)."s", "$origin},";}
				else
					{print FILE "$origin}";}
			}
			print FILE "};\n";
		}
	}
    print FILE "\n\n";

    print FILE "const DataNode::InputConstraintPattern DataNode::inputConstraintPatterns[nPrimitiveOperations] = \n";
    print FILE "{\n";
    $formatString = "\t{%-".($maxConstraintNameLength+2)."s%d, %-7s   // %-${maxPrimitiveNameLength}s\t%s\n";
    $commentFormatString = "\t%-".(1 + $maxConstraintNameLength+2 + 16 + $maxPrimitiveNameLength)."s\t%s\n";
    for ($i = 0; $i <= $#$primInfos; $i++)
    {
		my ($name, $usage, $comment) =
			($primInfos->[$i][$nameIndex], $primInfos->[$i][$usageIndex], $primInfos->[$i][$commentIndex]);

		# If it has a name, then we print out the input pattern, else we just print the comment.
		if ($name ne "") {
			my ($outputs, $inputs) = verifyUsage($usage);
			my ($constraintString, $repeat) = inputsToConstraintString($inputs);
			printf FILE $formatString,
						($constraintString eq "" ? "0" : "constraint$constraintString").",",
						scalar decodeConstraintString($constraintString),
						($repeat ? "true" : "false")."}".($i == $lastEnum ? "" : ","),
						$name, $comment;
			$constraintStrings{$constraintString} = 1;
		} elsif ($comment eq "")
			{printf FILE "\n";}
		else
			{printf FILE $commentFormatString, "", $comment;}
    }
    print FILE "};\n#endif\n\n\n";

    print FILE "#ifdef DEBUG_LOG\n";
    print FILE "static const char primitiveOperationNames[nPrimitiveOperations][16] = \n";
    print FILE "{\n";
    for ($i = 0; $i <= $#$primInfos; $i++)
    {
		my $name = $primInfos->[$i][$nameIndex];

		# If it has a name, then we print out the name.
		if ($name ne "")
		{
			$nameStr = $name;
			$nameStr = $1 if $name =~ /^[pc]o(\w+)$/;
			printf FILE "\t%-".($maxPrimitiveNameLength+1)."s\t// %s\n",
					 "\"$nameStr\"" . ($i == $lastEnum ? "" : ","), $name;
		}
    }
    print FILE "};\n#endif\n\n";

    close FILE;
}
