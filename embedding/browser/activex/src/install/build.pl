# Run this on Win32 only!

################################################
# GLOBAL SETTINGS
#
# Paths'n'things. Change as appropriate!

$dir_sep = '\\';
$moz = "$ENV{'MOZ_SRC'}\\mozilla";
$moz_dist = "$moz\\dist\\Embed";
$moz_redist = "$moz\\..\\redist";
$moz_embedding_config = "$moz\\embedding\\config";
$moz =~ s/\//$dir_sep/g;
$moz_major_version=1;
$moz_minor_version=5;
$moz_version = "v$moz_major_version.$moz_minor_version";

$makensis = "C:/Program Files/NSIS/makensis.exe";
$control_nsi = "control.nsi";
$local_nsh = "local.nsh";
$files_nsh = "files.nsh";

################################################

@dirs = ();

print "Mozilla ActiveX control builder\n\n";
print "Hold onto your hats, this could be a rough trip\n";

# Copy the client-win to embedding/config
# cp client-win $moz_embedding_config

# Run the make in embedding/config to ensure a dist
#chdir $moz_embedding_config or die("Can't cd into $moz_embedding_config!");
#@make = ( "make" );
#system(@make);

# Generate local settings
print "Opening $local_nsh for writing\n";
open(NSH, ">$local_nsh") or die("Can't write local settings to $local_nsh");
print NSH "!define DISTDIR \"$moz_dist\"\n";
print NSH "!define REDISTDIR \"$moz_redist\"\n";
print NSH "!define VERSION \"$moz_version\"\n";
print NSH "!define MAJOR_VERSION \"$moz_major_version\"\n";
print NSH "!define MINOR_VERSION \"$moz_minor_version\"\n";
close(NSH);

# Generate file manifest
print "Opening $files_nsh from $moz_dist for writing\n";
open(FILES_NSH, ">$files_nsh") or die("Can't write files to $files_nsh");
push @dirs, "";
read_dir("$moz_dist", "");
foreach (@dirs)
{
	if ($_ eq "")
	{
		print FILES_NSH "\n    SetOutPath \"\$INSTDIR\"\n";
	}
	else
	{
		print FILES_NSH "\n    SetOutPath \"\$INSTDIR\\$_\"\n";
	}

	$dir = "$moz_dist\\$_";

	opendir(DIR, $dir);
	@files = readdir(DIR);
	foreach (@files)
	{
		# Everything except mfcembed / winembed / readme.html
		next if (/.*mfcembed*/i ||
				 /.*winembed*/i ||
				 /readme.html/i);

		if (! -d "$dir\\$_")
		{
			print FILES_NSH "    File $dir\\$_\n";
		}
	}
	closedir(DIR);
}
close(FILES_NSH);

# Run NSIS

print "Running makensis.exe to compile it all...\n";
@nsis = ("$makensis", "$control_nsi");
system(@nsis) == 0 or die "system @args failed: $?\n";

# TODO - Run codesigning tool - if there is a cert to sign with


print "Finished\n";

exit;

sub read_dir($$)
{
	my ($absdir,$reldir) = @_;
	my @dirList;
	opendir(DIST, "$absdir");
	@dirList = readdir(DIST);
	closedir(DIST);
	foreach (@dirList)
	{
		next if ($_ eq "." || $_ eq "..");
		$absFileName = "$absdir\\$_";
		$fileName = ($reldir eq "") ? $_ : "$reldir\\$_";

		if (-d $absFileName)
		{
			push @dirs, $fileName;
			read_dir($absFileName, $fileName);
		}
	} 
}

