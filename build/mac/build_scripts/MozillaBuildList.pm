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
    InstallResources(":mozilla:modules:libpref:src:MANIFEST_PREFS",                    "$default_pref_dir", 0);
    if ($main::DEBUG) {
        InstallResources(":mozilla:modules:libpref:src:MANIFEST_DEBUG_PREFS",          "$default_pref_dir", 0);
    }
    InstallResources(":mozilla:modules:libpref:src:init:MANIFEST",                     "$default_pref_dir", 0);
    InstallResources(":mozilla:modules:libpref:src:mac:MANIFEST",                      "$default_pref_dir", 0);
    InstallResources(":mozilla:netwerk:base:public:MANIFEST_PREFS",                    "$default_pref_dir", 0);

    if ($main::options{inspector})
    {
      InstallResources(":mozilla:extensions:inspector:resources:content:prefs:MANIFEST", "$default_pref_dir", 0);
    }
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
    MakeAlias(":mozilla:layout:html:document:src:viewsource.css",                      "$resource_dir");
    MakeAlias(":mozilla:layout:html:document:src:arrow.gif",                           "$resource_dir"); 
    MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",            "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:charsetalias.properties",                       "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:maccharset.properties",                         "$resource_dir");
    MakeAlias(":mozilla:intl:uconv:src:charsetData.properties",                        "$resource_dir");
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

    # embedding UI
    InstallResources(":mozilla:embedding:components:ui:helperAppDlg:MANIFEST",             "$components_dir");

    # intl
    InstallResources(":mozilla:embedding:components:intl:MANIFEST",                        "$components_dir");
    
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
    }
    if ($main::options{ldap_experimental})
    {
      CreateJarFromManifest(":mozilla:directory:xpcom:tests:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{help})
    {
      CreateJarFromManifest(":mozilla:extensions:help:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{vixen})
    {
      CreateJarFromManifest(":mozilla:extensions:vixen:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{inspector})
    {
      CreateJarFromManifest(":mozilla:extensions:inspector:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{p3p})
    {
      CreateJarFromManifest(":mozilla:extensions:p3p:resources:jar.mn", $chrome_dir, \%jars);
    }
    if ($main::options{jsd} && $main::options{venkman})
    {
      CreateJarFromManifest(":mozilla:extensions:venkman:resources:jar.mn", $chrome_dir, \%jars);
    }
    
    CreateJarFromManifest(":mozilla:accessible:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:caps:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:docshell:base:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:editor:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:browser:chrome:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:browser:chrome:locale:en-US:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:cookie:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:extensions:wallet:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:intl:uconv:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:htmlparser:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:forms:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:layout:html:base:src:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:mailnews:jar.mn", $chrome_dir, \%jars);
    if ($main::options{smime}) {
    	CreateJarFromManifest(":mozilla:mailnews:extensions:smime:jar.mn", $chrome_dir, \%jars);
    }
    CreateJarFromManifest(":mozilla:netwerk:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:profile:pref-migrator:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:profile:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:search:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:sidebar:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:themes:classic:communicator:bookmarks:mac:jar.mn", , $chrome_dir, \%jars);
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
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:locale:en-US:unix:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:locale:en-US:win:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:browser:resources:locale:en-US:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:communicator:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:communicator:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:bookmarks:resources:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:unix:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:win:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:locale:en-US:unix:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:locale:en-US:win:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpfe:global:resources:locale:en-US:mac:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:xpinstall:res:jar.mn", $chrome_dir, \%jars);
    CreateJarFromManifest(":mozilla:embedding:components:ui:jar.mn", $chrome_dir, \%jars);

    if ($main::options{psm}) {
    	CreateJarFromManifest(":mozilla:security:manager:ssl:resources:jar.mn", $chrome_dir, \%jars);
    	CreateJarFromManifest(":mozilla:security:manager:pki:resources:jar.mn", $chrome_dir, \%jars);
    	InstallFromManifest(":mozilla:security:manager:ssl:src:MANIFEST_NSSIFAIL", "$resource_dir");
    }
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
    if ($main::options{mathml})
    {
        InstallFromManifest(":mozilla:intl:uconv:ucvmath:MANIFEST",                "$distdirectory:uconv:");
    }

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

    if ($main::options{useimg2}) {
	    #GFX2
	    InstallFromManifest(":mozilla:gfx2:public:MANIFEST",                            "$distdirectory:gfx2:");
	    InstallFromManifest(":mozilla:gfx2:public:MANIFEST_IDL",                        "$distdirectory:idl:");
	    
	    #LIBIMG2
	    InstallFromManifest(":mozilla:modules:libpr0n:public:MANIFEST_IDL",            "$distdirectory:libimg2:");
	    InstallFromManifest(":mozilla:modules:libpr0n:decoders:icon:MANIFEST_IDL",     "$distdirectory:icondecoder:");
    }
    
    #PLUGIN
    InstallFromManifest(":mozilla:modules:plugin:base:src:MANIFEST",               "$distdirectory:plugin:");
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
    InstallFromManifest(":mozilla:netwerk:mime:public:MANIFEST",                   "$distdirectory:netwerk:");

    #EXTENSIONS
    InstallFromManifest(":mozilla:extensions:cookie:MANIFEST_IDL",          			 "$distdirectory:idl:");
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
    InstallFromManifest(":mozilla:content:html:content:src:MANIFEST",              "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:document:public:MANIFEST",          "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:document:src:MANIFEST",             "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:style:public:MANIFEST",             "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:html:style:src:MANIFEST",                "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:shared:public:MANIFEST",                 "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xbl:public:MANIFEST",                    "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xml:content:public:MANIFEST",            "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xml:document:public:MANIFEST",           "$distdirectory:content:");
    InstallFromManifest(":mozilla:content:xsl:document:src:MANIFEST_IDL",          "$distdirectory:idl:");
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
    if ($main::options{svg})
    {
        InstallFromManifest(":mozilla:layout:svg:base:public:MANIFEST",                        "$distdirectory:layout:");
    }
    InstallFromManifest(":mozilla:layout:xul:base:public:Manifest",                "$distdirectory:layout:");

    #GFX
    InstallFromManifest(":mozilla:gfx:public:MANIFEST",                            "$distdirectory:gfx:");
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
    InstallFromManifest(":mozilla:dom:public:idl:xul:MANIFEST_IDL",                "$distdirectory:idl:");
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

    #WIDGET
    InstallFromManifest(":mozilla:widget:public:MANIFEST",                         "$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:public:MANIFEST_IDL",                     "$distdirectory:idl:");
    InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",                        "$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:timer:public:MANIFEST",                   "$distdirectory:widget:");

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
    InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST_IDL",              "$distdirectory:idl:");
    InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",                  "$distdirectory:editor:txtsvc");
    
    #SILENTDL
    #InstallFromManifest(":mozilla:silentdl:MANIFEST",                             "$distdirectory:silentdl:");

    #XPINSTALL (the one and only!)
    InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                      "$distdirectory:xpinstall:");
    InstallFromManifest(":mozilla:xpinstall:cleanup:MANIFEST",                      "$distdirectory:xpinstall:");

    my $dir = '';
    for $dir (qw(bookmarks find history related search shistory sidebar urlbarhistory xfer))
    {
        InstallFromManifest(":mozilla:xpfe:components:$dir:public:MANIFEST_IDL",       "$distdirectory:idl:");
    }
    
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

    #LDAP
    if ($main::options{ldap})
    {
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
    
    #TRANSFORMIIX
    if ($main::options{transformiix})
    {
        InstallFromManifest(":mozilla:extensions:transformiix:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    #XMLEXTRAS
    if ($main::options{xmlextras})
    {
        InstallFromManifest(":mozilla:extensions:xmlextras:base:public:MANIFEST_IDL", "$distdirectory:idl:");
    }
    if ($main::options{soap})
    {
        InstallFromManifest(":mozilla:extensions:xmlextras:soap:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    #DOCUMENT INSPECTOR
    if ($main::options{inspector})
    {
        InstallFromManifest(":mozilla:extensions:inspector:base:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    #P3P
    if ($main::options{p3p})
    {
        InstallFromManifest(":mozilla:extensions:p3p:public:MANIFEST", "$distdirectory:idl:");
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

    # $C becomes a component of target names for selecting either the Carbon or non-Carbon target of a project
    my($C) = $main::options{carbon} ? "Carbon" : "";

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
	# but not when targeting Carbon as Pro 6 doesn't have a MSL C.PPC MPW(NL).Lib, or project to build it
    if ($main::options{xptlink}  && !$main::options{carbon} )
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
    BuildIDLProject(":mozilla:uriloader:macbuild:uriLoaderIDL.mcp",                 "uriLoader");
    BuildIDLProject(":mozilla:netwerk:macbuild:cacheIDL.mcp", "cache");
	
    if ($main::options{psm}) {
    	BuildIDLProject(":mozilla:security:manager:ssl:macbuild:pipnssIDL.mcp",         "pipnss");
    	BuildIDLProject(":mozilla:security:manager:pki:macbuild:pippkiIDL.mcp",         "pippki");    	
    }
    
    BuildIDLProject(":mozilla:modules:libpref:macbuild:libprefIDL.mcp",             "libpref");
    BuildIDLProject(":mozilla:modules:libutil:macbuild:libutilIDL.mcp",             "libutil");
    BuildIDLProject(":mozilla:modules:libjar:macbuild:libjarIDL.mcp",               "libjar");
    
	if ($main::options{useimg2}) {
	    BuildIDLProject(":mozilla:gfx2:macbuild:gfx2IDL.mcp",                       "gfx2");      
	    BuildIDLProject(":mozilla:modules:libpr0n:macbuild:libimg2IDL.mcp",         "libimg2");
	    BuildIDLProject(":mozilla:modules:libpr0n:macbuild:icondecoderIDL.mcp",         "icondecoder");
    }
    
    BuildIDLProject(":mozilla:modules:plugin:base:macbuild:pluginIDL.mcp",          "plugin");
    BuildIDLProject(":mozilla:modules:oji:macbuild:ojiIDL.mcp",                     "oji");
    BuildIDLProject(":mozilla:js:macbuild:XPConnectIDL.mcp",                        "xpconnect");
    if ($main::options{xpctools}) {
        BuildIDLProject(":mozilla:js:macbuild:XPCToolsIDL.mcp",                     "xpctools");
    }
    BuildIDLProject(":mozilla:dom:macbuild:domIDL.mcp",                             "dom");
    BuildIDLProject(":mozilla:dom:macbuild:dom_baseIDL.mcp",                        "dom_base");
    BuildIDLProject(":mozilla:dom:macbuild:dom_coreIDL.mcp",                        "dom_core");
    BuildIDLProject(":mozilla:dom:macbuild:dom_cssIDL.mcp",                         "dom_css");
    BuildIDLProject(":mozilla:dom:macbuild:dom_eventsIDL.mcp",                      "dom_events");
    BuildIDLProject(":mozilla:dom:macbuild:dom_htmlIDL.mcp",                        "dom_html");
    BuildIDLProject(":mozilla:dom:macbuild:dom_rangeIDL.mcp",                       "dom_range");
    BuildIDLProject(":mozilla:dom:macbuild:dom_traversalIDL.mcp",                   "dom_traversal");
    BuildIDLProject(":mozilla:dom:macbuild:dom_stylesheetsIDL.mcp",                 "dom_stylesheets");
    BuildIDLProject(":mozilla:dom:macbuild:dom_viewsIDL.mcp",                       "dom_views");
    BuildIDLProject(":mozilla:dom:macbuild:dom_xblIDL.mcp",                         "dom_xbl");
    BuildIDLProject(":mozilla:dom:macbuild:dom_xulIDL.mcp",                         "dom_xul");

    BuildIDLProject(":mozilla:dom:src:jsurl:macbuild:JSUrlDL.mcp",                  "jsurl");
    
    BuildIDLProject(":mozilla:gfx:macbuild:gfxIDL.mcp",                             "gfx");
    BuildIDLProject(":mozilla:widget:macbuild:widgetIDL.mcp",                       "widget");
    BuildIDLProject(":mozilla:editor:macbuild:EditorIDL.mcp",                       "editor");
    BuildIDLProject(":mozilla:editor:txmgr:macbuild:txmgrIDL.mcp",                  "txmgr");
    BuildIDLProject(":mozilla:editor:txtsvc:macbuild:txtsvcIDL.mcp",                "txtsvc");
    BuildIDLProject(":mozilla:profile:macbuild:ProfileServicesIDL.mcp", "profileservices");
    BuildIDLProject(":mozilla:profile:pref-migrator:macbuild:prefmigratorIDL.mcp",  "prefm");
        
    BuildIDLProject(":mozilla:content:macbuild:contentIDL.mcp",                       "content");

    BuildIDLProject(":mozilla:layout:macbuild:layoutIDL.mcp",                       "layout");

    if ($main::options{accessible})
    {
        BuildIDLProject(":mozilla:accessible:macbuild:accessibleIDL.mcp",           "accessible"); 
    }

    BuildIDLProject(":mozilla:rdf:macbuild:RDFIDL.mcp",                             "rdf");

    BuildIDLProject(":mozilla:rdf:chrome:build:chromeIDL.mcp",                      "chrome");
        
    BuildIDLProject(":mozilla:webshell:macbuild:webshellIDL.mcp",                   "webshell");
    BuildIDLProject(":mozilla:docshell:macbuild:docshellIDL.mcp",                   "docshell");
    BuildIDLProject(":mozilla:embedding:base:macbuild:EmbedIDL.mcp",                "EmbedBase");
    BuildIDLProject(":mozilla:embedding:browser:macbuild:browserIDL.mcp",           "embeddingbrowser");
    BuildIDLProject(":mozilla:embedding:components:build:macbuild:EmbedComponentsIDL.mcp", "EmbedComponents");

    BuildIDLProject(":mozilla:extensions:cookie:macbuild:cookieIDL.mcp",						"cookie");
    BuildIDLProject(":mozilla:extensions:wallet:macbuild:walletIDL.mcp","wallet");
    BuildIDLProject(":mozilla:extensions:xml-rpc:macbuild:xml-rpcIDL.mcp","xml-rpc");
    BuildIDLProject(":mozilla:xpfe:components:bookmarks:macbuild:BookmarksIDL.mcp", "bookmarks");
    BuildIDLProject(":mozilla:xpfe:components:directory:DirectoryIDL.mcp",          "Directory");
    BuildIDLProject(":mozilla:xpfe:components:regviewer:RegViewerIDL.mcp",          "RegViewer");
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

    if ($main::options{ldap})
    {
        BuildIDLProject(":mozilla:directory:xpcom:macbuild:mozldapIDL.mcp", "mozldap");
        BuildIDLProject(":mozilla:xpfe:components:autocomplete:macbuild:ldapAutoCompleteIDL.mcp", "ldapAutoComplete");
    }

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
    if ($main::options{smime}) {
    	BuildIDLProject(":mozilla:mailnews:extensions:smime:macbuild:msgsmimeIDL.mcp",  "msgsmime");
    }

    BuildIDLProject(":mozilla:caps:macbuild:CapsIDL.mcp",                           "caps");

    BuildIDLProject(":mozilla:intl:locale:macbuild:nsLocaleIDL.mcp",                "nsLocale");
    BuildIDLProject(":mozilla:intl:strres:macbuild:strresIDL.mcp",                  "nsIStringBundle");
    BuildIDLProject(":mozilla:intl:unicharutil:macbuild:unicharutilIDL.mcp",        "unicharutil");
    BuildIDLProject(":mozilla:intl:uconv:macbuild:uconvIDL.mcp",                    "uconv");
    BuildIDLProject(":mozilla:intl:chardet:macbuild:chardetIDL.mcp",                "chardet");

    if ($main::options{transformiix})
    {
        BuildIDLProject(":mozilla:extensions:transformiix:macbuild:transformiixIDL.mcp", "transformiix");
    }

    if ($main::options{xmlextras})
    {
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlextrasIDL.mcp", "xmlextras");
    }
    if ($main::options{soap})
    {
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlsoapIDL.mcp", "xmlsoap");
    }

    if ($main::options{vixen})
    {
        BuildIDLProject(":mozilla:extensions:vixen:macbuild:vixenIDL.mcp", "vixen");
    }

    if ($main::options{inspector})
    {
        BuildIDLProject(":mozilla:extensions:inspector:macbuild:inspectorIDL.mcp", "inspector");
    }

    if ($main::options{p3p})
    {
        BuildIDLProject(":mozilla:extensions:p3p:macbuild:p3pIDL.mcp", "p3p");
    }

    if ($main::options{jsd})
    {
        BuildIDLProject(":mozilla:js:jsd:macbuild:jsdIDL.mcp", "jsdservice");
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
    my($EssentialFiles) = $main::DEBUG ? ":mozilla:dist:viewer_debug:Essential Files:" : ":mozilla:dist:viewer:Essential Files:";

    #//
    #// Shared libraries
    #//
    if ( $main::options{carbon} )
    {
        BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "Carbon Interfaces");       
        BuildProject(":mozilla:lib:mac:InterfaceLib:InterfaceOSX.mcp",         "MacOS X Interfaces");       
    }
    else
    {
        if ($main::UNIVERSAL_INTERFACES_VERSION >= 0x0330) {
    	    BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "MacOS Interfaces (3.3)");
        } else {
    	    BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "MacOS Interfaces");
	    }
        BuildProject(":mozilla:lib:mac:InterfaceLib:InterfaceOSX.mcp",             "MacOS Interfaces");       
    }
    
    #// Build all of the startup libraries, for Application, Component, and Shared Libraries. These are
    #// required for all subsequent libraries in the system.
    BuildProject(":mozilla:lib:mac:NSStartup:NSStartup.mcp",                           "NSStartup.all");
    
    BuildOneProjectWithOutput(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp", "NSRuntime$C$P$D.shlb", "NSRuntime$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",          "MoreFiles$D.o");

    if ($main::GC_LEAK_DETECTOR && !$main::options{carbon}) {
        BuildProject(":mozilla:gc:boehm:macbuild:gc.mcp",                    "gc.ppc.lib");
        MakeAlias(":mozilla:gc:boehm:macbuild:gc.PPC.lib",                   ":mozilla:dist:gc:gc.PPC.lib");
    	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp", "MemAllocatorGC.o");
    } else {
    	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp", "MemAllocator$C$D.o");
    }

    BuildOneProjectWithOutput(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp", "NSStdLib$C$D.shlb", "NSStdLib$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    if ($main::DEBUG && $main::options{carbon}) {
        BuildOneProject(":mozilla:lib:mac:NSStdLib:NSConsole.mcp", "NSConsoleDebug.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    }

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
    my $dist_dir = GetBinDirectory();
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";

    StartBuildModule("common");

    #//
    #// Static libraries
    #//

    BuildProject(":mozilla:string:macbuild:string.mcp",                      "string$D.o");
    MakeAlias(":mozilla:string:macbuild:string$D.o", ":mozilla:dist:string:");

    BuildProject(":mozilla:intl:unicharutil:macbuild:UnicharUtilsStaticLib.mcp", "UnicharUtilsStatic$D.o");
    MakeAlias(":mozilla:intl:unicharutil:macbuild:UnicharUtilsStatic$D.o", ":mozilla:dist:client_stubs:");

    #//
    #// Shared libraries
    #//

    BuildOneProject(":mozilla:modules:libreg:macbuild:libreg.mcp",              "libreg$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",                     "xpcom$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:js:macbuild:JavaScript.mcp",                      "JavaScript$D.shlb", 1, $main::ALIAS_SYM_FILES, 0); 
    BuildOneProject(":mozilla:js:macbuild:JSLoader.mcp",                        "JSLoader$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:js:macbuild:LiveConnect.mcp",                     "LiveConnect$D.$S", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",                  "zlib$D.$S", 1, $main::ALIAS_SYM_FILES, 0); 
    BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",                     "zlib$D.Lib"); 
    BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",              "libjar$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildProject(":mozilla:modules:libjar:macbuild:libjar.mcp",                 "libjar$D.Lib");

    BuildOneProject(":mozilla:modules:oji:macbuild:oji.mcp",                    "oji$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:caps:macbuild:Caps.mcp",                          "Caps$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.mcp",            "libpref$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:js:macbuild:XPConnect.mcp",                       "XPConnect$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if ($main::options{xpctools}) {
        BuildOneProject(":mozilla:js:macbuild:XPCTools.mcp",                    "XPCTools$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }
    BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.mcp",            "libutil$D.$S", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:db:mork:macbuild:mork.mcp",                       "Mork$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildProject(":mozilla:dbm:macbuild:DBM.mcp",                               "DBM$D.o");

    #// Static libraries
    # Static Libs
    BuildProject(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider.mcp", "mpfilelocprovider$D.o");
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

    BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",                          "JPEG$D.o");
    BuildProject(":mozilla:modules:libimg:macbuild:png.mcp",                 "png$D.o");

    # MNG
    if ($main::options{mng})
    {
        BuildProject(":mozilla:modules:libimg:macbuild:mng.mcp",                 "mng$D.o");
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
        BuildOneProject(":mozilla:gfx2:macbuild:gfx2.mcp",                          "gfx2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:libimg2.mcp",            "libimg2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:pngdecoder2.mcp",        "pngdecoder2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:gifdecoder2.mcp",        "gifdecoder2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:jpegdecoder2.mcp",       "jpegdecoder2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:icondecoder.mcp",        "icondecoder$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        BuildOneProject(":mozilla:modules:libpr0n:macbuild:bmpdecoder.mcp",         "bmpdecoder$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
        
        # MNG
        if ($main::options{mng})
        {
            #BuildProject(":mozilla:modules:libimg:macbuild:mng.mcp",                 "mng$D.o", 0, 0, 0);
            #BuildOneProject(":mozilla:modules:libimg:macbuild:mngdecoder.mcp",          "mngdecoder$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
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

    BuildOneProject(":mozilla:intl:chardet:macbuild:chardet.mcp",               "chardet$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:uconv.mcp",                   "uconv$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvlatin.mcp",                "ucvlatin$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja.mcp",                   "ucvja$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw.mcp",                   "ucvtw$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw2.mcp",                  "ucvtw2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvcn.mcp",                   "ucvcn$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvko.mcp",                   "ucvko$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:uconv:macbuild:ucvibm.mcp",                  "ucvibm$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if ($main::options{mathml})
    {
        BuildOneProject(":mozilla:intl:uconv:macbuild:ucvmath.mcp",             "ucvmath$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    BuildOneProject(":mozilla:intl:unicharutil:macbuild:unicharutil.mcp",       "unicharutil$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:locale:macbuild:locale.mcp",                 "nslocale$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:lwbrk:macbuild:lwbrk.mcp",                   "lwbrk$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:intl:strres:macbuild:strres.mcp",                 "strres$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

# BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja2.mcp",                    "ucvja2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
# BuildOneProject(":mozilla:intl:uconv:macbuild:ucvvt.mcp",                 "ucvvt$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
# BuildOneProject(":mozilla:intl:uconv:macbuild:ucvth.mcp",                 "ucvth$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

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

    BuildOneProjectWithOutput(":mozilla:netwerk:macbuild:netwerk.mcp", "Necko$C$D.$S", "Necko$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:netwerk:macbuild:netwerk2.mcp",          "Necko2$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:netwerk:macbuild:cache.mcp",         "Cache$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:dom:src:jsurl:macbuild:JSUrl.mcp",       "JSUrl$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
          
    InstallFromManifest(":mozilla:netwerk:base:src:MANIFEST_COMPONENTS", "${dist_dir}Components:");

    EndBuildModule("necko");
}


#//--------------------------------------------------------------------------------------------------
#// Build Security projects
#//--------------------------------------------------------------------------------------------------

sub BuildSecurityProjects()
{
    unless( $main::build{security} && $main::options{psm}) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    # $S becomes the target suffix for the shared lib or static build.
    my($S) = $main::options{static_build} ? "o" : "shlb";
    my $dist_dir = GetBinDirectory(); # the subdirectory with the libs and executable.

    StartBuildModule("security");

    BuildProject(":mozilla:security:nss:macbuild:NSS.mcp","NSS$D.o");
    BuildOneProject(":mozilla:security:manager:ssl:macbuild:PIPNSS.mcp", "PIPNSS$D.$S",  1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:security:manager:pki:macbuild:PIPPKI.mcp", "PIPPKI$D.$S",  1, $main::ALIAS_SYM_FILES, 1); 
    
	if ($main::options{static_build}) {
		BuildOneProject(":mozilla:modules:staticmod:macbuild:cryptoComponent.mcp",    "MetaCrypto$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
	}

	#Build the loadable module that contains the root certs. This is always built as a shared lib, even in the static build.
    BuildProject(":mozilla:security:nss:macbuild:NSSckfw.mcp", "NSSckfw$D.o");
    BuildProject(":mozilla:security:nss:macbuild:LoadableRoots.mcp", "NSSckbi$D.shlb");
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

    BuildOneProject(":mozilla:uriloader:macbuild:uriLoader.mcp",                "uriLoader$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:profile:macbuild:profile.mcp",                    "profile$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:profile:pref-migrator:macbuild:prefmigrator.mcp", "prefm$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:extensions:cookie:macbuild:cookie.mcp",           "Cookie$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.mcp",           "Wallet$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:walletviewers.mcp",    "WalletViewers$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:universalchardet:macbuild:Universalchardet.mcp", "Universalchardet$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:rdf:chrome:build:chrome.mcp",                     "ChomeRegistry$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
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
    my($EssentialFiles) = $main::DEBUG ? ":mozilla:dist:viewer_debug:Essential Files:" : ":mozilla:dist:viewer:Essential Files:";
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

    BuildProject(":mozilla:expat:macbuild:expat.mcp",                           "expat$D.o");
    BuildOneProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",              "htmlparser$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProjectWithOutput(":mozilla:gfx:macbuild:gfx.mcp",                  "gfx$C$D.$S", "gfx$D.$S", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:dom:macbuild:dom.mcp",                            "dom$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:plugin:base:macbuild:plugin.mcp",          "plugin$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    # Static library shared between different content- and layout-related libraries
    BuildProject(":mozilla:content:macbuild:contentshared.mcp",                 "contentshared$D.o");
    MakeAlias(":mozilla:content:macbuild:contentshared$D.o",                    ":mozilla:dist:content:");

    BuildOneProject(":mozilla:content:macbuild:content.mcp",                    "content$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if ($main::options{mathml})
    {
        BuildProject(":mozilla:layout:macbuild:layoutmathml.mcp",                "layoutmathml$D.o");
    }
    else
    {
        BuildProject(":mozilla:layout:macbuild:layoutmathml.mcp",                "layoutmathml$D.o stub");
    }
    if ($main::options{svg})
    {
        BuildProject(":mozilla:layout:macbuild:layoutsvg.mcp",                   "layoutsvg$D.o");
    }
    else
    {
        BuildProject(":mozilla:layout:macbuild:layoutsvg.mcp",                   "layoutsvg$D.o stub");
    }
    BuildOneProject(":mozilla:layout:macbuild:layout.mcp",                      "layout$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:view:macbuild:view.mcp",                          "view$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProjectWithOutput(":mozilla:widget:macbuild:widget.mcp",            "widget$C$D.$S", "widget$D.$S", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:docshell:macbuild:docshell.mcp",                  "docshell$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:webshell:embed:mac:RaptorShell.mcp",              "RaptorShell$D.$S", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",                            "RDFLibrary$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.mcp",                "xpinstall$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    if (!$main::options{carbon}) {
        BuildOneProject(":mozilla:xpinstall:cleanup:macbuild:XPICleanup.mcp",       "XPICleanup$D", 1, $main::ALIAS_SYM_FILES, 0);
        InstallFromManifest(":mozilla:xpinstall:cleanup:MANIFEST_CMESSAGE",         "$resource_dir");
    }
    BuildOneProject(":mozilla:xpinstall:macbuild:xpistub.mcp",                  "xpistub$D.$S", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:xpinstall:wizard:libxpnet:macbuild:xpnet.mcp",    "xpnet$D.Lib", 0, 0, 0);
    if (!($main::PROFILE)) {
        BuildOneProject(":mozilla:xpinstall:wizard:mac:macbuild:MIW.mcp",           "Mozilla Installer$D", 0, 0, 0);
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
      BuildOneProject(":mozilla:accessible:macbuild:accessible.mcp",   "accessible$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
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

    BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.mcp",                 "EditorTxmgr$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:editor:txtsvc:macbuild:txtsvc.mcp",               "TextServices$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    # note: only build one of the following targets
    BuildOneProject(":mozilla:editor:macbuild:editor.mcp",                      "htmleditor$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
#    BuildOneProject(":mozilla:editor:macbuild:editor.mcp",                      "texteditor$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:editor:macbuild:composer.mcp",                    "Composer$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

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
  		BuildProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",            "viewer$C$D");
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

    BuildOneProject(":mozilla:embedding:components:build:macbuild:EmbedComponents.mcp",       "EmbedComponents$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:embedding:browser:macbuild:webBrowser.mcp",       "webBrowser$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

    BuildProject(":mozilla:embedding:base:macbuild:EmbedAPI.mcp", "EmbedAPI$D.o");
    MakeAlias(":mozilla:embedding:base:macbuild:EmbedAPI$D.o", ":mozilla:dist:embedding:");

    if ((!$main::options{carbon} && $main::options{embedding_test}) || ($main::options{carbon} && $main::options{embedding_test_carbon}))
    {
    	my($PowerPlantPath) = $main::options{carbon} ? "Carbon Support:PowerPlant" : "MacOS Support:PowerPlant";
        if (-e GetCodeWarriorRelativePath($PowerPlantPath))
        {
		    if (! $main::options{"static_build"})
		    {
        	    # Build PowerPlant and export the lib and the precompiled header
                BuildOneProject(":mozilla:lib:mac:PowerPlant:PowerPlant.mcp", "PowerPlant$C$D.o",  0, 0, 0);
                MakeAlias(":mozilla:lib:mac:PowerPlant:PowerPlant$C$D.o", ":mozilla:dist:mac:powerplant:");
                MakeAlias(":mozilla:lib:mac:PowerPlant:pch:PPHeaders$D" . "_pch", ":mozilla:dist:mac:powerplant:");
            
                BuildOneProject(":mozilla:embedding:browser:powerplant:PPBrowser.mcp", "PPEmbed$C$D",  0, 0, 0);
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
    BuildOneProject(":mozilla:xpfe:components:find:macbuild:FindComponent.mcp", "FindComponent$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:xfer:macbuild:xfer.mcp",  "xfer$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:regviewer:RegViewer.mcp", "RegViewer$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:shistory:macbuild:shistory.mcp", "shistory$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:components:macbuild:appcomps.mcp", "appcomps$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    InstallFromManifest(":mozilla:xpfe:appshell:src:MANIFEST_COMPONENTS", "${dist_dir}Components:");

    # Applications
    BuildOneProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",             "AppShell$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpfe:browser:macbuild:mozBrowser.mcp",            "mozBrowser$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

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
        BuildOneProject(":mozilla:extensions:transformiix:macbuild:transformiix.mcp", "transformiix$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # LDAP Client
    if ($main::options{ldap})
    {
        my($experi) = $main::options{ldap_experimental} ? " experi" : "";

        BuildOneProjectWithOutput(":mozilla:directory:c-sdk:ldap:libraries:macintosh:LDAPClient.mcp", "LDAPClient$C$D.shlb", "LDAPClient$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
        BuildOneProjectWithOutput(":mozilla:directory:xpcom:macbuild:mozldap.mcp", "mozldap$D.$S$experi", "mozldap$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

        if ($main::options{ldap_experimental})
        {
            InstallResources(":mozilla:directory:xpcom:datasource:MANIFEST_COMPONENTS", "$components_dir");
        }
    }
    else
    {
        # build a project that outputs a dummy LDAPClient lib so that later projects (e.g. apprunner) have something
        # to link against. This is really only needed for the static build, but there is no harm in building it anyway.
        BuildOneProject(":mozilla:directory:xpcom:macbuild:LDAPClientDummyLib.mcp", "LDAPClient$D.shlb", 1, 0, 0);
    }
    
    # XML Extras
    if ($main::options{soap})
    {
        BuildProject(":mozilla:extensions:xmlextras:macbuild:xmlsoap.mcp", "xmlsoap$D.o");
    }
    else
    {
        BuildProject(":mozilla:extensions:xmlextras:macbuild:xmlsoap.mcp", "xmlsoap$D.o stub");
    }
    if ($main::options{xmlextras})
    {
        BuildOneProject(":mozilla:extensions:xmlextras:macbuild:xmlextras.mcp", "xmlextras$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # Vixen
    if ($main::options{vixen})
    {
        BuildOneProject(":mozilla:extensions:vixen:macbuild:vixen.mcp", "vixen$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

        InstallResources(":mozilla:extensions:vixen:base:src:MANIFEST_COMPONENTS", "$components_dir");
    }
    
    # Document Inspector
    if ($main::options{inspector})
    {
        BuildOneProject(":mozilla:extensions:inspector:macbuild:inspector.mcp", "inspector$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # P3P
    if ($main::options{p3p})
    {
        BuildOneProject(":mozilla:extensions:p3p:macbuild:p3p.mcp", "p3p$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    # JS Debugger
    if ($main::options{jsd})
    {
        BuildOneProject(":mozilla:js:jsd:macbuild:JSD.mcp", "jsdService$D.$S", 1, $main::ALIAS_SYM_FILES, 1);

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
    my($plugin_path) = ":mozilla:modules:plugin:samples:default:mac:";
    my($plugin_dist) = GetBinDirectory() . "Plug-ins:";

    BuildProject($plugin_path . "DefaultPlugin.mcp", "DefaultPlugin$C");
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

    BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.mcp",                 "MsgUtil$D.lib", 0, 0, 0);
    BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.mcp",                      "mailnews$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.mcp",                "MsgCompose$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.mcp",                          "MsgDB$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.mcp",                    "MsgLocal$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:imap:macbuild:msgimap.mcp",                      "MsgImap$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:news:macbuild:msgnews.mcp",                      "MsgNews$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbook.mcp",              "MsgAddrbook$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:absync:macbuild:AbSync.mcp",                     "AbSyncSvc$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.mcp",                         "Mime$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:emitters:macbuild:mimeEmitter.mcp",         "mimeEmitter$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:vcard:macbuild:vcard.mcp",       "vcard$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
#   BuildOneProject(":mozilla:mailnews:mime:cthandlers:calendar:macbuild:calendar.mcp", "calendar$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:macbuild:msgImport.mcp",                  "msgImport$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:text:macbuild:msgImportText.mcp",         "msgImportText$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:eudora:macbuild:msgImportEudora.mcp",     "msgImportEudora$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    
         
    if ($main::options{static_build}) {
        BuildOneProject(":mozilla:modules:staticmod:macbuild:mailnewsComponent.mcp",  "MetaMailNews$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }
             
    InstallResources(":mozilla:mailnews:addrbook:src:MANIFEST_COMPONENTS",              "${dist_dir}Components");
	if ($main::options{smime}) {
    	BuildOneProject(":mozilla:mailnews:extensions:smime:macbuild:smime.mcp",         "msgsmime$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
    	InstallResources(":mozilla:mailnews:extensions:smime:src:MANIFEST",				 "${dist_dir}Components");
    } else {
        BuildOneProject(":mozilla:mailnews:mime:cthandlers:smimestub:macbuild:smime.mcp",   "smime$D.$S", 1, $main::ALIAS_SYM_FILES, 1);
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
        BuildProject(":mozilla:xpfe:bootstrap:macbuild:StaticMerge.mcp",    "StaticMerge$D.o");
    } else {
        BuildProject(":mozilla:xpfe:bootstrap:macbuild:StaticMerge.mcp",    "StaticMergeDummy$D.o");
    }

    BuildProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.mcp",          "apprunner$C$D");

    # build tool to create Component Registry in release builds only.
    if (!($main::DEBUG)) {
        BuildProject(":mozilla:xpcom:tools:registry:macbuild:RegXPCOM.mcp", "RegXPCOM");
    }
    
    # build XPCShell to test the cache in debugging builds only.
    if ($main::DEBUG) {
        BuildProject(":mozilla:js:macbuild:XPCShell.mcp", "XPCShellDebug");
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
