#!perl
# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
# Sean Su <ssu@netscape.com>
# 

use Cwd;
return(1);

sub ZipChrome()
{
  # inOsType     - Os type
  # inUpdate     - update or noupdate
  #                update   - enables time/date compare file update of chrome archives
  #                noupdate - disables time/date compare file update of chrome archives.
  #                           it will always update chrome files regardless of time/date stamp.
  # inSourcePath - path to where the tmpchrome dir
  # inTargetPath - path to where the bin\chrome dir
  ($inOsType, $inUpdate, $inSourcePath, $inTargetPath) = @_;

  # check Os type
  if($inOsType =~ /win32/i)
  {
    $gPathDelimiter="\\";
  }
  elsif($inOsType =~ /mac/i)
  {
    $gPathDelimiter=":";
  }
  elsif($inOsType =~ /unix/i)
  {
    $gPathDelimiter="/";
  }
  else
  {
    return(2);
  }

  # Verify Update
  if(!($inUpdate =~ /update/i) &&
     !($inUpdate =~ /noupdate/i))
  {
    return(2);
  }

  if($inOsType =~ /win32/i)
  {
    # Convert all '/' to '\\' or else win32 will have problems
    $inSourcePath =~ s/\//\\/g;
    $inTargetPath =~ s/\//\\/g;
  }

  # Make sure $inSourcePath exists
  if(!(-e "$inSourcePath"))
  {
    return(1);
  }

  # Make sure $inTargetPath exists
  if(!(-e "$inTargetPath"))
  {
    mkdir("$inTargetPath", 775);
  }

  # Call CreateArchive() on locales, packages, and skins
#  CreateArchive("loc", $inSourcePath . $gPathDelimiter . "locales",  "$inTargetPath");
#  CreateArchive("pkg", $inSourcePath . $gPathDelimiter . "packages", "$inTargetPath");
#  CreateArchive("skn", $inSourcePath . $gPathDelimiter . "skins",    "$inTargetPath");
  CreateArchive("jar", $inSourcePath . $gPathDelimiter, "$inTargetPath");
  return(0);
}

sub CreateArchive()
{
  my($inExtension, $inSrc, $inDest) = @_;
  my($dir);
  my(@dirList);
  my(@dirItem);
  my($dirName);
  my($saveCwdir);
  my($mZipParam);

  # Make sure $inSrc exists
  if(!(-e "$inSrc"))
  {
    return(0);
  }

  # Make sure $inSrc is a directory
  if(!(-d "$inSrc"))
  {
    return(0);
  }

  # Make sure $inDest exists
  if(!(-e "$inDest"))
  {
    mkdir("$inDest", 775);
  }

  # Check for extension, if none is passed, use .jar as default
  if($inExtension eq "")
  {
    $inExtension = "jar";
  }

  # Save current working directory
  $saveCwdir = cwd();
  chdir($inSrc);

  # For all the subdirectories within $inSrc, create an archive
  # using the name of the subdirectory, but with the extension passed
  # in as a parameter.
  @dirList = <*>;
  foreach $dir (@dirList)
  {
    if($inOsType =~ /win32/i)
    {
      # Convert all '/' to '\\' or else win32 will have problems
      $dir =~ s/\//\\/g;
    }

    # Get the leaf dir name of full path
    # Get the leaf dir name from full path
    if($inOsType =~ /win32/i)
    {
      @dirItem = split(/\\/, $dir);
    }
    elsif($inOsType =~ /mac/i)
    {
      @dirItem = split(/:/, $dir);
    }
    elsif($inOsType =~ /unix/i)
    {
      @dirItem = split(/\//, $dir);
    }
    $dirName = $dirItem[$#dirItem];

    if(-d "$dir")
    {
      # Zip only works for win32 and unix systems
      if(($inOsType =~ /win32/i) || ($inOsType =~ /unix/i))
      {
        if($inUpdate =~ /noupdate/i)
        {
          print "\n";
          if(-e "$dirName.$inExtension")
          {
            # Delete archive is one already exists in target location
            print " Removing $dirName.$inExtension\n";
            unlink "$dirName.$inExtension";
          }

          print " Creating $dirName.$inExtension\n";
          $mZipParam = "";
        }
        elsif($inUpdate =~ /update/i)
        {
          if(!(-e "$dirName.$inExtension"))
          {
            print "\n";
            print " Creating $dirName.$inExtension\n";
            $mZipParam = "";
          }
          else
          {
            print " Updating $dirName.$inExtension\n";
            $mZipParam = "-u";
          }
        }

        # Create the archive in $inDest
        chdir("$dirName");
        if(system("zip $mZipParam -r ..$gPathDelimiter$dirName.$inExtension *") != 0)
        {
          print "Error: zip $mZipParam -r ..$gPathDelimiter$dirName.$inExtension *\n";
          chdir("..");
          return(1);
        }
        chdir("..");
      }
    }
  }

  # Restore to the current working dir
  chdir($saveCwdir);
  return(0);
}

