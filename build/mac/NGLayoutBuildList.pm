#!perl -w
package         NGLayoutBuildList;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Mac::StandardFile;
use Mac::Processes;
use Mac::Events;
use Mac::Files;
use Cwd;
use File::Path;
use File::Copy;

# homegrown
use Moz;
use MozJar;
use MacCVS;
use MANIFESTO;

@ISA        = qw(Exporter);
@EXPORT     = qw(ConfigureBuildSystem Checkout BuildDist BuildProjects BuildCommonProjects BuildLayoutProjects BuildOneProject);


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
#// Utility routines
#//--------------------------------------------------------------------------------------------------

# should really be importing these from Moz.pm.

sub current_directory()
{
    my $current_directory = cwd();
    chop($current_directory) if ( $current_directory =~ m/:$/ );
    return $current_directory;
}

# converts a mozilla-relative path to a full MacOS path.
sub full_path_to($)
{
    my ($path) = @_;
    if ( $path =~ m/^[^:]+$/ ) {
        $path = ":" . $path;
    }

    if ( $path =~ m/^:/ )
    {
        $path = current_directory() . $path;
    }

    return $path;
}

# pickWithMemoryFile stores the information about the user pick inside
# the file $session_storage
sub _pickWithMemoryFile($)
{
    my ($sessionStorage) = @_;
    my $cvsfile;

    if (( -e $sessionStorage) &&
        open( SESSIONFILE, $sessionStorage ))
    {
    # Read in the path if available
        $cvsfile = <SESSIONFILE>;
        chomp $cvsfile;
        close SESSIONFILE;
        if ( ! -e $cvsfile )
        {
            print STDERR "$cvsfile has disappeared\n";
            undef $cvsfile;
        }
    }
    unless (defined ($cvsfile))
    {
        # make sure that MacPerl is a front process
        ActivateApplication('McPL');
        MacPerl::Answer("Could not find your MacCVS session file. Please choose one", "OK");
        
        # prompt user for the file name, and store it
        my $macFile = StandardGetFile( 0, "McvD");  
        if ( $macFile->sfGood() )
        {
            $cvsfile = $macFile->sfFile();
            # save the choice if we can
            if ( open (SESSIONFILE, ">" . $sessionStorage))
            {
                printf SESSIONFILE $cvsfile, "\n";
                close SESSIONFILE;
            }
            else
            {
                print STDERR "Could not open storage file\n";
            }
        }
    }
    return $cvsfile;
}

# assert that we are in the correct directory for the build
sub _assertRightDirectory()
{
    unless (-e ":mozilla")
    {
        my($dir) = cwd();
        print STDERR "NGLayoutBuildList called from incorrect directory: $dir";
    } 
}

sub _getDistDirectory()
{
    return $main::DEBUG ? ":mozilla:dist:viewer_debug:" : ":mozilla:dist:viewer:";
}

sub BuildIDLProject($$)
{
    my ($project_path, $module_name) = @_;

    if ($main::CLOBBER_IDL_PROJECTS)
    {
        my($datafolder_path) = $project_path;
        $datafolder_path =~ s/\.mcp$/ Data:/;       # generate name of the project's data folder.
        print STDERR "Deleting IDL data folder: $datafolder_path\n";
        EmptyTree($datafolder_path);
    }

    BuildOneProject($project_path,  "headers", 0, 0, 0);
    BuildOneProject($project_path,  $module_name.".xpt", 1, 0, 1);
}

#--------------------------------------------------------------------------------------------------
# Support for BUILD_ROOT
#
# These underscore versions of functions in moz.pm check their first parameter to see if it is
# a path whose initial string matches the BUILD_ROOT string. If so, the original function in moz.pm
# is called with the same parameter list.
#--------------------------------------------------------------------------------------------------

sub _InstallFromManifest($;$$)
{
    if (!defined($main::BUILD_ROOT) || ($_[0] =~ m/^$main::BUILD_ROOT.+/))
    {
        &InstallFromManifest;
    }
}

sub _InstallResources($;$;$)
{
    if (!defined($main::BUILD_ROOT) || ($_[0] =~ m/^$main::BUILD_ROOT.+/))
    {
        &InstallResources;
    }
}

sub _MakeAlias($$;$)
{
    if (!defined($main::BUILD_ROOT) || ($_[0] =~ m/^$main::BUILD_ROOT.+/))
    {
        if ($_[2]) { print ("Making alias for $_[0]"); }
        &MakeAlias;
    }
}

sub _BuildProject($;$)
{
    if (!defined($main::BUILD_ROOT) || ($_[0] =~ m/^$main::BUILD_ROOT.+/))
    {
        &BuildProject;
    }
}

sub _BuildProjectClean($;$)
{
    if (!defined($main::BUILD_ROOT) || ($_[0] =~ m/^$main::BUILD_ROOT.+/))
    {
        &BuildProjectClean;
    }
}

sub _copy($$)
{
    if (!defined($main::BUILD_ROOT) || ($_[0] =~ m/^$main::BUILD_ROOT.+/))
    {
        print( "Copying $_[0] to $_[1]\n" );
        &copy;
    }
}

#//--------------------------------------------------------------------------------------------------
#// _InstallManifestRDF
#//--------------------------------------------------------------------------------------------------

sub _InstallManifestRDF($$$$)
{
    my($src, $manifest_subdir, $type, $building_jars) = @_;
    
    my($dist_dir) = _getDistDirectory();
    my $chrome_subdir = "Chrome:";
    my $chrome_dir = "$dist_dir" . $chrome_subdir;

    my($manifest_path) = $chrome_dir.$manifest_subdir;
    
    print "Installing manifest.rdf file in ".$manifest_path."\n";
    
    MakeAlias($src, "$manifest_path");
    
    open(CHROMEFILE, ">>${chrome_dir}installed-chrome.txt");

    $manifest_subdir =~ tr(:)(/);

    if ($building_jars)
    {
        # remove trailing / from subdir
        $manifest_subdir =~ s/\/$//;    
        print(CHROMEFILE "${type},install,url,jar:resource:/Chrome/${manifest_subdir}.jar!/\n");     
    }
    else
    {
        print(CHROMEFILE "${type},install,url,resource:/Chrome/${manifest_subdir}\n");
    }

    close(CHROMEFILE);
}


#//--------------------------------------------------------------------------------------------------
#// InstallManifestRDFFiles Install manifest.rdf files and build installed_chrome.txt
#//--------------------------------------------------------------------------------------------------

sub InstallManifestRDFFiles()
{
    unless( $main::build{resources} ) { return; }

    my($dist_dir) = _getDistDirectory();

    my $chrome_subdir = "Chrome:";
    my $chrome_dir = "$dist_dir" . $chrome_subdir;

    my($building_jars) = 0;     # $main::options{chrome_jars};

    # nuke installed-chrome.txt
    unlink ${chrome_dir}."installed-chrome.txt";

    # install manifest RDF files
    _InstallManifestRDF(":mozilla:extensions:irc:xul:manifest.rdf", "packages:chatzilla:", "content", $building_jars);
    _InstallManifestRDF(":mozilla:extensions:irc:xul:manifest.rdf", "packages:chatzilla:", "locale", $building_jars);
    _InstallManifestRDF(":mozilla:extensions:irc:xul:manifest.rdf", "packages:chatzilla:", "skin", $building_jars);
    
    _InstallManifestRDF(":mozilla:extensions:cview:resources:manifest.rdf", "packages:cview:", "content", $building_jars);
    _InstallManifestRDF(":mozilla:extensions:cview:resources:manifest.rdf", "packages:cview:", "locale", $building_jars);
    _InstallManifestRDF(":mozilla:extensions:cview:resources:manifest.rdf", "packages:cview:", "skin", $building_jars);
    
    if ($main::options{transformiix})
    {
        my($transformiix_manifest) = ":mozilla:extensions:transformiix:source:examples:mozilla:transformiix:manifest.rdf";
        _InstallManifestRDF($transformiix_manifest, "packages:transformiix:", "content", $building_jars);
        _InstallManifestRDF($transformiix_manifest, "packages:transformiix:", "locale", $building_jars);
        _InstallManifestRDF($transformiix_manifest, "packages:transformiix:", "skin", $building_jars);
    }

    _InstallManifestRDF(":mozilla:themes:classic:manifest.rdf", "skins:classic:", "skin", $building_jars);
    _InstallManifestRDF(":mozilla:themes:blue:manifest.rdf", "skins:blue:", "skin", $building_jars);
    _InstallManifestRDF(":mozilla:themes:modern:manifest.rdf", "skins:modern:", "skin", $building_jars);

    _InstallManifestRDF(":mozilla:xpfe:communicator:resources:content:manifest.rdf", "packages:core:", "content", $building_jars);
    _InstallManifestRDF(":mozilla:xpfe:global:resources:content:manifest.rdf", "packages:widget-toolkit:", "content", $building_jars);
    _InstallManifestRDF(":mozilla:mailnews:base:resources:content:manifest.rdf", "packages:messenger:", "content", $building_jars);

    _InstallManifestRDF(":mozilla:xpfe:communicator:resources:locale:en-US:manifest.rdf", "locales:en-US:", "locale", $building_jars);
    
    _InstallManifestRDF(":mozilla:l10n:langpacks:en-DE:chrome:en-DE:manifest.rdf", "locales:en-DE:", "locale", $building_jars);

}


#//--------------------------------------------------------------------------------------------------
#// Configure Build System
#//--------------------------------------------------------------------------------------------------

my($UNIVERSAL_INTERFACES_VERSION) = 0x0320;

sub _processRunning($)
{
    my($processName, $psn, $psi) = @_;
    while ( ($psn, $psi) = each(%Process) ) {
     if ($psi->processName eq $processName) { return 1; }
    }
    return 0;
}

sub _genBuildSystemInfo()
{
    # always rebuild the configuration program.
    BuildProjectClean(":mozilla:build:mac:tools:BuildSystemInfo:BuildSystemInfo.mcp", "BuildSystemInfo");

    # delete the configuration file.
    unlink(":mozilla:build:mac:BuildSystemInfo.pm");
    
    # run the program.
    system(":mozilla:build:mac:BuildSystemInfo");

    # wait for the file to be created.
    while (!(-e ":mozilla:build:mac:BuildSystemInfo.pm")) { WaitNextEvent(); }
    
    # wait for BuildSystemInfo to finish, so that we see correct results.
    while (_processRunning("BuildSystemInfo")) { WaitNextEvent(); }

    # now, evaluate the contents of the file.
    open(F, ":mozilla:build:mac:BuildSystemInfo.pm");
    while (<F>) { eval; }
    close(F);
}

