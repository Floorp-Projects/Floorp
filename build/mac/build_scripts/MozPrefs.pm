
package MozPrefs;

require 5.004;
require Exporter;

# Package that attempts to read a file from the Preferences folder,
# and get build settings out of it

use strict;

use Exporter;
use Mac::Files;

use vars qw(@ISA @EXPORT);

@ISA		  = qw(Exporter);
@EXPORT 	= qw(ReadMozUserPrefs);



#-------------------------------------------------------------------------------
#
# GetPrefsFolder
#
#-------------------------------------------------------------------------------

sub GetPrefsFolder()
{
  my($prefs_folder) = FindFolder(kOnSystemDisk, kPreferencesFolderType, 1);
  return $prefs_folder;
}


#-------------------------------------------------------------------------------
#
# SetArrayValue
#
#-------------------------------------------------------------------------------
sub SetArrayValue($$$)
{
  my($array_ref, $index1, $index2) = @_;

  my($index);
  foreach $index (@$array_ref)
  {
    if ($index->[0] eq $index1)
    {
      $index->[1] = $index2;
      return 1;
    }
  }
  
  return 0;
}


#-------------------------------------------------------------------------------
#
# WriteDefaultPrefsFile
#
#-------------------------------------------------------------------------------

sub WriteDefaultPrefsFile($)
{
  my($file_path) = @_;

  my($file_contents);
  $file_contents = <<'EOS';
% You can use this file to customize the Mozilla build system.
% The following kinds of lines are allowable:
%   Comment lines, which start with a '%' in the first column
%   Lines with modify the default build settings. Examples are:
%
%    pull        runtime         1       % just pull runtime
%    options     mng             1       % turn mng on
%    build       jars            0       % don't build jar files
%
EOS

  $file_contents =~ s/%/#/g;
   
  open(PREFS_FILE, "> $file_path") || die "Could not write default prefs file\n";
  print PREFS_FILE ($file_contents);
  close(PREFS_FILE);

  MacPerl::SetFileInfo("McPL", "TEXT", $file_path);
}


#-------------------------------------------------------------------------------
#
# HandlePrefSet
#
#-------------------------------------------------------------------------------
sub HandlePrefSet($$$$)
{
  my($flags, $name, $value, $desc) = @_;

  if (SetArrayValue($flags, $name, $value)) {
    print "Prefs set $desc flag $name to $value\n";
  } else {
    die "$desc setting '$name' is not a valid option\n";
  }

}


#-------------------------------------------------------------------------------
#
# HandleBuildFromPref
#
#-------------------------------------------------------------------------------
sub HandleBuildFromPref($$)
{
  my($build_array, $name) = @_;

  my($setting) = 0;
  my($index);
  foreach $index (@$build_array)
  {
    if ($index->[0] eq $name) {
      $setting = 1;
    }
    
    $index->[1] = $setting;    
  }

  if ($setting == 1) {
    print "Building from $name onwards, as specified by prefs\n";
  } else {
    printf "Failed to find buildfrom setting '$name'\n";
  }
}


#-------------------------------------------------------------------------------
#
# ReadPrefsFile
#
#-------------------------------------------------------------------------------

sub ReadPrefsFile($$$$)
{
  my($file_path, $pull_flags, $build_flags, $options_flags) = @_;
  
  if (open(PREFS_FILE, "< $file_path"))
  {
    print "Reading build prefs from $file_path\n";
  
    while (<PREFS_FILE>)
    {
      my($line) = $_;
            
      if ($line =~ /^\#/ || $line =~ /^\s+$/) {    # ignore comments and empty lines
        next;
      }
      
      if ($line =~ /^\s*(\w+)\s+(\w+)\s+(\w+)\s*/)
      {
        my($array_name) = $1;      
        my($option_name) = $2;
        my($option_value) = $3;      
      
        if ($array_name eq "pull")
        {
          HandlePrefSet($pull_flags, $option_name, $option_value, "Pull");
        }
        elsif ($array_name eq "build")
        {
          HandlePrefSet($build_flags, $option_name, $option_value, "Build");
        }
        elsif ($array_name eq "options")
        {
          HandlePrefSet($options_flags, $option_name, $option_value, "Options");
        }
        else
        {
          print "Unknown pref option at $line\n";
        }
      }
      elsif ($line =~ /^\s*buildfrom\s+(\w+)/)
      {
        my($build_start) = $1;
        HandleBuildFromPref($build_flags, $build_start);
      }
      else
      {
        print "Unknown pref option at $line\n";
      }
      
    }
    
    close(PREFS_FILE);  
  }
  else
  {
    print "No prefs file found at $file_path; using defaults\n";
    WriteDefaultPrefsFile($file_path);
  }
}


#-------------------------------------------------------------------------------
#
# ReadMozUserPrefs
#
#-------------------------------------------------------------------------------

sub ReadMozUserPrefs($$$$)
{
  my($prefs_file_name, $pull_flags, $build_flags, $options_flags) = @_;
  
  my($prefs_path) = GetPrefsFolder();
  $prefs_path .= ":$prefs_file_name";

  ReadPrefsFile($prefs_path, $pull_flags, $build_flags, $options_flags);
}

1;
