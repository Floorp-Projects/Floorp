#!perl

#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

package			Moz;
require			Exporter;

@ISA				= qw(Exporter);
@EXPORT			= qw();
@EXPORT_OK	= qw(BuildProject,OpenErrorLog,CloseErrorLog,UseCodeWarriorLib,Configure);

	use Cwd;

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

sub Configure($)
	{
		my ($config_file) = @_;
		# read in the configuration file
	}

$logging								= 0;
$recent_errors_file			= "";

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

sub log_recent_errors()
	{
		if ( $logging )
			{
				open(RECENT_ERRORS, "<$recent_errors_file");

				while( <RECENT_ERRORS> )
					{
						print ERROR_LOG $_;
					}

				close(RECENT_ERRORS);
				unlink("$recent_errors_file");
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
				log_recent_errors();
			}
	}

1;