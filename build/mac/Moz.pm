=head1 NAME

Moz

=head1 DESCRIPTION

...

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



package			Moz;
require			Exporter;

@ISA				= qw(Exporter);
@EXPORT			= qw(BuildProject OpenErrorLog MakeAlias StopForErrors DontStopForErrors);
@EXPORT_OK	= qw(CloseErrorLog UseCodeWarriorLib);

	use Cwd;
	use File::Path;

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

sub UseCodeWarriorLib($)
	{
		($CodeWarriorLib) = @_;
		$CodeWarriorLib = full_path_to($CodeWarriorLib);
	}

sub activate_CodeWarrior()
	{
MacPerl::DoAppleScript(<<END_OF_APPLESCRIPT);
	tell (load script file "$CodeWarriorLib") to ActivateCodeWarrior()
END_OF_APPLESCRIPT
	}

BEGIN
	{
		UseCodeWarriorLib(":CodeWarriorLib");
		activate_CodeWarrior();
	}

$logging								= 0;
$recent_errors_file			= "";
$stop_on_1st_error			= 1;

sub CloseErrorLog()
	{
		if ( $logging )
			{
				close(ERROR_LOG);
				$logging = 0;
			}
	}

sub OpenErrorLog($)
	{
		my ($log_file) = @_;

		CloseErrorLog();
		if ( $log_file )
			{
				$log_file = full_path_to($log_file);

				open(ERROR_LOG, ">$log_file");

				$log_file =~ m/.+:(.+)/;
				$recent_errors_file = full_path_to("$1.part");
				$logging = 1;
			}
	}

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
						if ( $_ =~ m/^Error/ )
							{
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

sub BuildProject($;$)
	{
		my ($project_path, $target_name) = @_;
		$project_path = full_path_to($project_path);

		$project_path =~ m/.+:(.+)/;
		my $project_name = $1;

		log_message_with_time("### Building \"$project_path\"");

		$had_errors =
MacPerl::DoAppleScript(<<END_OF_APPLESCRIPT);
	tell (load script file "$CodeWarriorLib") to BuildProject("$project_path", "$project_name", "$target_name", "$recent_errors_file")
END_OF_APPLESCRIPT

			# Append any errors to the globally accumulated log file
		if ( $had_errors )
			{
				log_recent_errors($project_path);
			}
	}

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

		die "Can't make an alias for \"$old_file\"; it doesn't exist.\n" unless -e $old_file;
		unlink $new_file;
		symlink($old_file, $new_file) || die "Can't make an alias for \"$old_file\"; unknown error.\n";
	}


1;