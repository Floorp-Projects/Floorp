
package MozBuildFlags;

require 5.004;
require Exporter;

# Package that attempts to read a file from the Preferences folder,
# and get build settings out of it

use strict;
use Exporter;

use MozPrefs;

use vars qw(@ISA @EXPORT);

@ISA      = qw(Exporter);
@EXPORT   = qw(SetupBuildParams);


#-------------------------------------------------------------------------------
# These 3 arrays are the 'master lists' to control what gets built.
# We use arrays here, instead of just intializing the hashes directly,
# so that we can start the build at a given stage using a stored key.
# 
# Ordering in these arrays is important; it has to reflect the order in
# which the build occurs.
#-------------------------------------------------------------------------------

my(@pull_flags) =
(
  ["moz",           1],    # pull everything needed for mozilla
  ["runtime",       0]     # used to just build runtime libs, up to NSPR
);

my(@build_flags) =
(
  ["all",           1],   # 'all' must come first!
  ["dist",          0],
  ["dist_runtime",  0],
  ["xpidl",         0],
  ["idl",           0],
  ["stubs",         0],
  ["runtime",       0],
  ["common",        0],
  ["imglib",        0],
  ["necko",         0],
  ["security",      0],
  ["browserutils",  0],
  ["intl",          0],
  ["nglayout",      0],
  ["editor",        0],
  ["viewer",        0],
  ["xpapp",         0],
  ["extensions",    0],
  ["plugins",       0],
  ["mailnews",      0],
  ["apprunner",     0],
  ["resources",     0]
);

my(@options_flags) = 
(
  ["jar_manifests", 0],     # use jar.mn files for resources, not MANIFESTs
  ["jars",          0],     # build jar files
  ["transformiix",  0],     # obsolete?
  ["mathml",        0],
  ["svg",           0],
  ["mng",           1],
  ["ldap",          0],
  ["xmlextras",     0],
  ["mailextras",    1],     # mail importers
  ["xptlink",       0]      # xpt linker codewarrior plugin
);


#-------------------------------------------------------------------------------
# End of build flags
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# flagsArrayToHash
# 
# Utility routine to migrate flag from a 2D array to a hash, where
# item[n][0] is the hash entry name, and item[n][1] is the hash entry value.
#-------------------------------------------------------------------------------

sub flagsArrayToHash($$)
{
  my($src_array, $dest_hash) = @_;
  
  my($item);
  foreach $item (@$src_array)
  {
    $dest_hash->{$item->[0]} = $item->[1];
  }
}

#-----------------------------------------------
# printHash
#
# Utility routine to print a hash
#-----------------------------------------------
sub printHash($)
{
	my($hash_ref) = @_;
 
  print "Printing hash:\n";
  
  my($key, $value);
  
  while (($key, $value) = each (%$hash_ref))
  {
    print "  $key $value\n";
  }
}


#-------------------------------------------------------------------------------
# SetPullFlags
#-------------------------------------------------------------------------------
sub SetPullFlags($)
{
  my($pull) = @_;
  
  flagsArrayToHash(\@pull_flags, $pull);
}

#-------------------------------------------------------------------------------
# SetBuildFlags
#-------------------------------------------------------------------------------
sub SetBuildFlags($)
{
  my($build) = @_;

  flagsArrayToHash(\@build_flags, $build);
}

#-------------------------------------------------------------------------------
# SetBuildOptions
#-------------------------------------------------------------------------------
sub SetBuildOptions($)
{
  my($options) = @_;

  flagsArrayToHash(\@options_flags, $options);
}

#-------------------------------------------------------------------------------
# SetOptionDefines
#-------------------------------------------------------------------------------
sub SetOptionDefines($)
{
  my($optiondefines) = @_;

  # These should remain unchanged
  $optiondefines->{"mathml"}{"MOZ_MATHML"}		= 1;
  $optiondefines->{"svg"}{"MOZ_SVG"}		    	= 1;
}


#-------------------------------------------------------------------------------
# PropagateAllFlags
#-------------------------------------------------------------------------------
sub PropagateAllFlags($)
{
  my($build_array) = @_;
  
  # if "all" is set, set all the flags to 1
  unless ($build_array->[0][0] eq "all") { die "'all' must come first in the build array\n"; }
  
  if ($build_array->[0][1] == 1)
  {
    my($index);
    foreach $index (@$build_array)
    {
      $index->[1] = 1;
    }
  }
}

#-------------------------------------------------------------------------------
# SetupBuildParams
#-------------------------------------------------------------------------------
sub SetupBuildParams($$$$$)
{
  my($pull, $build, $options, $optiondefines, $prefs_file) = @_;
    
  # read the user pref file, that can change values in the array
  ReadMozUserPrefs($prefs_file, \@pull_flags, \@build_flags, \@options_flags);

  PropagateAllFlags(\@build_flags);
  
	SetPullFlags($pull);
	SetBuildFlags($build);
	SetBuildOptions($options);
  SetOptionDefines($optiondefines);
  
  #printHash($build);
}


1;