# defines some build-system configuration variables.
sub ConfigureBuildSystem()
{
    #// In the future, we may want to do configurations based on the actual build system itself.
    #// _genBuildSystemInfo();

    # launch codewarrior and write idepath.txt
    LaunchCodeWarrior();
    
    SanityCheckJarOptions();
    
    #// For now, if we discover a newer header file than existed in Universal Interfaces 3.2,
    #// we'll assume that 3.3 or later is in use.
    my($universal_interfaces) = getCodeWarriorPath("MacOS Support:Universal:Interfaces:CIncludes:");
    if (-e ($universal_interfaces . "ControlDefinitions.h")) {
        $UNIVERSAL_INTERFACES_VERSION = 0x0330;
    }

    #// Rename IC SDK folder in the Mac OS Support folder
    my($ic_sdk_folder) = getCodeWarriorPath("MacOS Support:ICProgKit2.0.2");
    if( -e $ic_sdk_folder)
    {
        my($new_ic_folder_name) = getCodeWarriorPath("MacOS Support:(ICProgKit2.0.2)");
        rename ($ic_sdk_folder, $new_ic_folder_name);
        # note that CodeWarrior doesn't descnet into folders with () the name
        print "Mozilla no longer needs the Internet Config SDK to build:\n  Renaming the 'ICProgKit2.0.2' folder to '(ICProgKit2.0.2)'\n";
    }

    printf("UNIVERSAL_INTERFACES_VERSION = 0x%04X\n", $UNIVERSAL_INTERFACES_VERSION);

    my($line, $config, $oldconfig, $define, $definevalue, $defines);
    my($k, $l,);

    foreach $k (keys(%main::options))
    {
        if ($main::options{$k})
        {
            foreach $l (keys(%{$main::optiondefines{$k}}))
            {
                $my::defines{$l} = $main::optiondefines{$k}{$l};
            }
        }
    }

    my $config_headerfile = current_directory() . ":mozilla:config:mac:DefinesOptions.h";
    if (-e $config_headerfile)
    {
        open(CONFIG_HEADER, "< $config_headerfile") || die "Can't open configuration header, check the file path.\n";
        while ($line = <CONFIG_HEADER>)
        {
            $oldconfig .= $line;
            if ($line =~ m/#define (.*) (.*)\n/)
            {
                $define = $1;
                $definevalue = $2;
                if (exists ($my::defines{$define}) and ($my::defines{$define} == $definevalue))
                {
                    delete $my::defines{$define};
                    $config .= $line;
                }
            }
        }
        close(CONFIG_HEADER);
    }

    if (%my::defines)
    {
        foreach $k (keys(%my::defines))
        {
            $config .= "#define " . $k . " " . $my::defines{$k} . "\n";
        }
    }

    if (($config ne $oldconfig) || (!-e $config_headerfile))
    {
        printf("Writing new DefinesOptions.h\n");
        open(CONFIG_HEADER, "> $config_headerfile") || die "Can't open configuration header, check the file path.\n";
        MacPerl::SetFileInfo("CWIE", "TEXT", $config_headerfile);
        print CONFIG_HEADER ($config);
        close(CONFIG_HEADER);
    }
}

#//--------------------------------------------------------------------------------------------------
#// Check out everything
#//--------------------------------------------------------------------------------------------------

sub Checkout()
{
    unless ( $main::pull{all} || $main::pull{moz} || $main::pull{runtime} ) { return;}

    # give application activation a chance to happen
    WaitNextEvent();
    WaitNextEvent();
    WaitNextEvent();
    
    _assertRightDirectory();
    my($cvsfile) = _pickWithMemoryFile("::nglayout.cvsloc");
    my($session) = MacCVS->new( $cvsfile );
    unless (defined($session)) { die "Checkout aborted. Cannot create session file: $session" }

    # activate MacCVS
    ActivateApplication('Mcvs');

    my($nsprpub_tag) = "NSPRPUB_CLIENT_BRANCH";
    my($nss_tab) = "NSS_30_BRANCH";
    my($psm_tag) = "SECURITY_MAC_BRANCH";
    my($ldapsdk_tag) = "LDAPCSDK_40_BRANCH";
    
    #//
    #// Checkout commands
    #//
    if ($main::pull{moz})
    {
        $session->checkout("mozilla/nsprpub", $nsprpub_tag)            || print "checkout of nsprpub failed\n";        
        $session->checkout("mozilla/security/nss", $nss_tab)           || print "checkout of security/nss failed\n";
        $session->checkout("mozilla/security/psm", $psm_tag)           || print "checkout of security/psm failed\n";
        $session->checkout("DirectorySDKSourceC", $ldapsdk_tag)        || print "checkout of LDAP C SDK failed\n";

        # we need this jar.mn file on the jar branch
        # $session->checkout("mozilla/security/base/res/jar.mn", $jars_branch_tag) || print "checkout of jar.mn failed\n";

        $session->checkout("SeaMonkeyAll")           || 
            print "MacCVS reported some errors checking out SeaMonkeyAll, but these are probably not serious.\n";
    }
    elsif ($main::pull{runtime})
    {
        $session->checkout("mozilla/build/mac")                     || print "checkout failure\n";
        $session->checkout("mozilla/lib/mac/InterfaceLib")          || print "checkout failure\n";
        $session->checkout("mozilla/config/mac")                    || print "checkout failure\n";
        $session->checkout("mozilla/gc")                            || print "checkout failure\n";
        $session->checkout("mozilla/lib/mac/NSStartup")             || print "checkout failure\n";
        $session->checkout("mozilla/lib/mac/NSStdLib")              || print "checkout failure\n";
        $session->checkout("mozilla/lib/mac/NSRuntime")             || print "checkout failure\n";
        $session->checkout("mozilla/lib/mac/MoreFiles")             || print "checkout failure\n";
        $session->checkout("mozilla/lib/mac/MacMemoryAllocator")    || print "checkout failure\n";
        $session->checkout("mozilla/nsprpub", $nsprpub_tag)         || print "checkout failure\n";
    }
}

#//--------------------------------------------------------------------------------------------------
#// Remove all files from a tree, leaving directories intact (except "CVS").
#//--------------------------------------------------------------------------------------------------

sub EmptyTree($)
{
    my ($root) = @_; 
    #print "EmptyTree($root)\n";
    opendir(DIR, $root);
    my $sub;
    foreach $sub (readdir(DIR))
    {
        my $fullpathname = $root.$sub; # -f, -d only work on full paths

        # Don't call empty tree for the alias of a directory.
        # -d returns true for the alias of a directory, false for a broken alias)

        if (-d $fullpathname)
        {
            if (-l $fullpathname)
            {
                        print "еее $fullpathname is an alias to a directory. Not emptying!\n";
                next;
            }
            EmptyTree($fullpathname.":");
            if ($sub eq "CVS")
            {
                #print "rmdir $fullpathname\n";
                rmdir $fullpathname;
            }
        }
        else
        {
            #print "\tunlink $fullpathname\n";
            my $cnt = unlink $fullpathname; # this is perlspeak for deleting a file.
            if ($cnt ne 1)
            {
                print "Failed to delete $fullpathname";
                die;
            }
        }
    }
    closedir(DIR);
}

#//--------------------------------------------------------------------------------------------------
#// Make resource aliases for one directory
#//--------------------------------------------------------------------------------------------------

sub BuildFolderResourceAliases($$)
{
    my($src_dir, $dest_dir) = @_;
    
    unless ($src_dir =~ m/^$main::BUILD_ROOT.+/) { return; }
    
    # get a list of all the resource files
    opendir(SRCDIR, $src_dir) || die("can't open $src_dir");
    my(@resource_files) = readdir(SRCDIR);
    closedir(SRCDIR);
    
    # make aliases for each one into the dest directory
    print("Placing aliases to all files from $src_dir in $dest_dir\n");
    for ( @resource_files )
    {
        next if $_ eq "CVS";
        #print("    Doing $_\n");
        if (-l $src_dir.$_)
        {
            print("     $_ is an alias\n");
            next;
        }
        my($file_name) = $src_dir . $_; 
        MakeAlias($file_name, $dest_dir);
    }
}


#//--------------------------------------------------------------------------------------------------
#// Recurse into the skin directories
#//--------------------------------------------------------------------------------------------------

sub ScanForManifestFiles($$$$)
{
    my($dir, $theme_root, $theme_name, $dist_dir) = @_;

    opendir(DIR, $dir) or die "Cannot open dir $dir\n";
    my @files = readdir(DIR);
    closedir DIR;

    my $file;

    foreach $file (@files)
    {    
        my $filepath = $dir.":".$file;

        if (-d $filepath)
        {
            # print "Looking for MANIFEST files in $filepath\n";        
            ScanForManifestFiles($filepath, $theme_root, $theme_name, $dist_dir);
        }
        elsif ($file eq "MANIFEST")
        {
            # print "Doing manifest file $filepath\n";

            # Get the dest path from the first line of the file

            open(MANIFEST, $filepath) || die "Could not open file $file";
            # Read in the path if available
            my($dest_line) = <MANIFEST>;
            chomp $dest_line;
            close MANIFEST;

            $dest_line =~ s|^#!dest[\t ]+|| || die "No destination line found in $filepath\n";

            my($dest_path) = $dist_dir."chrome:skins:$theme_name:$dest_line";
            # print " Destination is $dest_path\n";

            InstallResources($filepath, "$dest_path", 0);
        }
    }
}

#//--------------------------------------------------------------------------------------------------
#// Install skin files
#//--------------------------------------------------------------------------------------------------

sub InstallSkinFiles($)
{
    my($theme_name) = @_;

    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    my($dist_dir) = _getDistDirectory();
    my($themes_dir) = ":mozilla:themes:".$theme_name;

    print "Installing skin files from $themes_dir\n";
    ScanForManifestFiles($themes_dir, $themes_dir, $theme_name, $dist_dir);
}

#//--------------------------------------------------------------------------------------------------
#// Select a default skin
#//--------------------------------------------------------------------------------------------------

sub SetDefaultSkin($)
{
    my($skin) = @_;

    _assertRightDirectory();

    my($dist_dir) = _getDistDirectory();
    my($chrome_subdir) = $dist_dir."Chrome";
    
    print "Setting default skin to $skin\n";
    
    open(CHROMEFILE, ">>${chrome_subdir}:installed-chrome.txt") || die "Failed to open installed_chrome.txt\n";
    print(CHROMEFILE "skin,install,select,$skin\n");
    close(CHROMEFILE);
}

#//--------------------------------------------------------------------------------------------------
#// Recurse into the provider directories
#//--------------------------------------------------------------------------------------------------

sub ProScanForManifestFiles($$$$$)
{
       ## diff from ScanForManifestFiles()
       my($dir, $theme_root, $provider, $theme_name, $dist_dir) = @_;

       opendir(DIR, $dir) or die "Cannot open dir $dir\n";
       my @files = readdir(DIR);
       closedir DIR;

       my $file;

       foreach $file (@files)
       {        
               my $filepath = $dir.":".$file;

               if (-d $filepath)
               {
                       # print "Looking for MANIFEST files in $filepath\n";            
                       ## diff from ScanForManifestFiles()
                       ProScanForManifestFiles($filepath, $theme_root, $provider, $theme_name, $dist_dir);
               }
               elsif ($file eq "MANIFEST")
               {
                       # print "Doing manifest file $filepath\n";

                       # Get the dest path from the first line of the file

                       open(MANIFEST, $filepath) || die "Could not open file $file";
                       # Read in the path if available
                       my($dest_line) = <MANIFEST>;
                       chomp $dest_line;
                       close MANIFEST;

                       $dest_line =~ s|^#!dest[\t ]+|| || die "No destination line found in $filepath\n";

                       ## diff from ScanForManifestFiles()
                       my($dest_path) = $dist_dir."chrome:$provider:$theme_name:$dest_line";
                       # print " Destination is $dest_path\n";
                       
                       InstallResources($filepath, "$dest_path", 0);
               }
       }
}

#//--------------------------------------------------------------------------------------------------
#// Install Provider files
#//--------------------------------------------------------------------------------------------------

sub InstallProviderFiles($$)
{
       ## diff from InstallSkinFiles() - new arg: provider
       my($provider, $theme_name) = @_;

       # unless( $main::build{resources} ) { return; }
       _assertRightDirectory();

       my($dist_dir) = _getDistDirectory();

       ## diff from InstallSkinFiles()
       my($themes_dir) = ":mozilla:l10n:langpacks:".$theme_name.":chrome:".$theme_name;

       print "Installing $provider files from $themes_dir\n";

       ## diff from InstallSkinFiles()
       ProScanForManifestFiles($themes_dir, $themes_dir, $provider, $theme_name, $dist_dir);
}

### defaults
#//--------------------------------------------------------------------------------------------------
#// Recurse into the defaults directories
#//--------------------------------------------------------------------------------------------------

sub DefScanForManifestFiles($$$$)
{
       my($dir, $theme_root, $theme_name, $dist_dir) = @_;

       opendir(DIR, $dir) or die "Cannot open dir $dir\n";
       my @files = readdir(DIR);
       closedir DIR;

       my $file;

       foreach $file (@files)
       {        
               my $filepath = $dir.":".$file;

               if (-d $filepath)
               {
                       # print "Looking for MANIFEST files in $filepath\n";            
                       ## diff from ScanForManifestFiles()
                       DefScanForManifestFiles($filepath, $theme_root, $theme_name, $dist_dir);
                                      }
               elsif ($file eq "MANIFEST")
               {
                       # print "Doing manifest file $filepath\n";

                       # Get the dest path from the first line of the file

                       open(MANIFEST, $filepath) || die "Could not open file $file";
                       # Read in the path if available
                       my($dest_line) = <MANIFEST>;
                       chomp $dest_line;
                       close MANIFEST;

                       $dest_line =~ s|^#!dest[\t ]+|| || die "No destination line found in $filepath\n";

                       ## diff from ScanForManifestFiles()
                       my($dest_path) = $dist_dir."defaults:$dest_line:$theme_name";
                       # print " Destination is $dest_path\n";

                       InstallResources($filepath, "$dest_path", 0);
               }
       }
}

#//--------------------------------------------------------------------------------------------------
#// InstallLangPackFiles
#//--------------------------------------------------------------------------------------------------

sub InstallLangPackFiles($)
{
       my($theme_name) = @_;
       
       # unless( $main::build{resources} ) { return; }
       _assertRightDirectory();

       my($dist_dir) = _getDistDirectory();

       ## diff from InstallSkinFiles()
       my($themes_dir) = ":mozilla:l10n:langpacks:".$theme_name.":defaults";

       print "Installing default files from $themes_dir\n";

       ## diff from InstallSkinFiles()
       DefScanForManifestFiles($themes_dir, $themes_dir, $theme_name, $dist_dir);
}

#//--------------------------------------------------------------------------------------------------
#// InstallDefaultsFiles
#//--------------------------------------------------------------------------------------------------

sub InstallDefaultsFiles()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Defaults copying ----\n");

    # default folder
    my($defaults_dir) = "$dist_dir" . "Defaults:";
    mkdir($defaults_dir, 0);

    {
    my($default_wallet_dir) = "$defaults_dir"."wallet:";
    mkdir($default_wallet_dir, 0);
    _InstallResources(":mozilla:extensions:wallet:src:MANIFEST",                        "$default_wallet_dir");
    }

    # Default _profile_ directory stuff
    {
    my($default_profile_dir) = "$defaults_dir"."Profile:";
    mkdir($default_profile_dir, 0);

    _copy(":mozilla:profile:defaults:bookmarks.html","$default_profile_dir"."bookmarks.html");
    _copy(":mozilla:profile:defaults:panels.rdf","$default_profile_dir"."panels.rdf");
    _copy(":mozilla:profile:defaults:localstore.rdf","$default_profile_dir"."localstore.rdf");
    _copy(":mozilla:profile:defaults:search.rdf","$default_profile_dir"."search.rdf");
    _copy(":mozilla:profile:defaults:mimeTypes.rdf","$default_profile_dir"."mimeTypes.rdf");

    # make a dup in en-US
    my($default_profile_dir_en_US) = "$default_profile_dir"."en-US:";
    mkdir($default_profile_dir_en_US, 0);

    _copy(":mozilla:profile:defaults:bookmarks.html","$default_profile_dir_en_US"."bookmarks.html");
    _copy(":mozilla:profile:defaults:panels.rdf","$default_profile_dir_en_US"."panels.rdf");
    _copy(":mozilla:profile:defaults:localstore.rdf","$default_profile_dir_en_US"."localstore.rdf");
    _copy(":mozilla:profile:defaults:search.rdf","$default_profile_dir_en_US"."search.rdf");
    _copy(":mozilla:profile:defaults:mimeTypes.rdf","$default_profile_dir_en_US"."mimeTypes.rdf");
    }
    
    # Default _pref_ directory stuff
    {
    my($default_pref_dir) = "$defaults_dir"."Pref:";
    mkdir($default_pref_dir, 0);
    _InstallResources(":mozilla:xpinstall:public:MANIFEST_PREFS",                       "$default_pref_dir", 0);
    _InstallResources(":mozilla:modules:libpref:src:MANIFEST_PREFS",                    "$default_pref_dir", 0);
    _InstallResources(":mozilla:modules:libpref:src:init:MANIFEST",                     "$default_pref_dir", 0);
    _InstallResources(":mozilla:modules:libpref:src:mac:MANIFEST",                      "$default_pref_dir", 0);
    }

    print("--- Defaults copying complete ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// InstallNonChromeResources
#//--------------------------------------------------------------------------------------------------

sub InstallNonChromeResources()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Resource copying ----\n");

    #//
    #// Most resources should all go into the chrome dir eventually
    #//
    my($resource_dir) = "$dist_dir" . "res:";
    my($samples_dir) = "$resource_dir" . "samples:";

    #//
    #// Make aliases of resource files
    #//
    if (! $main::options{mathml})
    {
        _MakeAlias(":mozilla:layout:html:document:src:ua.css",                          "$resource_dir");
    }
    else
    {
        #// Building MathML so include the mathml.css file in ua.css
        _MakeAlias(":mozilla:layout:mathml:content:src:mathml.css",                     "$resource_dir");
        _copy(":mozilla:layout:html:document:src:ua.css",                               "$resource_dir"."ua.css");
        @ARGV = ("$resource_dir"."ua.css");
        do ":mozilla:layout:mathml:content:src:mathml-css.pl";
    }
    
    _MakeAlias(":mozilla:layout:html:document:src:html.css",                            "$resource_dir");
    _MakeAlias(":mozilla:layout:html:document:src:quirk.css",                           "$resource_dir");
    _MakeAlias(":mozilla:layout:html:document:src:arrow.gif",                           "$resource_dir"); 
    _MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",            "$resource_dir");
    _MakeAlias(":mozilla:intl:uconv:src:charsetalias.properties",                       "$resource_dir");
    _MakeAlias(":mozilla:intl:uconv:src:acceptlanguage.properties",                     "$resource_dir");
    _MakeAlias(":mozilla:intl:uconv:src:maccharset.properties",                         "$resource_dir");
    _MakeAlias(":mozilla:intl:uconv:src:charsetData.properties",                        "$resource_dir");
    _MakeAlias(":mozilla:intl:uconv:src:acceptlanguage.properties",                     "$resource_dir");
    _MakeAlias(":mozilla:intl:locale:src:langGroups.properties",                        "$resource_dir");
    _MakeAlias(":mozilla:intl:locale:src:language.properties",                          "$resource_dir");

    _InstallResources(":mozilla:gfx:src:MANIFEST",                                      "$resource_dir"."gfx:");

    my($entitytab_dir) = "$resource_dir" . "entityTables";
    _InstallResources(":mozilla:intl:unicharutil:tables:MANIFEST",                      "$entitytab_dir");

    my($html_dir) = "$resource_dir" . "html:";
    _InstallResources(":mozilla:layout:html:base:src:MANIFEST_RES",                     "$html_dir");

    my($throbber_dir) = "$resource_dir" . "throbber:";
    BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:throbber:",              "$throbber_dir");
    
    BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:samples:",               "$samples_dir");
    BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:resources:",             "$samples_dir");
    BuildFolderResourceAliases(":mozilla:xpfe:browser:samples:",                        "$samples_dir");

    _InstallResources(":mozilla:xpfe:browser:src:MANIFEST",                             "$samples_dir");
    _MakeAlias(":mozilla:xpfe:browser:samples:sampleimages:",                           "$samples_dir");
    
    my($rdf_dir) = "$resource_dir" . "rdf:";
    BuildFolderResourceAliases(":mozilla:rdf:resources:",                               "$rdf_dir");

    my($domds_dir) = "$samples_dir" . "rdf:";
    _InstallResources(":mozilla:rdf:tests:domds:resources:MANIFEST",                    "$domds_dir");

    # QA Menu
    _InstallResources(":mozilla:intl:strres:tests:MANIFEST",            "$resource_dir");

    print("--- End Resource copying ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// InstallComponentFiles
#//--------------------------------------------------------------------------------------------------

sub InstallComponentFiles()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Text Components copying ----\n");

    my($components_dir) = "$dist_dir" . "Components:";

    # console
    _InstallResources(":mozilla:xpfe:components:console:MANIFEST",                          "$components_dir", 0);

    # sidebar
    _InstallResources(":mozilla:xpfe:components:sidebar:src:MANIFEST",                      "$components_dir");

    print("--- Done Text Components copying ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// InstallChromeFiles
#//--------------------------------------------------------------------------------------------------

sub InstallChromeFiles()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Chrome copying ----\n");

    #//
    #// Most resources should all go into the chrome dir eventually
    #//
    my $chrome_subdir = "Chrome:";
    my $chrome_dir = "$dist_dir" . $chrome_subdir;

    my($packages_chrome_dir) = "$chrome_dir" . "packages:";
    my($locales_chrome_dir) = "$chrome_dir" . "locales:";

    # Second level chrome directories

    my($core_packages_chrome_dir) = "$packages_chrome_dir" . "core:";
    my($messenger_packages_chrome_dir) = "$packages_chrome_dir" . "messenger:";
    my($widgettoolkit_packages_chrome_dir) = "$packages_chrome_dir" . "widget-toolkit:";
     
    my($enUS_locales_chrome_dir) = "$locales_chrome_dir" . "en-US:";

    # Third level chrome directories

    # navigator
    my($navigator_core_packages_chrome_dir) = "$core_packages_chrome_dir" . "navigator:";
    my($navigatorContent) = "$navigator_core_packages_chrome_dir" . "content:";

    my($navigator_enUS_locales_chrome_dir) = "$enUS_locales_chrome_dir" . "navigator:";
    my($navigatorLocale) = "$navigator_enUS_locales_chrome_dir" . "locale:";
    
    # global
    my($global_widgettoolkit_packages_chrome_dir) = "$widgettoolkit_packages_chrome_dir" . "global:";
    my($globalContent) = "$global_widgettoolkit_packages_chrome_dir" . "content:";

    my($global_enUS_locales_chrome_dir) = "$enUS_locales_chrome_dir" . "global:";
    my($globalLocale) = "$global_enUS_locales_chrome_dir" . "locale:";

    # communicator
    my($communicator_core_packages_chrome_dir) = "$core_packages_chrome_dir" . "communicator:";
    my($communicatorContent) = "$communicator_core_packages_chrome_dir" . "content:";

    my($communicator_enUS_locales_chrome_dir) = "$enUS_locales_chrome_dir" . "communicator:";
    my($communicatorLocale) = "$communicator_enUS_locales_chrome_dir" . "locale:";

    # copy the chrome registry (don't alias it)
    _copy( ":mozilla:rdf:chrome:build:registry.rdf", "$chrome_dir" . "registry.rdf" );
            
    _MakeAlias(":mozilla:xpcom:base:xpcom.properties",      "$globalLocale");   

    _MakeAlias(":mozilla:intl:uconv:src:charsetTitles.properties","$globalLocale");

    _InstallResources(":mozilla:xpfe:browser:resources:content:MANIFEST",               "$navigatorContent");
    _InstallResources(":mozilla:xpfe:browser:resources:content:mac:MANIFEST",               "$navigatorContent");
    _InstallResources(":mozilla:xpfe:browser:resources:locale:en-US:MANIFEST",          "$navigatorLocale", 0);

    # find
    _InstallResources(":mozilla:xpfe:components:find:resources:MANIFEST",                   "$globalContent");
    _InstallResources(":mozilla:xpfe:components:find:resources:locale:en-US:MANIFEST",      "$globalLocale");   

    # ucth
    _InstallResources(":mozilla:xpfe:components:ucth:resources:MANIFEST",                   "$globalContent");
    _InstallResources(":mozilla:xpfe:components:ucth:resources:locale:en-US:MANIFEST",      "$globalLocale");
    _InstallResources(":mozilla:xpfe:components:xfer:resources:MANIFEST",                   "$globalContent");
    _InstallResources(":mozilla:xpfe:components:xfer:resources:locale:en-US:MANIFEST",      "$globalLocale");

    #file picker
    _InstallResources(":mozilla:xpfe:components:filepicker:res:locale:en-US:MANIFEST",      "$globalLocale");
    
    # console
    _InstallResources(":mozilla:xpfe:components:console:resources:content:MANIFEST",        "$globalContent", 0);
    _InstallResources(":mozilla:xpfe:components:console:resources:locale:en-US:MANIFEST",   "$globalLocale", 0);

    # autocomplete
    _InstallResources(":mozilla:xpfe:components:autocomplete:resources:content:MANIFEST",   "$globalContent", 0);

    # widget-toolkit
    _InstallResources(":mozilla:xpfe:global:resources:content:MANIFEST",                "$globalContent");
    _InstallResources(":mozilla:xpfe:global:resources:content:mac:MANIFEST",            "$globalContent");
    _InstallResources(":mozilla:xpfe:global:resources:locale:en-US:MANIFEST",           "$globalLocale", 0);
    _InstallResources(":mozilla:xpfe:global:resources:locale:en-US:mac:MANIFEST",       "$globalLocale", 0);

    # communicator
    _InstallResources(":mozilla:xpfe:communicator:resources:content:MANIFEST",          "$communicatorContent");
    _InstallResources(":mozilla:xpfe:communicator:resources:content:mac:MANIFEST",      "$communicatorContent");
    _InstallResources(":mozilla:xpfe:communicator:resources:locale:en-US:MANIFEST",     "$communicatorLocale", 0);

    _InstallResources(":mozilla:docshell:base:MANIFEST",                                "$globalLocale", 0);

    # xpinstall
    {
    my($xpinstallContent) = "$communicatorContent" . "xpinstall:";
    my($xpinstallLocale) = "$communicatorLocale" . "xpinstall:";

    _InstallResources(":mozilla:xpinstall:res:content:MANIFEST","$xpinstallContent", 0);        
    #XXX these InstallResources calls should be down below with the rest of the calls
    _InstallResources(":mozilla:xpinstall:res:locale:en-US:MANIFEST","$xpinstallLocale", 0);
    _InstallResources(":mozilla:xpinstall:res:content:MANIFEST","$xpinstallContent", 0);
    }
    
    # profile
    {
    my($profileContent) = "$communicatorContent" . "profile:";
    my($profileLocale) = "$communicatorLocale" . "profile:";

    #XXX these InstallResourses calls should be down below with the rest of the calls
    _InstallResources(":mozilla:profile:resources:content:MANIFEST", "$profileContent", 0);
    _InstallResources(":mozilla:profile:resources:locale:en-US:MANIFEST", "$profileLocale", 0);
    _InstallResources(":mozilla:profile:pref-migrator:resources:content:MANIFEST", "$profileContent", 0);
    _InstallResources(":mozilla:profile:pref-migrator:resources:locale:en-US:MANIFEST", "$profileLocale", 0);
    }
    
    
    #NECKO
    {
    my($necko_chrome_dir) = "$chrome_dir" . "necko:";
    my($necko_content_chrome_dir) = "$necko_chrome_dir" . "content:";
    my($necko_locale_chrome_dir) = "$necko_chrome_dir" . "locale:";
    _InstallResources(":mozilla:netwerk:resources:content:MANIFEST",                    "$necko_content_chrome_dir");
    _InstallResources(":mozilla:netwerk:resources:locale:en-US:MANIFEST",               "$necko_locale_chrome_dir", 0);
    }

    # layout locale hack
    {
    my($layout_locale_hack_dir) = "$communicatorLocale"."layout:";
    mkdir($layout_locale_hack_dir, 0);
    _InstallResources(":mozilla:layout:html:forms:src:MANIFEST_PROPERTIES",             "$layout_locale_hack_dir", 0);
    }
    
    # editor
    {
    my($editor_core_packages_chrome_dir) = "$core_packages_chrome_dir" . "editor:";
    my($editorContent) = "$editor_core_packages_chrome_dir" . "content:";

    my($editor_enUS_locales_chrome_dir) = "$enUS_locales_chrome_dir" . "editor:";
    my($editorLocale) = "$editor_enUS_locales_chrome_dir" . "locale:";

    _InstallResources(":mozilla:editor:ui:composer:content:MANIFEST",               "$editorContent", 0);
    _InstallResources(":mozilla:editor:ui:composer:locale:en-US:MANIFEST",          "$editorLocale", 0);
    _InstallResources(":mozilla:editor:ui:dialogs:content:MANIFEST",                "$editorContent", 0);
    _InstallResources(":mozilla:editor:ui:dialogs:locale:en-US:MANIFEST",           "$editorLocale", 0);
    }
    
    # mailnews
    {
    # Messenger is a top level component
    my($messenger_chrome_dir) = "$chrome_dir" . "messenger:";

    my($messenger_packages_chrome_dir) = "$packages_chrome_dir" . "messenger:";
    my($messenger_messenger_packages_chrome_dir) = "$messenger_packages_chrome_dir" . "messenger:";
    my($messengerContent) = "$messenger_messenger_packages_chrome_dir" . "content:";

    my($messenger_enUS_locales_chrome_dir) = "$enUS_locales_chrome_dir" . "messenger:";
    my($messengerLocale) = "$messenger_enUS_locales_chrome_dir" . "locale:";

    _InstallResources(":mozilla:mailnews:base:resources:content:MANIFEST",                  "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:base:resources:content:mac:MANIFEST",              "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:base:resources:locale:en-US:MANIFEST",             "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:base:prefs:resources:content:MANIFEST",            "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:base:prefs:resources:locale:en-US:MANIFEST",       "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:base:search:resources:content:MANIFEST",           "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:base:search:resources:locale:en-US:MANIFEST",      "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:mime:resources:content:MANIFEST",                  "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:mime:emitters:resources:content:MANIFEST",         "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:local:resources:locale:en-US:MANIFEST",            "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:news:resources:content:MANIFEST",                  "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:news:resources:locale:en-US:MANIFEST",             "$messengerLocale", 0);

    _InstallResources(":mozilla:mailnews:imap:resources:locale:en-US:MANIFEST",             "$messengerLocale", 0);

    _InstallResources(":mozilla:mailnews:mime:resources:MANIFEST",                          "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:mime:cthandlers:resources:MANIFEST",               "$messengerLocale", 0);

    # messenger compose resides within messenger
    my($messengercomposeContent) = "$messengerContent" . "messengercompose:";
    my($messengercomposeLocale) = "$messengerLocale" . "messengercompose:";
    _InstallResources(":mozilla:mailnews:compose:resources:content:MANIFEST",               "$messengercomposeContent", 0);
    _InstallResources(":mozilla:mailnews:compose:resources:locale:en-US:MANIFEST",          "$messengercomposeLocale", 0);
    _InstallResources(":mozilla:mailnews:compose:prefs:resources:content:MANIFEST",         "$messengercomposeContent", 0);
    _InstallResources(":mozilla:mailnews:compose:prefs:resources:locale:en-US:MANIFEST",    "$messengercomposeLocale", 0);

    # addressbook resides within messenger
    my($addressbookContent) = "$messengerContent" . "addressbook:";
    my($addressbookLocale) = "$messengerLocale" . "addressbook:";
    _InstallResources(":mozilla:mailnews:addrbook:resources:content:MANIFEST",              "$addressbookContent", 0);
    _InstallResources(":mozilla:mailnews:addrbook:resources:locale:en-US:MANIFEST",         "$addressbookLocale", 0);
    _InstallResources(":mozilla:mailnews:addrbook:prefs:resources:content:MANIFEST",        "$addressbookContent", 0);
    _InstallResources(":mozilla:mailnews:addrbook:prefs:resources:locale:en-US:MANIFEST",   "$addressbookLocale", 0);
    _InstallResources(":mozilla:mailnews:absync:resources:locale:en-US:MANIFEST","$addressbookLocale", 0);
    
    # import
    _InstallResources(":mozilla:mailnews:import:resources:content:MANIFEST",                "$messengerContent", 0);
    _InstallResources(":mozilla:mailnews:import:resources:locale:en-US:MANIFEST",           "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:import:eudora:resources:locale:en-US:MANIFEST",    "$messengerLocale", 0);
    _InstallResources(":mozilla:mailnews:import:text:resources:locale:en-US:MANIFEST",      "$messengerLocale", 0);
    }
    
    # bookmarks
    {
    my($bookmarksContent) = "$communicatorContent"."bookmarks:";
    my($bookmarksLocale) = "$communicatorLocale"."bookmarks:";

    _InstallResources(":mozilla:xpfe:components:bookmarks:resources:MANIFEST-content",      "$bookmarksContent");
    _InstallResources(":mozilla:xpfe:components:bookmarks:resources:locale:en-US:MANIFEST", "$bookmarksLocale");
    }
    
    # directory
    {
    my($directoryContent) = "$communicatorContent"."directory:";
    my($directoryLocale) = "$communicatorLocale"."directory:";
        
    _InstallResources(":mozilla:xpfe:components:directory:MANIFEST-content",        "$directoryContent");
    _InstallResources(":mozilla:xpfe:components:directory:locale:en-US:MANIFEST",   "$directoryLocale");
    }
    
    # regViewer
    {
    my($regviewerContent) = "$communicatorContent"."regviewer:";
    my($regviewerLocale) = "$communicatorLocale"."regviewer:";

    _InstallResources(":mozilla:xpfe:components:regviewer:MANIFEST-content",        "$regviewerContent");
    _InstallResources(":mozilla:xpfe:components:regviewer:locale:en-US:MANIFEST",   "$regviewerLocale");
    }
    
    # history
    {
    my($historyContent) = "$communicatorContent"."history:";
    my($historyLocale) = "$communicatorLocale"."history:";

    _InstallResources(":mozilla:xpfe:components:history:resources:MANIFEST-content",        "$historyContent");
    _InstallResources(":mozilla:xpfe:components:history:resources:locale:en-US:MANIFEST",   "$historyLocale");
    }
    
    # related
    {
    my($relatedContent) = "$communicatorContent"."related:";
    my($relatedLocale) = "$communicatorLocale"."related:";

    _InstallResources(":mozilla:xpfe:components:related:resources:MANIFEST-content",        "$relatedContent");
    _InstallResources(":mozilla:xpfe:components:related:resources:locale:en-US:MANIFEST",   "$relatedLocale");
    }
    
    # search
    {
    my($searchContent) = "$communicatorContent"."search:";
    my($searchLocale) = "$communicatorLocale"."search:";
    my($searchPlugins) = "${dist_dir}Search Plugins";
    
    _InstallResources(":mozilla:xpfe:components:search:resources:MANIFEST-content",         "$searchContent");
    _InstallResources(":mozilla:xpfe:components:search:resources:locale:en-US:MANIFEST",    "$searchLocale");

    # Make copies (not aliases) of the various search files
    _InstallResources(":mozilla:xpfe:components:search:datasets:MANIFEST",                  "$searchPlugins", 1);
    }
    
    # sidebar
    {
    my($sidebarContent) = "$communicatorContent"."sidebar:";
    my($sidebarLocale) = "$communicatorLocale"."sidebar:";

    _InstallResources(":mozilla:xpfe:components:sidebar:resources:MANIFEST-content",        "$sidebarContent");
    _InstallResources(":mozilla:xpfe:components:sidebar:resources:locale:en-US:MANIFEST",   "$sidebarLocale");
    }
    
    # timebomb (aka tmbmb)
    {
    my($timebombContent) = "$communicatorContent"."timebomb:";
    my($timebombLocale) = "$communicatorLocale"."timebomb:";

    _InstallResources(":mozilla:xpfe:components:timebomb:resources:content:MANIFEST",       "$timebombContent");
    _InstallResources(":mozilla:xpfe:components:timebomb:resources:locale:en-US:MANIFEST",  "$timebombLocale");
    }

    # prefs
    {
    my($prefContent) = "$communicatorContent"."pref:";
    my($prefLocale) = "$communicatorLocale"."pref:";

    _InstallResources(":mozilla:xpfe:components:prefwindow:resources:content:MANIFEST",         "$prefContent", 0);
    _InstallResources(":mozilla:xpfe:components:prefwindow:resources:content:mac:MANIFEST",     "$prefContent", 0);
    _InstallResources(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:MANIFEST",    "$prefLocale", 0);
    }
    
    # wallet                         
    {
    my($walletContent) = "$communicatorContent"."wallet:";
    my($walletLocale) = "$communicatorLocale"."wallet:";

    _InstallResources(":mozilla:extensions:wallet:cookieviewer:MANIFEST",               "$walletContent", 0);
    _InstallResources(":mozilla:extensions:wallet:signonviewer:MANIFEST",               "$walletContent", 0);
    _InstallResources(":mozilla:extensions:wallet:walletpreview:MANIFEST",              "$walletContent", 0);
    _InstallResources(":mozilla:extensions:wallet:editor:MANIFEST",                     "$walletContent", 0);

    _InstallResources(":mozilla:extensions:wallet:cookieviewer:MANIFEST_PROPERTIES",    "$walletLocale", 0);
    _InstallResources(":mozilla:extensions:wallet:signonviewer:MANIFEST_PROPERTIES",    "$walletLocale", 0);
    _InstallResources(":mozilla:extensions:wallet:walletpreview:MANIFEST_PROPERTIES",   "$walletLocale", 0);
    _InstallResources(":mozilla:extensions:wallet:editor:MANIFEST_PROPERTIES",          "$walletLocale", 0);
    _InstallResources(":mozilla:extensions:wallet:src:MANIFEST_PROPERTIES",             "$walletLocale", 0);
    }
    
    # security
    {
    my($securityContent) = "$communicatorContent"."security:";
    my($securityLocale) = "$communicatorLocale"."security:";

    _InstallResources(":mozilla:caps:src:MANIFEST_PROPERTIES",  "$securityLocale", 0);
    }

    # Install skin files
   	InstallSkinFiles("classic"); # fix me
    InstallSkinFiles("blue"); # fix me
    InstallSkinFiles("modern"); # fix me

    # install locale provider
    InstallProviderFiles("locales", "en-DE");
    # install defaults
    InstallLangPackFiles("en-DE");

    print("--- Chrome copying complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// MakeNonChromeAliases
#//--------------------------------------------------------------------------------------------------
sub MakeNonChromeAliases()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    InstallNonChromeResources();
    InstallDefaultsFiles();
    InstallComponentFiles();
}

#//--------------------------------------------------------------------------------------------------
#// MakeResourceAliases
#//--------------------------------------------------------------------------------------------------

sub MakeResourceAliases()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    InstallChromeFiles();
    MakeNonChromeAliases();
}

#//--------------------------------------------------------------------------------------------------
#// ProcessJarManifests
#//--------------------------------------------------------------------------------------------------

sub ProcessJarManifests()
{
    my($dist_dir) = _getDistDirectory();
    my($chrome_dir) = "$dist_dir"."Chrome";

    # a hash of jars passed as context to the following calls
    my(%jars);
    
    if ($main::build{extensions})
    {
      MozJar::CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
      # cview needs a jar.mn file
      # transformiix needs a jar.mn file
    }
    
    MozJar::CreateJarFromManifest(":mozilla:caps:src:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:docshell:base:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:editor:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:embedding:browser:chrome:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:embedding:browser:chrome:locale:en-US:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:extensions:irc:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:extensions:wallet:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:intl:uconv:src:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:layout:html:forms:src:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:mailnews:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:netwerk:resources:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:profile:pref-migrator:resources:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:profile:resources:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:rdf:tests:domds:resources:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:blue:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:communicator:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:communicator:search:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:communicator:sidebar:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:global:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:navigator:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:messenger:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:classic:editor:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:themes:modern:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpcom:base:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:browser:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:browser:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:communicator:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:communicator:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:components:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:components:prefwindow:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:global:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:global:resources:content:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpfe:global:resources:locale:en-US:mac:jar.mn", $chrome_dir, \%jars);
    MozJar::CreateJarFromManifest(":mozilla:xpinstall:res:jar.mn", $chrome_dir, \%jars);

    # bad jar.mn files
#    MozJar::CreateJarFromManifest(":mozilla:extensions:xmlterm:jar.mn", $chrome_dir, \%jars);

    WriteOutJarFiles($chrome_dir, \%jars);
}


#//--------------------------------------------------------------------------------------------------
#// BuildJarFiles
#//--------------------------------------------------------------------------------------------------

sub BuildJarFiles()
{
    unless( $main::build{resources} ) { return; }
    _assertRightDirectory();

    print("--- Starting JAR building ----\n");

    ProcessJarManifests();

    print("--- JAR building done ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Make library aliases
#//--------------------------------------------------------------------------------------------------

sub MakeLibAliases()
{
    my($dist_dir) = _getDistDirectory();

    local(*F);
    my($filepath, $appath, $psi) = (':mozilla:build:mac:idepath.txt');
    if (open(F, $filepath)) {
        $appath = <F>;
        close(F);

        #// ProfilerLib
        if ($main::PROFILE)
        {
            my($profilerlibpath) = $appath;
            $profilerlibpath =~ s/[^:]*$/MacOS Support:Profiler:Profiler Common:ProfilerLib/;
            MakeAlias("$profilerlibpath", "$dist_dir"."Essential Files:");
        }
    }
    else {
        print STDERR "Can't find $filepath\n";
    }
}

#//--------------------------------------------------------------------------------------------------
#// Build the runtime 'dist' directories
#//--------------------------------------------------------------------------------------------------

sub BuildRuntimeDist()
{
    unless ( $main::build{dist} || $main::build{dist_runtime} ) { return;}
    _assertRightDirectory();
    
    my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers

    print("--- Starting Runtime Dist export ----\n");

    #MAC_COMMON
    _InstallFromManifest(":mozilla:build:mac:MANIFEST",                             "$distdirectory:mac:common:");
    _InstallFromManifest(":mozilla:lib:mac:NSRuntime:include:MANIFEST",             "$distdirectory:mac:common:");
    _InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:MANIFEST",              "$distdirectory:mac:common:");
    _InstallFromManifest(":mozilla:lib:mac:MoreFiles:MANIFEST",                     "$distdirectory:mac:common:morefiles:");

    #GC_LEAK_DETECTOR
    _InstallFromManifest(":mozilla:gc:boehm:MANIFEST",                              "$distdirectory:gc:");

    #INCLUDE
    _InstallFromManifest(":mozilla:config:mac:MANIFEST",                            "$distdirectory:config:");
    _InstallFromManifest(":mozilla:config:mac:MANIFEST_config",                     "$distdirectory:config:");
    
    #NSPR 
    _InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",                    "$distdirectory:nspr:");
    _InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:MANIFEST",                 "$distdirectory:nspr:mac:");
    _InstallFromManifest(":mozilla:nsprpub:lib:ds:MANIFEST",                        "$distdirectory:nspr:");
    _InstallFromManifest(":mozilla:nsprpub:lib:libc:include:MANIFEST",              "$distdirectory:nspr:");
    _InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:MANIFEST",              "$distdirectory:nspr:");

    print("--- Runtime Dist export complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build the client 'dist' directories
#//--------------------------------------------------------------------------------------------------

sub BuildClientDist()
{
    unless ( $main::build{dist} ) { return;}
    _assertRightDirectory();
    
    my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers
    my $dist_dir = _getDistDirectory(); # the subdirectory with the libs and executable.

    print("--- Starting Client Dist export ----\n");

    _InstallFromManifest(":mozilla:lib:mac:Misc:MANIFEST",                          "$distdirectory:mac:common:");
    _InstallFromManifest(":mozilla:lib:mac:Instrumentation:MANIFEST",               "$distdirectory:mac:inst:");

    #INCLUDE

    #// To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
    _MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h", ":mozilla:dist:config:MacConfigInclude.h");

    _InstallFromManifest(":mozilla:include:MANIFEST",                               "$distdirectory:include:");

    #INTL
    #CHARDET
    _InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST",                   "$distdirectory:chardet");

    #UCONV
    _InstallFromManifest(":mozilla:intl:uconv:idl:MANIFEST_IDL",                    "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:intl:uconv:public:MANIFEST",                     "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvlatin:MANIFEST",                   "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvja:MANIFEST",                      "$distdirectory:uconv:");
#   _InstallFromManifest(":mozilla:intl:uconv:ucvja2:MANIFEST",                     "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvtw:MANIFEST",                      "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvtw2:MANIFEST",                     "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvcn:MANIFEST",                      "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvko:MANIFEST",                      "$distdirectory:uconv:");
#   _InstallFromManifest(":mozilla:intl:uconv:ucvth:MANIFEST",                      "$distdirectory:uconv:");
#   _InstallFromManifest(":mozilla:intl:uconv:ucvvt:MANIFEST",                      "$distdirectory:uconv:");
    _InstallFromManifest(":mozilla:intl:uconv:ucvibm:MANIFEST",                     "$distdirectory:uconv:");

    #UNICHARUTIL
    _InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST",               "$distdirectory:unicharutil");
#   _InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST_IDL",           "$distdirectory:idl:");

    #LOCALE
    _InstallFromManifest(":mozilla:intl:locale:public:MANIFEST",                    "$distdirectory:locale:");
    _InstallFromManifest(":mozilla:intl:locale:idl:MANIFEST_IDL",                   "$distdirectory:idl:");

    #LWBRK
    _InstallFromManifest(":mozilla:intl:lwbrk:public:MANIFEST",                     "$distdirectory:lwbrk:");

    #STRRES
    _InstallFromManifest(":mozilla:intl:strres:public:MANIFEST_IDL",                "$distdirectory:idl:");

    #JPEG
    _InstallFromManifest(":mozilla:jpeg:MANIFEST",                                  "$distdirectory:jpeg:");

    #LIBREG
    _InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",                "$distdirectory:libreg:");

    #XPCOM
    _InstallFromManifest(":mozilla:xpcom:base:MANIFEST_IDL",                        "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpcom:io:MANIFEST_IDL",                          "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpcom:ds:MANIFEST_IDL",                          "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpcom:threads:MANIFEST_IDL",                     "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpcom:components:MANIFEST_IDL",                  "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpcom:components:MANIFEST_COMPONENTS",           "${dist_dir}Components:");

    _InstallFromManifest(":mozilla:xpcom:base:MANIFEST",                            "$distdirectory:xpcom:");
    _InstallFromManifest(":mozilla:xpcom:components:MANIFEST",                      "$distdirectory:xpcom:");
    _InstallFromManifest(":mozilla:xpcom:ds:MANIFEST",                              "$distdirectory:xpcom:");
    _InstallFromManifest(":mozilla:xpcom:io:MANIFEST",                              "$distdirectory:xpcom:");
    _InstallFromManifest(":mozilla:xpcom:threads:MANIFEST",                         "$distdirectory:xpcom:");
    _InstallFromManifest(":mozilla:xpcom:proxy:public:MANIFEST",                    "$distdirectory:xpcom:");

    _InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST",          "$distdirectory:xpcom:");
    _InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST_IDL",      "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpcom:reflect:xptcall:public:MANIFEST",          "$distdirectory:xpcom:");

    _InstallFromManifest(":mozilla:xpcom:typelib:xpt:public:MANIFEST",              "$distdirectory:xpcom:");
    
    #ZLIB
    _InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",                      "$distdirectory:zlib:");

    #LIBJAR
    _InstallFromManifest(":mozilla:modules:libjar:MANIFEST",                        "$distdirectory:libjar:");
    _InstallFromManifest(":mozilla:modules:libjar:MANIFEST_IDL",                    "$distdirectory:idl:");

    #LIBUTIL
    _InstallFromManifest(":mozilla:modules:libutil:public:MANIFEST",                "$distdirectory:libutil:");

	# APPFILELOCPROVIDER
    _InstallFromManifest(":mozilla:modules:appfilelocprovider:public:MANIFEST",     "$distdirectory:appfilelocprovider:");

	# MPFILELOCPROVIDER
    _InstallFromManifest(":mozilla:modules:mpfilelocprovider:public:MANIFEST",      "$distdirectory:mpfilelocprovider:");

    #SUN_JAVA
    _InstallFromManifest(":mozilla:sun-java:stubs:include:MANIFEST",                "$distdirectory:sun-java:");
    _InstallFromManifest(":mozilla:sun-java:stubs:macjri:MANIFEST",                 "$distdirectory:sun-java:");

    #JS
    _InstallFromManifest(":mozilla:js:src:MANIFEST",                                "$distdirectory:js:");

    #LIVECONNECT
    _InstallFromManifest(":mozilla:js:src:liveconnect:MANIFEST",                    "$distdirectory:liveconnect:");
    
    #XPCONNECT  
    _InstallFromManifest(":mozilla:js:src:xpconnect:idl:MANIFEST",                  "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:js:src:xpconnect:public:MANIFEST",               "$distdirectory:xpconnect:");

    #CAPS
    _InstallFromManifest(":mozilla:caps:include:MANIFEST",                          "$distdirectory:caps:");
    _InstallFromManifest(":mozilla:caps:idl:MANIFEST",                              "$distdirectory:idl:");

    #LIBPREF
    _InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",                "$distdirectory:libpref:");
    _InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST_IDL",            "$distdirectory:idl:");

    #PROFILE
    _InstallFromManifest(":mozilla:profile:public:MANIFEST_IDL",                    "$distdirectory:idl:");

    #PREF_MIGRATOR
    _InstallFromManifest(":mozilla:profile:pref-migrator:public:MANIFEST",          "$distdirectory:profile:");

    #LIBIMAGE
    _InstallFromManifest(":mozilla:modules:libimg:png:MANIFEST",                    "$distdirectory:libimg:");
    _InstallFromManifest(":mozilla:modules:libimg:src:MANIFEST",                    "$distdirectory:libimg:");
    _InstallFromManifest(":mozilla:modules:libimg:public:MANIFEST",                 "$distdirectory:libimg:");
    _InstallFromManifest(":mozilla:modules:libimg:public_com:MANIFEST",             "$distdirectory:libimg:");

    #PLUGIN
    _InstallFromManifest(":mozilla:modules:plugin:nglsrc:MANIFEST",                 "$distdirectory:plugin:");
    _InstallFromManifest(":mozilla:modules:plugin:public:MANIFEST",                 "$distdirectory:plugin:");
    _InstallFromManifest(":mozilla:modules:oji:src:MANIFEST",                       "$distdirectory:oji:");
    _InstallFromManifest(":mozilla:modules:oji:public:MANIFEST",                    "$distdirectory:oji:");
    
    #DB
    _InstallFromManifest(":mozilla:db:mdb:public:MANIFEST",                         "$distdirectory:db:");
    _InstallFromManifest(":mozilla:db:mork:build:MANIFEST",                         "$distdirectory:db:");

    #DBM
    _InstallFromManifest(":mozilla:dbm:include:MANIFEST",                           "$distdirectory:dbm:");
    
    #URILOADER
    _InstallFromManifest(":mozilla:uriloader:base:MANIFEST_IDL",                    "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:uriloader:exthandler:MANIFEST_IDL",              "$distdirectory:idl:");
    
    #NETWERK
    _InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST",                   "$distdirectory:netwerk:");
    _InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST_IDL",               "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:socket:base:MANIFEST_IDL",               "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:protocol:about:public:MANIFEST_IDL",     "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:protocol:data:public:MANIFEST_IDL",      "$distdirectory:idl:");
    #_InstallFromManifest(":mozilla:netwerk:protocol:file:public:MANIFEST_IDL",     "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST_IDL",      "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST",          "$distdirectory:netwerk:");
    _InstallFromManifest(":mozilla:netwerk:protocol:jar:public:MANIFEST_IDL",       "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:protocol:res:public:MANIFEST_IDL",       "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:cache:public:MANIFEST",                  "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:netwerk:mime:public:MANIFEST",                   "$distdirectory:netwerk:");

    #SECURITY
    _InstallFromManifest(":mozilla:extensions:psm-glue:public:MANIFEST",            "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:extensions:psm-glue:src:MANIFEST",               "$distdirectory:include:");

    _InstallFromManifest(":mozilla:security:psm:lib:client:MANIFEST",               "$distdirectory:security:");
    _InstallFromManifest(":mozilla:security:psm:lib:protocol:MANIFEST",             "$distdirectory:security:");
    
    #EXTENSIONS
    _InstallFromManifest(":mozilla:extensions:cookie:MANIFEST",                     "$distdirectory:cookie:");
    _InstallFromManifest(":mozilla:extensions:wallet:public:MANIFEST",              "$distdirectory:wallet:");

    #WEBSHELL
    _InstallFromManifest(":mozilla:webshell:public:MANIFEST",                       "$distdirectory:webshell:");
    _InstallFromManifest(":mozilla:webshell:public:MANIFEST_IDL",                   "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:webshell:tests:viewer:public:MANIFEST",          "$distdirectory:webshell:");

    #LAYOUT
    _InstallFromManifest(":mozilla:layout:build:MANIFEST",                          "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:base:public:MANIFEST",                    "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:base:public:MANIFEST_IDL",                "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:layout:html:content:public:MANIFEST",            "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:document:src:MANIFEST",              "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:document:public:MANIFEST",           "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:style:public:MANIFEST",              "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",                 "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",                  "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:forms:public:MANIFEST",              "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:table:public:MANIFEST",              "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:base:src:MANIFEST",                       "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:events:public:MANIFEST",                  "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:events:src:MANIFEST",                     "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:xml:document:public:MANIFEST",            "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:xml:content:public:MANIFEST",             "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:xsl:document:src:MANIFEST_IDL",           "$distdirectory:idl:");
    if ($main::options{svg})
    {
        _InstallFromManifest(":mozilla:layout:svg:base:public:MANIFEST",                        "$distdirectory:layout:");
    }
    _InstallFromManifest(":mozilla:layout:xul:base:public:Manifest",                "$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:xbl:public:Manifest",                     "$distdirectory:layout:");

    #GFX
    _InstallFromManifest(":mozilla:gfx:public:MANIFEST",                            "$distdirectory:gfx:");
    _InstallFromManifest(":mozilla:gfx:idl:MANIFEST_IDL",                           "$distdirectory:idl:");

    #VIEW
    _InstallFromManifest(":mozilla:view:public:MANIFEST",                           "$distdirectory:view:");

    #DOM
    _InstallFromManifest(":mozilla:dom:public:MANIFEST",                            "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:MANIFEST_IDL",                        "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:dom:public:base:MANIFEST",                       "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:coreDom:MANIFEST",                    "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",                 "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:events:MANIFEST",                     "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:range:MANIFEST",                      "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:html:MANIFEST",                       "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:public:css:MANIFEST",                        "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",                         "$distdirectory:dom:");
    _InstallFromManifest(":mozilla:dom:src:base:MANIFEST",                          "$distdirectory:dom:");

    #JSURL
    _InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST_IDL",                     "$distdirectory:idl:");

    #HTMLPARSER
    _InstallFromManifest(":mozilla:htmlparser:src:MANIFEST",                        "$distdirectory:htmlparser:");

    #EXPAT
    _InstallFromManifest(":mozilla:expat:xmlparse:MANIFEST",                        "$distdirectory:expat:");
    
    #DOCSHELL
    _InstallFromManifest(":mozilla:docshell:base:MANIFEST_IDL",                     "$distdirectory:idl:");

    #EMBEDDING
    _InstallFromManifest(":mozilla:embedding:browser:webbrowser:MANIFEST_IDL",      "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:embedding:browser:setup:MANIFEST_IDL",           "$distdirectory:idl:");

    #WIDGET
    _InstallFromManifest(":mozilla:widget:public:MANIFEST",                         "$distdirectory:widget:");
    _InstallFromManifest(":mozilla:widget:public:MANIFEST_IDL",                     "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",                        "$distdirectory:widget:");
    _InstallFromManifest(":mozilla:widget:timer:public:MANIFEST",                   "$distdirectory:widget:");

    #RDF
    _InstallFromManifest(":mozilla:rdf:base:idl:MANIFEST",                          "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:rdf:base:public:MANIFEST",                       "$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:util:public:MANIFEST",                       "$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:content:public:MANIFEST",                    "$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:datasource:public:MANIFEST",                 "$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:build:MANIFEST",                             "$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:tests:domds:MANIFEST",                       "$distdirectory:idl:");
    
    #CHROME
    _InstallFromManifest(":mozilla:rdf:chrome:public:MANIFEST",                     "$distdirectory:idl:");
    
    #EDITOR
    _InstallFromManifest(":mozilla:editor:idl:MANIFEST",                            "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:editor:txmgr:idl:MANIFEST",                      "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:editor:public:MANIFEST",                         "$distdirectory:editor:");
    _InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",                   "$distdirectory:editor:txmgr");
    _InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",                  "$distdirectory:editor:txtsvc");
    
    #SILENTDL
    #_InstallFromManifest(":mozilla:silentdl:MANIFEST",                             "$distdirectory:silentdl:");

    #XPINSTALL (the one and only!)
    _InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                      "$distdirectory:xpinstall:");

    # XPFE COMPONENTS
    _InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST",                "$distdirectory:xpfe:components");
    _InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST_IDL",            "$distdirectory:idl:");

    my $dir = '';
    for $dir (qw(bookmarks find history related sample search shistory sidebar ucth urlbarhistory xfer))
    {
        _InstallFromManifest(":mozilla:xpfe:components:$dir:public:MANIFEST_IDL",       "$distdirectory:idl:");
    }
    
    _InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST",              "$distdirectory:xpfe:");
    _InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST_IDL",          "$distdirectory:idl:");

    # directory
    _InstallFromManifest(":mozilla:xpfe:components:directory:MANIFEST_IDL",         "$distdirectory:idl:");
    # regviewer
    _InstallFromManifest(":mozilla:xpfe:components:regviewer:MANIFEST_IDL",     "$distdirectory:idl:");
    # autocomplete
    _InstallFromManifest(":mozilla:xpfe:components:autocomplete:public:MANIFEST_IDL", "$distdirectory:idl:");

    # XPAPPS
    _InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST",                  "$distdirectory:xpfe:");
    _InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST_IDL",              "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:xpfe:browser:public:MANIFEST_IDL",               "$distdirectory:idl:");

    # XML-RPC
    _InstallFromManifest(":mozilla:extensions:xml-rpc:idl:MANIFEST_IDL",                "$distdirectory:idl:");

    # MAILNEWS
    _InstallFromManifest(":mozilla:mailnews:public:MANIFEST",                       "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:public:MANIFEST_IDL",                   "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST",                  "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST_IDL",              "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:mailnews:base:build:MANIFEST",                   "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:base:src:MANIFEST",                     "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:base:util:MANIFEST",                    "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST",           "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST_IDL",       "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST",               "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST_IDL",           "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:mailnews:compose:build:MANIFEST",                "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:db:msgdb:public:MANIFEST",              "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:db:msgdb:build:MANIFEST",               "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:local:public:MANIFEST",                 "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:local:build:MANIFEST",                  "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:local:src:MANIFEST",                    "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:imap:public:MANIFEST",                  "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:imap:build:MANIFEST",                   "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:imap:src:MANIFEST",                     "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST",                  "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST_IDL",              "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:mailnews:mime:src:MANIFEST",                     "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:mime:build:MANIFEST",                   "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:mime:emitters:src:MANIFEST",            "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:news:public:MANIFEST",                  "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:news:build:MANIFEST",                   "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST",              "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST_IDL",          "$distdirectory:idl:");
    _InstallFromManifest(":mozilla:mailnews:addrbook:src:MANIFEST",                 "$distdirectory:mailnews:");
    _InstallFromManifest(":mozilla:mailnews:addrbook:build:MANIFEST",               "$distdirectory:mailnews:");
                                                     
    #LDAP
    if ($main::options{ldap})
    {
        _InstallFromManifest(":mozilla:directory:c-sdk:ldap:include:MANIFEST",      "$distdirectory:directory:");
        _InstallFromManifest(":mozilla:directory:xpcom:base:public:MANIFEST",       "$distdirectory:directory:");
        _InstallFromManifest(":mozilla:directory:xpcom:base:public:MANIFEST_IDL",   "$distdirectory:idl:");
    }

    #XMLEXTRAS
    if ($main::options{xmlextras})
    {
        _InstallFromManifest(":mozilla:extensions:xmlextras:base:public:MANIFEST_IDL", "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:extensions:xmlextras:soap:public:MANIFEST_IDL", "$distdirectory:idl:");
    }

    print("--- Client Dist export complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build the 'dist' directory
#//--------------------------------------------------------------------------------------------------

sub BuildDist()
{
    unless ( $main::build{dist} || $main::build{dist_runtime} ) { return;}
    _assertRightDirectory();
    
    # activate MacPerl
    ActivateApplication('McPL');

    my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers
    my $dist_dir = _getDistDirectory(); # the subdirectory with the libs and executable.
    if ($main::CLOBBER_DIST_ALL)
    {
        print "Clobbering ALL files inside :mozilla:dist:\n";
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
    mkpath([ ":mozilla:dist:", ":mozilla:dist:client:", ":mozilla:dist:client_debug:", ":mozilla:dist:client_stubs:" ]);
    mkpath([ ":mozilla:dist:viewer:", ":mozilla:dist:viewer_debug:" ]);
    
    #make default plugins folder so that apprunner won't go looking for 3.0 and 4.0 plugins.
    mkpath([ ":mozilla:dist:viewer:Plug-ins", ":mozilla:dist:viewer_debug:Plug-ins"]);
    mkpath([ ":mozilla:dist:client:Plugins", ":mozilla:dist:client_debug:Plugins"]);
    
    BuildRuntimeDist();
    BuildClientDist();
    
    print("--- Dist export complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build stub projects
#//--------------------------------------------------------------------------------------------------

sub BuildStubs()
{

    unless( $main::build{stubs} ) { return; }
    _assertRightDirectory();

    print("--- Starting Stubs projects ----\n");

    #//
    #// Clean projects
    #//
    _BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",                "Stubs");

    print("--- Stubs projects complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build one project, and make the alias. Parameters
#// are project path, target name, make shlb alias (boolean), make xSYM alias (boolean)
#// 
#// Note that this routine assumes that the target name and the shared libary name
#// are the same.
#//--------------------------------------------------------------------------------------------------

sub BuildOneProject($$$$$)
{
    my ($project_path, $target_name, $alias_shlb, $alias_xSYM, $component) = @_;

    unless ($project_path =~ m/^$main::BUILD_ROOT.+/) { return; }
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();
    
    # Put libraries in "Essential Files" folder, Components in "Components" folder
    my($component_dir) = $component ? "Components:" : "Essential Files:";

    my($project_dir) = $project_path;
    $project_dir =~ s/:[^:]+$/:/;           # chop off leaf name

    if ($main::CLOBBER_LIBS)
    {
        unlink "$project_dir$target_name";              # it's OK if these fail
        unlink "$project_dir$target_name.xSYM";
    }
    
    BuildProject($project_path, $target_name);
    
    $alias_shlb ? MakeAlias("$project_dir$target_name", "$dist_dir$component_dir") : 0;
    $alias_xSYM ? MakeAlias("$project_dir$target_name.xSYM", "$dist_dir$component_dir") : 0;
}

#//--------------------------------------------------------------------------------------------------
#// Build IDL projects
#//--------------------------------------------------------------------------------------------------

sub getCodeWarriorPath($)
{
    my($subfolder)=@_;
    my($filepath, $appath) = (':mozilla:build:mac:idepath.txt');
    if (open(F, $filepath)) {
        $appath = <F>;
        close(F);

        my($codewarrior_root) = $appath;
        $codewarrior_root =~ s/[^:]*$//;
        return ($codewarrior_root . $subfolder);
    } else {
        print("Can't locate CodeWarrior IDE.\n");
        die;
    }
}

sub getCodeWarriorIDEName()
{
     my($subfolder)=@_;
     my($filepath, $appath) = (':mozilla:build:mac:idepath.txt');
     if (open(F, $filepath)) {
         $appath = <F>;
         close(F);

         my(@codewarrior_path) = split(/:/, $appath);
         return pop(@codewarrior_path);
     } else {
         print("Can't locate CodeWarrior IDE.\n");
         die;
     }
}

sub getModificationDate($)
{
    my($filePath)=@_;
    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks) = stat($filePath);
    return $mtime;
}

sub BuildIDLProjects()
{
    unless( $main::build{idl} ) { return; }
    _assertRightDirectory();

    print("--- Starting IDL projects ----\n");

    if ( $main::build{xpidl} )
    {
        #// see if the xpidl compiler/linker has been rebuilt by comparing modification dates.
        my($codewarrior_plugins) = getCodeWarriorPath("CodeWarrior Plugins:");
        my($compiler_path) = $codewarrior_plugins . "Compilers:xpidl";
        my($linker_path) = $codewarrior_plugins . "Linkers:xpt Linker";
        my($compiler_modtime) = (-e $compiler_path ? getModificationDate($compiler_path) : 0);
        my($linker_modtime) = (-e $linker_path ? getModificationDate($linker_path) : 0);

        #// build the IDL compiler itself.
        BuildProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.mcp", "build all");

        #// was the compiler/linker rebuilt? if so, then clobber IDL projects as we go.
        if (getModificationDate($compiler_path) > $compiler_modtime || getModificationDate($linker_path) > $linker_modtime)
        {
            $main::CLOBBER_IDL_PROJECTS = 1;
            print("XPIDL tools have been updated, will clobber all IDL data folders.\n");
            
            # in this situation, we need to quit and restart the IDE to pick up the new plugin
            # sadly, this seems to crash MacPerl or CodeWarrior, so disabled for now.
#           CodeWarriorLib::quit();
#           WaitNextEvent();
#           CodeWarriorLib::activate();
#           WaitNextEvent();
        }
    }

    BuildIDLProject(":mozilla:xpcom:macbuild:XPCOMIDL.mcp",                         "xpcom");

    # necko
    BuildIDLProject(":mozilla:netwerk:macbuild:netwerkIDL.mcp","necko");
    BuildIDLProject(":mozilla:uriloader:macbuild:uriLoaderIDL.mcp",                 "uriloader");
    BuildIDLProject(":mozilla:uriloader:extprotocol:mac:extProtocolIDL.mcp",        "extprotocol");

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
    BuildIDLProject(":mozilla:editor:txmgr:macbuild:txmgrIDL.mcp",                      "txmgr");
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
    BuildIDLProject(":mozilla:xpfe:components:shistory:macbuild:shistoryIDL.mcp", "shistory");
    BuildIDLProject(":mozilla:xpfe:components:related:macbuild:RelatedIDL.mcp",     "related");
    BuildIDLProject(":mozilla:xpfe:components:search:macbuild:SearchIDL.mcp", "search");
    BuildIDLProject(":mozilla:xpfe:components:macbuild:mozcompsIDL.mcp",            "mozcomps");
    BuildIDLProject(":mozilla:xpfe:components:timebomb:macbuild:timebombIDL.mcp", "tmbm");
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
    BuildIDLProject(":mozilla:mailnews:absync:macbuild:abSyncIDL.mcp",      "AbSyncSvc");
    BuildIDLProject(":mozilla:mailnews:db:macbuild:msgDBIDL.mcp",                   "MsgDB");
    BuildIDLProject(":mozilla:mailnews:imap:macbuild:msgimapIDL.mcp",               "MsgImap");
    BuildIDLProject(":mozilla:mailnews:mime:macbuild:mimeIDL.mcp",                  "Mime");
    BuildIDLProject(":mozilla:mailnews:import:macbuild:msgImportIDL.mcp",           "msgImport");

    BuildIDLProject(":mozilla:caps:macbuild:CapsIDL.mcp",                           "caps");

    BuildIDLProject(":mozilla:intl:locale:macbuild:nsLocaleIDL.mcp",                "nsLocale");
    BuildIDLProject(":mozilla:intl:strres:macbuild:strresIDL.mcp",                  "nsIStringBundle");
    BuildIDLProject(":mozilla:intl:unicharutil:macbuild:unicharutilIDL.mcp",        "unicharutil");
    BuildIDLProject(":mozilla:intl:uconv:macbuild:uconvIDL.mcp",                    "uconv");

    if ($main::options{ldap})
    {
        BuildIDLProject(":mozilla:directory:xpcom:macbuild:mozldapIDL.mcp", "mozldap");
    }

    if ($main::options{xmlextras})
    {
        BuildIDLProject(":mozilla:extensions:xmlextras:macbuild:xmlextrasIDL.mcp", "xmlextras");
    }

	# xpt_link MPW tool, needed for merging xpt files (release build)
	
    if ($main::build{xptlink})
    {
        my($codewarrior_msl) = getCodeWarriorPath("MSL:MSL_C:MSL_MacOS:");
    	if ( ! -e $codewarrior_msl . "Lib:PPC:MSL C.PPC MPW(NL).Lib") {
        	print("MSL PPC MPW Lib not found... Let's build it.\n");
        	BuildProject($codewarrior_msl . "Project:PPC:MSL C.PPC MPW.mcp", "MSL C.PPC MPW");
    	}
        BuildOneProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.mcp", "xpt_link", 0, 0, 0);
    }
    
    print("--- IDL projects complete ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// Build runtime projects
#//--------------------------------------------------------------------------------------------------

sub BuildRuntimeProjects()
{
    unless( $main::build{runtime} ) { return; }
    _assertRightDirectory();

    print("--- Starting Runtime projects ----\n");

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    #//
    #// Shared libraries
    #//
    if ( $main::CARBON )
    {
        if ( $main::CARBONLITE ) {
            _BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "Carbon Interfaces (Lite)");
        }
        else {
            _BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "Carbon Interfaces");       
        }
    }
    else
    {
        if ($UNIVERSAL_INTERFACES_VERSION >= 0x0330) {
        _BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "MacOS Interfaces (3.3)");
        } else {
        _BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",            "MacOS Interfaces");
    }
    }
    
    #// Build all of the startup libraries, for Application, Component, and Shared Libraries. These are
    #// required for all subsequent libraries in the system.
    _BuildProject(":mozilla:lib:mac:NSStartup:NSStartup.mcp",                           "NSStartup.all");
    
    #// for NSRuntime under Carbon, don't use BuildOneProject to alias the shlb or the xsym since the 
    #// target names differ from the output names. Make them by hand instead.
    if ( $main::CARBON ) {
        if ($main::PROFILE) {
            BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",                 "NSRuntimeCarbonProfil.shlb", 0, 0, 0);
        }
        else {
            BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",                 "NSRuntimeCarbon$D.shlb", 0, 0, 0);
        }
        _MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", ":mozilla:dist:viewer_debug:Essential Files:");
        if ( $main::ALIAS_SYM_FILES ) {
            _MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb.xSYM", ":mozilla:dist:viewer_debug:Essential Files:");      
        }
    }
    else {
        if ($main::PROFILE) {
            BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",                 "NSRuntimeProfil.shlb", 0, 0, 0);
            _MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", ":mozilla:dist:viewer_debug:Essential Files:");
            if ( $main::ALIAS_SYM_FILES ) {
                _MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb.xSYM", ":mozilla:dist:viewer_debug:Essential Files:");      
            }
        }
        else {
            BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",                 "NSRuntime$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
        }
    }
    
    _BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",          "MoreFiles.o");
    
    #// for MemAllocator under Carbon, right now we have to use the MSL allocators because sfraser's heap zones
    #// don't exist in Carbon. Just use different targets. Since this is a static library, we don't have to fuss
    #// with the aliases and target name mismatches like we did above.
    if ( $main::CARBON ) {
        _BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",       "MemAllocatorCarbon$D.o");  
    }
    else {
        if ($main::GC_LEAK_DETECTOR) {
            _BuildProject(":mozilla:gc:boehm:macbuild:gc.mcp",                      "gc.ppc.lib");
            _MakeAlias(":mozilla:gc:boehm:macbuild:gc.PPC.lib",                     ":mozilla:dist:gc:gc.PPC.lib");
            _BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp", "MemAllocatorGC.o");
        } else {
            _BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp", "MemAllocator$D.o");
        }
    }

    BuildOneProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",                       "NSStdLib$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    #// for NSPR under Carbon, have to link against some additional static libraries when NOT building with TARGET_CARBON.
    if ( $main::CARBON ) {
        BuildOneProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",                  "NSPR20$D.shlb (Carbon)", 0, 0, 0);
        _MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb", ":mozilla:dist:viewer_debug:Essential Files:");
        if ($main::ALIAS_SYM_FILES) {
            _MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb.xSYM", ":mozilla:dist:viewer_debug:Essential Files:");
        }
    } else {
        BuildOneProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",                  "NSPR20$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    }

    print("--- Runtime projects complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build common projects
#//--------------------------------------------------------------------------------------------------

sub BuildCommonProjects()
{
    unless( $main::build{common} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    
    print("--- Starting Common projects ----\n");

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

    BuildOneProject(":mozilla:modules:oji:macbuild:oji.mcp",                    "oji$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:caps:macbuild:Caps.mcp",                          "Caps$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.mcp",            "libpref$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:js:macbuild:XPConnect.mcp",                       "XPConnect$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.mcp",            "libutil$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    BuildOneProject(":mozilla:db:mork:macbuild:mork.mcp",                       "Mork$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:dbm:macbuild:DBM.mcp",                            "DBM$D.o", 0, 0, 0);

    #// Static libraries
    # Static Libs
    BuildOneProject(":mozilla:modules:appfilelocprovider:macbuild:appfilelocprovider.mcp", "appfilelocprovider$D.o", 0, 0, 0);
    MakeAlias(":mozilla:modules:appfilelocprovider:macbuild:appfilelocprovider$D.o", ":mozilla:dist:appfilelocprovider:");
    BuildOneProject(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider.mcp", "mpfilelocprovider$D.o", 0, 0, 0);
    MakeAlias(":mozilla:modules:mpfilelocprovider:macbuild:mpfilelocprovider$D.o", ":mozilla:dist:mpfilelocprovider:");
    
    print("--- Common projects complete ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// Build imglib projects
#//--------------------------------------------------------------------------------------------------

sub BuildImglibProjects()
{
    unless( $main::build{imglib} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    print("--- Starting Imglib projects ----\n");

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

    print("--- Imglib projects complete ----\n");
} # imglib

    
#//--------------------------------------------------------------------------------------------------
#// Build international projects
#//--------------------------------------------------------------------------------------------------

sub BuildInternationalProjects()
{
    unless( $main::build{intl} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    print("--- Starting International projects ----\n");

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

    print("--- International projects complete ----\n");
} # intl


#//--------------------------------------------------------------------------------------------------
#// Build Necko projects
#//--------------------------------------------------------------------------------------------------

sub BuildNeckoProjects()
{
    unless( $main::build{necko} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    print("--- Starting Necko projects ----\n");

    if ( $main::CARBON ) {
        BuildOneProject(":mozilla:netwerk:macbuild:netwerk.mcp",                    "Necko$D.shlb (Carbon)", 0, 0, 0);
        _MakeAlias(":mozilla:netwerk:macbuild:Necko$D.shlb",                        ":mozilla:dist:viewer_debug:Components:");
        if ($main::ALIAS_SYM_FILES) {
            _MakeAlias(":mozilla:netwerk:macbuild:Necko$D.shlb.xSYM",               ":mozilla:dist:viewer_debug:Components:");
        }
    } else {
    BuildOneProject(":mozilla:netwerk:macbuild:netwerk.mcp",                    "Necko$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    }
    
    BuildOneProject(":mozilla:netwerk:macbuild:netwerk2.mcp",                   "Necko2$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:dom:src:jsurl:macbuild:JSUrl.mcp",                "JSUrl$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
          
    print("--- Necko projects complete ----\n");
} # necko


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
    unless( $main::build{security} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my $dist_dir = _getDistDirectory(); # the subdirectory with the libs and executable.

    print("--- Starting Security projects ----\n");

    BuildOneProject(":mozilla:security:nss:macbuild:NSS.mcp","NSS$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:security:psm:lib:macbuild:PSMClient.mcp","PSMClient$D.o", 0, 0, 0);
    BuildOneProject(":mozilla:security:psm:lib:macbuild:PSMProtocol.mcp","PSMProtocol$D.o", 0, 0, 0); 
    BuildOneProject(":mozilla:security:psm:macbuild:PersonalSecurityMgr.mcp","PSMStubs$D.shlb", 0, 0, 0);
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
    	_copy(":mozilla:security:psm:doc:".$file, $doc_dir.$file);
    } 
	
    print("--- Security projects complete ----\n");
} # Security


#//--------------------------------------------------------------------------------------------------
#// Build Browser utils projects
#//--------------------------------------------------------------------------------------------------

sub BuildBrowserUtilsProjects()
{
    unless( $main::build{browserutils} ) { return; }

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";

    print("--- Starting Browser utils projects ----\n");

    BuildOneProject(":mozilla:uriloader:macbuild:uriLoader.mcp",                "uriLoader$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:uriloader:extprotocol:mac:extProtocol.mcp",       "extProtocol$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:profile:macbuild:profile.mcp",                    "profile$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:profile:pref-migrator:macbuild:prefmigrator.mcp", "prefm$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:extensions:cookie:macbuild:cookie.mcp",           "Cookie$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.mcp",           "Wallet$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:extensions:wallet:macbuild:walletviewers.mcp",    "WalletViewers$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:rdf:chrome:build:chrome.mcp",                     "ChomeRegistry$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:rdf:tests:domds:macbuild:DOMDataSource.mcp",      "DOMDataSource$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    print("--- Browser utils projects complete ----\n");
} # browserutils


#//--------------------------------------------------------------------------------------------------
#// Build NGLayout
#//--------------------------------------------------------------------------------------------------

sub BuildLayoutProjects()
{
    unless( $main::build{nglayout} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();
    
    print("--- Starting Layout projects ---\n");

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
    BuildOneProject(":mozilla:widget:macbuild:widget.mcp",                      "widget$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    BuildOneProject(":mozilla:docshell:macbuild:docshell.mcp",                  "docshell$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:webshell:embed:mac:RaptorShell.mcp",              "RaptorShell$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);

    #// XXX this is here because of a very TEMPORARY dependency
    BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",                            "RDFLibrary$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.mcp",                "xpinstall$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:xpinstall:macbuild:xpistub.mcp",                  "xpistub$D.shlb", 1, $main::ALIAS_SYM_FILES, 0);
    if (!($main::PROFILE)) {
        BuildOneProject(":mozilla:xpinstall:wizard:mac:macbuild:MIW.mcp",           "Mozilla Installer$D", 0, 0, 0);
    }
    print("--- Layout projects complete ---\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build Editor Projects
#//--------------------------------------------------------------------------------------------------

sub BuildEditorProjects()
{
    unless( $main::build{editor} ) { return; }
    _assertRightDirectory();
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Editor projects ----\n");

    BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.mcp",                 "EditorTxmgr$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:editor:txtsvc:macbuild:txtsvc.mcp",               "TextServices$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:editor:macbuild:editor.mcp",                      "EditorCore$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    print("--- Editor projects complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build Viewer Projects
#//--------------------------------------------------------------------------------------------------

sub BuildViewerProjects()
{
    unless( $main::build{viewer} ) { return; }
    _assertRightDirectory();
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Viewer projects ----\n");

    BuildOneProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",            "viewer$D",  0, 0, 0);
    BuildOneProject(":mozilla:embedding:browser:macbuild:webBrowser.mcp",       "webBrowser$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    print("--- Viewer projects complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build XPApp Projects
#//--------------------------------------------------------------------------------------------------

sub BuildXPAppProjects()
{
    unless( $main::build{xpapp} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting XPApp projects ----\n");

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

    print("--- XPApp projects complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build Extensions Projects
#//--------------------------------------------------------------------------------------------------

sub BuildExtensionsProjects()
{
    unless( $main::build{extensions} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting Extensions projects ----\n");

    my($chrome_subdir) = "Chrome:";
    my($chrome_dir) = "$dist_dir"."$chrome_subdir";
    my($packages_chrome_dir) = "$chrome_dir" . "packages:";

    # Chatzilla
    _InstallResources(":mozilla:extensions:irc:js:lib:MANIFEST_COMPONENTS",     "${dist_dir}Components");
    
    if (!$main::options{jar_manifests})
    {
      my($chatzilla_packages_chrome_dir) = "$packages_chrome_dir"."chatzilla:";
      my($chatzilla_chatzilla_packages_chrome_dir) = "$chatzilla_packages_chrome_dir"."chatzilla:";
      my($chatzillaContent) = "$chatzilla_chatzilla_packages_chrome_dir"."content:";
      my($chatzillaLocale) = "$chatzilla_chatzilla_packages_chrome_dir"."locale:";
      my($chatzillaSkin) = "$chatzilla_chatzilla_packages_chrome_dir"."skin:";

      my($chatzillaContentLibJS) = "$chatzillaContent"."lib:js:";
      my($chatzillaContentLibXul) = "$chatzillaContent"."lib:xul:";
      
      _InstallResources(":mozilla:extensions:irc:js:lib:MANIFEST",            "$chatzillaContentLibJS");
      _InstallResources(":mozilla:extensions:irc:xul:lib:MANIFEST",           "$chatzillaContentLibXul");
      _InstallResources(":mozilla:extensions:irc:xul:content:MANIFEST",       "$chatzillaContent");
      _InstallResources(":mozilla:extensions:irc:xul:skin:MANIFEST",          "$chatzillaSkin");
    
      my($chatzillaSkinImages) = "$chatzillaSkin"."images:";
      _InstallResources(":mozilla:extensions:irc:xul:skin:images:MANIFEST",       "$chatzillaSkinImages");
      _InstallResources(":mozilla:extensions:irc:xul:locale:en-US:MANIFEST",      "$chatzillaLocale", 0);
    }

    # XML-RPC (whatever that is)
    _InstallFromManifest(":mozilla:extensions:xml-rpc:src:MANIFEST_COMPONENTS", "${dist_dir}Components");
    
    # Component viewer
    if (!$main::options{jar_manifests})
    {
      my($cview_cview_packages_chrome_dir) = "$packages_chrome_dir"."cview:cview:";

      my($cviewContent) = "$cview_cview_packages_chrome_dir"."content:";
      my($cviewLocale) = "$cview_cview_packages_chrome_dir"."locale:";
      my($cviewSkin) = "$cview_cview_packages_chrome_dir"."skin:";
  
      _InstallResources(":mozilla:extensions:cview:resources:content:MANIFEST", "$cviewContent");
      _InstallResources(":mozilla:extensions:cview:resources:skin:MANIFEST", "$cviewSkin");
      _InstallResources(":mozilla:extensions:cview:resources:locale:en-US:MANIFEST", "$cviewLocale", 0);
      
      # this needs a jar.mn file for jar_manifest builds
    }
    
    
    # Transformiix
    if ($main::options{transformiix})
    {
        BuildOneProject(":mozilla:extensions:transformiix:macbuild:transformiix.mcp", "transformiix$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

        if (!$main::options{jar_manifests})
        {
          my($transformiix_transformiix_packages_chrome_dir) = "$packages_chrome_dir"."transformiix:transformiix:";
  
          my($transformiixContent) = "$transformiix_transformiix_packages_chrome_dir"."content:";
          my($transformiixLocale) = "$transformiix_transformiix_packages_chrome_dir"."locale:";
          my($transformiixSkin) = "$transformiix_transformiix_packages_chrome_dir"."skin:";
  
          _InstallResources(":mozilla:extensions:transformiix:source:examples:mozilla:transformiix:content:MANIFEST", "$transformiixContent");
          _InstallResources(":mozilla:extensions:transformiix:source:examples:mozilla:transformiix:skin:MANIFEST", "$transformiixSkin");
          _InstallResources(":mozilla:extensions:transformiix:source:examples:mozilla:transformiix:locale:en-US:MANIFEST", "$transformiixLocale", 0);
          # this needs a jar.mn file
        }
        
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
    
    print("--- Extensions projects complete ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// Build Plugins Projects
#//--------------------------------------------------------------------------------------------------
                                         
sub ImportXMLProject($$)
{
    my ($xml_path, $project_path) = @_;
    my ($codewarrior_ide_name) = getCodeWarriorIDEName();
    my $ascript = <<EOS;
    tell application "$codewarrior_ide_name"
        make new (project document) as ("$project_path") with data ("$xml_path")
    end tell
EOS
	print $ascript."\n";
    MacPerl::DoAppleScript($ascript) or die($^E);
}

sub BuildPluginsProjects()                          
{
    unless( $main::build{plugins} ) { return; }

    # as a temporary measure, make sure that the folder "MacOS Support:JNIHeaders" exists,
    # before we attempt to build the MRJ plugin. This will allow a gradual transition.
    unless( -e getCodeWarriorPath("MacOS Support:JNIHeaders")) { return; }

    my($plugin_path) = ":mozilla:plugin:oji:MRJ:plugin:";
    my($project_path) = $plugin_path . "MRJPlugin.mcp";
    my($xml_path) = $plugin_path . "MRJPlugin.xml";
    my($project_modtime) = (-e $project_path ? getModificationDate($project_path) : 0);
    my($xml_modtime) = (-e $xml_path ? getModificationDate($xml_path) : 0);

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
    my($linker_path) = getCodeWarriorPath("CodeWarrior Plugins:Linkers:Java Linker");
    if (-e $linker_path) {
        print("CodeWarrior Java tools detected, building MRJPlugin.jar.\n");
        BuildProject($project_path, "MRJPlugin.jar");
    }
    # Copy MRJPlugin, MRJPlugin.jar to appropriate plugins folder.
    my($plugin_dist) = _getDistDirectory() . "Plug-ins:";
    MakeAlias($plugin_path . "MRJPlugin", $plugin_dist);
    MakeAlias($plugin_path . "MRJPlugin.jar", $plugin_dist);
}

#//--------------------------------------------------------------------------------------------------
#// Build MailNews Projects
#//--------------------------------------------------------------------------------------------------

sub BuildMailNewsProjects()
{
    unless( $main::build{mailnews} ) { return; }
    _assertRightDirectory();

    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = _getDistDirectory();

    print("--- Starting MailNews projects ----\n");

    BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.mcp",                 "MsgUtil$D.lib", 0, 0, 0);
    BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.mcp",                      "mailnews$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.mcp",                "MsgCompose$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.mcp",                          "MsgDB$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.mcp",                    "MsgLocal$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:imap:macbuild:msgimap.mcp",                      "MsgImap$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:news:macbuild:msgnews.mcp",                      "MsgNews$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbook.mcp",              "MsgAddrbook$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:absync:macbuild:AbSync.mcp",                 "AbSyncSvc$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.mcp",                         "Mime$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:emitters:macbuild:mimeEmitter.mcp",         "mimeEmitter$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:vcard:macbuild:vcard.mcp",       "vcard$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:smimestub:macbuild:smime.mcp", "smime$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:mime:cthandlers:signstub:macbuild:signed.mcp", "signed$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
#   BuildOneProject(":mozilla:mailnews:mime:cthandlers:calendar:macbuild:calendar.mcp", "calendar$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:macbuild:msgImport.mcp",                  "msgImport$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:text:macbuild:msgImportText.mcp",     "msgImportText$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:mailnews:import:eudora:macbuild:msgImportEudora.mcp",     "msgImportEudora$D.shlb", 1, $main::ALIAS_SYM_FILES, 1);

    print("--- MailNews projects complete ----\n");
}

#//--------------------------------------------------------------------------------------------------
#// Build Mozilla
#//--------------------------------------------------------------------------------------------------

sub BuildMozilla()
{
    unless( $main::build{apprunner} ) { return; }

    _assertRightDirectory();
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    BuildOneProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.mcp",           "apprunner$D", 0, 0, 1);


    # build tool to create Component Registry in release builds only.
    if (!($main::DEBUG)) {
        BuildOneProject(":mozilla:xpcom:tools:registry:macbuild:RegXPCOM.mcp", "RegXPCOM", 0, 0, 1);
    }
    
    # copy command line documents into the Apprunner folder and set correctly the signature
    my($dist_dir) = _getDistDirectory();
    my($cmd_file_path) = ":mozilla:xpfe:bootstrap:";
    my($cmd_file) = "";

    $cmd_file = "Mozilla Select Profile";
    _copy( $cmd_file_path . "Mozilla_Select_Profile", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Profile Wizard";
    _copy( $cmd_file_path . "Mozilla_Profile_Wizard", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Profile Manager";
    _copy( $cmd_file_path . "Mozilla_Profile_Manager", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Profile Migration";
    _copy( $cmd_file_path . "Mozilla_Installer", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Addressbook";
    _copy( $cmd_file_path . "Mozilla_Addressbook", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Editor";
    _copy( $cmd_file_path . "Mozilla_Editor", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Message Compose";
    _copy( $cmd_file_path . "Mozilla_Message_Compose", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Messenger";
    _copy( $cmd_file_path . "Mozilla_Messenger", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Preferences";
    _copy( $cmd_file_path . "Mozilla_Preference", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);
    
    $cmd_file = "NSPR Logging";
    _copy( $cmd_file_path . "Mozilla_NSPR_Log", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla JavaScript Console";
    _copy( $cmd_file_path . "Mozilla_JavaScript_Console", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);

    $cmd_file = "Mozilla Bloat URLs";
    _copy( $cmd_file_path . "Mozilla_Bloat_URLs", $dist_dir . $cmd_file );
    MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);
    _copy( ":mozilla:build:bloaturls.txt", $dist_dir . "bloaturls.txt" );
}


#//--------------------------------------------------------------------------------------------------
#// Build everything
#//--------------------------------------------------------------------------------------------------

sub BuildProjects()
{
    MakeLibAliases();

    # activate CodeWarrior
    ActivateApplication('CWIE');

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
    BuildViewerProjects();
    BuildXPAppProjects();
    BuildExtensionsProjects();
    BuildPluginsProjects();
    BuildMailNewsProjects();
    BuildMozilla();

    # do this last so as not to pollute dist with non-include files
    # before building projects.
    # activate CodeWarrior
    ActivateApplication('McPL');
    
    if ($main::options{jar_manifests})
    {
      MakeNonChromeAliases();   # Defaults, JS components etc.
      
      BuildJarFiles();    
    }
    else
    {
      MakeResourceAliases();
      # this builds installed_chrome.txt
      InstallManifestRDFFiles();
    }

    # Set the default skin to be classic
    SetDefaultSkin("classic/1.0"); 
}
