# Run this on Win32 only!

$dir_sep = '\\';
$moz = "$ENV{'MOZ_SRC'}\\mozilla";
$moz_dist = "$moz\\dist\\Embed";
$moz =~ s/\//$dir_sep/g;
$moz_version = "v1.5";

@dirs = ();

$local_nsh = "local.nsh";

open(NSH, ">$local_nsh") or die("Can't write to local manifest $local_nsh");
print NSH "!define DISTDIR \"$moz_dist\"\n";
print NSH "!define VERSION \"$moz_version\"\n";
close(NSH);

open(FILES_NSH, ">files.nsh");
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

