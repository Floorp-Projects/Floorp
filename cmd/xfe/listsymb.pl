########################################################
# listsymb.pl 
#    Generates a list of all global C function names
#    defined in Communicator except those with 
#    unwanted prefix specified below.
# 
########################################################

# Functions with the following prefixes will not be listed.
# DISPT_GetDispatchTable & DISPT_NumDispatchPoints are exported
# using linker facilities and should always be filtered.
@unwanted_prefixes = ("__","Java_","java_","awt_","sun_","PR_", "PL_", "LL_",
                      "DISPT_GetDispatchTable", "DISPT_NumDispatchPoints");

@objfiles = ();
@symbols = ();
$debug = 0;

#Step 1: Get command line options
$input_file = $ARGV[0];
$output_file = $ARGV[1];
UsageAndExit() if (scalar(@ARGV) != 2);

# Step 2: Open output file for output
open (SYMB_LIST, ">$output_file") or die("Can't open \"$output_file\" for writing.\n");

# Step 3: Read input file to determine list of all obj / lib files
# needed to link the executable
GetObjFileList($input_file, \@objfiles);

# Step 4: Run NM on every single obj / lib file
# to get the complelete list of global functions
CallNM(\@objfiles, \@symbols);

# Step 5: Put all the symbols in output file
foreach (@symbols) {
	print SYMB_LIST ("$_\n");
}


# ---------------  Sub-Routines -----------------
#


sub UsageAndExit {
	print ("Usage: perl gendisp.pl <list of obj files> <symbol list>\n");
	exit(-1);
}

# CallNM calls nm on each file in objfiles
# and finds out all symbols defined in them. 
# The list of symbols is returned.
sub CallNM {
	my($objfiles) = $_[0];
	my($symbols) = $_[1];
	my(%unique) = ();

	foreach (@{$objfiles}) {
		my($file) = $_;
		my(@results) = ();

		# if such file exists, call nm 
		# otherwise, print a warning
		if (-f $file) {
			# nm -g prints only global symbols
			# nm --demangle demangles C++ names
			@results = `nm -gp $file`;
			foreach (@results) {
				my($symbol) = ExtractFunctionSymbol($_);
				if ($symbol ne "") {
					$unique{$symbol} = "exists";
				}
			}	
		}
	}

	foreach (sort(keys(%unique))) {
		$symbol = FilterUnwantedPrefix($_);
		push(@{$symbols}, ($symbol)) if $symbol ne ""; 
	}
		
}

sub IsObjFile {
	# if 1st argument ends with .o .a or .so
	if ($_[0] =~ m/\.(o)|\.(a)|\.(so)\s*$/) {
		$firstchar = substr($_[0], 0, 1);
		return $_[0] if $firstchar ne '/';
	}
	return "";
}

sub IsLibFile {
        ($_[0] =~ m/^\s*-l\s*(.+)\s*$/);
        return $1;
}

sub IsLibPath {
	# if begins with -L, it's a path 
        if ($_[0] =~ m/^\s*-L\s*(.+)\s*$/) {
		# Igore absolute path, because they
		# tend to be system libraries
		$firstchar = substr($1, 0, 1);
		return "" if ($firstchar eq '/');

		# Make sure it ends with a '/'
		# so that appending it to filename will work
		$lastchar = substr($1, length($1)-1, 1);
		return (($lastchar eq '/')? $1 : $1.'/');
	} else {
		return "";
	}
}

# FilterUnwantedPrefix() returns 1st argument
# if 
#   its prefix doesn't match any of those 
#   defined in @unwanted_prefixes
# and
#   it contains at least at least 1 upper-case letter or 
#   an underscore.
sub FilterUnwantedPrefix {
	if ($_[0] !~ m/[A-Z_]/) {
		print ("Warning: $_[0] is probably a system symbol, and so filtered.\n") if $debug > 1;
		return "";
	}
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
         if ($_[0] =~ m/ T (\w+)\s*$/) {
		print ("Matched    :$_[0]\n") if $debug > 1;
               	return $1;
        } else {
		print ("Not Matched:$_[0]\n") if $debug > 1;
                return "";
        }
}

# GetObjFileLIst reads in a file containing LINK options
# and returns the list of .obj & .lib files in 1st argu
sub GetObjFileList {
        my($objfiles) = $_[1]; # path to .o's, .a's, and .so's
	my @libpath = (); # -Lstuff 
	my @libfiles = ();# -lstuff
	my @junkoptions = (); # any other options

	# 1. Separate different kinds of options
        open (OBJS_LIST, $_[0]) or die ("Can't open $_[0] for reading.\n");
        while ($line = <OBJS_LIST>) {
                # For each line, break up into words
                $line =~ s/^\s+//; # eat white spaces at beginning of line 
                @words = split(/\s+/, $line);
                foreach (@words) {
                        if (IsObjFile($_) ne "") {
			    # If the option ends with .so, .a , or .o, 
			    # assume it is an object file to be linked in.
			    print ("SEPARATE: $_ is obj file.\n") if $debug;
                            push(@{$objfiles}, ($_));

                        } elsif (($temp = IsLibPath($_)) ne "") {
			    # If begins with -L
			    print ("SEPARATE: $_ is -L$temp.\n") if $debug;
			    push(@libpath, $temp);

                        } elsif (($temp = IsLibFile($_)) ne "") {
			    # If begins with -l
			    print ("SEPARATE: $_ is -l$temp.\n") if $debug;
			    push(@libfiles, $temp);

			} else {
			    # Unrecognized Options
			    print ("SEPARATE: $_ option ignored.\n") if $debug;
			    push(@junkoptions, $_);
			}
                }
        }

	# 2. check to see if .a or .so exists under the specified path
	foreach (@libfiles) {
		if (-f $_) { #file exists?
			push (@{$objfiles}, $_);

		} else { 
			#file doesn't exist, try prepending libpath prefix
			$file = $_;
			$found = 0;
			foreach (@libpath) {
				if (-f $_."lib".$file.".a") {
				    print ("SEARCH: $_+lib+$file+.a found.\n") if $debug;
				    push (@{$objfiles}, $_."lib".$file.".a");
				    $found = 1;
				    last;

				} elsif (-f $_."lib".$file.".so") {
				    print ("SEARCH: $_+lib+$file+.so found.\n") if $debug; 
				    push (@{$objfiles}, $_."lib".$file.".so");
				    $found = 1;
				    last;

				}
			}
			if ($found == 0) {
				print ("SEARCH: $file not found.\n") if $debug;
			}
		}
	}

	# 3. Returns the list of all object, .a, .so files 
	foreach (@{$objfiles}) {
		print ("OBJ: $_\n") if $debug;
	}
}
