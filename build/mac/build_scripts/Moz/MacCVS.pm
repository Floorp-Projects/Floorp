#!perl -w
package Moz::MacCVS;

# package Mac::Apps::MacCVS; this should really be the name of the package
# but due to our directory hierarchy in mozilla, I am not doing it

require 5.004;
require Exporter;

use strict;
use Exporter;

use vars qw($VERSION @ISA @EXPORT);

use Cwd;

use File::Basename;

use Mac::StandardFile;
use Mac::AppleEvents;
use Mac::AppleEvents::Simple;

@ISA        = qw(Exporter);
@EXPORT     = qw(new describe checkout update);
$VERSION = "1.00";

# If you want to understand the gobbldeygook that's used to build Apple Events,
# you should start by reading the AEGizmos documentation.


# Architecture:
# cvs session object:
# name - session name
# session_file - session file
# 
# 

my($last_error) = 0;
my($gAppSig)    = 'Mcvs';   # MacCVS Pro

#
# utility routines
#


sub _checkForEventError($)
{
  my($evt) = @_;

  if ($evt->{ERRNO} != 0)
  {
    print STDERR "Error. Script returned '$evt->{ERROR} (error $evt->{ERRNO})\n";
    $last_error = $evt->{ERRNO};
    return 0;
  }
  
  return 1; # success
}


#
# Session object methods
#

sub new
{
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
# returns 1 on success
sub assertSessionOpen()
{
  my ($self) = shift;

  $last_error = 0;

  my($prm) =
    q"'----':obj {form:name, want:type(alis), seld:TEXT(@), from:'null'()}";

  my($evt) = do_event(qw/aevt odoc/, $gAppSig, $prm, $self->{session_file});
  return _checkForEventError($evt);
}

# prints the cvs object, used mostly for debugging
sub describe
{
  my($self) = shift;
  $last_error = 0;
  print "MacCVS:: name: ", $self->{name}, " session file: ", $self->{session_file}, "\n";
}

# checkout( self, module, revision, date)
# MacCVS checkout command
# returns 1 on success.
sub checkout()
{
  my($self, $module, $revision, $date ) = @_;
  unless( defined ($module) ) { $module = ""; } # get rid of the pesky undefined warnings
  unless( defined ($revision) ) { $revision = ""; }
  unless( defined ($date) ) { $date = ""; }

  $last_error = 0;
    
  $self->assertSessionOpen() || die "Error: failed to open MacCVS session file at $self->{session_file}\n";
  
  my($revstring) = ($revision ne "") ? $revision : "(none)";
  my($datestring) = ($date ne "") ? $date : "(none)";
  
  print "Checking out $module with revision $revstring, date $datestring\n";

  my($prm) =
    q"'----':obj {form:name, want:type(docu), seld:TEXT(@), from:'null'()}, ".
    q"modl:'TEXT'(@), tagr:'TEXT'(@), tagd:'TEXT'(@) ";
  
  my($evt) = do_event(qw/MCvs cout/, $gAppSig, $prm, $self->{name}, $module, $revision, $date);
  return _checkForEventError($evt);
}


# update( self, branch tag, list of paths)
# MacCVS udate command
# returns 1 on success.
# NOTE: MacCVS Pro does not correctly support this stuff yet (as of version 2.7d5).
sub update()
{
  my($self, $branch, $paths ) = @_;

  $last_error = 0;
    
  $self->assertSessionOpen() || die "Error: failed to open MacCVS session file at $self->{session_file}\n";

  if ($branch eq "HEAD") {
    $branch = "";
  }
  
  my($paths_list) = "";
  
  my($path);
  foreach $path (@$paths)
  {
    if ($paths_list ne "") {
      $paths_list = $paths_list.", ";
    }
    
    $paths_list = $paths_list."Ò".$path."Ó";
  }
    
  my($prm) =
    q"'----':obj {form:name, want:type(docu), seld:TEXT(@), from:'null'()}, ".
    q"tagr:'TEXT'(@), tFls:[";

  $prm = $prm.$paths_list."]";
    
  my($evt) = do_event(qw/MCvs updt/, $gAppSig, $prm, $self->{name}, $branch);
  return _checkForEventError($evt);
};


sub getLastError()
{
  return $last_error;
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
Simon Fraser sfraser@netscape.com

=cut

__END__
