#!perl -w
package         MozillaBuildList;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Mac::Processes;
use Mac::Events;
use Mac::Files;
use Cwd;
use Mac::MoreFiles;
use FileHandle;
use File::Path;
use File::Copy;
use File::Basename;

# homegrown
use Moz::Moz;
use Moz::BuildUtils;
use Moz::Jar;
use Moz::MacCVS;

@ISA        = qw(Exporter);
@EXPORT     = qw(
                    BuildDist
                    BuildProjects
                 );


#//--------------------------------------------------------------------------------------------------
#// assert that we are in the correct directory for the build
#//--------------------------------------------------------------------------------------------------
sub assertRightDirectory()
{
    unless (-e ":mozilla")
    {
        my($dir) = cwd();
        print STDERR "MozillaBuildList called from incorrect directory: $dir";
    } 
}


#//--------------------------------------------------------------------------------------------------
#// UpdateBuildNumberFiles
#//--------------------------------------------------------------------------------------------------
sub UpdateBuildNumberFiles()
{
    my(@gen_files) = (
        ":mozilla:config:nsBuildID.h",
        ":mozilla:xpfe:global:build.dtd"
    );
    SetBuildNumber(":mozilla:config:build_number", \@gen_files);
}

#//--------------------------------------------------------------------------------------------------
#// Select a default skin
#//--------------------------------------------------------------------------------------------------

sub SetDefaultSkin($)
{
    my($skin) = @_;

    assertRightDirectory();

    my($dist_dir) = GetBinDirectory();
    my($chrome_subdir) = $dist_dir."Chrome";
    
    print "Setting default skin to $skin\n";
    
  	local(*CHROMEFILE);
    open(CHROMEFILE, ">>${chrome_subdir}:installed-chrome.txt") || die "Failed to open installed_chrome.txt\n";
    print(CHROMEFILE "skin,install,select,$skin\n");
    close(CHROMEFILE);
}


#//--------------------------------------------------------------------------------------------------
#// Select a default locale
#//--------------------------------------------------------------------------------------------------

sub SetDefaultLocale($$)
{
    my($locale, $chrome_dir) = @_;

    print "Setting default locale to $locale\n";
    
  	local(*CHROMEFILE);
    open(CHROMEFILE, ">>${chrome_dir}:installed-chrome.txt") || die "Failed to open installed_chrome.txt\n";
    print(CHROMEFILE "locale,install,select,$locale\n");
    close(CHROMEFILE);
}


#//--------------------------------------------------------------------------------------------------
#// InstallDefaultsFiles
#//--------------------------------------------------------------------------------------------------

