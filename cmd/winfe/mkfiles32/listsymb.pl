########################################################
# ls_symb.pl 
#    Generates a list of all global C function names
#    defined in Communicator except those with 
#    unwanted prefix specified below.
# 
########################################################

# Functions with the following prefixes will not be listed.
# DISPT_InitializeTable & DISPT_LookupSymbol are exported
# using linker facilities and should always be filtered.
@unwanted_prefixes = ("__","Java_","java_","awt_","sun_","PR_", "PL_", "LL_",
                      "DISPT_InitializeTable", "DISPT_LookupSymbol");

@objfiles = ();
@symbols = ();
%unique_symbols = {};

#Step 1: Get command line options
$input_file = $ARGV[0];
$output_file = $ARGV[1];
UsageAndExit() if (scalar(@ARGV) != 2);

# Step 2: Open output file for output
open (SYMB_LIST, ">$output_file") or die("Can't open \"$output_file\" for writing.\n");

# Step 3: Read input file to determine list of all obj / lib files
# needed to link the executable
GetObjFileList($input_file, \@objfiles);

# Step 4: Run DUMPBIN on every single obj / lib file
# to get the complelete list of global functions
CallDumpBin(\@objfiles, \@symbols, \%unique_symbols);

# Step 5: Put all the symbols in output file
foreach (sort(@symbols)) {
	print SYMB_LIST ("$_\n");
}


# ---------------  Sub-Routines -----------------
#


sub UsageAndExit {
	print ("Usage: perl gendisp.pl <list of obj files> <symbol list>");
	exit(-1);
}

# CallDumpBin calls dumpbin on each file in objfiles
# and finds out all symbols defined in them. 
# The list of symbols is returned.
sub CallDumpBin {
	my($objfiles) = $_[0];
	my($symbols) = $_[1];
	my($unique) = $_[2];
	my($num_symbols) = 0;
	my($num_unique) = 0;
	foreach (@{$objfiles}) {
		my($file) = $_;
		my(@results) = ();
		# if such file exists, call dumpbin
		# otherwise, print a warning
		if (-f $file) {
			@results = `dumpbin /SYMBOLS $file`;
			foreach (@results) {
				my($symbol) = ExtractFunctionSymbol($_);
				$symbol = FilterUnwantedPrefix($symbol);
				if ($symbol ne "") {
					push(@{$symbols}, ($symbol)); 
					if (!exists(${$unique}{$symbol})) {
						${$unique}{$symbol} = $symbol;
						$num_unique++;
					}
					$num_symbols++;
				}
			}	
		}
	}
}

sub IsObjFile {
	return ($_[0] =~ m/\.(obj)|\.(lib)\s*$/i);
}

# FilterUnwantedPrefix() returns 1st argument
# if its prefix doesn't match any of those 
# defined in @unwanted_prefixes

sub FilterUnwantedPrefix {
        foreach $prefix (@unwanted_prefixes) {
                if ($_[0] =~ m/^$prefix/) {
                        return "";
                }
        }
        return $_[0];
}

# Returns a non-empty string if the line contains a function symbol
# Returns an empty string otherwise
# Note: a line containing a function symbol looks like this:
#       048 00000000 SECTA  notype ()    External     | _lm_DefinePkcs11
#       This function searches for entries that contains SECT,(),External
sub ExtractFunctionSymbol {
         if ($_[0] =~ m/SECT.+\(\)\s+External\s+\|\s*_(\w+)\s*$/) {
               	return $1;
        } else {
                return "";
        }
}

# GetObjFileLIst reads in a file containing LINK options
# and returns the list of .obj & .lib files in the options
sub GetObjFileList {
        my($objfiles) = $_[1];
        open (OBJS_LIST, $_[0]) or die ("Can't open $_[0] for reading.\n");
        while ($line = <OBJS_LIST>) {
                # For each line, break up into words
                $line =~ s/^\s+//;
                @words = split(/\s+/, $line);
                foreach (@words) {
                        if (IsObjFile($_)) {
                                push(@{$objfiles}, ($_));
                        }
                }
        }
}
