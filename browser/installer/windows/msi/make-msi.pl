#!/usr/bin/perl

use Cwd;

my $oldfh = select(STDOUT);
$| = 1;
select($oldfh);

$win32 = ($^O =~ / ((MS)?win32)|cygwin|os2/i) ? 1 : 0;
if (!$win32) {
    die "ERROR: MSI installers can currently only be made on Windows platforms\n";
}

#--------------------------------------------------------------------------
# If $ENV{MOZ_PACKAGE_MSI} is not true, exit.
#--------------------------------------------------------------------------

if (!exists($ENV{MOZ_PACKAGE_MSI}) or !defined($ENV{MOZ_PACKAGE_MSI}) or ($ENV{MOZ_PACKAGE_MSI} ne 1) ) {
    Print("MOZ_PACKAGE_MSI environment variable is not true.. exiting.\n");
    exit;
}

#--------------------------------------------------------------------------
# Find the MakeMSI mm.cmd program.
#--------------------------------------------------------------------------

$mm = `which mm.cmd`;
if (!defined($mm) or length($mm) lt 1) {
    die "ERROR: mm.cmd not found.  Is MakeMSI installed and in $PATH?\n";
}

Print("Found mm.cmd as $mm\n");

ParseArgv(@ARGV);

if ($inSrcDir eq "") {
    die "ERROR: Mozilla source directory must be specified with -srcDir\n";
}

if ($inObjDir eq "") {
    Print("Objdir not specified, using $inSrcDir\n");
    $inObjDir = $inSrcDir;
}

if ($inConfigFiles eq "") {
    die "ERROR: Packager manifests and install script location must be specified with -config\n";
}

$DIST  = "$inObjDir/dist";
$MSRC  = "$inSrcDir/browser/installer/windows/msi";
$MMH   = "$inSrcDir/toolkit/mozapps/installer/windows/msi";
$MDIST = "$DIST/msi";
$STAGE = "$DIST/install";

$STAGE =~ s:/+:/:g;
$DIST  =~ s:/+:/:g;
$MSRC  =~ s:/+:/:g;
$MDIST =~ s:/+:/:g;

if ( ! -e $DIST or ! -d $DIST ) {
    die "ERROR: make-msi requires\n\n\t$DIST\n\nto have been created by a proper build.\n";
}

#--------------------------------------------------------------------------
# Parse the installer config file.
#--------------------------------------------------------------------------

ParseInstallerCfg();

#--------------------------------------------------------------------------
# Prepare the staging directory and temp MSI location.
#--------------------------------------------------------------------------

if ( ! -d $STAGE ) {
    mkdir($STAGE, 0775);
}

if ( -e $MDIST ) {
    Print("Found old $MDIST entry.  Removing in 5 seconds..");
    for my $i (1..5) {
        printf("$i..");
        sleep(1);
    }
    printf("\n");

    Print("Removing $MDIST.\n");
    system("rm -rf \"$MDIST\"");
}

mkdir($MDIST, 0775);

#--------------------------------------------------------------------------
# Place MSI files as needed.
#--------------------------------------------------------------------------

Print("Placing MSI files in $MDIST/...\n");
system("cp \"$MSRC/firefox.mm\" \"$MDIST/\"");
system("cp \"$MSRC/firefox.mmh\" \"$MDIST/\"");
system("cp \"$MSRC/firefox.ver\" \"$MDIST/\"");
system("cp \"$MMH/mozilla-dept.mmh\" \"$MDIST/\"");
system("cp \"$MMH/mozilla-company.mmh\" \"$MDIST/\"");

#--------------------------------------------------------------------------
# Generate MSI file.
#--------------------------------------------------------------------------

Print("Generating MSI file...\n");
chdir($MDIST);
system("mm.cmd firefox P");

if ( ! -e "out/firefox.mm/MSI/firefox.msi" ) {
    die "ERROR: Failed to build MSI -- $MDIST/out/firefox.mm/MSI/firefox.msi missing!\n";
}

#--------------------------------------------------------------------------
# Move MSI file to stage.
#--------------------------------------------------------------------------

Print("Copying MSI file to staging area...\n");
my $cmd = "cp out/firefox.mm/MSI/firefox.msi \"$STAGE/$output_filename\"";
Print("$cmd\n");
system($cmd);

#--------------------------------------------------------------------------
# Exit.
#--------------------------------------------------------------------------

exit;

sub Print {
    my($text) = @_;
    print("make-msi.pl: " . $text);
}

sub PrintUsage {
    printf("$0: Usage\n\n");
    printf("\t-config [dir]\n\n\t\tInstaller config directory.\n\n");
    printf("\t-objDir [dir]\n\n\t\tThe main project object directory.\n\n");
    printf("\t-srcDir [dir]\n\n\t\tThe main project source directory.\n\n");
}

sub ParseArgv {
    my @argv = @_;
    my $counter;

    for($counter = 0; $counter <= $#argv; $counter++) {
        if($argv[$counter] =~ /^[-,\/]h$/i) {
            PrintUsage() and exit();
        } elsif ($argv[$counter] =~ /^[-,\/]srcDir$/i) {
	    if ($#argv >= ($counter + 1)) {
                $counter++;
                $inSrcDir = $argv[$counter];
	    }
        } elsif ($argv[$counter] =~ /^[-,\/]objDir$/i) {
	    if ($#argv >= ($counter + 1)) {
                $counter++;
                $inObjDir = $argv[$counter];
	    }
        } elsif ($argv[$counter] =~ /^[-,\/]config$/i) {
	    if ($#argv >= ($counter + 1)) {
                $counter++;
                $inConfigFiles = $argv[$counter];
	    }
        }
    }
}

sub ParseInstallerCfg {
    open(fpInstallCfg, "$inConfigFiles/installer.cfg") || die "Couldn't open $inConfigFiles/installer.cfg: $!\n";

    while ($line = <fpInstallCfg>) {
	if (substr($line, -2, 2) eq "\r\n") {
	    $line = substr($line, 0, length($line) - 2) . "\n";
	}

	($prop, $value) = ($line =~ m/(\w*)\s+=\s+(.*)\n/);

	if ($prop eq "FileInstallerMSI") {
	    $output_filename = $value;
	}
    }

    close(fpInstallCfg);
}
