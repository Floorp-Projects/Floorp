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
$cmoffset_file = $ARGV[3];
UsageAndExit() if (scalar(@ARGV) != 4);

#####################################################
# Step 1: Read $input_file which contains list of functions
#####################################################
GetFunctionList($input_file, \@symbols);

#####################################################
# Step 2: Open dispatch.h and dispatch.c for output
#####################################################
open (DISPATCH_H, ">$header_file") or die("Can't open $header_file for output.\n");
open (DISPATCH_C, ">$source_file") or die("Can't open $source_file for output.\n");
open (CMOFFSET_H, ">$cmoffset_file") or die("Can't open $cmoffset_file for output.\n");


#####################################################
# Step 3: Write out the declarations in of header file
#####################################################
print DISPATCH_H ("#include \"nspr.h\"\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("typedef void (*void_fun)(void);\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("PR_EXTERN_DATA (void_fun) DISPT_Dispatch_Table[];\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("PR_EXTERN (void_fun*)  DISPT_GetDispatchTable(void);\n");
print DISPATCH_H ("\n");
print DISPATCH_H ("PR_EXTERN (int) DISPT_NumDispatchPoints(void);\n");
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
print DISPATCH_C ("PR_IMPLEMENT_DATA (void_fun) DISPT_Dispatch_Table[] = {\n");

$first_entry = 1;
foreach (@symbols) {
        if (!$first_entry) {
             print DISPATCH_C (",\n");
        }
	print DISPATCH_C ("\t$_");
        $first_entry = 0;
}

print DISPATCH_C ("\n};\n");
print DISPATCH_C ("\n");
print DISPATCH_C ("PR_IMPLEMENT (void_fun*)  DISPT_GetDispatchTable(void) {\n");
print DISPATCH_C ("    return DISPT_Dispatch_Table;\n");
print DISPATCH_C ("}\n");
print DISPATCH_C ("\n");
print DISPATCH_C ("PR_IMPLEMENT (int) DISPT_NumDispatchPoints(void) {\n");
print DISPATCH_C ("    return (sizeof(DISPT_Dispatch_Table)/sizeof(DISPT_Dispatch_Table[0]));\n");
print DISPATCH_C ("}\n");


#####################################################
# Step 7: Write out cmoffset.h
#####################################################
$offset = 0;
foreach (@symbols) {
    printf CMOFFSET_H ("#define CM_OFFSET_%-45s %5d\n",$_, $offset);
    $offset++;
}

# ---------------------- Sub-routines -------------------------
sub UsageAndExit {
	print ("Usage: perl gendisp.pl <symbols.txt> <out.h> <out.c> <offset.h>\n");
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

