#!perl -w
package	MacCVS;

# package	Mac::Apps::MacCVS; this should really be the name of the package
# but due to our directory hierarchy in mozilla, I am not doing it

require 5.004;
require	Exporter;

use strict;

use vars qw($VERSION @ISA @EXPORT $MacCVSLib);
use Mac::StandardFile;
use Moz;
use Cwd;
use Exporter;
use File::Basename;

@ISA				= qw(Exporter);
@EXPORT			= qw( new print checkout);
$VERSION = "1.00";

# Architecture:
# cvs session object:
# name - session name
# session_file - session file
# 
# globals
# $MacCVSLib - location of MacCVS applescript library
#
# 

#
# utility routines
#

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

# _useMacCVSLib
# returns 1 on success
# Search the include path for the file called MacCVSLib
sub _useMacCVSLib() 
{
	unless ( defined($MacCVSLib) )
	{
		my($libname) = "MacCVSLib";
# try the directory we were run from
		my($c) = dirname($0) . ":" . $libname;
		if ( -e $c)
		{
			$MacCVSLib = $c;
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
						$MacCVSLib = $c;
						last;
					}
				}
			}
		}
		if (! (-e $MacCVSLib))
		{
			print STDERR "MacCVS lib could not be found! $MacCVSLib";
			return 0;
		}
	}
	return 1;
}


#
# Session object methods
#

sub new		{
	my ( $proto, $session_file) = @_;
	my $class = ref($proto) || $proto;
	my $self = {};

	if ( defined($session_file) && ( -e $session_file) )
	{
		$self->{"name"} = basename( $session_file );
		$self->{"session_file"} = $session_file;
		bless $self, $class;
		return $self;
	}
	else
	{
		print STDERR "MacCVS->new cvs file < $session_file > does not exist\n";
		return;
	}
}

# makes sure that the session is open
# assertSessionOpen()
# returns 1 on failure
sub assertSessionOpen()	{
	my ($self) = shift;
	_useMacCVSLib() || die "Could not load MacCVSLib\n";
	my $script = <<END_OF_APPLESCRIPT;
	tell (load script file "$MacCVSLib") to OpenSession("$self->{session_file}")
END_OF_APPLESCRIPT
	return _myDoAppleScript($script);
}

# prints the cvs object, used mostly for debugging
sub print {
	my($self) = shift;
	print "MacCVS:: name: ", $self->{name}, " session file: ", $self->{session_file}, "\n";
}

# checkout( self, module, revision, date)
# MacCVS checkout command
# returns 1 on failure
sub checkout
{
	my($self, $module, $revision, $date ) = @_;
	unless( defined ($module) ) { $module = ""; }	# get rid of the pesky undefined warnings
	unless( defined ($revision) ) { $revision = ""; }
	unless( defined ($date) ) { $date = ""; }

	$self->assertSessionOpen() || return 1;
	
	my($revstring) = ($revision ne "") ? $revision : "(none)";
	my($datestring) = ($date ne "") ? $date : "(none)";
	
	print "Checking out $module with revision $revstring, date $datestring\n";
	
	my $script = <<END_OF_APPLESCRIPT;
	tell (load script file "$MacCVSLib") to Checkout given sessionName:"$self->{name}", module:"$module", revision:"$revision", date:"$date"
END_OF_APPLESCRIPT
	return _myDoAppleScript($script);
}

1;
=pod

=head1 NAME

MacCVS - Interface to MacCVS

=head1 SYNOPSIS

	use MacCVS;
	$session = MacCVS->new( <session_file_path>) || die "cannot create session";
	$session->checkout([module] [revision] [date]) || die "Could not check out";
		
=head1 DESCRIPTION

This is a MacCVS interface for talking to MacCVS Pro client.
MacCVSSession is the class used to manipulate the session

=item new
	MacCVS->new( <cvs session file path>);
	
	Creates a new session. Returns undef on failure.

=item checkout( <module> [revision] [date] )

	cvs checkout command. Revision and date are optional
	returns 0 on failure
	
=cut

=head1 SEE ALSO

=over

=item MacCVS Home Page

http://www.maccvs.org/

=back 

=head1 AUTHORS

Aleks Totic atotic@netscape.com

=cut

__END__
