#!perl -w
package	MANIFESTO;

require 5.004;
require	Exporter;

use strict;

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

# just like Mac::DoAppleScript, 1 is success, 0 is failure
sub _myDoAppleScript($)
{
	my($script) = @_;
	my $asresult = MacPerl::DoAppleScript($script);
	if ($asresult eq "0")
	{
		return 1;
	}
	else
	{
		print STDERR "AppleScript error: $asresult\n";
		print STDERR "AppleScript was: \n $script \n";
		return 0;
	}
}

# _useMANIFESTOLib
# returns 1 on success
# Search the include path for the file called MANIFESTOLib
sub _useMANIFESTOLib() 
{
	unless ( defined($MANIFESTOLib) )
	{
		my($libname) = "MANIFESTOLib";
# try the directory we were run from
		my($c) = dirname($0) . ":" . $libname;
		if ( -e $c)
		{
			$MANIFESTOLib = $c;
		}
		else
		{
	# now search the include directories
			foreach (@INC)
			{
				unless ( m/^Dev:Pseudo/ )	# This is some bizarre MacPerl special-case directory
				{
					$c = $_ . $libname;
					if (-e $c)
					{
						$MANIFESTOLib = $c;
						last;
					}
				}
			}
		}
		if (! (-e $MANIFESTOLib))
		{
			print STDERR "MANIFESTOLib lib could not be found! $MANIFESTOLib";
			return 0;
		}
	}
	return 1;
}

#
# ReconcileProject(projectPath, manifestoPath)
#
# Uses MANIFESTOLib AppleScript to reconcile the contents (toc?) of a
# CodeWarrior project with an external MANIFEST file.
#

sub ReconcileProject($;$) {
	#// turn this feature on by removing the following line.
	return 0;

	my($projectPath, $manifestoPath) = @_;
	my($sourceTree) = current_directory();

	_useMANIFESTOLib() || die "Could not load MANIFESTOLib\n";
	
	my $script = <<END_OF_APPLESCRIPT;
	tell (load script file "$MANIFESTOLib") to ReconcileProject("$sourceTree:", "$sourceTree$projectPath", "$sourceTree$manifestoPath")
END_OF_APPLESCRIPT
	print STDERR "# Reconciling Project: $projectPath with $manifestoPath\n";
	return _myDoAppleScript($script);
}

1;
=pod

=head1 NAME

MANIFEST - Scripts to process source MANIFEST files. Really need a better name.

=head1 SYNOPSIS

	use MacCVS;
	$session = MacCVS->new( <session_file_path>) || die "cannot create session";
	$session->checkout([module] [revision] [date]) || die "Could not check out";
		
=head1 DESCRIPTION

This is a PERL interface for talking to MANIFEST AppleScripts.

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
