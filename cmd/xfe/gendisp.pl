########################################################
# gendispw.pl 
#    Generates a table of pointers pointing to
#    functions in Communicator being exported.
#
#    The list of functions are read from the input file.
# 
########################################################

@symbols = ();

#####################################################
#Step 0: Get command line options
#####################################################
$input_file = $ARGV[0];
$header_file = $ARGV[1];
$source_file = $ARGV[2];
UsageAndExit() if (scalar(@ARGV) != 3);

#####################################################
# Step 1: Read $input_file which contains list of functions
#####################################################
GetFunctionList($input_file, \@symbols);

#####################################################
# Step 2: Open dispatch.h and dispatch.c for output
#####################################################
open (DISPATCH_H, ">$header_file") or die("Can't open $header_file for output.\n");
open (DISPATCH_C, ">$source_file") or die("Can't open $source_file for output.\n");


#####################################################
# Step 3: Write out the declarations in of header file
#####################################################
print DISPATCH_H ("#include \"nspr.h\"\n");
print DISPATCH_H ("#include \"plhash.h\"\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("typedef void (*void_fun)(void);\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("PR_EXTERN (void_fun)  DISPT_LookupSymbol(const char *);\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("PR_EXTERN (int) DISPT_InitializeTable(void);\n");
print DISPATCH_H ("\n");

foreach (@symbols) {
	print DISPATCH_H ("PR_EXTERN (void) $_(void);\n");
}
print DISPATCH_H ("\n");

#####################################################
# Step 4: Write out the definitions in source file
#####################################################
$header_file = ExtractFilename($header_file);
print DISPATCH_C ("#include \"$header_file\"\n");
print DISPATCH_C ("\n");
print DISPATCH_C ("static PLHashTable * dispTable = NULL; \n");
print DISPATCH_C ("\n");

$num_functions = 1;
$num_symbols = 0;

printf DISPATCH_C ("void initializeTable%d (void) {\n", $num_functions);
foreach (@symbols) {
	if ($num_symbols == 1000) {
		$num_functions++;
		$num_symbols = 0;
		print DISPATCH_C ("}\n");
		printf DISPATCH_C ("void initializeTable%d (void) {\n", $num_functions);
	}
	print DISPATCH_C ("\tPL_HashTableAdd(dispTable, \"$_\", $_);\n");
	$num_symbols++;
}
print DISPATCH_C ("}\n");
print DISPATCH_C ("\n");
print DISPATCH_C ("PR_IMPLEMENT (int) DISPT_InitializeTable(void) {\n");
print DISPATCH_C ("\tdispTable = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL);\n");
print DISPATCH_C ("\tif (dispTable == NULL) return 1;\n");
print DISPATCH_C ("\n");
for ($i = 1; $i <= $num_functions; $i++) {
	printf DISPATCH_C ("\tinitializeTable%d();\n", $i);
}
print DISPATCH_C ("\treturn 0;\n}");
print DISPATCH_C ("\n");
print DISPATCH_C ("PR_IMPLEMENT (void_fun)  DISPT_LookupSymbol(const char * key) {\n");
print DISPATCH_C ("\tif (dispTable) {\n");
print DISPATCH_C ("\t\treturn PL_HashTableLookup(dispTable, key);\n");
print DISPATCH_C ("\t} else {\n");
print DISPATCH_C ("\t\treturn NULL;\n");
print DISPATCH_C ("\t}\n}\n");
print DISPATCH_C ("\n");

# ---------------------- Sub-routines -------------------------
sub UsageAndExit {
	print ("Usage: perl gendisp.pl <link.cl> <out.h> <out.c> <offset.h>");
	exit(-1);
}

# GetFunctionLIst reads in a file containing a list
# of function names, and returns an array containing
# them
sub GetFunctionList {
	my($function_list) = $_[1];
	open (FUN_LIST, $_[0]) or die ("Can't open $_[0] for reading.\n");
	while ($line = <FUN_LIST>) {
		if ($line =~ m/^\s*(\w+)/) {
			push(@{$function_list}, $1);
		}
	}
}

# Extract the filename part of a full path.
# Extracts the part of the string that appears after the last '\' or '/'.
sub ExtractFilename {
    my $slash_offset = rindex($_[0], "/");
    my $bslash_offset = rindex($_[0],"\\");
    my $offset = (($slash_offset > $bslash_offset)? $slash_offset : $bslash_offset);
    return substr($_[0], $offset+1);
}

