#!perl -w
package	MANIFESTO;

require 5.004;
require	Exporter;

#use strict;

use vars qw($VERSION @ISA @EXPORT $MANIFESTOLib);
use Mac::StandardFile;
use Moz;
use Cwd;
use Exporter;
use File::Basename;

@ISA				= qw(Exporter);
@EXPORT			= qw(ReconcileProject);
$VERSION = "1.00";

=head1 NAME

MANIFESTO - drives the Mac Project Reconciliation tool.

=head1 SYNOPSIS

You want to use this script. It will make your life easier.

=head1 COPYRIGHT

The contents of this file are subject to the Netscape Public License
Version 1.0 (the "NPL"); you may not use this file except in
compliance with the NPL.  You may obtain a copy of the NPL at
http://www.mozilla.org/NPL/

Software distributed under the NPL is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
for the specific language governing rights and limitations under the
NPL.

The Initial Developer of this code under the NPL is Netscape
Communications Corporation.  Portions created by Netscape are
Copyright (C) 1998 Netscape Communications Corporation.  All Rights
Reserved.

=cut

# 
# globals
# $MANIFESTOLib - location of MANIFESTO applescript library
#

#
# utility routines
#

sub current_directory()
{
	my $current_directory = cwd();
	chop($current_directory) if ( $current_directory =~ m/:$/ );
	return $current_directory;
}

# Uses the "compile script" extension to compile a script.
sub compile_script($;$) {
	my($scriptPath, $outputPath) = @_;
	
	#// generate a script to compile a script file.
	my $script = <<END_OF_APPLESCRIPT;
	store script (compile script (alias "$scriptPath")) in (file "$outputPath") replacing yes
END_OF_APPLESCRIPT

	#// run the script.
	MacPerl::DoAppleScript($script);
}

# _useMANIFESTOLib()
# returns 1 on success
# Search the include path for the file called MANIFESTOLib
sub _useMANIFESTOLib() 
{
	unless ( defined($MANIFESTOLib) )
	{
		my($scriptName) = "MANIFESTOLib.script";
		my($libName) = "MANIFESTOLib";
		# try the directory we were run from
		my($scriptPath) = dirname($0) . ":" . $scriptName;
		my($libPath) = dirname($0) . ":" . $libName;
		# make sure that the compiled script is up to date with the textual script.
		unless (-e $libPath && getModificationDate($libPath) >= getModificationDate($scriptPath)) {
			print "# Recompiling MANIFESTOLib.script.\n";
			compile_script($scriptPath, $libPath);
		}
		if ( -e $libPath) {
			$MANIFESTOLib = $libPath;
		} else {
			# now search the include directories
			foreach (@INC)
			{
				unless ( m/^Dev:Pseudo/ )	# This is some bizarre MacPerl special-case directory
				{
					$libPath = $_ . $libName;
					if (-e $libPath)
					{
						$MANIFESTOLib = $libPath;
						last;
					}
				}
			}
		}
		if (! (-e $MANIFESTOLib)) {
			print STDERR "MANIFESTOLib lib could not be found! $MANIFESTOLib";
			return 0;
		}
	}
	return 1;
}

sub getModificationDate($) {
	my($filePath)=@_;
	my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
		$atime,$mtime,$ctime,$blksize,$blocks) = stat($filePath);
	return $mtime;
}

sub setExtension($;$;$) {
	my($filePath, $oldExtension, $newExtension)=@_;
	my($name, $dir, $type) = fileparse($filePath, $oldExtension);
	return "$dir$name$newExtension";
}

#
# ReconcileProject(projectPath, manifestoPath)
#
# Uses MANIFESTOLib AppleScript to reconcile the contents (toc?) of a
# CodeWarrior project with an external MANIFEST file.
#

sub ReconcileProject($;$) {
	#// turn this feature on by removing the following line.
	return 1;

	my($projectPath, $manifestoPath) = @_;
	my($sourceTree) = current_directory();
	my($logPath) = setExtension($manifestoPath, ".toc", ".log");

	print STDERR "# Reconciling Project: $projectPath with $manifestoPath\n";
	
	#// compare the modification dates of the .toc and .log files. If .log is newer, do nothing.
	if (-e $logPath && getModificationDate($logPath) >= getModificationDate($manifestoPath)) {
		print "# Project is up to date.\n";
		return 1;
	}

	_useMANIFESTOLib() || die "Could not load MANIFESTOLib\n";
	
	my $script = <<END_OF_APPLESCRIPT;
	tell (load script file "$MANIFESTOLib") to ReconcileProject("$sourceTree:", "$sourceTree$projectPath", "$sourceTree$manifestoPath")
END_OF_APPLESCRIPT

	#// run the script, and store the results in a file called "$manifestoPath.log"
	my $asresult = substr(MacPerl::DoAppleScript($script), 1, -1);	#// chops off leading, trailing quotes.

	#// print out to STDOUT to show progress.
	print $asresult;

	#// store the results in "$manifestoPath.log", which will act as a cache for later checks.
	open(OUTPUT, ">$logPath") || die "can't open log file $logPath.";
	print OUTPUT $asresult;
	close(OUTPUT);

	return 1;
}

1;
=pod

=head1 NAME

MANIFESTO - Scripts to process source .toc files.

=head1 SYNOPSIS

	use MANIFESTO;
	ReconcileProject(<path to Mac project file>, <path to table of contents file>) || die "cannot reconcile project";
		
=head1 DESCRIPTION

This is a PERL interface for talking to MANIFESTOLib AppleScripts.

=item ReconcileProject
	ReconcileProject(<path to Mac project file>, <path to table of contents file>);
	
	Reconciles the contents of a project with an external .toc file.

=cut

=head1 SEE ALSO

=over

=item MacCVS Home Page

http://www.maccvs.org/

=back 

=head1 AUTHORS

Patrick Beard beard@netscape.com

based on work by

Aleks Totic atotic@netscape.com

=cut

__END__
