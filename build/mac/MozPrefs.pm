
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
% Note that by default, the scripts have $build{"all"} and $pull{"all"}
% turned on, which overrides other settings. To do partial builds, turn
% these off thus:
%    build       all             0
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
# ReadPrefsFile
#
#-------------------------------------------------------------------------------

sub ReadPrefsFile($$$$)
{
  my($file_path, $pull_hash, $build_hash, $options_hash) = @_;
  
  if (open(PREFS_FILE, "< $file_path"))
  {
    print "Reading build prefs from $file_path\n";
  
    while (<PREFS_FILE>)
    {
      my($line) = $_;
            
      if ($line =~ /$\#/) {    # ignore comments
        next;
      }
      
      if ($line =~ /$\s*(\w+)\s+(\w+)\s+(\w+)\s*/)
      {
        my($array_name) = $1;      
        my($option_name) = $2;
        my($option_value) = $3;      
      
        if ($array_name eq "pull")
        {
          $pull_hash->{$option_name} = $option_value;
        }
        elsif ($array_name eq "build")
        {
          $build_hash->{$option_name} = $option_value;
        }
        elsif ($array_name eq "options")
        {
          $options_hash->{$option_name} = $option_value;
        }
        else
        {
          print "Unknown pref option at $line\n";
        }
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
  my($prefs_file_name, $pull_hash, $build_hash, $options_hash) = @_;
  
  my($prefs_path) = GetPrefsFolder();
  $prefs_path .= ":$prefs_file_name";

  ReadPrefsFile($prefs_path, $pull_hash, $build_hash, $options_hash);
}

1;
