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
use FileHandle;
use File::Path;
use File::Copy;

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


# NGLayoutBuildList builds the nglayout project
# it is configured by setting the following variables in the caller:
# Usage:
# caller variables that affect behaviour:
# DEBUG     : 1 if we are building a debug version
# 3-part build process: checkout, dist, and build_projects
# Hack alert:
# NGLayout defines are located in :mozilla:config:mac:NGLayoutConfigInclude.h
# An alias "MacConfigInclude.h" to this file is created inside dist:config
# Note that the name of alias is different than the name of the file. This
# is to trick CW into including NGLayout defines 


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
    
    open(CHROMEFILE, ">>${chrome_subdir}:installed-chrome.txt") || die "Failed to open installed_chrome.txt\n";
    print(CHROMEFILE "skin,install,select,$skin\n");
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

    # make a dup in en-US
    my($default_profile_dir_en_US) = "$default_profile_dir"."en-US:";
    mkdir($default_profile_dir_en_US, 0);

    InstallResources(":mozilla:profile:defaults:MANIFEST",                             "$default_profile_dir_en_US", 1);
    }
    
    # Default _pref_ directory stuff
    {
    my($default_pref_dir) = "$defaults_dir"."Pref:";
    mkdir($default_pref_dir, 0);
    InstallResources(":mozilla:xpinstall:public:MANIFEST_PREFS",                       "$default_pref_dir", 0);
    InstallResources(":mozilla:modules:libpref:src:MANIFEST_PREFS",                    "$default_pref_dir", 0);
    InstallResources(":mozilla:modules:libpref:src:init:MANIFEST",                     "$default_pref_dir", 0);
    InstallResources(":mozilla:modules:libpref:src:mac:MANIFEST",                      "$default_pref_dir", 0);
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
    if (! $main::options{mathml})
    {
        MakeAlias(":mozilla:layout:html:document:src:ua.css",                          "$resource_dir");
    }
    else
    {
        #// Building MathML so include the mathml.css file in ua.css
        MakeAlias(":mozilla:layout:mathml:content:src:mathml.css",                     "$resource_dir");
        copy(":mozilla:layout:html:document:src:ua.css",                               "$resource_dir"."ua.css");
        @ARGV = ("$resource_dir"."ua.css");
        do ":mozilla:layout:mathml:content:src:mathml-css.pl";
    }
    
    MakeAlias(":mozilla:layout:html:document:src:html.css",                            "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:forms.css",                           "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:quirk.css",                           "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:arrow.gif",                           "$resource_dir"); 
    MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",            "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:charsetalias.properties",                       "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:acceptlanguage.properties",                     "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:maccharset.properties",                         "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:charsetData.properties",                        "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:acceptlanguage.properties",                     "$resource_dir");
    MakeAlias(":mozilla:intl:locale:src:langGroups.properties",                        "$resource_dir");
    MakeAlias(":mozilla:intl:locale:src:language.properties",                          "$resource_dir");

    InstallResources(":mozilla:gfx:src:MANIFEST",                                      "$resource_dir"."gfx:");

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

    my($domds_dir) = "$samples_dir" . "rdf:";
    InstallResources(":mozilla:rdf:tests:domds:resources:MANIFEST",                    "$domds_dir");

    # Search - make copies (not aliases) of the various search files
    my($searchPlugins) = "${dist_dir}Search Plugins";
    print("--- Starting Search Plugins copying: $searchPlugins\n");
    InstallResources(":mozilla:xpfe:components:search:datasets:MANIFEST",              "$searchPlugins", 1);

    # QA Menu
    InstallResources(":mozilla:intl:strres:tests:MANIFEST",            "$resource_dir");

    # install builtin XBL bindings
    MakeAlias(":mozilla:layout:xbl:builtin:htmlbindings.xml",                      "$builtin_dir");
    MakeAlias(":mozilla:layout:xbl:builtin:mac:platformHTMLBindings.xml",          "$builtin_dir");

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
#// ProcessJarManifests
#//--------------------------------------------------------------------------------------------------

sub ProcessJarManifests()
{
    my($dist_dir) = GetBinDirectory();
    my($chrome_dir) = "$dist_dir"."Chrome";

    # a hash of jars passed as context to the following calls
    my(%jars);
    
    if ($main::build{extensions})
    {
      CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
      # cview needs a jar.mn file
      # transformiix needs a jar.mn file
    }
    
    CreateJarFromManifest(":mozilla:caps:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:docshell:base:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:editor:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:browser:chrome:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:browser:chrome:locale:en-US:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:wallet:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:intl:uconv:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:forms:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:base:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:mailnews:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:netwerk:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:profile:pref-migrator:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:profile:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:rdf:tests:domds:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:blue:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:search:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:sidebar:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:global:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:navigator:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:messenger:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:messenger:addressbook:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:editor:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:preview:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:modern:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpcom:base:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:communicator:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:communicator:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:bookmarks:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:locale:en-US:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpinstall:res:jar.mn", $chrome_dir, \%jars);

    # bad jar.mn files
#    CreateJarFromManifest(":mozilla:extensions:xmlterm:jar.mn", $chrome_dir, \%jars);

    WriteOutJarFiles($chrome_dir, \%jars);
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

    ActivateApplication('McPL');
    
    MakeNonChromeAliases();   # Defaults, JS components etc.      
    BuildJarFiles();    

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

    #GC_LEAK_DETECTOR
    InstallFromManifest(":mozilla:gc:boehm:MANIFEST",                              "$distdirectory:gc:");

    #INCLUDE
    InstallFromManifest(":mozilla:config:mac:MANIFEST",                            "$distdirectory:config:");
    InstallFromManifest(":mozilla:config:mac:MANIFEST_config",                     "$distdirectory:config:");
    
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

    #// To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
    MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h", ":mozilla:dist:config:MacConfigInclude.h");

    InstallFromManifest(":mozilla:include:MANIFEST",                               "$distdirectory:include:");

    #INTL
    #CHARDET
    InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST_IDL",               "$distdirectory:idl:");
    InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST",                   "$distdirectory:chardet");

    #UCONV
    InstallFromManifest(":mozilla:intl:uconv:idl:MANIFEST_IDL",                    "$distdirectory:idl:");
    InstallFromManifest(":mozilla:intl:uconv:public:MANIFEST",                     "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvlatin:MANIFEST",                   "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvja:MANIFEST",                      "$distdirectory:uconv:");
#   InstallFromManifest(":mozilla:intl:uconv:ucvja2:MANIFEST",                     "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvtw:MANIFEST",                      "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvtw2:MANIFEST",                     "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvcn:MANIFEST",                      "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvko:MANIFEST",                      "$distdirectory:uconv:");
#   InstallFromManifest(":mozilla:intl:uconv:ucvth:MANIFEST",                      "$distdirectory:uconv:");
#   InstallFromManifest(":mozilla:intl:uconv:ucvvt:MANIFEST",                      "$distdirectory:uconv:");
    InstallFromManifest(":mozilla:intl:uconv:ucvibm:MANIFEST",                     "$distdirectory:uconv:");

    #UNICHARUTIL
    InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST",               "$distdirectory:unicharutil");
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

    #XPCOM
    InstallFromManifest(":mozilla:xpcom:base:MANIFEST_IDL",                        "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:io:MANIFEST_IDL",                          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:ds:MANIFEST_IDL",                          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:threads:MANIFEST_IDL",                     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:components:MANIFEST_IDL",                  "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpcom:components:MANIFEST_COMPONENTS",           "${dist_dir}Components:");

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
    InstallFromManifest(":mozilla:modules:libimg:src:MANIFEST",                    "$distdirectory:libimg:");
    InstallFromManifest(":mozilla:modules:libimg:public:MANIFEST",                 "$distdirectory:libimg:");
    InstallFromManifest(":mozilla:modules:libimg:public_com:MANIFEST",             "$distdirectory:libimg:");

    #PLUGIN
    InstallFromManifest(":mozilla:modules:plugin:nglsrc:MANIFEST",                 "$distdirectory:plugin:");
    InstallFromManifest(":mozilla:modules:plugin:public:MANIFEST",                 "$distdirectory:plugin:");
    InstallFromManifest(":mozilla:modules:oji:src:MANIFEST",                       "$distdirectory:oji:");
    InstallFromManifest(":mozilla:modules:oji:public:MANIFEST",                    "$distdirectory:oji:");
    
    #DB
    InstallFromManifest(":mozilla:db:mdb:public:MANIFEST",                         "$distdirectory:db:");
    InstallFromManifest(":mozilla:db:mork:build:MANIFEST",                         "$distdirectory:db:");

    #DBM
    InstallFromManifest(":mozilla:dbm:include:MANIFEST",                           "$distdirectory:dbm:");
    
    #URILOADER
    InstallFromManifest(":mozilla:uriloader:base:MANIFEST_IDL",                    "$distdirectory:idl:");
    InstallFromManifest(":mozilla:uriloader:exthandler:MANIFEST_IDL",              "$distdirectory:idl:");
    
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
    InstallFromManifest(":mozilla:netwerk:protocol:jar:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:protocol:res:public:MANIFEST_IDL",       "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:cache:public:MANIFEST",                  "$distdirectory:idl:");
    InstallFromManifest(":mozilla:netwerk:mime:public:MANIFEST",                   "$distdirectory:netwerk:");

    #SECURITY
    InstallFromManifest(":mozilla:extensions:psm-glue:public:MANIFEST",            "$distdirectory:idl:");
    InstallFromManifest(":mozilla:extensions:psm-glue:src:MANIFEST",               "$distdirectory:include:");

    InstallFromManifest(":mozilla:security:psm:lib:client:MANIFEST",               "$distdirectory:security:");
    InstallFromManifest(":mozilla:security:psm:lib:protocol:MANIFEST",             "$distdirectory:security:");
    
    #EXTENSIONS
    InstallFromManifest(":mozilla:extensions:cookie:MANIFEST",                     "$distdirectory:cookie:");
    InstallFromManifest(":mozilla:extensions:wallet:public:MANIFEST",              "$distdirectory:wallet:");

    #WEBSHELL
    InstallFromManifest(":mozilla:webshell:public:MANIFEST",                       "$distdirectory:webshell:");
    InstallFromManifest(":mozilla:webshell:public:MANIFEST_IDL",                   "$distdirectory:idl:");
    InstallFromManifest(":mozilla:webshell:tests:viewer:public:MANIFEST",          "$distdirectory:webshell:");

    #LAYOUT
    InstallFromManifest(":mozilla:layout:build:MANIFEST",                          "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:public:MANIFEST",                    "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:public:MANIFEST_IDL",                "$distdirectory:idl:");
    InstallFromManifest(":mozilla:layout:html:content:public:MANIFEST",            "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:document:src:MANIFEST",              "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:document:public:MANIFEST",           "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:style:public:MANIFEST",              "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",                 "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",                  "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:forms:public:MANIFEST",              "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:table:public:MANIFEST",              "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:src:MANIFEST",                       "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:events:public:MANIFEST",                  "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:events:src:MANIFEST",                     "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:xml:document:public:MANIFEST",            "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:xml:content:public:MANIFEST",             "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:xsl:document:src:MANIFEST_IDL",           "$distdirectory:idl:");
    if ($main::options{svg})
    {
        InstallFromManifest(":mozilla:layout:svg:base:public:MANIFEST",                        "$distdirectory:layout:");
    }
    InstallFromManifest(":mozilla:layout:xul:base:public:Manifest",                "$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:xbl:public:Manifest",                     "$distdirectory:layout:");

    #GFX
    InstallFromManifest(":mozilla:gfx:public:MANIFEST",                            "$distdirectory:gfx:");
    InstallFromManifest(":mozilla:gfx:idl:MANIFEST_IDL",                           "$distdirectory:idl:");

    #VIEW
    InstallFromManifest(":mozilla:view:public:MANIFEST",                           "$distdirectory:view:");

    #DOM
    InstallFromManifest(":mozilla:dom:public:MANIFEST",                            "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:MANIFEST_IDL",                        "$distdirectory:idl:");
    InstallFromManifest(":mozilla:dom:public:base:MANIFEST",                       "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:coreDom:MANIFEST",                    "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",                 "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:events:MANIFEST",                     "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:range:MANIFEST",                      "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:html:MANIFEST",                       "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:public:css:MANIFEST",                        "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",                         "$distdirectory:dom:");
    InstallFromManifest(":mozilla:dom:src:base:MANIFEST",                          "$distdirectory:dom:");

    #JSURL
    InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST_IDL",                     "$distdirectory:idl:");

    #HTMLPARSER
    InstallFromManifest(":mozilla:htmlparser:src:MANIFEST",                        "$distdirectory:htmlparser:");

    #EXPAT
    InstallFromManifest(":mozilla:expat:xmlparse:MANIFEST",                        "$distdirectory:expat:");
    
    #DOCSHELL
    InstallFromManifest(":mozilla:docshell:base:MANIFEST_IDL",                     "$distdirectory:idl:");

    #EMBEDDING
    InstallFromManifest(":mozilla:embedding:browser:webbrowser:MANIFEST_IDL",      "$distdirectory:idl:");

    #WIDGET
    InstallFromManifest(":mozilla:widget:public:MANIFEST",                         "$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:public:MANIFEST_IDL",                     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",                        "$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:timer:public:MANIFEST",                   "$distdirectory:widget:");

    #RDF
    InstallFromManifest(":mozilla:rdf:base:idl:MANIFEST",                          "$distdirectory:idl:");
    InstallFromManifest(":mozilla:rdf:base:public:MANIFEST",                       "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:util:public:MANIFEST",                       "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:content:public:MANIFEST",                    "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:datasource:public:MANIFEST",                 "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:build:MANIFEST",                             "$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:tests:domds:MANIFEST",                       "$distdirectory:idl:");
    
    #CHROME
    InstallFromManifest(":mozilla:rdf:chrome:public:MANIFEST",                     "$distdirectory:idl:");
    
    #EDITOR
    InstallFromManifest(":mozilla:editor:idl:MANIFEST",                            "$distdirectory:idl:");
    InstallFromManifest(":mozilla:editor:txmgr:idl:MANIFEST",                      "$distdirectory:idl:");
    InstallFromManifest(":mozilla:editor:public:MANIFEST",                         "$distdirectory:editor:");
    InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",                   "$distdirectory:editor:txmgr");
    InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",                  "$distdirectory:editor:txtsvc");
    
    #SILENTDL
    #InstallFromManifest(":mozilla:silentdl:MANIFEST",                             "$distdirectory:silentdl:");

    #XPINSTALL (the one and only!)
    InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                      "$distdirectory:xpinstall:");

    # XPFE COMPONENTS
    InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST",                "$distdirectory:xpfe:components");
    InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST_IDL",            "$distdirectory:idl:");

    my $dir = '';
    for $dir (qw(bookmarks find history related sample search shistory sidebar ucth urlbarhistory xfer))
    {
        InstallFromManifest(":mozilla:xpfe:components:$dir:public:MANIFEST_IDL",       "$distdirectory:idl:");
    }
    
    InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST",              "$distdirectory:xpfe:");
    InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST_IDL",          "$distdirectory:idl:");

    # directory
    InstallFromManifest(":mozilla:xpfe:components:directory:MANIFEST_IDL",         "$distdirectory:idl:");
    # regviewer
    InstallFromManifest(":mozilla:xpfe:components:regviewer:MANIFEST_IDL",     "$distdirectory:idl:");
    # autocomplete
    InstallFromManifest(":mozilla:xpfe:components:autocomplete:public:MANIFEST_IDL", "$distdirectory:idl:");

    # XPAPPS
    InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST",                  "$distdirectory:xpfe:");
    InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:xpfe:browser:public:MANIFEST_IDL",               "$distdirectory:idl:");

    # XML-RPC
    InstallFromManifest(":mozilla:extensions:xml-rpc:idl:MANIFEST_IDL",                "$distdirectory:idl:");

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
                                                     
    #LDAP
    if ($main::options{ldap})
    {
        InstallFromManifest(":mozilla:directory:c-sdk:ldap:include:MANIFEST",      "$distdirectory:directory:");
        InstallFromManifest(":mozilla:directory:xpcom:base:public:MANIFEST",       "$distdirectory:directory:");
        InstallFromManifest(":mozilla:directory:xpcom:base:public:MANIFEST_IDL",   "$distdirectory:idl:");
    }

    #XMLEXTRAS
    if ($main::options{xmlextras})
    {
        InstallFromManifest(":mozilla:extensions:xmlextras:base:public:MANIFEST_IDL", "$distdirectory:idl:");
        InstallFromManifest(":mozilla:extensions:xmlextras:soap:public:MANIFEST_IDL", "$distdirectory:idl:");
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
#// Build stub projects
#//--------------------------------------------------------------------------------------------------

sub BuildStubs()
{

    unless( $main::build{stubs} ) { return; }
    assertRightDirectory();

    my($distdirectory) = ":mozilla:dist";

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::CARBON ? "Carbon" : "";

    StartBuildModule("stubs");

    #//
    #// Clean projects
    #//
    BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp", "Stubs$C");

    # because ToolServer can fail mysteriously, explicitly detect failure here
    if (! -e "$distdirectory:client_stubs:NSStdLibStubs")
    {
        die "Error: failed to build NSStdLib stubs. Check your ToolServer installation\n";
    }
    
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
    BuildProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.mcp", "build all");
  
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
    	if ( ! -e $codewarrior_msl . "Lib:PPC:MSL C.PPC MPW(NL).Lib") {
        	print("MSL PPC MPW Lib not found... Let's build it.\n");
        	BuildProject($codewarrior_msl . "Project:PPC:MSL C.PPC MPW.mcp", "MSL C.PPC MPW");
    	}
        BuildOneProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.mcp", "xpt_link", 0, 0, 0);
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
    BuildIDLProject(":mozilla:xpcom:macbuild:XPCOMIDL.mcp",                         "xpcom");

    # necko
    BuildIDLProject(":mozilla:netwerk:macbuild:netwerkIDL.mcp","necko");
    BuildIDLProject(":mozilla:uriloader:macbuild:uriLoaderIDL.mcp",                 "uriloader");

    # psm glue
    BuildIDLProject(":mozilla:extensions:psm-glue:macbuild:psmglueIDL.mcp",         "psmglue"); 
    
    BuildIDLProject(":mozilla:modules:libpref:macbuild:libprefIDL.mcp",             "libpref");
    BuildIDLProject(":mozilla:modules:libutil:macbuild:libutilIDL.mcp",             "libutil");
    BuildIDLProject(":mozilla:modules:libjar:macbuild:libjarIDL.mcp",               "libjar");
    BuildIDLProject(":mozilla:modules:plugin:macbuild:pluginIDL.mcp",               "plugin");
    BuildIDLProject(":mozilla:modules:oji:macbuild:ojiIDL.mcp",                     "oji");
    BuildIDLProject(":mozilla:js:macbuild:XPConnectIDL.mcp",                        "xpconnect");
    BuildIDLProject(":mozilla:dom:macbuild:domIDL.mcp",                             "dom");

    BuildIDLProject(":mozilla:dom:src:jsurl:macbuild:JSUrlDL.mcp",                  "jsurl");
    
    BuildIDLProject(":mozilla:gfx:macbuild:gfxIDL.mcp",                             "gfx");
    BuildIDLProject(":mozilla:widget:macbuild:widgetIDL.mcp",                       "widget");
    BuildIDLProject(":mozilla:editor:macbuild:EditorIDL.mcp",                       "editor");
    BuildIDLProject(":mozilla:editor:txmgr:macbuild:txmgrIDL.mcp",                  "txmgr");
    BuildIDLProject(":mozilla:editor:txtsvc:macbuild:txtsvcIDL.mcp",                "txtsvc");
    BuildIDLProject(":mozilla:profile:macbuild:ProfileServicesIDL.mcp", "profileservices");
    BuildIDLProject(":mozilla:profile:pref-migrator:macbuild:prefmigratorIDL.mcp",  "prefm");
        
    BuildIDLProject(":mozilla:layout:macbuild:layoutIDL.mcp",                       "layout");

    BuildIDLProject(":mozilla:rdf:macbuild:RDFIDL.mcp",                             "rdf");
    BuildIDLProject(":mozilla:rdf:tests:domds:macbuild:DOMDataSourceIDL.mcp",       "domds");

    BuildIDLProject(":mozilla:rdf:chrome:build:chromeIDL.mcp",                      "chrome");
        
    BuildIDLProject(":mozilla:webshell:macbuild:webshellIDL.mcp",                   "webshell");
    BuildIDLProject(":mozilla:docshell:macbuild:docshellIDL.mcp",                   "docshell");
    BuildIDLProject(":mozilla:embedding:browser:macbuild:browserIDL.mcp",           "embeddingbrowser");

    BuildIDLProject(":mozilla:extensions:wallet:macbuild:walletIDL.mcp","wallet");
    BuildIDLProject(":mozilla:extensions:xml-rpc:macbuild:xml-rpcIDL.mcp","xml-rpc");
    BuildIDLProject(":mozilla:xpfe:components:bookmarks:macbuild:BookmarksIDL.mcp", "bookmarks");
    BuildIDLProject(":mozilla:xpfe:components:directory:DirectoryIDL.mcp",          "directory");
    BuildIDLProject(":mozilla:xpfe:components:regviewer:RegViewerIDL.mcp",          "regviewer");
    BuildIDLProject(":mozilla:xpfe:components:history:macbuild:historyIDL.mcp",     "history");
    BuildIDLProject(":mozilla:xpfe:components:shistory:macbuild:shistoryIDL.mcp",   "shistory");
    BuildIDLProject(":mozilla:xpfe:components:related:macbuild:RelatedIDL.mcp",     "related");
    BuildIDLProject(":mozilla:xpfe:components:search:macbuild:SearchIDL.mcp",       "search");
    BuildIDLProject(":mozilla:xpfe:components:macbuild:mozcompsIDL.mcp",            "mozcomps");
    BuildIDLProject(":mozilla:xpfe:components:timebomb:macbuild:timebombIDL.mcp",   "tmbm");
    BuildIDLProject(":mozilla:xpfe:components:urlbarhistory:macbuild:urlbarhistoryIDL.mcp", "urlbarhistory");
    BuildIDLProject(":mozilla:xpfe:components:autocomplete:macbuild:AutoCompleteIDL.mcp", "autocomplete");

    BuildIDLProject(":mozilla:xpfe:appshell:macbuild:appshellIDL.mcp",              "appshell");
    
    BuildIDLProject(":mozilla:xpfe:browser:macbuild:mozBrowserIDL.mcp",             "mozBrowser");
    
    BuildIDLProject(":mozilla:xpinstall:macbuild:xpinstallIDL.mcp",                 "xpinstall");

    BuildIDLProject(":mozilla:mailnews:base:macbuild:msgCoreIDL.mcp",               "mailnews");
    BuildIDLProject(":mozilla:mailnews:compose:macbuild:msgComposeIDL.mcp",         "MsgCompose");
    BuildIDLProject(":mozilla:mailnews:local:macbuild:msglocalIDL.mcp",             "MsgLocal");
    BuildIDLProject(":mozilla:mailnews:news:macbuild:msgnewsIDL.mcp",               "MsgNews");
    BuildIDLProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbookIDL.mcp",       "MsgAddrbook");
    BuildIDLProject(":mozilla:mailnews:absync:macbuild:abSyncIDL.mcp",              "AbSyncSvc");
    BuildIDLProject(":mozilla:mailnews:db:macbuild:msgDBIDL.mcp",                   "MsgDB");
    BuildIDLProject(":mozilla:mailnews:imap:macbuild:msgimapIDL.mcp",               "MsgImap");
    BuildIDLProject(":mozilla:mailnews:mime:macbuild:mimeIDL.mcp",                  "Mime");
    BuildIDLProject(":mozilla:mailnews:import:macbuild:msgImportIDL.mcp",           "msgImport");

    BuildIDLProject(":mozilla:caps:macbuild:CapsIDL.mcp",                           "caps");

    BuildIDLProject(":mozilla:intl:locale:macbuild:nsLocaleIDL.mcp",                "nsLocale");
    BuildIDLProject(":mozilla:intl:strres:macbuild:strresIDL.mcp",                  "nsIStringBundle");
    BuildIDLProject(":mozilla:intl:unicharutil:macbuild:unicharutilIDL.mcp",        "unicharutil");
    BuildIDLProject(":mozilla:intl:uconv:macbuild:uconvIDL.mcp",                    "uconv");
    BuildIDLProject(":mozilla:intl:chardet:macbuild:chardetIDL.mcp",                "chardet");

    if ($main::options{ldap})
    {
        BuildIDLProject(":mozilla:directory:xpcom:macbuild:mozldapIDL.mcp", "mozldap");
    }

    if ($main::options{xmlextras})
    {
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlextrasIDL.mcp", "xmlextras");
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

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::CARBON ? "Carbon" : "";
    my($P) = $main::PROFILE ? "Profil" : "";
    my($EssentialFiles) = $main::DEBUG ? ":mozilla:dist:viewer_debug:Essential Files:" : ":mozilla:dist:viewer:Essential Files:";

    #//
    #// Shared libraries
    #//
    if ( $main::CARBON )
    {
        if ( $main::CARBONLITE ) {
            BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "Carbon Interfaces (Lite)");
        }
        else {
            BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "Carbon Interfaces");       
        }
    }
    else
    {
        if ($main::UNIVERSAL_INTERFACES_VERSION >= 0x0330) {
    	    BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "MacOS Interfaces (3.3)");
        } else {
    	    BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "MacOS Interfaces");
	    }
    }
    
    #// Build all of the startup libraries, for Application, Component, and Shared Libraries. These are
    #// required for all subsequent libraries in the system.
    BuildProject(":mozilla:lib:mac:NSStartup:NSStartup.mcp",                           "NSStartup.all");
    
    BuildOneProjectWithOutput(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp", "NSRuntime$C$P$D.shlb", "NSRuntime$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",          "MoreFiles.o");
    
	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp", "MemAllocator$C$D.o");

    BuildOneProjectWithOutput(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp", "NSStdLib$C$D.shlb", "NSStdLib$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProjectWithOutput(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp", "NSPR20$C$D.shlb", "NSPR20$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

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

    StartBuildModule("common");

    #//
    #// Shared libraries
    #//

    BuildOneProject(":mozilla:modules:libreg:macbuild:libreg.mcp",              "libreg$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",                     "xpcom$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:js:macbuild:JavaScript.mcp",                      "JavaScript$D.shlb", 1, $main::ALIAS_SYM_FILES, 0); 
    BuildOneProject(":mozilla:js:macbuild:JSLoader.mcp",                        "JSLoader$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:js:macbuild:LiveConnect.mcp",                     "LiveConnect$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",                  "zlib$D.shlb", 1, $main::ALIAS_SYM_FILES, 0); 
    BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",                  "zlib$D.Lib", 0, 0, 0); 
    BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",              "libjar$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",              "libjar$D.Lib", 0, 0, 0);

    BuildOneProject(":mozilla:modules:oji:macbuild:oji.mcp",                    "oji$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:caps:macbuild:Caps.mcp",                          "Caps$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.mcp",            "libpref$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:js:macbuild:XPConnect.mcp",                       "XPConnect$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.mcp",            "libutil$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:db:mork:macbuild:mork.mcp",                       "Mork$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:dbm:macbuild:DBM.mcp",                            "DBM$D.o", 0, 0, 0);

    #// Static libraries
    # Static Libs
    BuildOneProject(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider.mcp", "mpfilelocprovider$D.o", 0, 0, 0);
    MakeAlias(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider$D.o", ":mozilla:dist:mpfilelocprovider:");
    
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

    BuildOneProject(":mozilla:jpeg:macbuild:JPEG.mcp",                          "JPEG$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:modules:libimg:macbuild:png.mcp",                 "png$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:modules:libimg:macbuild:libimg.mcp",              "libimg$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:modules:libimg:macbuild:gifdecoder.mcp",          "gifdecoder$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libimg:macbuild:pngdecoder.mcp",          "pngdecoder$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libimg:macbuild:jpgdecoder.mcp",          "jpgdecoder$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    # MNG
    if ($main::options{mng})
    {
        BuildOneProject(":mozilla:modules:libimg:macbuild:mng.mcp",                 "mng$D.o", 0, 0, 0);
        BuildOneProject(":mozilla:modules:libimg:macbuild:mngdecoder.mcp",          "mngdecoder$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }

    EndBuildModule("imglib");
} # imglib

    
#//--------------------------------------------------------------------------------------------------
#// Build international projects
#//--------------------------------------------------------------------------------------------------

sub BuildInternationalProjects()
{
    unless( $main::build{intl} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    StartBuildModule("intl");

    BuildOneProject(":mozilla:intl:chardet:macbuild:chardet.mcp",               "chardet$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:uconv.mcp",                   "uconv$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvlatin.mcp",                "ucvlatin$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja.mcp",                   "ucvja$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw.mcp",                   "ucvtw$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw2.mcp",                  "ucvtw2$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvcn.mcp",                   "ucvcn$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvko.mcp",                   "ucvko$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvibm.mcp",                  "ucvibm$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:unicharutil:macbuild:unicharutil.mcp",       "unicharutil$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:locale:macbuild:locale.mcp",                 "nslocale$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:lwbrk:macbuild:lwbrk.mcp",                   "lwbrk$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:strres:macbuild:strres.mcp",                 "strres$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

# BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja2.mcp",                    "ucvja2$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
# BuildOneProject(":mozilla:intl:uconv:macbuild:ucvvt.mcp",                 "ucvvt$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
# BuildOneProject(":mozilla:intl:uconv:macbuild:ucvth.mcp",                 "ucvth$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

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
    my($C) = $main::CARBON ? "Carbon" : "";

    my($Components) = $main::DEBUG ? ":mozilla:dist:viewer_debug:Components:" : ":mozilla:dist:viewer:Components:";

    StartBuildModule("necko");

    BuildOneProjectWithOutput(":mozilla:netwerk:macbuild:netwerk.mcp", "Necko$C$D.shlb", "Necko$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:netwerk:macbuild:netwerk2.mcp",                   "Necko2$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:dom:src:jsurl:macbuild:JSUrl.mcp",                "JSUrl$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
          
    EndBuildModule("necko");
}


#//--------------------------------------------------------------------------------------------------
#// Build Security projects
#//--------------------------------------------------------------------------------------------------

sub makeprops
{
    @ARGV = @_;

    do ":mozilla:security:psm:ui:makeprops.pl";
}

sub BuildSecurityProjects()
{
    unless( $main::build{security} && $main::options{psm}) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my $dist_dir = GetBinDirectory(); # the subdirectory with the libs and executable.

    StartBuildModule("security");

    BuildOneProject(":mozilla:security:nss:macbuild:NSS.mcp","NSS$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:security:psm:lib:macbuild:PSMClient.mcp","PSMClient$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:security:psm:lib:macbuild:PSMProtocol.mcp","PSMProtocol$D.o", 0, 0, 0); 
    BuildOneProject(":mozilla:security:psm:macbuild:PersonalSecurityMgr.mcp","PSM$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:extensions:psm-glue:macbuild:PSMGlue.mcp","PSMGlue$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

	 # make properties files for PSM User Interface
    my($src_dir) = ":mozilla:security:psm:ui:";

    opendir(DIR,$src_dir) || die "can't open directory $src_dir\n";
    my(@prop_files) = grep { /\.properties.in$/ } readdir(DIR);
    closedir DIR;
	my($psm_data_dir) = $dist_dir."psmdata:";
    mkdir($psm_data_dir, 0);
    my($dest_dir) = $psm_data_dir."UI:";
    mkdir($dest_dir, 0);
	my($file);
    foreach $file (@prop_files) {
        $file =~ /(.+\.properties)\.in$/;
        &makeprops($src_dir.$file, $dest_dir.$1);
    }
    
    
    my($doc_dir) = $psm_data_dir."doc:";
    mkdir($doc_dir, 0);
    opendir(DOC_DIR,":mozilla:security:psm:doc") || die ("Unable to open PSM doc directory");
    my(@doc_files);
    @doc_files = readdir(DOC_DIR);
    closedir(DOC_DIR);
    foreach $file (@doc_files) {
    	copy(":mozilla:security:psm:doc:".$file, $doc_dir.$file);
    }
     
    #Build the loadable module that contains the root certs.

	BuildOneProject(":mozilla:security:nss:macbuild:NSSckfw.mcp", "NSSckfw$D.o", 0, 0, 0);
	BuildOneProject(":mozilla:security:nss:macbuild:LoadableRoots.mcp", "NSSckbi$D.shlb", 0, $main::ALIAS_SYM_FILES, 0);
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

    StartBuildModule("browserutils");

    BuildOneProject(":mozilla:uriloader:macbuild:uriLoader.mcp",                "uriLoader$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:profile:macbuild:profile.mcp",                    "profile$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:profile:pref-migrator:macbuild:prefmigrator.mcp", "prefm$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:extensions:cookie:macbuild:cookie.mcp",           "Cookie$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.mcp",           "Wallet$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:walletviewers.mcp",    "WalletViewers$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:rdf:chrome:build:chrome.mcp",                     "ChomeRegistry$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:rdf:tests:domds:macbuild:DOMDataSource.mcp",      "DOMDataSource$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

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
    my($C) = $main::CARBON ? "Carbon" : "";
    my($dist_dir) = GetBinDirectory();
    
    StartBuildModule("nglayout");

    open(OUTPUT, ">:mozilla:layout:build:gbdate.h") || die "could not open gbdate.h";
    my($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;
    # localtime returns year minus 1900
    $year = $year + 1900;
    printf(OUTPUT "#define PRODUCT_VERSION \"%04d%02d%02d\"\n", $year, 1+$mon, $mday);
    close(OUTPUT);
    #//
    #// Build Layout projects
    #//

    BuildOneProject(":mozilla:expat:macbuild:expat.mcp",                        "expat$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",              "htmlparser$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:gfx:macbuild:gfx.mcp",                            "gfx$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:dom:macbuild:dom.mcp",                            "dom$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:modules:plugin:macbuild:plugin.mcp",              "plugin$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:layout:macbuild:layoutxsl.mcp",                   "layoutxsl$D.o", 0, 0, 0);
    if ($main::options{mathml})
    {
        BuildOneProject(":mozilla:layout:macbuild:layoutmathml.mcp",                "layoutmathml$D.o", 0, 0, 0);
    }
    else
    {
        BuildOneProject(":mozilla:layout:macbuild:layoutmathml.mcp",                "layoutmathml$D.o stub", 0, 0, 0);
    }
    if ($main::options{svg})
    {
        BuildOneProject(":mozilla:layout:macbuild:layoutsvg.mcp",                   "layoutsvg$D.o", 0, 0, 0);
    }
    else
    {
        BuildOneProject(":mozilla:layout:macbuild:layoutsvg.mcp",                   "layoutsvg$D.o stub", 0, 0, 0);
    }
    BuildOneProject(":mozilla:layout:macbuild:layout.mcp",                      "layout$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:view:macbuild:view.mcp",                          "view$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProjectWithOutput(":mozilla:widget:macbuild:widget.mcp",            "widget$C$D.shlb", "widget$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:docshell:macbuild:docshell.mcp",                  "docshell$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:webshell:embed:mac:RaptorShell.mcp",              "RaptorShell$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    #// XXX this is here because of a very TEMPORARY dependency
    BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",                            "RDFLibrary$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.mcp",                "xpinstall$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpinstall:macbuild:xpistub.mcp",                  "xpistub$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    if (!($main::PROFILE)) {
        BuildOneProject(":mozilla:xpinstall:wizard:mac:macbuild:MIW.mcp",           "Mozilla Installer$D", 0, 0, 0);
    }

    EndBuildModule("nglayout");
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
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("editor");

    BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.mcp",                 "EditorTxmgr$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:editor:txtsvc:macbuild:txtsvc.mcp",               "TextServices$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:editor:macbuild:editor.mcp",                      "EditorCore$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

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
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("viewer");

    BuildOneProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",            "viewer$D",  0, 0, 0);

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
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("embedding");

    BuildOneProject(":mozilla:embedding:browser:macbuild:webBrowser.mcp",       "webBrowser$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:embedding:base:macbuild:EmbedAPI.mcp", "EmbedAPI$D.o", 0, 0, 0);
    MakeAlias(":mozilla:embedding:base:macbuild:EmbedAPI$D.o", ":mozilla:dist:embedding:");

    if ($main::options{embedding_test} && !$main::CARBON)
    {
        if (-e GetCodeWarriorRelativePath("MacOS Support:PowerPlant"))
        {
            BuildOneProject(":mozilla:embedding:browser:powerplant:PPBrowser.mcp",            "PPEmbed$D",  0, 0, 0);
        }
        else
        {
            print("MacOS Support:PowerPlant does not exist - embedding sample will not be built\n");
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
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("xpapp");

    # Components
    BuildOneProject(":mozilla:xpfe:components:find:macbuild:FindComponent.mcp", "FindComponent$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:ucth:macbuild:ucth.mcp",  "ucth$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:xfer:macbuild:xfer.mcp",  "xfer$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:regviewer:RegViewer.mcp", "RegViewer$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:shistory:macbuild:shistory.mcp", "shistory$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:macbuild:appcomps.mcp", "appcomps$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    # Applications
    BuildOneProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",             "AppShell$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:browser:macbuild:mozBrowser.mcp",            "mozBrowser$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    EndBuildModule("xpapp");
}


#//--------------------------------------------------------------------------------------------------
#// Build Extensions Projects
#//--------------------------------------------------------------------------------------------------

sub BuildExtensionsProjects()
{
    unless( $main::build{extensions} ) { return; }
    assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = GetBinDirectory();

    StartBuildModule("extensions");

    my($chrome_subdir) = "Chrome:";
    my($chrome_dir) = "$dist_dir"."$chrome_subdir";
    my($packages_chrome_dir) = "$chrome_dir" . "packages:";

    # Chatzilla
    InstallResources(":mozilla:extensions:irc:js:lib:MANIFEST_COMPONENTS",     "${dist_dir}Components");
    
    # XML-RPC
    InstallFromManifest(":mozilla:extensions:xml-rpc:src:MANIFEST_COMPONENTS", "${dist_dir}Components");
    
    # Component viewer
    print "Need to make jar.mn file for cview\n";

    # Transformiix
    if ($main::options{transformiix})
    {
        BuildOneProject(":mozilla:extensions:transformiix:macbuild:transformiix.mcp", "transformiix$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

        print "Need to make a jar.mn file for transformiix\n";
    }
    
    # LDAP Client
    if ($main::options{ldap})
    {
        BuildOneProject(":mozilla:directory:c-sdk:ldap:libraries:macintosh:LDAPClient.mcp", "LDAPClient$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
        BuildOneProject(":mozilla:directory:xpcom:macbuild:mozldap.mcp", "mozldap$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # XML Extras
    if ($main::options{xmlextras})
    {
        BuildOneProject(":mozilla:extensions:xmlextras:macbuild:xmlextras.mcp", "xmlextras$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
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

    # as a temporary measure, make sure that the folder "MacOS Support:JNIHeaders" exists,
    # before we attempt to build the MRJ plugin. This will allow a gradual transition.
    if ( -e GetCodeWarriorRelativePath("MacOS Support:JNIHeaders"))
    {
	    my($plugin_path) = ":mozilla:plugin:oji:MRJ:plugin:";
	    my($project_path) = $plugin_path . "MRJPlugin.mcp";
	    my($xml_path) = $plugin_path . "MRJPlugin.xml";
	    my($project_modtime) = (-e $project_path ? GetFileModDate($project_path) : 0);
	    my($xml_modtime) = (-e $xml_path ? GetFileModDate($xml_path) : 0);

	    if ($xml_modtime > $project_modtime) {
	        print("MRJPlugin.mcp is out of date, reimporting from MRJPlugin.xml.\n");
	        # delete the old project file.
	        unlink($project_path);
	        # import the xml project.
	        ImportXMLProject(full_path_to($xml_path), full_path_to($project_path));
	    }

	    # Build MRJPlugin
	    BuildProject($project_path, "MRJPlugin");
	    # Build MRJPlugin.jar (if Java tools exist)
	    my($linker_path) = GetCodeWarriorRelativePath("CodeWarrior Plugins:Linkers:Java Linker");
	    if (-e $linker_path) {
	        print("CodeWarrior Java tools detected, building MRJPlugin.jar.\n");
	        BuildProject($project_path, "MRJPlugin.jar");
	    }
	    # Copy MRJPlugin, MRJPlugin.jar to appropriate plugins folder.
	    my($plugin_dist) = GetBinDirectory() . "Plug-ins:";
	    MakeAlias($plugin_path . "MRJPlugin", $plugin_dist);
	    MakeAlias($plugin_path . "MRJPlugin.jar", $plugin_dist);
	}

    # Build the Default Plug-in and place an alias in the appropriate plugins folder.
    my($plugin_path) = ":mozilla:modules:plugin:default:mac:";
    my($plugin_dist) = GetBinDirectory() . "Plug-ins:";

    BuildProject($plugin_path . "NullPlugin.mcp", "NullPluginPPC");
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

    StartBuildModule("mailnews");

    BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.mcp",                 "MsgUtil$D.lib", 0, 0, 0);
    BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.mcp",                      "mailnews$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.mcp",                "MsgCompose$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.mcp",                          "MsgDB$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.mcp",                    "MsgLocal$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:imap:macbuild:msgimap.mcp",                      "MsgImap$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:news:macbuild:msgnews.mcp",                      "MsgNews$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbook.mcp",              "MsgAddrbook$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:absync:macbuild:AbSync.mcp",                     "AbSyncSvc$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.mcp",                         "Mime$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:emitters:macbuild:mimeEmitter.mcp",         "mimeEmitter$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:vcard:macbuild:vcard.mcp",       "vcard$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:smimestub:macbuild:smime.mcp",   "smime$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:signstub:macbuild:signed.mcp",   "signed$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
#   BuildOneProject(":mozilla:mailnews:mime:cthandlers:calendar:macbuild:calendar.mcp", "calendar$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:macbuild:msgImport.mcp",                  "msgImport$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:text:macbuild:msgImportText.mcp",         "msgImportText$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:eudora:macbuild:msgImportEudora.mcp",     "msgImportEudora$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

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

    StartBuildModule("apprunner");

    BuildOneProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.mcp",           "apprunner$D", 0, 0, 1);

    # build tool to create Component Registry in release builds only.
    if (!($main::DEBUG)) {
        BuildOneProject(":mozilla:xpcom:tools:registry:macbuild:RegXPCOM.mcp", "RegXPCOM", 0, 0, 1);
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
    BuildNeckoProjects();
    BuildSecurityProjects();
    BuildBrowserUtilsProjects();        
    BuildInternationalProjects();
    BuildLayoutProjects();
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
