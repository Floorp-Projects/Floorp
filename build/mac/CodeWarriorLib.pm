#!perl
package CodeWarriorLib;

=pod

=head1 NAME

CodeWarriorLib - supply interface to CodeWarrior

=head1 SYNOPSIS

	#!perl
	use CodeWarriorLib;
	CodeWarriorLib::activate();
	$had_errors = CodeWarriorLib::build_project(
		$project_path, $target_name, $recent_errors_file, $clean_build
	);

=head1 DESCRIPTION

Replaces the AppleScript library I<CodeWarriorLib>.

=over 4

=cut

use strict;
use Mac::Types;
use Mac::AppleEvents;
use Mac::AppleEvents::Simple;
use Mac::Processes;
use Mac::MoreFiles;
use Mac::StandardFile;
use File::Basename;

use vars qw($VERSION);
$VERSION = '1.02';

my($app)  = 'CWIE';

# 0 == don't switch CWIE to front app in do_event(), 1 == do switch
# note: activate() still switches when called
$Mac::AppleEvents::Simple::SWITCH = 0;

# supply your own path to the source here
#_test('PowerPudgeIV:mozilla:mozilla:');


=pod

=item _get_project($full_path)

A private routine returning a reference to the open project with the given name,
or else the empty string (when that project is not open)

full_path is a string identifying the project to be built and is of the form, 
e.g., "HD:ProjectFolder:MyProject.mcp".  It must be supplied.

=cut

sub _get_project ($) {
	my(
		$full_path, $candidate_projects
	) = @_;
	$candidate_projects = _doc_named(basename($full_path, '*'));
	if ($candidate_projects) {
		my($cps) = _get_dobj($candidate_projects);
		my($num) = AECountItems($cps);
		if ($num) {  # is a list
			foreach (1 .. AECountItems($cps)) {
				my($cp) = AEGetNthDesc($cps, $_);
				if ($full_path eq _full_path($cp)) {
					return($cp);
				}
			}
		} else {     # is only one, not a list
			if ($full_path eq _full_path($cps)) {
				return($cps);
			}
		}
	}
	return;
}

=pod

=item build_project

Build a selected target of a project, saving any errors to a file, if supplied.
	
full_path is a string identifying the project to be built and is of the form, 
e.g., "HD:ProjectFolder:MyProject.mcp".  It must be supplied.

If target_name is the empty string, the current target of the selected project 
will be built, else, target_name should be a string matching a target name in 
the selected project.
	
If error_path is the empty string, errors will not be saved to a file,
else, error_path should be the full path of a file to save error messages into.

=cut

sub build_project ($;$$$) {
	my(
		$full_path, $target_name, $error_path,
		$remove_object, $p, $project_was_closed, $had_errors
	) = @_;
	_close_errors_window();

	while (1) {
		$p = _get_project($full_path);
		if (!$p) {
			if ($project_was_closed) {
				print "### Error - request for project document failed after opening\n";
				die "### possibly CW Pro 4 bug: be sure to close your Find window\n";
			}
			$project_was_closed = 1;
			_open_file($full_path);
		} else {
			last;
		}
	}

	$had_errors = 0;
	if ($target_name eq '') {
		if ($remove_object) {_remove_object($p)}
		_build($p);
	} else {
		if ($remove_object) {_remove_object($p, $target_name)}
		_build($p, $target_name);
	}

	if ($error_path ne '') {
		_save_errors_window($error_path);
	}
	$had_errors = _close_errors_window();

	if ($project_was_closed) {
		$p = _get_project($full_path);
		_close($p);
	}

	return($had_errors);
}

=pod

=item activate()

Launches CodeWarrior and brings it to the front.

Once found, path will be saved in ':idepath.txt' for future reference.
Edit or delete this file to change the location of the IDE.  If app is
moved, C<activate()> will prompt for a new location.

First looks for an open CodeWarrior app.  Second, tries to open previously
saved location in ':idepath.txt'.  Third, tries to find it and allow user
to choose it with Navigation Services (if present).  Fourth, uses good old
GUSI routines built-in to MacPerl for a Choose Directory dialog box.

=cut

sub activate () {
	local(*F);
	my($filepath, $appath, $psi) = (':idepath.txt');

	foreach $psi (values(%Process)) {
		if ($psi->processSignature() eq $app) {
			$appath = $psi->processAppSpec(), "\n";
                        _save_appath($filepath, $appath);
                        last;
		}
	}

	if ((!$appath || ! -x $appath) && open(F, $filepath)) {
		$appath = <F>;
		close(F);
	}

	if (!$appath || ! -x $appath)
	{
		# make sure that MacPerl is a front process
		#ActivateApplication('McPL');
		MacPerl::Answer("Please locate the CodeWarrior application.", "OK");
		
		# prompt user for the file name, and store it
		my $macFile = StandardGetFile( 0, "APPL");
		if ( $macFile->sfGood() )
		{
			$appath = $macFile->sfFile();
		}
		else
		{
			die "Operation canceled\n";
		}
	
#		if (eval {require Mac::Navigation}) {
#			my($options, $nav);
#			Mac::Navigation->import();
#			$options = NavGetDefaultDialogOptions();
#			$options->message('Where is CodeWarrior IDE?');
#			$options->windowTitle('Find CodeWarrior IDE');
#			$nav = NavChooseObject($Application{$app}, $options);
#			die "CodeWarrior IDE not found.\n" if (!$nav || !$nav->file(1));
#			$appath = $nav->file(1);
#		} else {
#			local(*D);
#			my $cwd = `pwd`;
#			$appath = _get_folder(
#                               'Where is the CW IDE folder?',
#				dirname($Application{$app})
#			);
#			die "CodeWarrior IDE not found.\n" if !$appath;
#			opendir(D, $appath) or die $!;
#			chdir($appath);
#			foreach my $file (sort readdir (D)) {
#				my(@app) = MacPerl::GetFileInfo($file);
#				if ($app[0] && $app[1] &&
#					$app[1] eq 'APPL' && $app[0] eq $app
#				) {
#					$appath .= $file;
#					last;
#				}
#			}
#			chomp($cwd);
#			chdir($cwd);
#		}
		_save_appath($filepath, $appath);
	}

	my($lp) = LaunchParam->new(
		launchAppSpec => $appath,
		launchControlFlags => launchContinue() + launchNoFileFlags()
	);
        unless (LaunchApplication($lp)) {
                unlink($filepath);
                die $^E;
        }
}

