#!perl -w
package GenerateManifest;

require 5.004;

use strict;
use File::stat;
require Exporter;

use vars qw(@ISA @EXPORT);

# Package that generates a jar manifest from an input file

@ISA      = qw(Exporter);
@EXPORT   = qw(
                GenerateManifest
              );

my(%embed_files) = ();


sub GenerateManifest ($$$$$$$$) {
  my($moz, $manifest, $chrome, $locale, $platform, $out_desc, $dir_sep, $verbose) = @_;
  local(*OUTDESC) = $out_desc;  

  parse_input_manifest($moz, $manifest, $chrome, $locale, $verbose);
  dump_output_manifest($moz, $manifest, $chrome, $locale, $platform, *OUTDESC, $dir_sep, $verbose);
}


sub parse_input_manifest ($$$$$) {
  my($moz, $manifest, $chrome, $locale, $verbose) = @_;  

  print STDERR "Parsing \"$manifest\"\n" unless !$verbose;

  local(*MANIFEST);
  open(MANIFEST, "<$manifest") or die ("Error: Cannot open manifest \"$manifest\".\n");
  while(<MANIFEST>) {
    chomp;
    s/^\s+//;
    s/\s+$//;

    # Skip comments & blank lines
    next if (/^\#/);
    next if (/^\s*$/);

    # Read key & data
    my($key, $value) = split(/,/, $_);
    
    # Strip out any remaining whitespace from key & value
    for ($key) {
        s/\s+$//;
    }
    for ($value) {
        s/^\s+//;
    }

    $embed_files{$key} = $value;
  }
  close(MANIFEST);
}

sub dump_output_manifest ($$$$$$$$) {
  my($moz, $manifest, $chrome, $locale, $platform, $out_desc, $dir_sep, $verbose) = @_;
  local(*OUTDESC) = $out_desc;

  print OUTDESC "embed.jar:\n";
  while (my($key, $value) = each %embed_files) {

    $key =~ s/XXXX/$locale/g;
    $value =~ s/XXXX/$locale/g;
    $key =~ s/YYYY/$platform/g;
    $value =~ s/YYYY/$platform/g;
    if ( $dir_sep ne "/" ) {      # swap / for $dir_sep
      $value =~ s/\//$dir_sep/g;
    }

    # Run ls on the dir/file to ensure it's there and to get a file list back
    my($ls_path) = "$chrome$dir_sep$value";
    my($is_dir) = (-d $ls_path) ? 1 : 0;
    my($is_file) = (-f $ls_path) ? 1 : 0;

    print STDERR "Listing \"$ls_path\"\n" unless !$verbose;

    if (!$is_dir && !$is_file) {
      print STDERR "Warning: File or directory \"$ls_path\" does not exist.\n";
      next;
    }

    # this code previously used |ls -1| to get a dir listing, but that
    # doesn't work in MacPerl. Instead just use opendir() to get the 
    # file list (if it's a directory), or add the single file to our list
    # if it's called out individually in the manifest.
    my(@dirList) = ();
    if ( $is_file ) {
      @dirList = ($ls_path);
    }
    else {
      opendir(CHROMEDIR, $ls_path);
      @dirList = readdir(CHROMEDIR);
      closedir(CHROMEDIR);
    }
    
    my($chrome_file) = "";
    my($real_file) = "";
    foreach (@dirList) {
      if ($is_dir) {
        $chrome_file = "$key$dir_sep$_";
        $real_file = "$value$dir_sep$_";
      }
      else {
        $chrome_file = $key;
        $real_file = $value;
      }
      # Ignore directories which are returned by ls
      if (! -d "$chrome$dir_sep$real_file") {
        # before we put the file into the manifest, make sure it
        # uses '/' as the separator. That's what manifest files expect.
        $real_file =~ s/$dir_sep/\//g;
        $chrome_file =~ s/$dir_sep/\//g;
        print OUTDESC "  $chrome_file   ($real_file)\n";
      }
    }
  }
}

1;

