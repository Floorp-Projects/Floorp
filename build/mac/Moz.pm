=head1 NAME

B<Moz> - routines for automating CodeWarrior builds, and some extra-curricular activities related to building Mozilla

=head1 SYNOPSIS

    use Moz;

    OpenErrorLog(":::BuildLog");
    StopForErrors();

		$Moz::QUIET = 1;
		InstallFromManifest(":projects:MANIFEST", $dist_dir);

    BuildProjectClean(":projects:SomeProject.mcp", "SomeTarget");
    MakeAlias(":projects:SomeProject.shlb", $dist_dir);

    DontStopForErrors();

    BuildProject(":projects:SomeOtherProject.mcp", "SomeTarget");

=head1 DESCRIPTION

B<Moz> comprises the routines needed to slap CodeWarrior around, force it to build a sequence of projects, report the results, and a few other things.

=cut




package			Moz;
require			Exporter;

use Mac::Types;
use Mac::Events;
use Mac::Processes;

@ISA				= qw(Exporter);
@EXPORT			= qw(BuildProject BuildProjectClean OpenErrorLog MakeAlias StopForErrors DontStopForErrors InstallFromManifest SetBuildNumber SetAgentString SetTimeBomb Delay ActivateApplication);
@EXPORT_OK	= qw(CloseErrorLog UseCodeWarriorLib QUIET);

	use Cwd;
	use File::Path;
	use ExtUtils::Manifest 'maniread';

	use CodeWarriorLib;

sub current_directory()
	{
		my $current_directory = cwd();
		chop($current_directory) if ( $current_directory =~ m/:$/ );
		return $current_directory;
	}

sub full_path_to($)
	{
		my ($path) = @_;
		if ( $path =~ m/^[^:]+$/ )
			{
				$path = ":" . $path;
			}

		if ( $path =~ m/^:/ )
			{
				$path = current_directory() . $path;
			}

		return $path;
	}

=head2 Setup

Pretty much, everything is taken care of for you.
  However, B<Moz> does use a little compiled AppleScript library (the file CodeWarriorLib) for some of its communcication with CodeWarrior.
  If this library isn't in the same directory as "Moz.pm", then you need to tell B<Moz> where to find it.
  Call C<UseCodeWarriorLib($path_to_CodeWarriorLib)>.
  This routine is not exported by default, nor are you likely to need it.

=cut

sub UseCodeWarriorLib($)
	{
#		($CodeWarriorLib) = @_;
#		$CodeWarriorLib = full_path_to($CodeWarriorLib);
	}

sub activate_CodeWarrior()
	{
#MacPerl::DoAppleScript(<<END_OF_APPLESCRIPT);
#	tell (load script file "$CodeWarriorLib") to ActivateCodeWarrior()
#END_OF_APPLESCRIPT
	}

BEGIN
	{
#		UseCodeWarriorLib(":CodeWarriorLib");
#		activate_CodeWarrior();
		CodeWarriorLib::activate();
	}

$logging								= 0;
$recent_errors_file			= "";
$stop_on_1st_error			= 1;
$QUIET									= 0;



=head2 Logging all the errors and warnings - C<OpenErrorLog($log_file)>, C<CloseErrorLog()>

The warnings and errors generated in the course of building projects can be logged to a file.
Tinderbox uses this facility to show why a remote build failed.

Logging is off by default.
  Start logging at any point in your build process with C<OpenErrorLog($log_file)>.
  Stop with C<CloseErrorLog()>.
  You never need to close the log explicitly, unless you want to just log a couple of projects in the middle of a big list.
  C<CloseErrorLog()> is not exported by default. 

=cut

sub CloseErrorLog()
	{
		if ( $logging )
			{
				close(ERROR_LOG);
				$logging = 0;
				StopForErrors() if $stop_on_1st_error;
			}
	}



sub OpenErrorLog($)
	{
		my ($log_file) = @_;

		CloseErrorLog();
		if ( $log_file )
			{
				$log_file = full_path_to($log_file);
 
				open(ERROR_LOG, ">$log_file") || die "Can't open logfile, check the file path.\n";
				MacPerl::SetFileInfo("CWIE", "TEXT", $log_file);

				$log_file =~ m/.+:(.+)/;
				$recent_errors_file = full_path_to("$1.part");
				$logging = 1;
			}
	}


=head2 Stopping before it's too late - C<StopForErrors()>, C<DontStopForErrors()>