sub _build ($;$) {
	my($evt);
	if ($_[1]) {
		my($prm) =
			q"'----':obj {form:name, want:type(TRGT), seld:TEXT(@), from:" .
			AEPrint($_[0]) . '}';
		$evt = do_event(qw/CWIE MAKE/, $app, $prm, $_[1]);
	} else {
		my($prm) = q"'----':" . AEPrint($_[0]);
		$evt = do_event(qw/CWIE MAKE/, $app, $prm);
	}
}

sub _remove_object ($;$) {
	my($evt);
	if ($_[1]) {
		my($prm) =
			q"'----':obj {form:name, want:type(TRGT), seld:TEXT(@), from:" .
			AEPrint($_[0]) . '}';
		$evt = do_event(qw/CWIE RMOB/, $app, $prm, $_[1]);
	} else {
		my($prm) = q"'----':" . AEPrint($_[0]);
		$evt = do_event(qw/CWIE RMOB/, $app, $prm);
	}
}

sub _open_file ($) {
	my($prm) =
		q"'----':obj {form:name, want:type(alis), " .
		q"seld:TEXT(@), from:'null'()}";

	do_event(qw/aevt odoc/, $app, $prm, $_[0]);
}

sub _doc_named ($) {
	my($prm) =
		q"'----':obj {form:test, want:type(docu), from:'null'(), " .
		q"seld:cmpd{relo:'=   ', 'obj1':obj {form:prop, want:type" .
		q"(prop), seld:type(pnam), from:'exmn'()}, 'obj2':TEXT(@)}}";

	my($evt) = do_event(qw/core getd/, $app, $prm, $_[0]);
	return($evt->{REPLY} eq 'aevt\ansr{}' ? undef : $evt);
}

sub _full_path ($) {
	my($obj) = $_[0];
	my($prm) =
		q"'----':obj {form:prop, want:type(prop), seld:type(FILE), " .
		q"from:" . AEPrint($_[0]) . q"}, rtyp:type(TEXT)";
	my($evt) = do_event(qw/core getd/, $app, $prm);

	return MacPerl::MakePath(
		MacUnpack('fss ', (
			AEGetParamDesc($evt->{REP}, keyDirectObject()))->data()->get()
		)
	);
}

sub _save_errors_window ($) {
	my($prm) = 
		q"'----':obj {form:name, want:type(alis), seld:TEXT(@), from:'null'()}";
	do_event(qw/MMPR SvMs/, $app, $prm, $_[0]);
}


sub _close_errors_window () {
	my($prm) =
		q"'----':obj {form:name, want:type(cwin), " .
		q"seld:TEXT(@), from:'null'()}";

	my($evt) = do_event(qw/core clos/, $app, $prm, 'Errors & Warnings');
	return($evt->{REPLY} eq 'aevt\ansr{}' ? 1 : 0);
}

sub _close () {
	my($prm) = q"'----':" . AEPrint($_[0]);
	do_event(qw/core clos/, $app, $prm);
}

sub _get_dobj ($) {
	return(AEGetParamDesc($_[0]->{REP}, keyDirectObject()));
}

sub _get_folder ($$) {
    require 'GUSI.ph';
    my($prompt, $default) = @_;
    MacPerl::Choose(
        GUSI::AF_FILE(), 0, $prompt, '',
        GUSI::CHOOSE_DIR() + ($default ? &GUSI::CHOOSE_DEFAULT : 0),
        $default
    );
}

sub _save_appath ($$) {
	open(F, '>' . $_[0]) or die $!;
	print F $_[1];
	close(F);
}

sub _test ($) {
	activate();
	my($path) = $_[0];
	build_project(
	  "${path}modules:xml:macbuild:XML.mcp", '',
	  "${path}build:mac:Mozilla.BuildLog.part"
	);
}

1;

=pod

=back

=head1 HISTORY

=over 4

=item v1.02, September 23, 1998

Made fixes in finding and saving location of CodeWarrior IDE.

=item v1.01, June 1, 1998

Made fixes to C<chdir()> in C<activate()>, made C<activate()> more robust 
in finding CodeWarrior IDE, added global variable to NOT switch to IDE
for each sent event, a few other fixes.

=item v1.00, May 30, 1998

First shot

=back


=head1 AUTHORS

Chris Nandor F<E<lt>pudge@pobox.comE<gt>>, and the author of the
original I<CodeWarriorLib>, Scott Collins F<E<lt>scc@netscape.comE<gt>>.

=head1 SEE ALSO

BuildProject L<Moz>.

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