sub InstallDefaultsFiles()
{
    unless( $main::build{resources} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = GetBinDirectory();

    print("--- Starting Defaults copying ----\n");

    # default folder
    my($defaults_dir) = "$dist_dir" . "Defaults:";
    mkdir($defaults_dir, 0);

    {
    my($default_wallet_dir) = "$defaults_dir"."wallet:";
    mkdir($default_wallet_dir, 0);
    InstallResources(":mozilla:extensions:wallet:src:MANIFEST",                        "$default_wallet_dir");
    }

    # Default _profile_ directory stuff
    {
    my($default_profile_dir) = "$defaults_dir"."Profile:";
    mkdir($default_profile_dir, 0);

    InstallResources(":mozilla:profile:defaults:MANIFEST",                             "$default_profile_dir", 1);

    my($default_profile_chrome_dir) = "$default_profile_dir"."chrome:";
    mkdir($default_profile_chrome_dir, 0);

    InstallResources(":mozilla:profile:defaults:chrome:MANIFEST",                             "$default_profile_chrome_dir", 1);

    # make a dup in en-US
    my($default_profile_dir_US) = "$default_profile_dir"."US:";
    mkdir($default_profile_dir_US, 0);

    InstallResources(":mozilla:profile:defaults:MANIFEST",                             "$default_profile_dir_US", 1);

    my($default_profile_chrome_dir_US) = "$default_profile_dir_US"."chrome:";
    mkdir($default_profile_chrome_dir_US, 0);

    InstallResources(":mozilla:profile:defaults:chrome:MANIFEST",                             "$default_profile_chrome_dir_US", 1);

    }
    
    # Default _pref_ directory stuff
    {
    my($default_pref_dir) = "$defaults_dir"."Pref:";
    mkdir($default_pref_dir, 0);
    InstallResources(":mozilla:xpinstall:public:MANIFEST_PREFS",                       "$default_pref_dir", 0);
    InstallResources(":mozilla:modules:libpref:src:init:MANIFEST",                     "$default_pref_dir", 0);
    InstallResources(":mozilla:modules:libpref:src:mac:MANIFEST",                      "$default_pref_dir", 0);
    InstallResources(":mozilla:netwerk:base:public:MANIFEST_PREFS",                    "$default_pref_dir", 0);

    if ($main::options{inspector})
    {
      InstallResources(":mozilla:extensions:inspector:resources:content:prefs:MANIFEST", "$default_pref_dir", 0);
    }

    if ($main::options{typeaheadfind})
    {
      InstallResources(":mozilla:extensions:typeaheadfind:resources:content:prefs:MANIFEST", "$default_pref_dir", 0);
    }


    if ($main::options{mdn}) {
    	InstallResources(":mozilla:mailnews:extensions:mdn:resources:content:MANIFEST_PREFS", "$default_pref_dir", 0);
    }
    }

    {
    my($default_autoconfig_dir) = "$defaults_dir"."autoconfig:";
    mkdir($default_autoconfig_dir, 0);
    InstallResources(":mozilla:extensions:pref:autoconfig:src:MANIFEST", "$default_autoconfig_dir");
    InstallResources(":mozilla:extensions:pref:autoconfig:src:mac:MANIFEST", "$default_autoconfig_dir");
    }

    print("--- Defaults copying complete ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// InstallNonChromeResources
#//--------------------------------------------------------------------------------------------------

sub InstallNonChromeResources()
{
    unless( $main::build{resources} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = GetBinDirectory();

    print("--- Starting Resource copying ----\n");

    #//
    #// Most resources should all go into the chrome dir eventually
    #//
    my($resource_dir) = "$dist_dir" . "res:";
    my($samples_dir) = "$resource_dir" . "samples:";
    my($builtin_dir) = "$resource_dir" . "builtin:";

    #//
    #// Make aliases of resource files
    #//
    
    MakeAlias(":mozilla:content:xml:content:src:xhtml11.dtd",                          "$resource_dir"."dtd:");

    if ($main::options{svg})
    {
        MakeAlias(":mozilla:layout:svg:base:src:svg.dtd",                              "$resource_dir"."dtd:");
    }
    
    if ($main::options{mathml})
    {
        MakeAlias(":mozilla:layout:mathml:content:src:mathml.dtd",                     "$resource_dir"."dtd:");
        MakeAlias(":mozilla:layout:mathml:content:src:mathml.css",                     "$resource_dir");
        InstallResources(":mozilla:layout:mathml:base:src:MANIFEST",                   "$resource_dir"."fonts:");
    }

    MakeAlias(":mozilla:layout:html:document:src:ua.css",                          "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:html.css",                            "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:forms.css",                           "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:quirk.css",                           "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:viewsource.css",                      "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:arrow.gif",                           "$resource_dir"); 
    MakeAlias(":mozilla:layout:html:base:src:loading-image.gif",                       "$resource_dir"); 
    MakeAlias(":mozilla:layout:html:base:src:broken-image.gif",                        "$resource_dir"); 
    MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",            "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:charsetalias.properties",                       "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:maccharset.properties",                         "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:charsetData.properties",                        "$resource_dir");
    MakeAlias(":mozilla:intl:locale:src:langGroups.properties",                        "$resource_dir");
    MakeAlias(":mozilla:intl:locale:src:language.properties",                          "$resource_dir");

    MakeAlias(":mozilla:gfx:src:mac:fontEncoding.properties",                          "$resource_dir"."fonts:");
    InstallResources(":mozilla:gfx:src:MANIFEST_RES",                                  "$resource_dir"."gfx:");

    my($entitytab_dir) = "$resource_dir" . "entityTables";
    InstallResources(":mozilla:intl:unicharutil:tables:MANIFEST",                      "$entitytab_dir");

    my($html_dir) = "$resource_dir" . "html:";
    InstallResources(":mozilla:layout:html:base:src:MANIFEST_RES",                     "$html_dir");

    my($throbber_dir) = "$resource_dir" . "throbber:";
    BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:throbber:",              "$throbber_dir");
    
    BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:samples:",               "$samples_dir");
    BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:resources:",             "$samples_dir");
    BuildFolderResourceAliases(":mozilla:xpfe:browser:samples:",                        "$samples_dir");

    InstallResources(":mozilla:xpfe:browser:src:MANIFEST",                             "$samples_dir");
    MakeAlias(":mozilla:xpfe:browser:samples:sampleimages:",                           "$samples_dir");
    
    my($rdf_dir) = "$resource_dir" . "rdf:";
    BuildFolderResourceAliases(":mozilla:rdf:resources:",                               "$rdf_dir");

    # Search - make copies (not aliases) of the various search files
    my($searchPlugins) = "${dist_dir}Search Plugins";
    print("--- Starting Search Plugins copying: $searchPlugins\n");
    InstallResources(":mozilla:xpfe:components:search:datasets:MANIFEST",              "$searchPlugins", 1);

    # QA Menu
    InstallResources(":mozilla:intl:strres:tests:MANIFEST",            "$resource_dir");

    # install builtin XBL bindings
    MakeAlias(":mozilla:content:xbl:builtin:htmlbindings.xml",                     "$builtin_dir");
    MakeAlias(":mozilla:content:xbl:builtin:mac:platformHTMLBindings.xml",         "$builtin_dir");

    if ($main::options{inspector})
    {
        InstallResources(":mozilla:extensions:inspector:resources:content:res:MANIFEST",   "$resource_dir" . "inspector:");
    }
    
    if ($main::options{typeaheadfind})
    {
        MakeAlias(":mozilla:extensions:typeaheadfind:resources:locale:en-US:typeaheadfind.properties", "$resource_dir");
    }
	
    print("--- End Resource copying ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// InstallComponentFiles
#//--------------------------------------------------------------------------------------------------

sub InstallComponentFiles()
{
    unless( $main::build{resources} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = GetBinDirectory();

    print("--- Starting Text Components copying ----\n");

    my($components_dir) = "$dist_dir" . "Components:";

    # console
    InstallResources(":mozilla:xpfe:components:console:MANIFEST",                          "$components_dir", 0);

    # sidebar
    InstallResources(":mozilla:xpfe:components:sidebar:src:MANIFEST",                      "$components_dir");

    # update notifications
    InstallResources(":mozilla:xpfe:components:updates:src:MANIFEST",                      "$components_dir");

    # download manager
    InstallResources(":mozilla:xpfe:components:download-manager:src:MANIFEST_COMPONENTS",  "$components_dir");

    # embedding UI
    InstallResources(":mozilla:embedding:components:ui:helperAppDlg:MANIFEST",             "$components_dir");
    InstallResources(":mozilla:embedding:components:ui:progressDlg:MANIFEST",              "$components_dir");

    # resetPref
    InstallResources(":mozilla:xpfe:components:resetPref:MANIFEST",                        "$components_dir");
    
    print("--- Done Text Components copying ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// MakeNonChromeAliases
#//--------------------------------------------------------------------------------------------------
sub MakeNonChromeAliases()
{
    unless( $main::build{resources} ) { return; }
    assertRightDirectory();

    InstallNonChromeResources();
    InstallDefaultsFiles();
    InstallComponentFiles();
}


#//--------------------------------------------------------------------------------------------------
#// DumpChromeToTemp
#//
#// Iterates over all the .jar files in $chrome_dir and unzips them into a temp 
#// directory organized by the name of the jar file. This matches the expected 
#// format in the embedding jar manifest.
#//--------------------------------------------------------------------------------------------------

sub DumpChromeToTemp($$$$)
{
  my($dist_dir, $chrome_dir, $temp_chrome_dir, $verbose) = @_;
  use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
  
  opendir(CHROMEDIR, $chrome_dir);
  my(@jarList) = readdir(CHROMEDIR);
  closedir(CHROMEDIR);
  
  # the jar manifest generator expects the dumped files to be in
  # a certain hierarchy:
  #  <jar name>/<...path within jar...>
  
  mkpath($temp_chrome_dir, $verbose, 0777);
  my($file);
  foreach $file ( @jarList ) {
    if ( $file =~ /\.jar/ ) {
      print "-- unzipping $file\n" if $verbose;
      
      # pull everything from the jar's name (eg: classic.jar) up to the
      # last "." and make that the foldername that everything in this
      # jar file goes into
      my($foldername) = substr($file, 0, rindex($file,".")) . ":";
      
      my($zip) = Archive::Zip->new("$chrome_dir:$file");
      my(@members) = $zip->members();
      my($item);
      foreach $item ( @members ) {
        my($name) = $item->fileName();
        $name =~ s/\//:/g;
#        print("+ extracting $name\n") if $verbose;
        die "extract error" if $item->extractToFileNamed($temp_chrome_dir . $foldername . $name) != AZ_OK;
      }
    }
  }
}


#//--------------------------------------------------------------------------------------------------
#// ProcessEmbedJarManifest
#//
#// Pick up an embedding manifest specified by $jar_file. We first need to do some extra processing on
#// it to get the correct locale. 
#//--------------------------------------------------------------------------------------------------
sub ProcessEmbedJarManifest ( $$$$$$$ )
{
  my($top_path, $inc_path, $temp_chrome_dir, $jar_file, $locale, $platform, $jars) = @_;
  
  # copy over our manifest so that it's also at the root
  # of our chrome tree then run it through the jar machine. We need to
  # use GenerateManifest to get the locales correct.
  copy("$inc_path$jar_file", "$temp_chrome_dir:$jar_file") || die "can't copy $jar_file manifest";
  my($temp_manifest) = "$temp_chrome_dir:$jar_file.tmp.mn";
  local(*TMPMANIFEST);
  open(TMPMANIFEST, ">$temp_manifest") || die "couldn't create embed jar manifest";
  GenerateManifest::GenerateManifest($top_path, "$inc_path$jar_file", $temp_chrome_dir,
                                      $locale, $platform, *TMPMANIFEST, ":", 1);
  close(TMPMANIFEST);

  CreateJarFromManifest($temp_manifest, "$temp_chrome_dir:", $jars);

  # clean up our temp files
  unlink("$temp_chrome_dir:$jar_file");
  unlink($temp_manifest);
}


#//--------------------------------------------------------------------------------------------------
#// PackageEmbeddingChrome
#//
#// Make use of mozilla/embedding/config's jar packaging scripts to build embed.jar
#// with the bare-minimum chrome required for embedding. This process is unpleasant and
#// suboptimal, but it allows us to reuse scripts and manifests so as to not get
#// out of sync when changes are made for win/unix.
#//
#// Basically, this takes all the jar files in mozilla/viewer/Chrome, unzips them,
#// runs a script to generate a manifest file with some substiutions, then rejars
#// based on the new, minimal manifest.
#//--------------------------------------------------------------------------------------------------

sub PackageEmbeddingChrome($$)
{
  my($dist_dir, $chrome_dir) = @_;
  
  # we prefer not to use the jars when packaging, since it adds a lot of time and 
  # effort to the build process. use the raw files if they're at all available.
  my($must_use_jars) = !$main::options{chrome_files};
  
  my($embed_dir) = $dist_dir . ":Embed";
  mkdir($embed_dir, 0);

  # unzip the existing jar files and dump them in a tempdir or point our
  # tempdir at the existing chrome files.
  my($temp_chrome_dir) = "$embed_dir:tempchrome";
  if ( $must_use_jars ) {
    DumpChromeToTemp($dist_dir, $chrome_dir, "$temp_chrome_dir:", 1);  
  }
  else {
    $temp_chrome_dir = $chrome_dir;
  }
  
  # Make sure we add the config dir to search, to pick up GenerateManifest.pm
  # Need to do this dynamically, because this module can be used before
  # mozilla/config has been checked out.
  my ($top_path) = $0;        # $0 is the path to the parent script
  $top_path =~ s/:build:mac:build_scripts:.+$//;
  my($inc_path) = $top_path . ":embedding:config:";
  push(@INC, $inc_path);
  require GenerateManifest;
  
  # generate the embedding manifest from the template in embedding/config. The
  # resulting manifest must go at the root of the tree it describes. The paths
  # in the manifest are local to that location. As a result, we dump it
  # in our temp chrome dir.
  my($temp_manifest) = "$temp_chrome_dir:embed-jar.tmp.mn";
  local(*MANIFEST);
  open(MANIFEST, ">$temp_manifest") || die "couldn't create embed jar manifest";
  GenerateManifest::GenerateManifest($top_path, $inc_path . "embed-jar.mn", $temp_chrome_dir,
                                      "en-US", "en-mac", *MANIFEST, ":", 1);
  close(MANIFEST);
  
  # make embed.jar from the above manifest, adding in any other manifests if the
  # right options are set.
  my(%jars);
  CreateJarFromManifest($temp_manifest, "$temp_chrome_dir:", \%jars);
  if ($main::options{embedding_xulprefs}) {
    ProcessEmbedJarManifest($top_path, $inc_path, $temp_chrome_dir, "xulprefs.mn", "en-US", "en-mac", \%jars);
  }
  if ($main::options{embedding_xulsecurity}) {
    ProcessEmbedJarManifest($top_path, $inc_path, $temp_chrome_dir, "xulsecurity.mn", "en-US", "en-mac", \%jars);
  }  
  WriteOutJarFiles("$temp_chrome_dir:", \%jars);

  # clean up after ourselves and move everything to the right locations. The embed.jar
  # and resulting installed-chrome need to end up in dist/Embed in either case.
  if ( $must_use_jars ) {
    print("deleting temp chrome dir $temp_chrome_dir\n");
    rmtree($temp_chrome_dir, 0, 0);
  }
  else {
    # since we used chrome files from $dist_dir, our new jar needs to be moved and
    # we need to clean up the copied chrome files
    move($dist_dir . "Embed.jar", $embed_dir);
    move($dist_dir . "installed-chrome.txt", $embed_dir);
    rmtree($dist_dir . "embed");
    unlink($temp_manifest);    
  }
  
  # we have to select a default locale, otherwise we won't know about a bunch
  # of files in embed.jar if the app doesn't call the code to auto-detect the locale.
  SetDefaultLocale("en-US", $embed_dir);
}

#//--------------------------------------------------------------------------------------------------
#// BuildEmbeddingPackage
#//
#// Run through the basebrowser manifest file and copy all the files into dist/Embed
#//--------------------------------------------------------------------------------------------------
sub BuildEmbeddingPackage
{
  unless ($main::options{embedding_chrome}) { return; }
  my($D) = $main::DEBUG ? "Debug" : "";
  
  my($dist_dir) = GetBinDirectory();

  # Make sure we add the config dir to search, to pick up Packager.pm
  # Need to do this dynamically, because this module can be used before
  # mozilla/xpinstall has been checked out.
  my($top_path) = $0;        # $0 is the path to the parent script
  $top_path =~ s/:build:mac:build_scripts:.+$//;
  my($inc_path) = "$top_path:xpinstall:packager:";
  push(@INC, $inc_path);
  require Packager;
  
  # final destination will be a sibling of $dist_dir in dist
  my($destination) = "$top_path:dist";  
  my($manifest) = "$top_path:embedding:config:basebrowser-mac-cfm$D";
  chop $dist_dir;             # Copy() expects the src/dest dirs not to have a ':' at the end
  Packager::Copy($dist_dir, $destination, $manifest, "mac", 0, 0, 0, () );

  # the Embed.jar is in the wrong place, move it into the chrome dir
  mkdir("$destination:Embed:Chrome", 0);
  move("$destination:Embed:embed.jar", "$destination:Embed:Chrome:embed.jar");
  move("$destination:Embed:installed-chrome.txt", "$destination:Embed:Chrome:installed-chrome.txt");
  
  # copy PPEmbed into our new package
  print("-- copying PPEmbed to embed package\n");
  copy("$dist_dir:PPEmbed$D", "$destination:Embed:PPEmbed$D");
}


#//--------------------------------------------------------------------------------------------------
#// ProcessJarManifests
#//--------------------------------------------------------------------------------------------------

sub ProcessJarManifests()
{
    my($dist_dir) = GetBinDirectory();
    my($chrome_dir) = "$dist_dir"."Chrome";
    my($resource_dir) = "$dist_dir" . "res:";

    # a hash of jars passed as context to the following calls
    my(%jars);
    
    if ($main::options{chatzilla})
    {
      CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{content_packs})
    {
      CreateJarFromManifest(":mozilla:extensions:content-packs:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{cview})
    {
      CreateJarFromManifest(":mozilla:extensions:cview:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{ldap})
    {
      CreateJarFromManifest(":mozilla:directory:xpcom:base:resources:jar.mn", $chrome_dir, \%jars);
      if ($main::options{ldap_experimental})
      {
        CreateJarFromManifest(":mozilla:directory:xpcom:tests:jar.mn", $chrome_dir, \%jars);
      }
    }
    if ($main::options{help})
    {
      CreateJarFromManifest(":mozilla:extensions:help:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{inspector})
    {
      CreateJarFromManifest(":mozilla:extensions:inspector:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{jsd} && $main::options{venkman})
    {
      CreateJarFromManifest(":mozilla:extensions:venkman:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{typeaheadfind})
    {
      CreateJarFromManifest(":mozilla:extensions:typeaheadfind:resources:jar.mn", $chrome_dir, \%jars);
    }
    
    CreateJarFromManifest(":mozilla:accessible:src:base:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:caps:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:content:html:style:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:docshell:resources:content:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:docshell:resources:locale:en-US:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:editor:ui:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:browser:chrome:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:browser:chrome:locale:en-US:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:cookie:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:pref:autoconfig:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:wallet:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:intl:uconv:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:htmlparser:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:forms:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:document:src:xbl-marquee:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:forms:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:base:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:mailnews:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:mailnews:base:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    if ($main::options{smime} && $main::options{psm}) {
    	CreateJarFromManifest(":mozilla:mailnews:extensions:smime:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{mdn}) {
    	CreateJarFromManifest(":mozilla:mailnews:extensions:mdn:jar.mn", $chrome_dir, \%jars);
    }
    CreateJarFromManifest(":mozilla:netwerk:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:profile:pref-migrator:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:profile:resources:jar.mn", $chrome_dir, \%jars);
    
    # use different css files for scrollbars on OSX and OS9 in classic skin. On OSX, use
    # ones that specify native scrollbars via xbl. OS9 can't use these because of
    # mousewheel driver problems that would be exacerbated by using native scrollbars 
    # everywhere
    if ($main::options{carbon}) {
      MakeAlias(":mozilla:themes:classic:global:mac:nativescrollbars.css", ":mozilla:themes:classic:global:mac:scrollbars.css");
    }
    else {
      MakeAlias(":mozilla:themes:classic:global:mac:xulscrollbars.css", ":mozilla:themes:classic:global:mac:scrollbars.css");    
    }
    CreateJarFromManifest(":mozilla:content:xml:document:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:global:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:modern:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpcom:base:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:locale:en-US:unix:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:locale:en-US:win:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:locale:en-US:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:communicator:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:communicator:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:bookmarks:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:download-manager:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:download-manager:resources:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:unix:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:win:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpinstall:res:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:components:webbrowserpersist:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:components:ui:jar.mn", $chrome_dir, \%jars);

    if ($main::options{psm}) {
    	CreateJarFromManifest(":mozilla:security:manager:ssl:resources:jar.mn", $chrome_dir, \%jars);
    	CreateJarFromManifest(":mozilla:security:manager:pki:resources:jar.mn", $chrome_dir, \%jars);
    }
    # bad jar.mn files
#    CreateJarFromManifest(":mozilla:extensions:xmlterm:jar.mn", $chrome_dir, \%jars);
    
    WriteOutJarFiles($chrome_dir, \%jars);
    
    # generate a jar manifest for embedding and package it. This needs to be done _after_
    # all of the other jar files are created.
    if ($main::options{embedding_chrome}) {
      PackageEmbeddingChrome($dist_dir, $chrome_dir);
    }
}


#//--------------------------------------------------------------------------------------------------
#// BuildJarFiles
#//--------------------------------------------------------------------------------------------------

sub BuildJarFiles()
{
    unless( $main::build{resources} ) { return; }
    assertRightDirectory();

    print("--- Starting JAR building ----\n");

    ProcessJarManifests();

    print("--- JAR building done ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// BuildResources
#//--------------------------------------------------------------------------------------------------
sub BuildResources()
{
    unless( $main::build{resources} ) { return; }
    assertRightDirectory();

    StartBuildModule("resources");

    #just  copy this plugin to this directory.. temporary solution until the plug in is part of the build
    my($dist_dir) = GetBinDirectory();
    my($essentials_dir) = "$dist_dir" . "Essential Files:";
    my($existing_plugin) = $essentials_dir."PrintDialogPDE.plugin";
    
    if (-e $existing_plugin)
    {
        my $deadPlugin = full_path_to($existing_plugin);
        my $ascript = <<EOS;
        tell application "Finder"
            delete alias "$deadPlugin"
        end tell
EOS
        MacPerl::DoAppleScript($ascript) or die($^E);
    }

    FSpDirectoryCopy(":mozilla:gfx:src:mac:printerplugin:PrintDialogPDE.plugin:","$essentials_dir", 0);  

    ActivateApplication('McPL');
    
    MakeNonChromeAliases();   # Defaults, JS components etc.      
    BuildJarFiles();

    BuildEmbeddingPackage();

    # Set the default skin to be classic
    SetDefaultSkin("classic/1.0"); 

    EndBuildModule("resources");
}


#//--------------------------------------------------------------------------------------------------
#// Build the runtime 'dist' directories
#//--------------------------------------------------------------------------------------------------

sub BuildRuntimeDist()
{
    unless ( $main::build{dist} ) { return;}
    assertRightDirectory();
    
    my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers

    print("--- Starting Runtime Dist export ----\n");

    #MAC_COMMON
    InstallFromManifest(":mozilla:build:mac:MANIFEST",                             "$distdirectory:mac:common:");
    InstallFromManifest(":mozilla:lib:mac:NSRuntime:include:MANIFEST",             "$distdirectory:mac:common:");
    InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:MANIFEST",              "$distdirectory:mac:common:");
    InstallFromManifest(":mozilla:lib:mac:MoreFiles:MANIFEST",                     "$distdirectory:mac:common:morefiles:");
    InstallFromManifest(":mozilla:lib:mac:NSStartup:MANIFEST",                     "$distdirectory:mac:common:");

    #GC_LEAK_DETECTOR
    InstallFromManifest(":mozilla:gc:boehm:MANIFEST",                              "$distdirectory:gc:");

    #INCLUDE
    InstallFromManifest(":mozilla:config:MANIFEST_xpfe",                           "$distdirectory:xpfe:");
    InstallFromManifest(":mozilla:config:mac:MANIFEST",                            "$distdirectory:config:");
    
    #NSPR 
    InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",                    "$distdirectory:nspr:");
    InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:MANIFEST",                 "$distdirectory:nspr:mac:");
    InstallFromManifest(":mozilla:nsprpub:lib:ds:MANIFEST",                        "$distdirectory:nspr:");
    InstallFromManifest(":mozilla:nsprpub:lib:libc:include:MANIFEST",              "$distdirectory:nspr:");
    InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:MANIFEST",              "$distdirectory:nspr:");

    print("--- Runtime Dist export complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build the client 'dist' directories
#//--------------------------------------------------------------------------------------------------

sub BuildClientDist()
{
    unless ( $main::build{dist} ) { return;}
    assertRightDirectory();
    
    my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers
    my $dist_dir = GetBinDirectory(); # the subdirectory with the libs and executable.

    print("--- Starting Client Dist export ----\n");

    InstallFromManifest(":mozilla:lib:mac:Misc:MANIFEST",                          "$distdirectory:mac:common:");
    InstallFromManifest(":mozilla:lib:mac:Instrumentation:MANIFEST",               "$distdirectory:mac:inst:");

    #INCLUDE
    InstallFromManifest(":mozilla:include:MANIFEST",                               "$distdirectory:include:");

    #INTL
    #CHARDET
    InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST",                   "$distdirectory:chardet");

    #UCONV
    InstallFromManifest(":mozilla:intl:uconv:idl:MANIFEST_IDL",                    "$distdirectory:idl:");
    InstallFromManifest(":mozilla:intl:uconv:public:MANIFEST",                     "$distdirectory:uconv:");

    #UNICHARUTIL
    InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST",               "$distdirectory:unicharutil");
    InstallFromManifest(":mozilla:intl:unicharutil:util:MANIFEST",
    "$distdirectory:unicharutil");
#   InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST_IDL",           "$distdirectory:idl:");

    #LOCALE
    InstallFromManifest(":mozilla:intl:locale:public:MANIFEST",                    "$distdirectory:locale:");
    InstallFromManifest(":mozilla:intl:locale:idl:MANIFEST_IDL",                   "$distdirectory:idl:");

    #LWBRK
    InstallFromManifest(":mozilla:intl:lwbrk:public:MANIFEST",                     "$distdirectory:lwbrk:");

    #STRRES
    InstallFromManifest(":mozilla:intl:strres:public:MANIFEST_IDL",                "$distdirectory:idl:");

    #JPEG
    InstallFromManifest(":mozilla:jpeg:MANIFEST",                                  "$distdirectory:jpeg:");

    #LIBREG
    InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",                "$distdirectory:libreg:");

    #STRING
    InstallFromManifest(":mozilla:string:public:MANIFEST",                         "$distdirectory:string:");
    InstallFromManifest(":mozilla:string:obsolete:MANIFEST",                       "$distdirectory:string:");

    #XPCOM
    InstallFromManifest(":mozilla:xpcom:base:MANIFEST_IDL",                        "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:io:MANIFEST_IDL",                          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:ds:MANIFEST_IDL",                          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:threads:MANIFEST_IDL",                     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:components:MANIFEST_IDL",                  "$distdirectory:idl:");

    InstallFromManifest(":mozilla:xpcom:build:MANIFEST",                           "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:glue:MANIFEST",                            "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:base:MANIFEST",                            "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:components:MANIFEST",                      "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:ds:MANIFEST",                              "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:io:MANIFEST",                              "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:threads:MANIFEST",                         "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:proxy:public:MANIFEST",                    "$distdirectory:xpcom:");

    InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST",          "$distdirectory:xpcom:");
    InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST_IDL",      "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:reflect:xptcall:public:MANIFEST",          "$distdirectory:xpcom:");

    InstallFromManifest(":mozilla:xpcom:typelib:xpt:public:MANIFEST",              "$distdirectory:xpcom:");
    
    #ZLIB
    InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",                      "$distdirectory:zlib:");

    #LIBJAR
    InstallFromManifest(":mozilla:modules:libjar:MANIFEST",                        "$distdirectory:libjar:");
    InstallFromManifest(":mozilla:modules:libjar:MANIFEST_IDL",                    "$distdirectory:idl:");

    #LIBUTIL
    InstallFromManifest(":mozilla:modules:libutil:public:MANIFEST",                "$distdirectory:libutil:");

    # MPFILELOCPROVIDER
    InstallFromManifest(":mozilla:modules:mpfilelocprovider:public:MANIFEST",      "$distdirectory:mpfilelocprovider:");

    #SUN_JAVA
    InstallFromManifest(":mozilla:sun-java:stubs:include:MANIFEST",                "$distdirectory:sun-java:");
    InstallFromManifest(":mozilla:sun-java:stubs:macjri:MANIFEST",                 "$distdirectory:sun-java:");

    #JS
    InstallFromManifest(":mozilla:js:src:MANIFEST",                                "$distdirectory:js:");

    #LIVECONNECT
    InstallFromManifest(":mozilla:js:src:liveconnect:MANIFEST",                    "$distdirectory:liveconnect:");
    
    #XPCONNECT  
    InstallFromManifest(":mozilla:js:src:xpconnect:idl:MANIFEST",                  "$distdirectory:idl:");
    InstallFromManifest(":mozilla:js:src:xpconnect:public:MANIFEST",               "$distdirectory:xpconnect:");

    #CAPS
    InstallFromManifest(":mozilla:caps:include:MANIFEST",                          "$distdirectory:caps:");
    InstallFromManifest(":mozilla:caps:idl:MANIFEST",                              "$distdirectory:idl:");

    #LIBPREF
    InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",                "$distdirectory:libpref:");
    InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST_IDL",            "$distdirectory:idl:");

    #PROFILE
    InstallFromManifest(":mozilla:profile:public:MANIFEST_IDL",                    "$distdirectory:idl:");

    #PREF_MIGRATOR
    InstallFromManifest(":mozilla:profile:pref-migrator:public:MANIFEST",          "$distdirectory:profile:");

    #LIBIMAGE
    InstallFromManifest(":mozilla:modules:libimg:png:MANIFEST",                    "$distdirectory:libimg:");
    InstallFromManifest(":mozilla:modules:libimg:mng:MANIFEST",                    "$distdirectory:libimg:");

    if ($main::options{useimg2}) {
	    #LIBIMG2
	    InstallFromManifest(":mozilla:modules:libpr0n:public:MANIFEST_IDL",            "$distdirectory:libimg2:");
	    InstallFromManifest(":mozilla:modules:libpr0n:decoders:icon:MANIFEST_IDL",     "$distdirectory:icondecoder:");
    }
    
    #PLUGIN
    InstallFromManifest(":mozilla:modules:plugin:base:public:MANIFEST",            "$distdirectory:plugin:");
    InstallFromManifest(":mozilla:modules:plugin:base:public:MANIFEST_IDL",        "$distdirectory:idl:");
    InstallFromManifest(":mozilla:modules:oji:src:MANIFEST",                       "$distdirectory:oji:");
    InstallFromManifest(":mozilla:modules:oji:public:MANIFEST",                    "$distdirectory:oji:");
    InstallFromManifest(":mozilla:modules:oji:public:MANIFEST_IDL",                "$distdirectory:idl:");
    
    #DB
    InstallFromManifest(":mozilla:db:mdb:public:MANIFEST",                         "$distdirectory:db:");
    InstallFromManifest(":mozilla:db:mork:build:MANIFEST",                         "$distdirectory:db:");

    #DBM
    InstallFromManifest(":mozilla:dbm:include:MANIFEST",                           "$distdirectory:dbm:");
    
    #URILOADER
    InstallFromManifest(":mozilla:uriloader:base:MANIFEST_IDL",                    "$distdirectory:idl:");
    InstallFromManifest(":mozilla:uriloader:exthandler:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:uriloader:prefetch:MANIFEST_IDL",                "$distdirectory:idl:");
    InstallFromManifest(":mozilla:uriloader:prefetch:MANIFEST",                    "$distdirectory:uriloader:");
    
    #NETWERK
    InstallFromManifest(":mozilla:netwerk:build:MANIFEST",                         "$distdirectory:netwerk:");
    InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST",                   "$distdirectory:netwerk:");
    InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:socket:base:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:protocol:about:public:MANIFEST_IDL",     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:protocol:data:public:MANIFEST_IDL",      "$distdirectory:idl:");
    #InstallFromManifest(":mozilla:netwerk:protocol:file:public:MANIFEST_IDL",     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST_IDL",      "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST",          "$distdirectory:netwerk:");
    InstallFromManifest(":mozilla:netwerk:protocol:ftp:public:MANIFEST",           "$distdirectory:netwerk:");    
    InstallFromManifest(":mozilla:netwerk:protocol:jar:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:protocol:res:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:mime:public:MANIFEST",                   "$distdirectory:netwerk:");
    InstallFromManifest(":mozilla:netwerk:cookie:public:MANIFEST_IDL",             "$distdirectory:idl:");

    #EXTENSIONS
    InstallFromManifest(":mozilla:extensions:cookie:MANIFEST_IDL",                 "$distdirectory:idl:");
    InstallFromManifest(":mozilla:extensions:cookie:MANIFEST",                     "$distdirectory:cookie:");
    InstallFromManifest(":mozilla:extensions:wallet:public:MANIFEST",              "$distdirectory:wallet:");

    #WEBSHELL
    InstallFromManifest(":mozilla:webshell:public:MANIFEST",                       "$distdirectory:webshell:");
    InstallFromManifest(":mozilla:webshell:public:MANIFEST_IDL",                   "$distdirectory:idl:");
    InstallFromManifest(":mozilla:webshell:tests:viewer:public:MANIFEST",          "$distdirectory:webshell:");

    #CONTENT    
    InstallFromManifest(":mozilla:content:base:public:MANIFEST",                   "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:base:public:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:content:base:src:MANIFEST",                      "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:build:MANIFEST",                         "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:events:public:MANIFEST",                 "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:events:src:MANIFEST",                    "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:content:public:MANIFEST",           "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:content:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:content:html:content:src:MANIFEST",              "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:document:public:MANIFEST",          "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:document:public:MANIFEST_IDL",      "$distdirectory:idl:");
    InstallFromManifest(":mozilla:content:html:document:src:MANIFEST",             "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:style:public:MANIFEST",             "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:style:src:MANIFEST",                "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:shared:public:MANIFEST",                 "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xbl:public:MANIFEST",                    "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xml:content:public:MANIFEST",            "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xml:document:public:MANIFEST",           "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xsl:public:MANIFEST",                    "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xsl:public:MANIFEST_IDL",                "$distdirectory:idl:");
    InstallFromManifest(":mozilla:content:xul:content:public:MANIFEST",            "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xul:document:public:MANIFEST",           "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xul:document:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:content:xul:templates:public:MANIFEST",          "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xul:templates:public:MANIFEST_IDL",      "$distdirectory:idl:");

    #LAYOUT
    InstallFromManifest(":mozilla:layout:build:MANIFEST",                          "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:public:MANIFEST",                    "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:public:MANIFEST_IDL",                "$distdirectory:idl:");
    InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",                 "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",                  "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:forms:public:MANIFEST",              "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:table:public:MANIFEST",              "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:xul:base:public:Manifest",                "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:xul:base:src:MANIFEST",                   "$distdirectory:layout:");
    
    #GFX
    InstallFromManifest(":mozilla:gfx:public:MANIFEST",                            "$distdirectory:gfx:");
    InstallFromManifest(":mozilla:gfx:src:MANIFEST",                               "$distdirectory:gfx:");
    InstallFromManifest(":mozilla:gfx:idl:MANIFEST_IDL",                           "$distdirectory:idl:");

    #VIEW
    InstallFromManifest(":mozilla:view:public:MANIFEST",                           "$distdirectory:view:");

    #DOM
    InstallFromManifest(":mozilla:dom:public:MANIFEST_IDL",                        "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:base:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:core:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:css:MANIFEST_IDL",                "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:events:MANIFEST_IDL",             "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:html:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:traversal:MANIFEST_IDL",          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:range:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:stylesheets:MANIFEST_IDL",        "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:views:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:xbl:MANIFEST_IDL",                "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:xpath:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:idl:xul:MANIFEST_IDL",                "$distdirectory:idl:");
    
    # SVG
    if ($main::options{libart_lgpl}) {
        InstallFromManifest(":mozilla:other-licenses:libart_lgpl:MANIFEST",        "$distdirectory:include:");
    }
    if ($main::options{svg}) {
        InstallFromManifest(":mozilla:content:svg:content:src:MANIFEST",           "$distdirectory:content:");
        InstallFromManifest(":mozilla:layout:svg:base:src:MANIFEST",               "$distdirectory:layout:");
    }
    
    InstallFromManifest(":mozilla:dom:public:MANIFEST",                            "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:base:MANIFEST",                       "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",                 "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",                         "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:src:base:MANIFEST",                          "$distdirectory:dom:");

    #ACCESSIBLE
    if ($main::options{accessible})
    {
        InstallFromManifest(":mozilla:accessible:public:MANIFEST",                 "$distdirectory:accessible:");    
    }

    #JSURL
    InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST_IDL",                     "$distdirectory:idl:");

    #HTMLPARSER
    InstallFromManifest(":mozilla:htmlparser:public:MANIFEST",                     "$distdirectory:htmlparser:");
    InstallFromManifest(":mozilla:htmlparser:public:MANIFEST_IDL",                 "$distdirectory:idl:");
    
    #EXPAT
    InstallFromManifest(":mozilla:expat:xmlparse:MANIFEST",                        "$distdirectory:expat:");
    
    #DOCSHELL
    InstallFromManifest(":mozilla:docshell:base:MANIFEST_IDL",                     "$distdirectory:idl:");

    #EMBEDDING
    InstallFromManifest(":mozilla:embedding:base:MANIFEST_IDL",                    "$distdirectory:idl:");
    InstallFromManifest(":mozilla:embedding:browser:webbrowser:MANIFEST_IDL",      "$distdirectory:idl:");
    InstallFromManifest(":mozilla:embedding:components:windowwatcher:public:MANIFEST_IDL", "$distdirectory:idl:");
    InstallFromManifest(":mozilla:embedding:components:jsconsole:public:MANIFEST_IDL", "$distdirectory:idl:");
    InstallFromManifest(":mozilla:embedding:components:appstartup:src:MANIFEST",   "$distdirectory:embedding:components:");
    InstallFromManifest(":mozilla:embedding:components:find:public:MANIFEST_IDL",  "$distdirectory:idl:");
    InstallFromManifest(":mozilla:embedding:components:ui:helperAppDlg:MANIFEST_IDL", "$distdirectory:idl:");
    InstallFromManifest(":mozilla:embedding:components:ui:progressDlg:MANIFEST_IDL", "$distdirectory:idl:");

    #WIDGET
    InstallFromManifest(":mozilla:widget:public:MANIFEST",                         "$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:public:MANIFEST_IDL",                     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",                        "$distdirectory:widget:");

    #RDF
    InstallFromManifest(":mozilla:rdf:base:idl:MANIFEST",                          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:rdf:base:public:MANIFEST",                       "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:util:public:MANIFEST",                       "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:datasource:public:MANIFEST",                 "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:build:MANIFEST",                             "$distdirectory:rdf:");
    
    #CHROME
    InstallFromManifest(":mozilla:rdf:chrome:public:MANIFEST",                     "$distdirectory:idl:");
    
    #EDITOR
    InstallFromManifest(":mozilla:editor:idl:MANIFEST",                            "$distdirectory:idl:");
    InstallFromManifest(":mozilla:editor:txmgr:idl:MANIFEST",                      "$distdirectory:idl:");
    InstallFromManifest(":mozilla:editor:public:MANIFEST",                         "$distdirectory:editor:");
    InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",                   "$distdirectory:editor:txmgr");
    InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",                  "$distdirectory:editor:txtsvc");
    
    #SILENTDL
    #InstallFromManifest(":mozilla:silentdl:MANIFEST",                             "$distdirectory:silentdl:");

    #XPINSTALL (the one and only!)
    InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                      "$distdirectory:xpinstall:");
    InstallFromManifest(":mozilla:xpinstall:cleanup:MANIFEST",                      "$distdirectory:xpinstall:");

    my $dir = '';
    for $dir (qw(autocomplete bookmarks download-manager find history related search shistory sidebar urlbarhistory))
    {
        InstallFromManifest(":mozilla:xpfe:components:$dir:public:MANIFEST_IDL",       "$distdirectory:idl:");
    }
    
    InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST",              "$distdirectory:xpfe:");
    InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST_IDL",          "$distdirectory:idl:");

    # directory
    InstallFromManifest(":mozilla:xpfe:components:directory:MANIFEST_IDL",         "$distdirectory:idl:");
    # regviewer
    InstallFromManifest(":mozilla:xpfe:components:regviewer:MANIFEST_IDL",     "$distdirectory:idl:");

    InstallFromManifest(":mozilla:xpfe:components:intl:MANIFEST",                  "$distdirectory:xpfe:");

    # XPAPPS
    InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST",                  "$distdirectory:xpfe:");
    InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpfe:browser:public:MANIFEST_IDL",               "$distdirectory:idl:");

    # XML-RPC
    InstallFromManifest(":mozilla:extensions:xml-rpc:idl:MANIFEST_IDL",                "$distdirectory:idl:");

    #LDAP
    if ($main::options{ldap})
    {
        # Create the ldap-standard.h file from the template
        #
        my($template) = ":mozilla:directory:c-sdk:ldap:include:ldap-standard-tmpl.h";
        my($template_modtime) = GetFileModDate($template);
        my($header) = ":mozilla:directory:c-sdk:ldap:include:ldap-standard.h";
        my($header_modtime) = (-e $header ? GetFileModDate($header) : 0);
        if ($template_modtime > $header_modtime) {
            # Make sure we add the config dir to search, to pick up GenerateManifest.pm
            # Need to do this dynamically, because this module can be used before
            # mozilla/directory/c-sdk has been checked out.
            my($top_path) = $0;        # $0 is the path to the parent script
            $top_path =~ s/:build:mac:build_scripts:.+$//;
            my($inc_path) = $top_path . ":directory:c-sdk:ldap:build:";
            push(@INC, $inc_path);
            require replace;

            local(*TEMPLATE);
            open(TEMPLATE, "<$template") || die "couldn't open ldap header template";
            local(*HEADER);
            open(HEADER, ">$header") || die "couldn't create ldap header";
            replace::GenerateHeader(*TEMPLATE, *HEADER, {"LDAP_VENDOR_NAME" => "mozilla.org", "LDAP_VENDOR_VERSION" => "500"});
            close(TEMPLATE);
            close(HEADER);
        }

        InstallFromManifest(":mozilla:directory:c-sdk:ldap:include:MANIFEST",      		"$distdirectory:directory:");
        InstallFromManifest(":mozilla:directory:xpcom:base:public:MANIFEST",       		"$distdirectory:directory:");
        InstallFromManifest(":mozilla:directory:xpcom:base:public:MANIFEST_IDL",   		"$distdirectory:idl:");
        InstallFromManifest(":mozilla:xpfe:components:autocomplete:public:MANIFEST_IDL",	"$distdirectory:idl:");
    }

    # MAILNEWS
    InstallFromManifest(":mozilla:mailnews:public:MANIFEST",                       "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:public:MANIFEST_IDL",                   "$distdirectory:idl:");
    InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST",                  "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:mailnews:base:build:MANIFEST",                   "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:base:src:MANIFEST",                     "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:base:util:MANIFEST",                    "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST",           "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST",               "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST_IDL",           "$distdirectory:idl:");
    InstallFromManifest(":mozilla:mailnews:compose:build:MANIFEST",                "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:db:msgdb:public:MANIFEST",              "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:db:msgdb:build:MANIFEST",               "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:local:public:MANIFEST",                 "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:local:build:MANIFEST",                  "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:local:src:MANIFEST",                    "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:imap:public:MANIFEST",                  "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:imap:build:MANIFEST",                   "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:imap:src:MANIFEST",                     "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST",                  "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:mailnews:mime:src:MANIFEST",                     "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:mime:build:MANIFEST",                   "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:mime:emitters:src:MANIFEST",            "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:news:public:MANIFEST",                  "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:news:build:MANIFEST",                   "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST",              "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST_IDL",          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:mailnews:addrbook:src:MANIFEST",                 "$distdirectory:mailnews:");
    InstallFromManifest(":mozilla:mailnews:addrbook:build:MANIFEST",               "$distdirectory:mailnews:");
    if ($main::options{smime} && $main::options{psm}) {
    	InstallFromManifest(":mozilla:mailnews:extensions:smime:public:MANIFEST_IDL", "$distdirectory:idl:");
    }
    

    #IIEXTRAS
    if ($main::options{iiextras})
    {
        InstallFromManifest(":mozilla:extensions:interfaceinfo:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    #XMLEXTRAS
    if ($main::options{xmlextras})
    {
        InstallFromManifest(":mozilla:extensions:xmlextras:base:public:MANIFEST_IDL", "$distdirectory:idl:");
        InstallFromManifest(":mozilla:extensions:xmlextras:schema:public:MANIFEST_IDL", "$distdirectory:idl:");
        InstallFromManifest(":mozilla:extensions:xmlextras:soap:public:MANIFEST_IDL", "$distdirectory:idl:");
        if ($main::options{wsp})
        {
            InstallFromManifest(":mozilla:extensions:xmlextras:proxy:public:MANIFEST_IDL", "$distdirectory:idl:");
            InstallFromManifest(":mozilla:extensions:xmlextras:wsdl:public:MANIFEST_IDL", "$distdirectory:idl:");
        }
    }

    #DOCUMENT INSPECTOR
    if ($main::options{inspector})
    {
        InstallFromManifest(":mozilla:extensions:inspector:base:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    #TYPE AHEAD FIND
    if ($main::options{typeaheadfind})
    {
        InstallFromManifest(":mozilla:extensions:typeaheadfind:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    #P3P
    if ($main::options{p3p})
    {
        InstallFromManifest(":mozilla:extensions:p3p:public:MANIFEST_IDL", "$distdirectory:idl:");
        InstallFromManifest(":mozilla:extensions:p3p:public:MANIFEST",     "$distdirectory:p3p:");
    }

    #JS DEBUGGER
    if ($main::options{jsd})
    {
        InstallFromManifest(":mozilla:js:jsd:idl:MANIFEST_IDL", "$distdirectory:idl:");
        InstallFromManifest(":mozilla:js:jsd:MANIFEST", "$distdirectory:jsdebug:");
    }

    print("--- Client Dist export complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build the 'dist' directory
#//--------------------------------------------------------------------------------------------------

sub BuildDist()
{
    unless ( $main::build{dist} ) { return;}
    assertRightDirectory();
    
    # activate MacPerl
    ActivateApplication('McPL');

    StartBuildModule("dist");
        
    my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers
    my $dist_dir = GetBinDirectory(); # the subdirectory with the libs and executable.
    if ($main::CLOBBER_DIST_ALL)
    {
        print "Clobbering dist in 5 seconds. Press command-. to stop\n";
        
        DelayFor(5);
        
        print "Clobbering all files inside :mozilla:dist:\n";
        EmptyTree($distdirectory.":");
    }
    else
    {
        if ($main::CLOBBER_DIST_LIBS)
        {
            print "Clobbering library aliases and executables inside ".$dist_dir."\n";
            EmptyTree($dist_dir);
        }
    }

    # we really do not need all these paths, but many client projects include them
    mkpath([ ":mozilla:dist:", ":mozilla:dist:client_stubs:" ]);
    mkpath([ ":mozilla:dist:static_libs:", ":mozilla:dist:static_libs_debug:" ]);
    mkpath([ ":mozilla:dist:viewer:", ":mozilla:dist:viewer_debug:" ]);
    
    #make default plugins folder so that apprunner won't go looking for 3.0 and 4.0 plugins.
    mkpath([ ":mozilla:dist:viewer:Plug-ins", ":mozilla:dist:viewer_debug:Plug-ins"]);
    #mkpath([ ":mozilla:dist:client:Plugins", ":mozilla:dist:client_debug:Plugins"]);
    
    UpdateBuildNumberFiles();

    BuildRuntimeDist();
    
    if (!$main::RUNTIME) {
        BuildClientDist();
    }
    
    EndBuildModule("dist");
}

#//--------------------------------------------------------------------------------------------------
#// Do some stuff between exporting dist and compilation
#//--------------------------------------------------------------------------------------------------

sub PrepareBuild()
{
    unless( $main::build{config} ) { return; }
    assertRightDirectory();

    StartBuildModule("config");

    UpdateConfigHeader($main::DEFINESOPTIONS_FILE);
    
    my($file_name) = basename($main::DEFINESOPTIONS_FILE);
    MakeAlias($main::DEFINESOPTIONS_FILE, ":mozilla:dist:config:$file_name");

    EndBuildModule("config");
}

#//--------------------------------------------------------------------------------------------------
#// Build stub projects
#//--------------------------------------------------------------------------------------------------

sub BuildStubs()
{
    unless( $main::build{stubs} ) { return; }
    assertRightDirectory();

    my($distdirectory) = ":mozilla:dist";

    StartBuildModule("stubs");

    #//
    #// Clean projects
    #//
    BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.xml", "Stubs");
    
    EndBuildModule("stubs");
}


#//--------------------------------------------------------------------------------------------------
#// Build the CodeWarrior XPIDL plugins
#//--------------------------------------------------------------------------------------------------
sub BuildXPIDLCompiler()
{
    unless( $main::build{xpidl} ) { return; }
    assertRightDirectory();

    StartBuildModule("xpidl");

    #// see if the xpidl compiler/linker has been rebuilt by comparing modification dates.
    my($codewarrior_plugins) = GetCodeWarriorRelativePath("CodeWarrior Plugins:");
    my($compiler_path) = $codewarrior_plugins . "Compilers:xpidl";
    my($linker_path) = $codewarrior_plugins . "Linkers:xpt Linker";
    my($compiler_modtime) = (-e $compiler_path ? GetFileModDate($compiler_path) : 0);
    my($linker_modtime) = (-e $linker_path ? GetFileModDate($linker_path) : 0);
  
    #// build the IDL compiler itself.
    BuildProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.xml", "build all");
  
    #// was the compiler/linker rebuilt? if so, then clobber IDL projects as we go.
    if (GetFileModDate($compiler_path) > $compiler_modtime || GetFileModDate($linker_path) > $linker_modtime)
    {
      $main::CLOBBER_IDL_PROJECTS = 1;
      print("XPIDL tools have been updated, will clobber all IDL data folders.\n");
    }

	# xpt_link MPW tool, needed for merging xpt files (release build)
    if ($main::options{xptlink})
    {
        my($codewarrior_msl) = GetCodeWarriorRelativePath("MSL:MSL_C:MSL_MacOS:");
    	if ( ! -e $codewarrior_msl . "Lib:PPC:MSL_C_PPC_MPW.Lib") {
        	print("MSL_C_PPC_MPW.Lib not found... Let's build it.\n");
        	BuildProject($codewarrior_msl . "Project:PPC:MSL C.PPC.MTrgt.mcp", "MSL C PPC MPW (NL)");
    	}
        BuildOneProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.xml", "xpt_link", 0, 0, 0);
    }

    EndBuildModule("xpidl");
}

#//--------------------------------------------------------------------------------------------------
#// Build IDL projects
#//--------------------------------------------------------------------------------------------------

sub BuildIDLProjects()
{
    unless( $main::build{idl} ) { return; }
    assertRightDirectory();

    StartBuildModule("idl");

    # XPCOM
    BuildIDLProject(":mozilla:xpcom:macbuild:XPCOMIDL.xml",                         "xpcom");

    # necko
    BuildIDLProject(":mozilla:netwerk:macbuild:netwerkIDL.xml","necko");
    BuildIDLProject(":mozilla:uriloader:macbuild:uriLoaderIDL.xml",                 "uriLoader");
    BuildIDLProject(":mozilla:netwerk:macbuild:cacheIDL.xml", "cache");
	
    if ($main::options{psm}) {
    	BuildIDLProject(":mozilla:security:manager:ssl:macbuild:pipnssIDL.xml",         "pipnss");
    	BuildIDLProject(":mozilla:security:manager:pki:macbuild:pippkiIDL.xml",         "pippki");
    	BuildIDLProject(":mozilla:security:manager:boot:macbuild:pipbootIDL.xml",		"pipboot");
    }
    
    BuildIDLProject(":mozilla:modules:libpref:macbuild:libprefIDL.xml",             "libpref");
    BuildIDLProject(":mozilla:modules:libutil:macbuild:libutilIDL.xml",             "libutil");
    BuildIDLProject(":mozilla:modules:libjar:macbuild:libjarIDL.xml",               "libjar");
    
	if ($main::options{useimg2}) {
	    BuildIDLProject(":mozilla:modules:libpr0n:macbuild:libimg2IDL.xml",         "libimg2");
	    BuildIDLProject(":mozilla:modules:libpr0n:macbuild:icondecoderIDL.xml",         "icondecoder");
    }
    
    BuildIDLProject(":mozilla:modules:plugin:base:macbuild:pluginIDL.xml",          "plugin");
    BuildIDLProject(":mozilla:modules:oji:macbuild:ojiIDL.xml",                     "oji");
    BuildIDLProject(":mozilla:js:macbuild:XPConnectIDL.xml",                        "xpconnect");
    if ($main::options{xpctools}) {
        BuildIDLProject(":mozilla:js:macbuild:XPCToolsIDL.xml",                     "xpctools");
    }
    BuildIDLProject(":mozilla:dom:macbuild:domIDL.xml",                             "dom");
    BuildIDLProject(":mozilla:dom:macbuild:dom_baseIDL.xml",                        "dom_base");
    BuildIDLProject(":mozilla:dom:macbuild:dom_coreIDL.xml",                        "dom_core");
    BuildIDLProject(":mozilla:dom:macbuild:dom_cssIDL.xml",                         "dom_css");
    BuildIDLProject(":mozilla:dom:macbuild:dom_eventsIDL.xml",                      "dom_events");
    BuildIDLProject(":mozilla:dom:macbuild:dom_htmlIDL.xml",                        "dom_html");
    BuildIDLProject(":mozilla:dom:macbuild:dom_rangeIDL.xml",                       "dom_range");
    BuildIDLProject(":mozilla:dom:macbuild:dom_traversalIDL.xml",                   "dom_traversal");
    BuildIDLProject(":mozilla:dom:macbuild:dom_stylesheetsIDL.xml",                 "dom_stylesheets");
    BuildIDLProject(":mozilla:dom:macbuild:dom_viewsIDL.xml",                       "dom_views");
    BuildIDLProject(":mozilla:dom:macbuild:dom_xblIDL.xml",                         "dom_xbl");
    BuildIDLProject(":mozilla:dom:macbuild:dom_xpathIDL.xml",                       "dom_xpath");
    BuildIDLProject(":mozilla:dom:macbuild:dom_xulIDL.xml",                         "dom_xul");

	if ($main::options{svg}) {
		BuildIDLProject(":mozilla:dom:macbuild:dom_svgIDL.xml",                      "dom_svg");
	}

    BuildIDLProject(":mozilla:dom:src:jsurl:macbuild:JSUrlDL.xml",                  "jsurl");
    
    BuildIDLProject(":mozilla:gfx:macbuild:gfxIDL.xml",                             "gfx");
    BuildIDLProject(":mozilla:widget:macbuild:widgetIDL.xml",                       "widget");
    BuildIDLProject(":mozilla:editor:macbuild:EditorIDL.xml",                       "editor");
    BuildIDLProject(":mozilla:editor:txmgr:macbuild:txmgrIDL.xml",                  "txmgr");
    BuildIDLProject(":mozilla:profile:macbuild:ProfileServicesIDL.xml", "profileservices");
    BuildIDLProject(":mozilla:profile:pref-migrator:macbuild:prefmigratorIDL.xml",  "prefm");
        

    BuildIDLProject(":mozilla:htmlparser:macbuild:htmlparserIDL.xml",               "htmlparser");
    BuildIDLProject(":mozilla:content:macbuild:contentIDL.xml",                     "content");

    BuildIDLProject(":mozilla:layout:macbuild:layoutIDL.xml",                       "layout");

    if ($main::options{accessible})
    {
        BuildIDLProject(":mozilla:accessible:macbuild:accessibleIDL.xml",           "accessible"); 
    }

    BuildIDLProject(":mozilla:rdf:macbuild:RDFIDL.xml",                             "rdf");

    BuildIDLProject(":mozilla:rdf:chrome:build:chromeIDL.xml",                      "chrome");
        
    BuildIDLProject(":mozilla:webshell:macbuild:webshellIDL.xml",                   "webshell");
    BuildIDLProject(":mozilla:docshell:macbuild:docshellIDL.xml",                   "docshell");
    BuildIDLProject(":mozilla:embedding:base:macbuild:EmbedIDL.xml",                "EmbedBase");
    BuildIDLProject(":mozilla:embedding:browser:macbuild:browserIDL.xml",           "embeddingbrowser");
    BuildIDLProject(":mozilla:embedding:components:build:macbuild:EmbedComponentsIDL.xml", "EmbedComponents");

    BuildIDLProject(":mozilla:extensions:cookie:macbuild:cookieIDL.xml",            "cookie");
    BuildIDLProject(":mozilla:extensions:pref:macbuild:prefextrasIDL.xml",          "prefextras");
    BuildIDLProject(":mozilla:extensions:wallet:macbuild:walletIDL.xml","wallet");
    BuildIDLProject(":mozilla:extensions:xml-rpc:macbuild:xml-rpcIDL.xml","xml-rpc");
    BuildIDLProject(":mozilla:xpfe:components:bookmarks:macbuild:BookmarksIDL.xml", "bookmarks");
    BuildIDLProject(":mozilla:xpfe:components:directory:DirectoryIDL.xml",          "Directory");
    BuildIDLProject(":mozilla:xpfe:components:regviewer:RegViewerIDL.xml",          "RegViewer");
    BuildIDLProject(":mozilla:xpfe:components:history:macbuild:historyIDL.xml",     "history");
    BuildIDLProject(":mozilla:xpfe:components:shistory:macbuild:shistoryIDL.xml",   "shistory");
    BuildIDLProject(":mozilla:xpfe:components:related:macbuild:RelatedIDL.xml",     "related");
    BuildIDLProject(":mozilla:xpfe:components:search:macbuild:SearchIDL.xml",       "search");
    BuildIDLProject(":mozilla:xpfe:components:macbuild:mozcompsIDL.xml",            "mozcomps");
    BuildIDLProject(":mozilla:xpfe:components:timebomb:macbuild:timebombIDL.xml",   "tmbm");
    BuildIDLProject(":mozilla:xpfe:components:urlbarhistory:macbuild:urlbarhistoryIDL.xml", "urlbarhistory");
    BuildIDLProject(":mozilla:xpfe:components:autocomplete:macbuild:AutoCompleteIDL.xml", "autocomplete");
    BuildIDLProject(":mozilla:xpfe:components:download-manager:macbuild:DownloadManagerIDL.xml", "downloadmanager");

    BuildIDLProject(":mozilla:xpfe:appshell:macbuild:appshellIDL.xml",              "appshell");
    
    BuildIDLProject(":mozilla:xpfe:browser:macbuild:mozBrowserIDL.xml",             "mozBrowser");
    
    BuildIDLProject(":mozilla:xpinstall:macbuild:xpinstallIDL.xml",                 "xpinstall");

    if ($main::options{ldap})
    {
        BuildIDLProject(":mozilla:directory:xpcom:macbuild:mozldapIDL.xml", "mozldap");
        BuildIDLProject(":mozilla:xpfe:components:autocomplete:macbuild:ldapAutoCompleteIDL.xml", "ldapAutoComplete");
    }

    BuildIDLProject(":mozilla:mailnews:base:macbuild:msgCoreIDL.xml",               "mailnews");
    BuildIDLProject(":mozilla:mailnews:compose:macbuild:msgComposeIDL.xml",         "MsgCompose");
    BuildIDLProject(":mozilla:mailnews:local:macbuild:msglocalIDL.xml",             "MsgLocal");
    BuildIDLProject(":mozilla:mailnews:news:macbuild:msgnewsIDL.xml",               "MsgNews");
    BuildIDLProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbookIDL.xml",       "MsgAddrbook");
    BuildIDLProject(":mozilla:mailnews:absync:macbuild:abSyncIDL.xml",              "AbSyncSvc");
    BuildIDLProject(":mozilla:mailnews:db:macbuild:msgDBIDL.xml",                   "MsgDB");
    BuildIDLProject(":mozilla:mailnews:imap:macbuild:msgimapIDL.xml",               "MsgImap");
    BuildIDLProject(":mozilla:mailnews:mime:macbuild:mimeIDL.xml",                  "Mime");
    BuildIDLProject(":mozilla:mailnews:import:macbuild:msgImportIDL.xml",           "msgImport");
    BuildIDLProject(":mozilla:mailnews:import:comm4x:macbuild:msgImportComm4xMIDL.xml",    "msgImportComm4xM");
    if ($main::options{smime} && $main::options{psm}) {
    	BuildIDLProject(":mozilla:mailnews:extensions:smime:macbuild:msgsmimeIDL.xml",  "msgsmime");
    }

    BuildIDLProject(":mozilla:caps:macbuild:CapsIDL.xml",                           "caps");

    BuildIDLProject(":mozilla:intl:locale:macbuild:nsLocaleIDL.xml",                "nsLocale");
    BuildIDLProject(":mozilla:intl:strres:macbuild:strresIDL.xml",                  "nsIStringBundle");
    BuildIDLProject(":mozilla:intl:unicharutil:macbuild:unicharutilIDL.xml",        "unicharutil");
    BuildIDLProject(":mozilla:intl:uconv:macbuild:uconvIDL.xml",                    "uconv");
    BuildIDLProject(":mozilla:intl:chardet:macbuild:chardetIDL.xml",                "chardet");

    if ($main::options{iiextras})
    {
        BuildIDLProject(":mozilla:extensions:interfaceinfo:macbuild:iiextrasIDL.xml", "iiextras");
    }

    if ($main::options{xmlextras})
    {
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlextrasIDL.xml", "xmlextras");
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlschemaIDL.xml", "xmlschema");
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlsoapIDL.xml", "xmlsoap");
        if ($main::options{wsp})
        {
            BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlwsproxyIDL.xml", "wsproxy");
            BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlwsdlIDL.xml", "wsdl");
        }
    }

    if ($main::options{inspector})
    {
        BuildIDLProject(":mozilla:extensions:inspector:macbuild:inspectorIDL.xml", "inspector");
    }

    if ($main::options{typeaheadfind})
    {
        BuildIDLProject(":mozilla:extensions:typeaheadfind:macbuild:typeaheadfindIDL.xml", "typeaheadfind");
    }

    if ($main::options{p3p})
    {
        BuildIDLProject(":mozilla:extensions:p3p:macbuild:p3pIDL.xml", "p3p");
    }

    if ($main::options{jsd})
    {
        BuildIDLProject(":mozilla:js:jsd:macbuild:jsdIDL.xml", "jsdservice");
    }

    EndBuildModule("idl");
}

#//--------------------------------------------------------------------------------------------------
#// Build runtime projects
#//--------------------------------------------------------------------------------------------------

sub BuildRuntimeProjects()
{
    unless( $main::build{runtime} ) { return; }
    assertRightDirectory();

    StartBuildModule("runtime");

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";
    my($P) = $main::PROFILE ? "Profil" : "";

    #//
    #// Shared libraries
    #//
    if ( $main::options{carbon} )
    {
        BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.xml",            "Carbon Interfaces");       
        BuildProject(":mozilla:lib:mac:InterfaceLib:InterfaceOSX.xml",         "MacOS X Interfaces");       
    }
    else
    {
        BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.xml",            "MacOS Interfaces");
        BuildProject(":mozilla:lib:mac:InterfaceLib:InterfaceOSX.xml",         "MacOS Interfaces");       
    }
    
    #// Build all of the startup libraries, for Application, Component, and Shared Libraries. These are
    #// required for all subsequent libraries in the system.
    BuildProject(":mozilla:lib:mac:NSStartup:NSStartup.xml",                           "NSStartup.all");

    BuildOneProjectWithOutput(":mozilla:lib:mac:NSRuntime:NSRuntime.xml", "NSRuntime$C$P$D.shlb", "NSRuntime$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.xml",          "MoreFiles$D.o");

    if ($main::GC_LEAK_DETECTOR && !$main::options{carbon}) {
        BuildProject(":mozilla:gc:boehm:macbuild:gc.xml",                    "gc.ppc.lib");
        MakeAlias(":mozilla:gc:boehm:macbuild:gc.PPC.lib",                   ":mozilla:dist:gc:gc.PPC.lib");
    	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.xml", "MemAllocatorGC.o");
    } else {
    	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.xml", "MemAllocator$C$D.o");
    }

    BuildOneProjectWithOutput(":mozilla:lib:mac:NSStdLib:NSStdLib.xml", "NSStdLib$D.shlb", "NSStdLib$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    if ($main::DEBUG) {
        BuildOneProject(":mozilla:lib:mac:NSStdLib:NSConsole.xml", "NSConsoleDebug.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    }

	BuildOneProjectWithOutput(":mozilla:nsprpub:macbuild:NSPR20PPC.xml", "NSPR20$C$D.shlb", "NSPR20$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    EndBuildModule("runtime");
}


#//--------------------------------------------------------------------------------------------------
#// Build common projects
#//--------------------------------------------------------------------------------------------------

sub BuildCommonProjects()
{
    unless( $main::build{common} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my $dist_dir = GetBinDirectory();
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    StartBuildModule("common");

    #//
    #// Static libraries
    #//

    BuildProject(":mozilla:string:macbuild:string.xml",                      "string$D.o");
    MakeAlias(":mozilla:string:macbuild:string$D.o", ":mozilla:dist:string:");

    BuildProject(":mozilla:intl:unicharutil:macbuild:UnicharUtilsStaticLib.xml", "UnicharUtilsStatic$D.o");
    MakeAlias(":mozilla:intl:unicharutil:macbuild:UnicharUtilsStatic$D.o", ":mozilla:dist:unicharutil:");

    #//
    #// Shared libraries
    #//

    BuildOneProject(":mozilla:modules:libreg:macbuild:libreg.xml",              "libreg$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:xpcom:macbuild:xpcomPPC.xml",                     "xpcom$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:js:macbuild:JavaScript.xml",                      "JavaScript$D.shlb", 1, $main::ALIAS_SYM_FILES, 0); 
    BuildOneProject(":mozilla:js:macbuild:LiveConnect.xml",                     "LiveConnect$D.$S", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.xml",                  "zlib$D.$S", 1, $main::ALIAS_SYM_FILES, 0); 
    BuildProject(":mozilla:modules:zlib:macbuild:zlib.xml",                     "zlib$D.Lib"); 
    BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.xml",              "libjar$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildProject(":mozilla:modules:libjar:macbuild:libjar.xml",                 "libjar$D.Lib");

    BuildOneProject(":mozilla:modules:oji:macbuild:oji.xml",                    "oji$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:caps:macbuild:Caps.xml",                          "Caps$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.xml",            "libpref$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:js:macbuild:XPConnect.xml",                       "XPConnect$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if ($main::options{xpctools}) {
        BuildOneProject(":mozilla:js:macbuild:XPCTools.xml",                    "XPCTools$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }
    BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.xml",            "libutil$D.$S", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:db:mork:macbuild:mork.xml",                       "Mork$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildProject(":mozilla:dbm:macbuild:DBM.xml",                               "DBM$D.o");
	MakeAlias(":mozilla:dbm:macbuild:DBM$D.o",                                  ":mozilla:dist:dbm:");

    #// Static libraries
    # Static Libs
    BuildProject(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider.xml", "mpfilelocprovider$D.o");
    MakeAlias(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider$D.o", ":mozilla:dist:mpfilelocprovider:");
    
    InstallFromManifest(":mozilla:xpcom:components:MANIFEST_COMPONENTS",         "${dist_dir}Components:");

    EndBuildModule("common");
}

#//--------------------------------------------------------------------------------------------------
#// Build imglib projects
#//--------------------------------------------------------------------------------------------------

sub BuildImglibProjects()
{
    unless( $main::build{imglib} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    StartBuildModule("imglib");

    BuildProject(":mozilla:jpeg:macbuild:JPEG.xml",                          "JPEG$D.o");
    MakeAlias(":mozilla:jpeg:macbuild:JPEG$D.o",                             ":mozilla:dist:libimg:");
    BuildProject(":mozilla:modules:libimg:macbuild:png.xml",                 "png$D.o");
    MakeAlias(":mozilla:modules:libimg:macbuild:png$D.o",                    ":mozilla:dist:libimg:");

    # MNG
    if ($main::options{mng})
    {
        BuildProject(":mozilla:modules:libimg:macbuild:mng.xml",             "mng$D.o");
        MakeAlias(":mozilla:modules:libimg:macbuild:mng$D.o",                ":mozilla:dist:libimg:");
    }

    EndBuildModule("imglib");
} # imglib

#//--------------------------------------------------------------------------------------------------
#// Build libimg2 projects
#//--------------------------------------------------------------------------------------------------

sub BuildImglib2Projects()
{
    unless( $main::build{libimg2} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    StartBuildModule("libimg2");    
    
    if ($main::options{useimg2})
    {
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:libimg2.xml",            "libimg2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:icondecoder.xml",        "icondecoder$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        
        # MNG
        if ($main::options{mng})
        {
            BuildOneProject(":mozilla:modules:libpr0n:macbuild:mngdecoder.xml",     "mngdecoder$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        }
    }
    
    EndBuildModule("libimg2");
} # imglib2
    
#//--------------------------------------------------------------------------------------------------
#// Build international projects
#//--------------------------------------------------------------------------------------------------

sub BuildInternationalProjects()
{
    unless( $main::build{intl} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    StartBuildModule("intl");

    BuildOneProject(":mozilla:intl:macbuild:i18n.xml",               "i18n$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:uconv.xml",                   "uconv$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if ($main::options{mathml})
    {
        BuildOneProject(":mozilla:intl:uconv:macbuild:ucvmath.xml",             "ucvmath$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    EndBuildModule("intl");
} # intl


#//--------------------------------------------------------------------------------------------------
#// Build Necko projects
#//--------------------------------------------------------------------------------------------------

sub BuildNeckoProjects()
{
    unless( $main::build{necko} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";

    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    my $dist_dir = GetBinDirectory();

    StartBuildModule("necko");

    BuildOneProjectWithOutput(":mozilla:netwerk:macbuild:netwerk.xml", "Necko$C$D.$S", "Necko$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:netwerk:macbuild:netwerk2.xml",          "Necko2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:dom:src:jsurl:macbuild:JSUrl.xml",       "JSUrl$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
          
    InstallFromManifest(":mozilla:netwerk:base:src:MANIFEST_COMPONENTS", "${dist_dir}Components:");

    EndBuildModule("necko");
}


#//--------------------------------------------------------------------------------------------------
#// Build Security projects
#//--------------------------------------------------------------------------------------------------


sub CreateNSSExportList($$)
{
  my ($source_def_file, $final_exp_file) = @_;
  
  if (! -e $source_def_file) {
  	die "NSS file $source_def_file does not exist";
  }
  
  my($def_mod_time) = GetFileModDate($source_def_file);
  my($exp_mod_time) = (-e $final_exp_file ? GetFileModDate($final_exp_file) : 0);
  
  if ($exp_mod_time > $def_mod_time) {
    return;
  }
  
  open (DEF_FILE, "< $source_def_file") || die "can't open NSS def file $!";
  open (EXP_FILE, "> $final_exp_file");
  
  print("Creating $final_exp_file from $source_def_file\n");
  while (<DEF_FILE>) {
    my ($line) = $_;
    if ($line =~ /^;\+./) {
      next;
    }
    if ($line =~ /^LIBRARY/) {
      next;
    }
    if ($line =~ /^EXPORTS/) {
      next;
    }
    if ($line =~ s/^;;(.*) DATA ;//) {
      print EXP_FILE "$1\n";
    }
    if ($line =~ s/^(.*) DATA ;//) {
      print EXP_FILE "$1\n";
    }
    if ($line =~ s/^(.*);//) {
      print EXP_FILE "$1\n";
    }
  }
  close DEF_FILE;
  close EXP_FILE;
}

sub BuildSecurityProjects()
{
    unless( $main::build{security} && $main::options{psm}) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    my $dist_dir = GetBinDirectory(); # the subdirectory with the libs and executable.

    StartBuildModule("security");

    #First we need to build all of the NSS shared libraries now.  
    #The order is important because they link against each other.
    BuildProject(":mozilla:security:nss:macbuild:util.xml","util$D.o");
    BuildProject(":mozilla:security:nss:macbuild:crmf.xml","crmf$D.o");
    
    CreateNSSExportList(":mozilla:security:nss:lib:softoken:softokn.def",
                        ":mozilla:security:nss:macbuild:softoken.mcp.exp");                        
    BuildOneProjectWithOutput(":mozilla:security:nss:macbuild:softoken.xml","Softoken3$D.shlb","Softoken3$D.shlb",1, $main::ALIAS_SYM_FILES, 0);
    
    CreateNSSExportList(":mozilla:security:nss:lib:nss:nss.def",
                        ":mozilla:security:nss:macbuild:NSS.mcp.exp"); 
    BuildOneProjectWithOutput(":mozilla:security:nss:macbuild:NSS.xml","NSS3$D.shlb","NSS3$D.shlb",1, $main::ALIAS_SYM_FILES, 0);
    
    CreateNSSExportList(":mozilla:security:nss:lib:ssl:ssl.def",
                        ":mozilla:security:nss:macbuild:ssl.mcp.exp");
    BuildOneProjectWithOutput(":mozilla:security:nss:macbuild:ssl.xml","SSL3$D.shlb","SSL3$D.shlb",1, $main::ALIAS_SYM_FILES, 0);
    
    CreateNSSExportList(":mozilla:security:nss:lib:smime:smime.def",
                        ":mozilla:security:nss:macbuild:smime.mcp.exp");
    BuildOneProjectWithOutput(":mozilla:security:nss:macbuild:smime.xml","SMIME3$D.shlb","SMIME3$D.shlb",1, $main::ALIAS_SYM_FILES, 0);
    
    
    BuildOneProject(":mozilla:security:manager:boot:macbuild:pipboot.xml", "pipboot$D.$S",  1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:security:manager:ssl:macbuild:PIPNSS.xml",   "PIPNSS$D.$S",  1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:security:manager:pki:macbuild:PIPPKI.xml",   "PIPPKI$D.$S",  1, $main::ALIAS_SYM_FILES, 1); 
    
	if ($main::options{static_build}) {
		BuildOneProject(":mozilla:modules:staticmod:macbuild:cryptoComponent.xml",    "MetaCrypto$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
	}

	#Build the loadable module that contains the root certs. This is always built as a shared lib, even in the static build.
    BuildProject(":mozilla:security:nss:macbuild:NSSckfw.xml", "NSSckfw$D.o");
    BuildProject(":mozilla:security:nss:macbuild:LoadableRoots.xml", "NSSckbi$D.shlb");
    # NSS doesn't properly load the shared library created above if it's an alias, so we'll just copy it so that
    # all builds will just work.  It's 140K optimized and 164K debug so it's not too much disk space.
    copy(":mozilla:security:nss:macbuild:NSSckbi$D.shlb",$dist_dir."Essential Files:NSSckbi$D.shlb");
	
    EndBuildModule("security");
} # Security


#//--------------------------------------------------------------------------------------------------
#// Build Browser utils projects
#//--------------------------------------------------------------------------------------------------

sub BuildBrowserUtilsProjects()
{
    unless( $main::build{browserutils} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    StartBuildModule("browserutils");

    BuildOneProject(":mozilla:uriloader:macbuild:uriLoader.xml",                "uriLoader$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:profile:macbuild:profile.xml",                    "profile$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:profile:pref-migrator:macbuild:prefmigrator.xml", "prefm$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:extensions:cookie:macbuild:cookie.xml",           "Cookie$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:pref:macbuild:prefextras.xml",         "prefExtras$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.xml",           "Wallet$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:walletviewers.xml",    "WalletViewers$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:universalchardet:macbuild:Universalchardet.xml", "Universalchardet$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:rdf:chrome:build:chrome.xml",                     "ChomeRegistry$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
    EndBuildModule("browserutils");
}


#//--------------------------------------------------------------------------------------------------
#// Build NGLayout
#//--------------------------------------------------------------------------------------------------

sub BuildLayoutProjects()
{
    unless( $main::build{nglayout} ) { return; }
    assertRightDirectory();


    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";
    
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    
    my($dist_dir) = GetBinDirectory();
    my($resource_dir) = "$dist_dir" . "res:";

    
    StartBuildModule("nglayout");

    open(OUTPUT, ">:mozilla:content:build:gbdate.h") || die "could not open gbdate.h";
    my($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;
    # localtime returns year minus 1900
    $year = $year + 1900;
    printf(OUTPUT "#define PRODUCT_VERSION \"%04d%02d%02d\"\n", $year, 1+$mon, $mday);
    close(OUTPUT);

    #//
    #// Build Layout projects
    #//

    BuildProject(":mozilla:expat:macbuild:expat.xml",                           "expat$D.o");
    BuildOneProject(":mozilla:htmlparser:macbuild:htmlparser.xml",              "htmlparser$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:gfx:macbuild:gfx.xml",                            "gfx$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    my($dbg) = $main::DEBUG ? "Dbg" : "";
    BuildOneProjectWithOutput(":mozilla:gfx:macbuild:gfxComponent.xml",         "gfxComponent$C$dbg.$S", "gfxComponent$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:dom:macbuild:dom.xml",                            "dom$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:plugin:base:macbuild:plugin.xml",         "plugin$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:plugin:base:macbuild:pluginClassic.xml",  "pluginClassic$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
    # Static library shared between different content- and layout-related libraries
    BuildProject(":mozilla:content:macbuild:contentshared.xml",                 "contentshared$D.o");
    MakeAlias(":mozilla:content:macbuild:contentshared$D.o",                    ":mozilla:dist:content:");

    if ($main::options{svg})
    {
        BuildOneProject(":mozilla:content:macbuild:contentSVG.xml",             "contentSVG$D.o", 0, 0, 0);
    }
    else
    {
        BuildOneProject(":mozilla:content:macbuild:contentSVG.xml",             "contentSVG$D.o stub", 0, 0, 0);
    }

    BuildOneProject(":mozilla:content:macbuild:content.xml",                    "content$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if ($main::options{mathml})
    {
        BuildProject(":mozilla:layout:macbuild:layoutmathml.xml",                "layoutmathml$D.o");
    }
    else
    {
        BuildProject(":mozilla:layout:macbuild:layoutmathml.xml",                "layoutmathml$D.o stub");
    }
    if ($main::options{libart_lgpl})
    {
        BuildOneProject(":mozilla:other-licenses:libart_lgpl:macbuild:libart.xml", "libart$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
     }
    if ($main::options{svg})
    {
        BuildProject(":mozilla:layout:macbuild:layoutsvg.xml",                   "layoutsvg$D.o");
    }
    else
    {
        BuildProject(":mozilla:layout:macbuild:layoutsvg.xml",                   "layoutsvg$D.o stub");
    }
    BuildOneProject(":mozilla:layout:macbuild:layout.xml",                      "layout$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:view:macbuild:view.xml",                          "view$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:widget:macbuild:WidgetSupport.xml",               "WidgetSupport$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProjectWithOutput(":mozilla:widget:macbuild:widget.xml",            "widget$C$D.$S", "widget$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:docshell:macbuild:docshell.xml",                  "docshell$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:rdf:macbuild:rdf.xml",                            "RDFLibrary$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.xml",                "xpinstall$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProjectWithOutput(":mozilla:xpinstall:cleanup:macbuild:XPICleanup.xml",       "XPICleanup$D$C", "XPICleanup$D", 1, $main::ALIAS_SYM_FILES, 0);
    InstallFromManifest(":mozilla:xpinstall:cleanup:MANIFEST_CMESSAGE",         "$resource_dir");

    BuildOneProject(":mozilla:xpinstall:macbuild:xpistub.xml",                  "xpistub$D.$S", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:xpinstall:wizard:libxpnet:macbuild:xpnet.xml",    "xpnet$D.Lib", 0, 0, 0);
    if (!($main::PROFILE)) {
        BuildOneProject(":mozilla:xpinstall:wizard:mac:macbuild:MIW.xml",           "Mozilla Installer$D", 0, 0, 0);
    }
    
    EndBuildModule("nglayout");
}

#//--------------------------------------------------------------------------------------------------
#// Build Accessiblity Projects
#//--------------------------------------------------------------------------------------------------
sub BuildAccessiblityProjects()
{
    unless( $main::build{accessiblity} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    StartBuildModule("accessiblity");    
    
    if ($main::options{accessible})
    {
      if ($main::options{moz_xul})
      {
          BuildProject(":mozilla:accessible:macbuild:accessible_xul.xml",           "accessible_xul$D.o");
      }
      else
      {
          BuildProject(":mozilla:accessible:macbuild:accessible_xul.xml",           "accessible_xul$D.o stub");
      }
      BuildOneProject(":mozilla:accessible:macbuild:accessible.xml",   "accessible$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    EndBuildModule("accessiblity");
}

#//--------------------------------------------------------------------------------------------------
#// Build Editor Projects
#//--------------------------------------------------------------------------------------------------

sub BuildEditorProjects()
{
    unless( $main::build{editor} ) { return; }
    assertRightDirectory();
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("editor");

    BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.xml",                 "EditorTxmgr$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    # note: only build one of the following targets
    BuildOneProject(":mozilla:editor:macbuild:editor.xml",                      "htmleditor$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
#    BuildOneProject(":mozilla:editor:macbuild:editor.mcp",                      "texteditor$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:editor:macbuild:composer.xml",                    "Composer$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    EndBuildModule("editor");
}


#//--------------------------------------------------------------------------------------------------
#// Build Viewer Projects
#//--------------------------------------------------------------------------------------------------

sub BuildViewerProjects()
{
    unless( $main::build{viewer} ) { return; }
    assertRightDirectory();
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";

    my($dist_dir) = GetBinDirectory();

    StartBuildModule("viewer");

    if (! $main::options{"static_build"})
    {
  		BuildProject(":mozilla:webshell:tests:viewer:mac:viewer.xml",            "viewer$C$D");
    }

    EndBuildModule("viewer");
}


#//--------------------------------------------------------------------------------------------------
#// Build Embedding Projects
#//--------------------------------------------------------------------------------------------------

sub BuildEmbeddingProjects()
{
    unless( $main::build{embedding} ) { return; }
    assertRightDirectory();
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";

    my($dist_dir) = GetBinDirectory();

    StartBuildModule("embedding");

    # Since there are separate Carbon targets, but the name is the same.
    unlink ":mozilla:embedding:components:printingui:macbuild:printingUI$D.o";
    BuildProject(":mozilla:embedding:components:printingui:macbuild:printingUI.xml", "printingUI$C$D.o");
    MakeAlias(":mozilla:embedding:components:printingui:macbuild:printingUI$D.o", ":mozilla:dist:embedding:components:");

    BuildOneProject(":mozilla:embedding:components:build:macbuild:EmbedComponents.xml",       "EmbedComponents$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:embedding:browser:macbuild:webBrowser.xml",       "webBrowser$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildProject(":mozilla:embedding:base:macbuild:EmbedAPI.xml", "EmbedAPI$D.o");
    MakeAlias(":mozilla:embedding:base:macbuild:EmbedAPI$D.o", ":mozilla:dist:embedding:");

    if ($main::options{embedding_test})
    {
    	my($PowerPlantPath) = "MacOS Support:PowerPlant";
        if (-e GetCodeWarriorRelativePath($PowerPlantPath))
        {
		    if (! $main::options{"static_build"})
		    {
        	    # Build PowerPlant and export the lib and the precompiled header
                BuildOneProject(":mozilla:lib:mac:PowerPlant:PowerPlant.xml", "PowerPlant$D.o",  0, 0, 0);
                MakeAlias(":mozilla:lib:mac:PowerPlant:PowerPlant$D.o", ":mozilla:dist:mac:powerplant:");
                MakeAlias(":mozilla:lib:mac:PowerPlant:pch:PPHeaders$D" . "_pch", ":mozilla:dist:mac:powerplant:");
            
                BuildOneProject(":mozilla:embedding:browser:powerplant:PPBrowser.xml", "PPEmbed$C$D",  0, 0, 0);
            }
        }
        else
        {
            print("$PowerPlantPath does not exist - embedding sample will not be built\n");
        }
    }
    
    EndBuildModule("embedding");
}


#//--------------------------------------------------------------------------------------------------
#// Build XPApp Projects
#//--------------------------------------------------------------------------------------------------

sub BuildXPAppProjects()
{
    unless( $main::build{xpapp} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("xpapp");

    # Components
    BuildOneProject(":mozilla:xpfe:components:find:macbuild:FindComponent.xml", "FindComponent$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:regviewer:RegViewer.xml", "RegViewer$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:shistory:macbuild:shistory.xml", "shistory$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:macbuild:appcomps.xml", "appcomps$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    InstallFromManifest(":mozilla:xpfe:appshell:src:MANIFEST_COMPONENTS", "${dist_dir}Components:");

    # Applications
    BuildOneProject(":mozilla:xpfe:appshell:macbuild:AppShell.xml",             "AppShell$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    EndBuildModule("xpapp");
}


#//--------------------------------------------------------------------------------------------------
#// Build Extensions Projects
#//--------------------------------------------------------------------------------------------------

sub BuildExtensionsProjects()
{
    unless( $main::build{extensions} ) { return; }
    assertRightDirectory();

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("extensions");

    my($components_dir) = "$dist_dir" . "Components:";
    my($chrome_subdir) = "Chrome:";
    my($chrome_dir) = "$dist_dir"."$chrome_subdir";
    my($packages_chrome_dir) = "$chrome_dir" . "packages:";

    # Chatzilla
    if ($main::options{chatzilla})
    {
      InstallResources(":mozilla:extensions:irc:js:lib:MANIFEST_COMPONENTS", "$components_dir");
    }
    
    # XML-RPC
    if ($main::options{xml_rpc})
    {
      InstallFromManifest(":mozilla:extensions:xml-rpc:src:MANIFEST_COMPONENTS", "$components_dir");
    }
    
    # Transformiix
    if ($main::options{transformiix})
    {
        BuildOneProject(":mozilla:extensions:transformiix:macbuild:transformiix.xml", "transformiix$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # LDAP Client
    if ($main::options{ldap})
    {
        my($experi) = $main::options{ldap_experimental} ? " experi" : "";

        BuildOneProjectWithOutput(":mozilla:directory:c-sdk:ldap:libraries:macintosh:LDAPClient.xml", "LDAPClient$C$D.shlb", "LDAPClient$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
        BuildOneProjectWithOutput(":mozilla:directory:xpcom:macbuild:mozldap.xml", "mozldap$D.$S$experi", "mozldap$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

        if ($main::options{ldap_experimental})
        {
            InstallResources(":mozilla:directory:xpcom:datasource:MANIFEST_COMPONENTS", "$components_dir");
        }
    }
    else
    {
        # build a project that outputs a dummy LDAPClient lib so that later projects (e.g. apprunner) have something
        # to link against. This is really only needed for the static build, but there is no harm in building it anyway.
        BuildOneProject(":mozilla:directory:xpcom:macbuild:LDAPClientDummyLib.xml", "LDAPClient$D.shlb", 1, 0, 0);
    }

    # InterfaceInfo Extras
    if ($main::options{iiextras})
    {
        BuildOneProject(":mozilla:extensions:interfaceinfo:macbuild:iiextras.xml", "iiextras$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        InstallResources(":mozilla:extensions:interfaceinfo:src:MANIFEST_COMPONENTS", "$components_dir");
    }

    # XML Extras
    if ($main::options{xmlextras})
    {
        if ($main::options{wsp}) {
            BuildProject(":mozilla:extensions:xmlextras:macbuild:xmlwsp.xml", "xmlwsp$D.o");
        }
        else {
            BuildProject(":mozilla:extensions:xmlextras:macbuild:xmlwsp.xml", "xmlwsp$D.o stub");
        }
        BuildOneProject(":mozilla:extensions:xmlextras:macbuild:xmlextras.xml", "xmlextras$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # Document Inspector
    if ($main::options{inspector})
    {
        BuildOneProject(":mozilla:extensions:inspector:macbuild:inspector.xml", "inspector$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # Type Ahead Find
    if ($main::options{typeaheadfind})
    {
        BuildOneProject(":mozilla:extensions:typeaheadfind:macbuild:typeaheadfind.xml", "typeaheadfind$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # P3P
    if ($main::options{p3p})
    {
        BuildOneProject(":mozilla:extensions:p3p:macbuild:p3p.xml", "p3p$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # JS Debugger
    if ($main::options{jsd})
    {
        BuildOneProject(":mozilla:js:jsd:macbuild:JSD.xml", "jsdService$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

        if ($main::options{venkman})
        {
            InstallResources(":mozilla:extensions:venkman:js:MANIFEST_COMPONENTS", "$components_dir");
        }
    }
    
    EndBuildModule("extensions");
}

#//--------------------------------------------------------------------------------------------------
#// Build Plugins Projects
#//--------------------------------------------------------------------------------------------------

sub BuildPluginsProjects()                          
{
    unless( $main::build{plugins} ) { return; }

    StartBuildModule("plugins");

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";

    # as a temporary measure, make sure that the folder "MacOS Support:JNIHeaders" exists,
    # before we attempt to build the MRJ plugin. This will allow a gradual transition.
    if ($main::options{carbon} || -e GetCodeWarriorRelativePath("Java_Support:VM_Support:MRJ_Support"))
    {
	    my($plugin_path) = ":mozilla:plugin:oji:MRJ$C:plugin:";

	    # Build MRJPlugin
	    BuildProject($plugin_path . "MRJPlugin$C.xml", "MRJPlugin$C");
	    # Build MRJPlugin.jar (if Java tools exist)
	    my($linker_path) = GetCodeWarriorRelativePath("CodeWarrior Plugins:Linkers:Java Linker");
	    if (-e $linker_path) {
	        print("CodeWarrior Java tools detected, building MRJPlugin.jar.\n");
	        BuildProject($plugin_path . "MRJPlugin$C.xml", "MRJPlugin.jar");
	    }
	    # Copy MRJPlugin, MRJPlugin.jar to appropriate plugins folder.
	    my($plugin_dist) = GetBinDirectory() . "Plug-ins:";
	    MakeAlias($plugin_path . "MRJPlugin$C", $plugin_dist);
        MakeAlias($plugin_path . "MRJPlugin.jar", $plugin_dist);
	    if ($main::options{carbon}) {
            MakeAlias($plugin_path . "MRJPlugin.policy", $plugin_dist);
            MakeAlias($plugin_path . "MRJPlugin.properties", $plugin_dist);
	    }
	}

    # Build the Default Plug-in and place an alias in the appropriate plugins folder.
    my($plugin_path) = ":mozilla:modules:plugin:samples:default:mac:";
    my($plugin_dist) = GetBinDirectory() . "Plug-ins:";

    BuildProject($plugin_path . "DefaultPlugin.xml", "DefaultPlugin$C");
    MakeAlias($plugin_path . "Default Plug-in", $plugin_dist);

    EndBuildModule("plugins");
}

#//--------------------------------------------------------------------------------------------------
#// Build MailNews Projects
#//--------------------------------------------------------------------------------------------------

sub BuildMailNewsProjects()
{
    unless( $main::build{mailnews} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("mailnews");

    BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.xml",                 "MsgUtil$D.lib", 0, 0, 0);
    BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.xml",                      "mailnews$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.xml",                "MsgCompose$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.xml",                          "MsgDB$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.xml",                    "MsgLocal$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:imap:macbuild:msgimap.xml",                      "MsgImap$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:news:macbuild:msgnews.xml",                      "MsgNews$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbook.xml",              "MsgAddrbook$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:absync:macbuild:AbSync.xml",                     "AbSyncSvc$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.xml",                         "Mime$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:emitters:macbuild:mimeEmitter.xml",         "mimeEmitter$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:vcard:macbuild:vcard.xml",       "vcard$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
#   BuildOneProject(":mozilla:mailnews:mime:cthandlers:calendar:macbuild:calendar.mcp", "calendar$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:macbuild:msgImport.xml",                  "msgImport$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:text:macbuild:msgImportText.xml",         "msgImportText$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:eudora:macbuild:msgImportEudora.xml",     "msgImportEudora$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:comm4x:macbuild:msgImportComm4xM.xml",    "msgImportComm4xM$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
         
    if ($main::options{static_build}) {
        BuildOneProject(":mozilla:modules:staticmod:macbuild:mailnewsComponent.xml",  "MetaMailNews$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }
             
    InstallResources(":mozilla:mailnews:addrbook:src:MANIFEST_COMPONENTS",              "${dist_dir}Components");
	  if ($main::options{smime} && $main::options{psm}) {
    	BuildOneProject(":mozilla:mailnews:extensions:smime:macbuild:smime.xml",         "msgsmime$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    	InstallResources(":mozilla:mailnews:extensions:smime:src:MANIFEST",				 "${dist_dir}Components");
    } else {
        BuildOneProject(":mozilla:mailnews:mime:cthandlers:smimestub:macbuild:smime.xml",   "smime$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
	  if ($main::options{mdn}) {
    	BuildOneProject(":mozilla:mailnews:extensions:mdn:macbuild:msgmdn.xml",         "msgmdn$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }

    if ($main::options{mdn}) {
    	InstallResources(":mozilla:mailnews:extensions:mdn:src:MANIFEST",				 "${dist_dir}Components");
    }

    EndBuildModule("mailnews");
}


#//--------------------------------------------------------------------------------------------------
#// Build Mozilla
#//--------------------------------------------------------------------------------------------------

sub BuildMozilla()
{
    unless( $main::build{apprunner} ) { return; }
    assertRightDirectory();
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";

    StartBuildModule("apprunner");

    if ($main::options{static_build}) {
        BuildProject(":mozilla:xpfe:bootstrap:macbuild:StaticMerge.xml",    "StaticMerge$D.o");
    } else {
        BuildProject(":mozilla:xpfe:bootstrap:macbuild:StaticMerge.xml",    "StaticMergeDummy$D.o");
    }

    BuildProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.xml",          "apprunner$C$D");

    # build tool to create Component Registry in release builds only.
    if (!($main::DEBUG)) {
        BuildProject(":mozilla:xpcom:tools:registry:macbuild:RegXPCOM.xml", "RegXPCOM");
    }
    
    # build XPCShell to test the cache in debugging builds only.
    if ($main::DEBUG) {
        BuildProject(":mozilla:js:macbuild:XPCShell.xml", "XPCShellDebug");
    }
    
    # copy command line documents into the Apprunner folder and set correctly the signature
    my($dist_dir) = GetBinDirectory();
    my($cmd_file_path) = ":mozilla:xpfe:bootstrap:";
    my($cmd_file) = "";

    $cmd_file = "Mozilla Select Profile";
    copy( $cmd_file_path . "Mozilla_Select_Profile", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Profile Wizard";
    copy( $cmd_file_path . "Mozilla_Profile_Wizard", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Profile Manager";
    copy( $cmd_file_path . "Mozilla_Profile_Manager", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Profile Migration";
    copy( $cmd_file_path . "Mozilla_Installer", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Addressbook";
    copy( $cmd_file_path . "Mozilla_Addressbook", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Editor";
    copy( $cmd_file_path . "Mozilla_Editor", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Message Compose";
    copy( $cmd_file_path . "Mozilla_Message_Compose", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Messenger";
    copy( $cmd_file_path . "Mozilla_Messenger", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Preferences";
    copy( $cmd_file_path . "Mozilla_Preference", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);
    
    $cmd_file = "NSPR Logging";
    copy( $cmd_file_path . "Mozilla_NSPR_Log", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla JavaScript Console";
    copy( $cmd_file_path . "Mozilla_JavaScript_Console", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Bloat URLs";
    copy( $cmd_file_path . "Mozilla_Bloat_URLs", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);
    copy( ":mozilla:build:bloaturls.txt", $dist_dir . "bloaturls.txt" );

    EndBuildModule("apprunner");
}


#//--------------------------------------------------------------------------------------------------
#// Build everything
#//--------------------------------------------------------------------------------------------------

sub BuildProjects()
{
    # activate CodeWarrior
    ActivateApplication('CWIE');

    PrepareBuild();

    if ($main::RUNTIME)
    {
        BuildStubs();
        BuildRuntimeProjects();
        return;        
    }

    BuildXPIDLCompiler();
    BuildIDLProjects();
    BuildStubs();
    BuildRuntimeProjects();
    BuildCommonProjects();
    BuildImglibProjects();
    BuildImglib2Projects();
    BuildNeckoProjects();
    BuildSecurityProjects();
    BuildBrowserUtilsProjects();        
    BuildInternationalProjects();
    BuildLayoutProjects();
    BuildAccessiblityProjects();   
    BuildEditorProjects();
    BuildEmbeddingProjects();
    BuildViewerProjects();
    BuildXPAppProjects();
    BuildExtensionsProjects();
    BuildPluginsProjects();
    BuildMailNewsProjects();
    BuildMozilla();
    # do this last so as not to pollute dist with
    # non-include files before building projects.
    BuildResources();
}


1;