When building a long list of projects, you decide whether to continue building subsequent projects when one fails.
  By default, your build script will C<die> after the first project that generates an error while building.
  Change this behavior with C<DontStopForErrors()>.
  Re-enable it with C<StopForErrors()>.

=cut

sub StopForErrors()
	{
		$stop_on_1st_error = 1;
		
			# Can't stop for errors unless we notice them.
			# Can't notice them unless we are logging.
			# If the user didn't explicitly request logging, log to a temporary file.

		if ( ! $recent_errors_file )
			{
				OpenErrorLog("${TMPDIR}BuildResults");
			}
	}

sub DontStopForErrors()
	{
		$stop_on_1st_error = 0;		
	}

sub log_message($)
	{
		if ( $logging )
			{
				my ($message) = @_;
				print ERROR_LOG $message;
			}
	}

sub log_message_with_time($)
	{
		if ( $logging )
			{
				my ($message) = @_;
				my $time_stamp = localtime();
				log_message("$message ($time_stamp)\n");
			}
	}

sub log_recent_errors($)
	{
		my ($project_name) = @_;
		my $found_errors = 0;
	
		if ( $logging )
			{
				open(RECENT_ERRORS, "<$recent_errors_file");

				while( <RECENT_ERRORS> )
					{
						if ( /^Error/ || /^Couldn’t find project file/ || /^Link Error/ )
							{
#								if (!$found_errors)
#									print $_;
								$found_errors = 1;
							}
						print ERROR_LOG $_;
					}

				close(RECENT_ERRORS);
				unlink("$recent_errors_file");
			}
		
		if ( $stop_on_1st_error && $found_errors )
			{
				print ERROR_LOG "### Build failed.\n";
				die "### Errors encountered building \"$project_name\".\n";
			}
	}

sub build_project($$$)
	{
		my ($project_path, $target_name, $clean_build) = @_;
		$project_path = full_path_to($project_path);

#		$project_path =~ m/.+:(.+)/;
#		my $project_name = $1;

		log_message_with_time("### Building \"$project_path\"");

			# Check that the given project exists
		if (! -e $project_path)
			{
				print ERROR_LOG "### Build failed.\n";
				die "### Can't find project file \"$project_path\".\n";
			}
		
		print "Building \"$project_path\[$target_name\]\"\n";
		
		$had_errors = CodeWarriorLib::build_project(
			$project_path, $target_name, $recent_errors_file, $clean_build
		);
		WaitNextEvent();

#		$had_errors =
#MacPerl::DoAppleScript(<<END_OF_APPLESCRIPT);
#	tell (load script file "$CodeWarriorLib") to BuildProject("$project_path", "$project_name", "$target_name", "$recent_errors_file", $clean_build)
#END_OF_APPLESCRIPT

			# Append any errors to the globally accumulated log file
#		if ( $had_errors ) # Removed this test, because we want warnings, too.  -- jrm
			{
				log_recent_errors($project_path);
			}
	}

=head2 Getting CodeWarrior to build projects - C<BuildProject($project, $opt_target)>, C<BuildProjectClean($project, $opt_target)>

C<BuildProject()> and C<BuildProjectClean()> are identical, except that the latter first removes object code.
  In both, CodeWarrior opens the project if it wasn't already open; builds the given (or else current) target; and finally closes
 the project, if it wasn't already open.

=cut

sub BuildProject($;$)
	{
		my ($project_path, $target_name) = @_;
		build_project($project_path, $target_name, 0);
	}

sub BuildProjectClean($;$)
	{
		my ($project_path, $target_name) = @_;
		build_project($project_path, $target_name, 1);
	}


=head2 Miscellaneous

C<MakeAlias($old_file, $new_file)> functions like C<symlink()>, except with better argument defaulting and more explicit error messages.

=cut

