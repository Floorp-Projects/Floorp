#!/usr/bin/perl
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla Browser code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Chris Mcafee <mcafee@netscape.com>
#  Brian Ryner <bryner@brianryner.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

use Cwd;
use File::Find ();

use POSIX qw(sys_wait_h);

sub kill_process {
    my ($target_pid) = @_;
    my $start_time = time;

    # Try to kill and wait 10 seconds, then try a kill -9
    my $sig;
    for $sig ('TERM', 'KILL') {
        print "kill $sig $target_pid\n";
        kill $sig => $target_pid;
        my $interval_start = time;
        while (time - $interval_start < 10) {
            # the following will work with 'cygwin' perl on win32, but not
            # with 'MSWin32' (ActiveState) perl
            my $pid = waitpid($target_pid, POSIX::WNOHANG());
            if (($pid == $target_pid and POSIX::WIFEXITED($?)) or $pid == -1) {
                my $secs = time - $start_time;
                $secs = $secs == 1 ? '1 second' : "$secs seconds";
                print "Process killed. Took $secs to die.\n";
                return;
            }
            sleep 1;
        }
    }
    die "Unable to kill process: $target_pid";
}

# Stripped down version of fork_and_log().
sub system_fork_and_log {
    # Fork a sub process and log the output.
    my ($cmd) = @_;

    my $pid = fork; # Fork off a child process.

    unless ($pid) { # child
        exec { $cmd->[0] } @$cmd;
        die "Could not exec()";
    }
    return $pid;
}


sub wait_for_pid {
    # Wait for a process to exit or kill it if it takes too long.
    my ($pid, $timeout_secs) = @_;
    my ($exit_value, $signal_num, $dumped_core, $timed_out) = (0,0,0,0);
    my $sig_name;
    my $loop_count;

    die ("Invalid timeout value passed to wait_for_pid()\n")
        if ($timeout_secs <= 0);

    eval {
        $loop_count = 0;
        while (++$loop_count < $timeout_secs) {
            my $wait_pid = waitpid($pid, POSIX::WNOHANG());
            # the following will work with 'cygwin' perl on win32, but not 
            # with 'MSWin32' (ActiveState) perl
            last if ($wait_pid == $pid and POSIX::WIFEXITED($?)) or $wait_pid == -1;
            sleep 1;
        }

        $exit_value = $? >> 8;
        $signal_num = $? >> 127;
        $dumped_core = $? & 128;
        if ($loop_count >= $timeout_secs) {
            die "timeout";
        }
        return "done";
    };

    if ($@) {
        if ($@ =~ /timeout/) {
            kill_process($pid);
            $timed_out = 1;
        } else { # Died for some other reason.
            die; # Propagate the error up.
        }
    }
#    $sig_name = $signal_num ? signal_name($signal_num) : '';
#
#    return { timed_out=>$timed_out,
#             exit_value=>$exit_value,
#             sig_name=>$sig_name,
#             dumped_core=>$dumped_core };
}

# System version of run_cmd().
sub run_system_cmd {
    my ($cmd, $timeout_secs) = @_;

#    print_log "cmd = $cmd\n";
    my $pid = system_fork_and_log($cmd);
    my $result = wait_for_pid($pid, $timeout_secs);

    return $result;
}

#
# Given profile directory, find pref file hidden in salt directory.
# profile $Settings::MozProfileName must exist before calling this sub.
#
sub find_pref_file {
    my $profile_dir = shift;

    # default to *nix
    my $pref_file = "prefs.js";

    unless (-e $profile_dir) {
        return; # empty list
    }

    my $found = undef;
    my $sub = sub {$pref_file = $File::Find::name, $found++ if $pref_file eq $_};
    File::Find::find($sub, $profile_dir);
    unless ($found) {
        return; # empty list
    }

    return $pref_file;
}

my $topdir = cwd();

chdir $ENV{OBJDIR};
my $app_name = `grep "MOZ_APP_NAME	" config/autoconf.mk | sed "s/.*= //"`;
chomp($app_name);

# On mac, the app directory is the product name with the first
# letter capitalized

my $toolkit = `grep "MOZ_WIDGET_TOOLKIT        " config/autoconf.mk  |sed "s/.*= //"`;
chomp($toolkit);

if ($toolkit =~ /(mac|cocoa)/) {
    my $app_dir = uc(substr($app_name, 0, 1)).substr($app_name, 1);
    chdir "dist/$app_dir.app/Contents/MacOS";
} else {
    chdir "dist/bin";
}

my $bin_suffix = "";
if ($toolkit =~ /(windows|os2)/) {
    $bin_suffix = ".exe";
}

my $old_home = $ENV{HOME};
$ENV{HOME} = cwd();

# Create a profile to test with.
run_system_cmd(["./".$app_name.$bin_suffix, "-createProfile", "testprofile"], 45);

my $pref_file = find_pref_file(".".$app_name);
open PREFS, ">>$pref_file";
# Add allow_scripts_to_close_windows; this lets us cleanly exit.
print PREFS "user_pref(\"dom.allow_scripts_to_close_windows\", true);\n";
# Suppress the default browser dialog since it keeps the test from starting.
print PREFS "user_pref(\"browser.shell.checkDefaultBrowser\", false);\n";
close PREFS;

# Run the pageload test.
run_system_cmd(["./".$app_name.$bin_suffix, $ENV{PAGELOAD_URL}."/loader.pl?maxcyc=2&delay=500&nocache=0&timeout=30000&auto=1"], 240);

# Start up again; this will gather data for reading global history and
# reading the fastload file.
run_system_cmd(["./".$app_name.$bin_suffix, "file://$topdir/build/profile_pageloader.html"], 45);

chdir $topdir;