sub MakeAlias($$)
	{
		my ($old_file, $new_file) = @_;

			# if the directory to hold $new_file doesn't exist, create it
		if ( ($new_file =~ m/(.+:)/) && !-d $1 )
			{
				mkpath($1);
			}

			# if a leaf name wasn't specified for $new_file, use the leaf from $old_file
		if ( ($new_file =~ m/:$/) && ($old_file =~ m/.+:(.+)/) )
			{
				$new_file .= $1;
			}

		my $message = "Can't create a Finder alias (at \"$new_file\")\n for \"$old_file\"; because ";

		die "$message \"$old_file\" doesn't exist.\n" unless -e $old_file;
		die "$message I won't replace an existing (non-alias) file with an alias.\n" if ( -e $new_file && ! -l $new_file );

			# now: $old_file exists; $new_file doesn't (or else, is an alias already)

		if ( -l $new_file )
			{
					# ...then see if it already points to $old_file
				my $current_target	= full_path_to(readlink($new_file));
				my $new_target			= full_path_to($old_file);

				return if ( $current_target eq $new_target );
					# if the desired alias already exists and points to the right thing, then we're done
				
				unlink $new_file;
			}
		
		symlink($old_file, $new_file) || die "$message symlink returned an unexpected error.\n";
	}
	
	
=pod

C<InstallFromManifest()>

=cut

sub InstallFromManifest($;$)
	{
		my ($manifest_file, $dest_dir) = @_;

		$dest_dir ||= ":";

		$manifest_file =~ m/(.+):/;
		my $source_dir =  $1;

		chop($dest_dir) if $dest_dir =~ m/:$/;

		#Mac::Events->import();
		WaitNextEvent();
		print "Doing manifest on \"$manifest_file\"\n" unless $QUIET;
		
		my $read = maniread(full_path_to($manifest_file));
		foreach $file (keys %$read)
			{
				next unless $file;

				$subdir = ":";
				if ( $file =~ /:.+:/ )
					{
						$subdir = $&;
					}

				$file = ":$file" unless $file =~ m/^:/;
				MakeAlias("$source_dir$file", "$dest_dir$subdir");
			}
	}

 sub SetBuildNumber
 {

   open (OUTPUT, ">:mozilla:config:build_number") || die "could not open buildnumber";

   open (BDATE, "perl :mozilla:config:bdate.pl|");
   
   while (<BDATE>) {
     print OUTPUT $_;
   }

   close (BDATE);
   close (OUTPUT);

   system ("perl :mozilla:config:aboutime.pl :mozilla:l10n:us:xp:about-all.html :mozilla:config:build_number");

 }

sub SetAgentString
{
	
	open (BDATE, ":mozilla:config:build_number") || die "could not open buildnumber";
	   
	while (<BDATE>) {
		$build_number = $_;
	}
	
	close (BDATE);
	
	open (ORIGFILE, ":mozilla:cmd:macfe:restext:custom.r") || die "no original file";
	open (OUTPUT, ">:mozilla:cmd:macfe:restext:agent.r") || die "no output file";
	
	chop($build_number);
	
	while (<ORIGFILE>) {
	
		$tempstring = $_;
		if ($tempstring =~	"\#define		VERSION_MAJOR_STR") {
			$tempstring = "\#define		VERSION_MAJOR_STR	\"5.0a1-" . $build_number . " Development\"\n";
		}
		print OUTPUT $tempstring;
	}
	
	close (ORIGFILE);
	close (OUTPUT);
	
	unlink (":mozilla:cmd:macfe:restext:custom.r");
	rename (":mozilla:cmd:macfe:restext:agent.r", ":mozilla:cmd:macfe:restext:custom.r");
}

sub SetTimeBomb($$)
		
{
  my ($warn_days, $bomb_days) = @_;
  
  system("perl :mozilla:config:mac-set-timebomb.pl $warn_days $bomb_days");
	
}

sub Delay($)
		
{
  my ($delay_seconds) = @_;

  $now = time;
  
  $exit_time = $now + $delay_seconds;

  while ($exit_time > $now) {
     $now = time;
  }
	
}


sub ActivateApplication($)
{
	my ($appSignature) = @_;
	my ($psi, $found);
	my ($appPSN);
	
	$found = 0;
	
	foreach $psi (values(%Process))
	{
		if ($psi->processSignature() eq $appSignature)
		{
			$appPSN = $psi->processNumber();
			$found = 1;
      last;
		}
	}

	if ($found == 0)
	{
		return;
	}
		
	SetFrontProcess($appPSN);

	while (GetFrontProcess() != $appPSN)
	{
		WaitNextEvent();
	}

}

1;

=head1 AUTHORS

Scott Collins <scc@netscape.com>, Simon Fraser <sfraser@netscape.com>, Chris Yeh <cyeh@netscape.com>

=head1 SEE ALSO

BuildMozillaDebug.pl (et al), BuildList.pm, CodeWarriorLib (an AppleScript library)

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
