#!perl -w
package			NGLayoutBuildList;

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

@ISA		= qw(Exporter);
@EXPORT		= qw(ConfigureBuildSystem Checkout BuildDist BuildProjects BuildCommonProjects BuildLayoutProjects BuildOneProject);

# NGLayoutBuildList builds the nglayout project
# it is configured by setting the following variables in the caller:
# Usage:
# caller variables that affect behaviour:
# DEBUG		: 1 if we are building a debug version
# 3-part build process: checkout, dist, and build_projects
# Hack alert:
# NGLayout defines are located in :mozilla:config:mac:NGLayoutConfigInclude.h
# An alias "MacConfigInclude.h" to this file is created inside dist:config
# Note that the name of alias is different than the name of the file. This
# is to trick CW into including NGLayout defines 


#//--------------------------------------------------------------------------------------------------
#// Utility routines
#//--------------------------------------------------------------------------------------------------

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
		$datafolder_path =~ s/\.mcp$/ Data:/;		# generate name of the project's data folder.
		print STDERR "Deleting IDL data folder:	 $datafolder_path\n";
		EmptyTree($datafolder_path);
	}

	BuildOneProject($project_path,	"headers", "", 0, 0, 0);
	BuildOneProject($project_path,	$module_name.".xpt", "", 1, 0, 1);
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

	#// For now, if we discover a newer header file than existed in Universal Interfaces 3.2,
	#// we'll assume that 3.3 or later is in use.
	my($universal_interfaces) = getCodeWarriorPath("MacOS Support:Universal:Interfaces:CIncludes:");
	if (-e ($universal_interfaces . "ControlDefinitions.h")) {
		$UNIVERSAL_INTERFACES_VERSION = 0x0330;
	}

	printf("UNIVERSAL_INTERFACES_VERSION = 0x%04X\n", $UNIVERSAL_INTERFACES_VERSION);
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
	
	#//
	#// Checkout commands
	#//
	if ($main::pull{moz})
	{
		$session->checkout("mozilla/nsprpub",	$nsprpub_tag)				|| print "checkout of nsprpub failed\n";		
		$session->checkout("mozilla/security",	"SeaMonkey_M14_BRANCH")		|| print "checkout of security failed\n";		
		$session->checkout("SeaMonkeyAll")									|| 
		     print "MacCVS reported some errors checking out SeaMonkeyAll, but these are probably not serious.\n";
	}
	elsif ($main::pull{runtime})
	{
		$session->checkout("mozilla/build/mac")						|| print "checkout failure";
		$session->checkout("mozilla/lib/mac/InterfaceLib")			|| print "checkout failure";
		$session->checkout("mozilla/config/mac")					|| print "checkout failure";
		$session->checkout("mozilla/gc")							|| print "checkout failure";
		$session->checkout("mozilla/lib/mac/NSStartup")				|| print "checkout failure";
		$session->checkout("mozilla/lib/mac/NSStdLib")				|| print "checkout failure";
		$session->checkout("mozilla/lib/mac/NSRuntime")				|| print "checkout failure";
		$session->checkout("mozilla/lib/mac/MoreFiles")				|| print "checkout failure";
		$session->checkout("mozilla/lib/mac/MacMemoryAllocator")	|| print "checkout failure";
		$session->checkout("mozilla/nsprpub",	$nsprpub_tag)		|| print "checkout failure";
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
		#print("	Doing $_\n");
		if (-l $src_dir.$_)
		{
			print("		$_ is an alias\n");
			next;
		}
		my($file_name) = $src_dir . $_; 
		MakeAlias($file_name, $dest_dir);
	}
}


#//--------------------------------------------------------------------------------------------------
#// Make resource aliases
#//--------------------------------------------------------------------------------------------------

sub MakeResourceAliases()
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
	my($chrome_dir) = "$dist_dir" . "Chrome:";
	my($resource_dir) = "$dist_dir" . "res:";
	my($samples_dir) = "$resource_dir" . "samples:";

	#//
	#// Make aliases of resource files
	#//
	_MakeAlias(":mozilla:layout:html:document:src:ua.css",								"$resource_dir");
	_MakeAlias(":mozilla:layout:html:document:src:html.css",							"$resource_dir");
	_MakeAlias(":mozilla:layout:html:document:src:arrow.gif",							"$resource_dir");	
	_MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",			"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:charsetalias.properties",						"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:acceptlanguage.properties",						"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:maccharset.properties",							"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:charsetData.properties",						"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:charsetTitles.properties",						"$resource_dir");

	_InstallResources(":mozilla:gfx:src:MANIFEST",										"$resource_dir"."gfx:");

	_InstallResources(":mozilla:extensions:wallet:src:MANIFEST",						"$resource_dir");
	my($entitytab_dir) = "$resource_dir" . "entityTables";
	_InstallResources(":mozilla:intl:unicharutil:tables:MANIFEST",						"$entitytab_dir");

	my($html_dir) = "$resource_dir" . "html:";
	_InstallResources(":mozilla:layout:html:base:src:MANIFEST_RES",						"$html_dir");

	my($throbber_dir) = "$resource_dir" . "throbber:";
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:throbber:",				"$throbber_dir");
	
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:samples:",				"$samples_dir");
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:resources:",				"$samples_dir");
	
	my($rdf_dir) = "$resource_dir" . "rdf:";
	BuildFolderResourceAliases(":mozilla:rdf:resources:",								"$rdf_dir");

	my($domds_dir) = "$samples_dir" . "rdf:";
        _InstallResources(":mozilla:rdf:tests:domds:resources:MANIFEST",                                        "$domds_dir");

    # Top level chrome directories

    my($navigator_chrome_dir) = "$chrome_dir" . "navigator:";
    my($navigator_content_chrome_dir) = "$navigator_chrome_dir" . "content:";
    my($navigator_locale_chrome_dir) = "$navigator_chrome_dir" . "locale:";
    my($navigator_skin_chrome_dir) = "$navigator_chrome_dir" . "skin:";

    my($necko_chrome_dir) = "$chrome_dir" . "necko:";
    my($necko_content_chrome_dir) = "$necko_chrome_dir" . "content:";
    my($necko_locale_chrome_dir) = "$necko_chrome_dir" . "locale:";

        my($global_chrome_dir) = "$chrome_dir" . "global:";
        my($global_content_chrome_dir) = "$global_chrome_dir" . "content:";
    my($global_locale_chrome_dir) = "$global_chrome_dir" . "locale:";
    my($global_skin_chrome_dir) = "$global_chrome_dir" . "skin:";

    my($communicator_chrome_dir) = "$chrome_dir" . "communicator:";
    my($communicator_content_chrome_dir) = "$communicator_chrome_dir" . "content:";
    my($communicator_locale_chrome_dir) = "$communicator_chrome_dir" . "locale:";
    my($communicator_skin_chrome_dir) = "$communicator_chrome_dir" . "skin:";

    my($messenger_chrome_dir) = "$chrome_dir" . "messenger:";
    my($messenger_content_chrome_dir) = "$messenger_chrome_dir" . "content:";
    my($messenger_locale_chrome_dir) = "$messenger_chrome_dir" . "locale:";
    my($messenger_skin_chrome_dir) = "$messenger_chrome_dir" . "skin:";

    my($editor_chrome_dir) = "$chrome_dir" . "editor:";
    my($editor_content_chrome_dir) = "$editor_chrome_dir" . "content:";
    my($editor_locale_chrome_dir) = "$editor_chrome_dir" . "locale:";
    my($editor_skin_chrome_dir) = "$editor_chrome_dir" . "skin:";

    my($aim_chrome_dir) = "$chrome_dir" . "aim:";


    # xpinstall lives inside of the top level communicator dist dir

        my($xpinstall_content_chrome_dir) = "$communicator_content_chrome_dir" . "xpinstall:";
        my($xpinstall_locale_chrome_dir) = "$communicator_locale_chrome_dir" . "xpinstall:";
        my($xpinstall_skin_chrome_dir) = "$communicator_skin_chrome_dir" . "xpinstall:";

        _InstallResources(":mozilla:xpinstall:res:locale:en-US:MANIFEST",                                       "$xpinstall_locale_chrome_dir", 0);
        _InstallResources(":mozilla:xpinstall:res:content:MANIFEST",                                            "$xpinstall_content_chrome_dir", 0);
        _InstallResources(":mozilla:xpinstall:res:skin:MANIFEST",                                                       "$xpinstall_skin_chrome_dir", 0);

        # profile lives inside of the top level communicator dist dir

    my($profile_content_chrome_dir) = "$communicator_content_chrome_dir" . "profile";
        my($profile_locale_chrome_dir) = "$communicator_locale_chrome_dir" . "profile";
        my($profile_skin_chrome_dir) = "$communicator_skin_chrome_dir" . "profile";

        _InstallResources(":mozilla:profile:resources:content:MANIFEST",                                        "$profile_content_chrome_dir", 0);
        _InstallResources(":mozilla:profile:resources:skin:MANIFEST",                                           "$profile_skin_chrome_dir", 0);
        _InstallResources(":mozilla:profile:resources:locale:en-US:MANIFEST",                           "$profile_locale_chrome_dir", 0);
        _InstallResources(":mozilla:profile:pref-migrator:resources:content:MANIFEST",          "$profile_content_chrome_dir", 0);
        _InstallResources(":mozilla:profile:pref-migrator:resources:locale:en-US:MANIFEST",     "$profile_locale_chrome_dir", 0);

	# need to duplicate this line if more files in default profile folder
	my($defaults_dir) = "$dist_dir" . "Defaults:";
	mkdir($defaults_dir, 0);

	# Default _profile_ directory stuff
	my($default_profile_dir) = "$defaults_dir"."Profile:";
	mkdir($default_profile_dir, 0);

        _copy(":mozilla:profile:defaults:bookmarks.html",                                                                       "$default_profile_dir"."bookmarks.html");
        _copy(":mozilla:profile:defaults:panels.rdf",                                                                           "$default_profile_dir"."panels.rdf");
        _copy(":mozilla:profile:defaults:search.rdf",                                                                           "$default_profile_dir"."search.rdf");

	# Default _pref_ directory stuff
	my($default_pref_dir) = "$defaults_dir"."Pref:";
	mkdir($default_pref_dir, 0);
        _InstallResources(":mozilla:xpinstall:public:MANIFEST_PREFS",                                           "$default_pref_dir", 0);
        _InstallResources(":mozilla:modules:libpref:src:MANIFEST_PREFS",                                        "$default_pref_dir", 0);
        _InstallResources(":mozilla:modules:libpref:src:init:MANIFEST",                                         "$default_pref_dir", 0);
        _InstallResources(":mozilla:modules:libpref:src:mac:MANIFEST",                                          "$default_pref_dir", 0);

	# NOTE: this will change as we move the toolbar/appshell chrome files to a real place

        _InstallResources(":mozilla:xpfe:browser:resources:content:MANIFEST",                           "$navigator_content_chrome_dir");
        _InstallResources(":mozilla:xpfe:browser:resources:skin:MANIFEST",                                      "$navigator_skin_chrome_dir");
        _InstallResources(":mozilla:xpfe:browser:resources:locale:en-US:MANIFEST",                      "$navigator_locale_chrome_dir", 0);

        #NECKO
        _InstallResources(":mozilla:netwerk:resources:content:MANIFEST",                           "$necko_content_chrome_dir");
        _InstallResources(":mozilla:netwerk:resources:locale:en-US:MANIFEST",                      "$necko_locale_chrome_dir", 0);

        #SECURITY
        _InstallResources(":mozilla:extensions:psm-glue:res:content:MANIFEST_GLOBAL",           "$global_content_chrome_dir");
        _InstallResources(":mozilla:extensions:psm-glue:res:content:MANIFEST_NAV",                      "$navigator_content_chrome_dir");
        _InstallResources(":mozilla:extensions:psm-glue:res:locale:en-US:MANIFEST",                     "$global_locale_chrome_dir");
        _InstallResources(":mozilla:extensions:psm-glue:res:skin:MANIFEST",                                     "$navigator_skin_chrome_dir");

        _InstallResources(":mozilla:xpfe:global:resources:content:MANIFEST",                            "$global_content_chrome_dir");
        _InstallResources(":mozilla:xpfe:global:resources:content:mac:MANIFEST",                        "$global_content_chrome_dir");
        _InstallResources(":mozilla:xpfe:global:resources:skin:MANIFEST",                                       "$global_skin_chrome_dir");
        _InstallResources(":mozilla:xpfe:global:resources:skin:mac:MANIFEST",                           "$chrome_dir");
        _InstallResources(":mozilla:xpfe:global:resources:skin:MANIFEST_CHROME",                        "$chrome_dir");
        _InstallResources(":mozilla:xpfe:global:resources:locale:en-US:MANIFEST",                       "$global_locale_chrome_dir", 0);
        _InstallResources(":mozilla:xpfe:global:resources:locale:en-US:mac:MANIFEST",           "$global_locale_chrome_dir", 0);

        _InstallResources(":mozilla:xpfe:communicator:resources:skin:MANIFEST", "$communicator_skin_chrome_dir");

        _InstallResources(":mozilla:docshell:base:MANIFEST",                                                            "$global_locale_chrome_dir", 0);

        my($layout_locale_hack_dir) = "$communicator_locale_chrome_dir"."layout:";
        mkdir($layout_locale_hack_dir, 0);
        _InstallResources(":mozilla:layout:html:forms:src:MANIFEST_PROPERTIES",                 "$layout_locale_hack_dir", 0);

        _InstallResources(":mozilla:xpfe:browser:src:MANIFEST",                                                         "$samples_dir");

        BuildFolderResourceAliases(":mozilla:xpfe:browser:samples:",                                            "$samples_dir");
        _MakeAlias(":mozilla:xpfe:browser:samples:sampleimages:",                                                       "$samples_dir");
		
	# if ($main::build{editor})
	{
                _InstallResources(":mozilla:editor:ui:composer:content:MANIFEST",                               "$editor_content_chrome_dir", 0);
                _InstallResources(":mozilla:editor:ui:composer:skin:MANIFEST",                                  "$editor_skin_chrome_dir", 0);
                _InstallResources(":mozilla:editor:ui:composer:locale:en-US:MANIFEST",                  "$editor_locale_chrome_dir", 0);

                _InstallResources(":mozilla:editor:ui:dialogs:content:MANIFEST",                                "$editor_content_chrome_dir", 0);
                _InstallResources(":mozilla:editor:ui:dialogs:skin:MANIFEST",                                   "$editor_skin_chrome_dir", 0);
                _InstallResources(":mozilla:editor:ui:dialogs:locale:en-US:MANIFEST",                   "$editor_locale_chrome_dir", 0);
	}

	if ($main::build{extensions})
	{
            # Chatzilla is a toplevel component
                my($chatzilla_bin_dir) = "$chrome_dir"."chatzilla";

                _InstallResources(":mozilla:extensions:irc:js:lib:MANIFEST",                                    "$chatzilla_bin_dir:content:lib:js");
                _InstallResources(":mozilla:extensions:irc:js:lib:MANIFEST_COMPONENTS","${dist_dir}Components");
                _InstallResources(":mozilla:extensions:irc:xul:lib:MANIFEST",                                   "$chatzilla_bin_dir:content:lib:xul");
                _InstallResources(":mozilla:extensions:irc:xul:content:MANIFEST",                               "$chatzilla_bin_dir:content");
                _InstallResources(":mozilla:extensions:irc:xul:skin:MANIFEST",                                  "$chatzilla_bin_dir:skin");
                _InstallResources(":mozilla:extensions:irc:xul:skin:images:MANIFEST",                   "$chatzilla_bin_dir:skin:images");
	}

	# if ($main::build{mailnews})
	{
                # Messenger is a top level component
                my($mailnews_dir) = "$resource_dir" . "messenger";
                my($messenger_chrome_dir) = "$chrome_dir" . "messenger:";
                my($messenger_content_chrome_dir) = "$messenger_chrome_dir" . "content:";
                my($messenger_locale_chrome_dir) = "$messenger_chrome_dir" . "locale:";
                my($messenger_skin_chrome_dir) = "$messenger_chrome_dir" . "skin:";

                # messenger compose resides within messenger
                my($messengercompose_content_chrome_dir) = "$messenger_content_chrome_dir" . "messengercompose:";
                my($messengercompose_locale_chrome_dir) = "$messenger_locale_chrome_dir" . "messengercompose:";
                my($messengercompose_skin_chrome_dir) = "$messenger_skin_chrome_dir" . "messengercompose:";

            # addressbook resides within messenger
            my($addressbook_content_chrome_dir) = "$messenger_content_chrome_dir" . "addressbook:";
                my($addressbook_locale_chrome_dir) = "$messenger_locale_chrome_dir" . "addressbook:";
                my($addressbook_skin_chrome_dir) = "$messenger_skin_chrome_dir" . "addressbook:";

                _InstallResources(":mozilla:mailnews:base:resources:content:MANIFEST",                   "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:resources:content:mac:MANIFEST",               "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:resources:skin:MANIFEST",                              "$messenger_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:resources:locale:en-US:MANIFEST",              "$messenger_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:prefs:resources:content:MANIFEST",     "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:prefs:resources:locale:en-US:MANIFEST", "$messenger_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:prefs:resources:skin:MANIFEST",                "$messenger_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:search:resources:content:MANIFEST",    "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:base:search:resources:locale:en-US:MANIFEST",              "$messenger_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:mime:resources:skin:MANIFEST",                              "$messenger_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:mime:resources:content:MANIFEST",                          "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:mime:emitters:resources:skin:MANIFEST",     "$messenger_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:mime:emitters:resources:content:MANIFEST",         "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:local:resources:skin:MANIFEST",                     "$messenger_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:local:resources:locale:en-US:MANIFEST",     "$messenger_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:news:resources:skin:MANIFEST",                              "$messenger_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:news:resources:content:MANIFEST",                   "$messenger_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:news:resources:locale:en-US:MANIFEST",              "$messenger_locale_chrome_dir", 0);

                _InstallResources(":mozilla:mailnews:imap:resources:locale:en-US:MANIFEST",              "$messenger_locale_chrome_dir", 0);

                _InstallResources(":mozilla:mailnews:mime:resources:MANIFEST",                                   "$messenger_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:mime:cthandlers:resources:MANIFEST",                "$mailnews_dir:messenger:", 0);

                _InstallResources(":mozilla:mailnews:compose:resources:content:MANIFEST",                "$messengercompose_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:compose:resources:skin:MANIFEST",                   "$messengercompose_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:compose:resources:locale:en-US:MANIFEST",   "$messengercompose_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:compose:prefs:resources:content:MANIFEST",  "$messengercompose_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:compose:prefs:resources:locale:en-US:MANIFEST",    "$messengercompose_locale_chrome_dir", 0);

                _InstallResources(":mozilla:mailnews:addrbook:resources:content:MANIFEST",               "$addressbook_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:addrbook:resources:skin:MANIFEST",                  "$addressbook_skin_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:addrbook:resources:locale:en-US:MANIFEST",  "$addressbook_locale_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:addrbook:prefs:resources:content:MANIFEST", "$addressbook_content_chrome_dir", 0);
                _InstallResources(":mozilla:mailnews:addrbook:prefs:resources:locale:en-US:MANIFEST", "$addressbook_locale_chrome_dir", 0);

        _InstallResources(":mozilla:mailnews:import:resources:content:MANIFEST",                "$messenger_content_chrome_dir", 0);
        _InstallResources(":mozilla:mailnews:import:resources:skin:MANIFEST",                   "$messenger_skin_chrome_dir", 0);
        _InstallResources(":mozilla:mailnews:import:resources:locale:en-US:MANIFEST",   "$messenger_locale_chrome_dir", 0);
        _InstallResources(":mozilla:mailnews:import:eudora:resources:locale:en-US:MANIFEST",    "$messenger_locale_chrome_dir", 0);
        _InstallResources(":mozilla:mailnews:import:text:resources:locale:en-US:MANIFEST",      "$messenger_locale_chrome_dir", 0);
	
	}
	
	# copy the chrome registry. We want an actual copy so that changes for custom UI's
	# don't accidentally get checked into the tree. (pinkerton, bug#5296).
	_copy( ":mozilla:rdf:chrome:build:registry.rdf", "$chrome_dir" . "registry.rdf" );
		
	# Install XPFE component resources

        _InstallResources(":mozilla:xpfe:components:find:resources:MANIFEST",                                   "$global_content_chrome_dir");
        _InstallResources(":mozilla:xpfe:components:find:resources:locale:en-US:MANIFEST",                      "$global_locale_chrome_dir");
        {
                my($bookmarks_content_chrome_dir) = "$communicator_content_chrome_dir"."bookmarks:";
                my($bookmarks_locale_chrome_dir) = "$communicator_locale_chrome_dir"."bookmarks:";
                my($bookmarks_skin_chrome_dir) = "$communicator_skin_chrome_dir"."bookmarks:";

                _InstallResources(":mozilla:xpfe:components:bookmarks:resources:MANIFEST-content",      "$bookmarks_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:bookmarks:resources:MANIFEST-skin",         "$bookmarks_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:bookmarks:resources:locale:en-US:MANIFEST", "$bookmarks_locale_chrome_dir");
        }
        {
            my($directory_content_chrome_dir) = "$communicator_content_chrome_dir"."directory:";
                my($directory_locale_chrome_dir) = "$communicator_locale_chrome_dir"."directory:";
                my($directory_skin_chrome_dir) = "$communicator_skin_chrome_dir"."directory:";

                _InstallResources(":mozilla:xpfe:components:directory:MANIFEST-content",                        "$directory_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:directory:MANIFEST-skin",                           "$directory_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:directory:locale:en-US:MANIFEST",                           "$directory_locale_chrome_dir");
        }
        {
            my($regviewer_content_chrome_dir) = "$communicator_content_chrome_dir"."regviewer:";
                my($regviewer_locale_chrome_dir) = "$communicator_locale_chrome_dir"."regviewer:";
                my($regviewer_skin_chrome_dir) = "$communicator_skin_chrome_dir"."regviewer:";

                _InstallResources(":mozilla:xpfe:components:regviewer:MANIFEST-content",                        "$regviewer_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:regviewer:MANIFEST-skin",                           "$regviewer_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:regviewer:locale:en-US:MANIFEST",                           "$regviewer_locale_chrome_dir");
        }
        {
                my($history_content_chrome_dir) = "$communicator_content_chrome_dir"."history:";
                my($history_locale_chrome_dir) = "$communicator_locale_chrome_dir"."history:";
                my($history_skin_chrome_dir) = "$communicator_skin_chrome_dir"."history:";

                _InstallResources(":mozilla:xpfe:components:history:resources:MANIFEST-content",        "$history_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:history:resources:MANIFEST-skin",           "$history_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:history:resources:locale:en-US:MANIFEST",           "$history_locale_chrome_dir");
        }
        {
            my($related_content_chrome_dir) = "$communicator_content_chrome_dir"."related:";
                my($related_locale_chrome_dir) = "$communicator_locale_chrome_dir"."related:";
                my($related_skin_chrome_dir) = "$communicator_skin_chrome_dir"."related:";

                _InstallResources(":mozilla:xpfe:components:related:resources:MANIFEST-content",        "$related_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:related:resources:MANIFEST-skin",           "$related_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:related:resources:locale:en-US:MANIFEST",           "$related_locale_chrome_dir");
        }
        {
            my($search_content_chrome_dir) = "$communicator_content_chrome_dir"."search:";
                my($search_locale_chrome_dir) = "$communicator_locale_chrome_dir"."search:";
                my($search_skin_chrome_dir) = "$communicator_skin_chrome_dir"."search:";

                _InstallResources(":mozilla:xpfe:components:search:resources:MANIFEST-content",         "$search_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:search:resources:MANIFEST-skin",            "$search_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:search:resources:locale:en-US:MANIFEST",            "$search_locale_chrome_dir");

		# Make copies (not aliases) of the various search files
                _InstallResources(":mozilla:xpfe:components:search:datasets:MANIFEST",                  "${dist_dir}Search Plugins", 1);
        }
        {
            my($sidebar_content_chrome_dir) = "$communicator_content_chrome_dir"."sidebar:";
                my($sidebar_locale_chrome_dir) = "$communicator_locale_chrome_dir"."sidebar:";
                my($sidebar_skin_chrome_dir) = "$communicator_skin_chrome_dir"."sidebar:";

                _InstallResources(":mozilla:xpfe:components:sidebar:resources:MANIFEST-content",        "$sidebar_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:sidebar:resources:MANIFEST-skin",           "$sidebar_skin_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:sidebar:resources:locale:en-US:MANIFEST",           "$sidebar_locale_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:sidebar:src:MANIFEST",                                      "${dist_dir}Components");
        }
        {
            my($timebomb_content_chrome_dir) = "$communicator_content_chrome_dir"."timebomb:";
                my($timebomb_locale_chrome_dir) = "$communicator_locale_chrome_dir"."timebomb:";
                my($timebomb_skin_chrome_dir) = "$communicator_skin_chrome_dir"."timebomb:";

                _InstallResources(":mozilla:xpfe:components:timebomb:resources:content:MANIFEST",   "$timebomb_content_chrome_dir");
                _InstallResources(":mozilla:xpfe:components:timebomb:resources:locale:en-US:MANIFEST",  "$timebomb_locale_chrome_dir");
        }

        _InstallResources(":mozilla:xpfe:components:ucth:resources:MANIFEST",                                   "$global_content_chrome_dir");
        _InstallResources(":mozilla:xpfe:components:ucth:resources:locale:en-US:MANIFEST",                      "$global_locale_chrome_dir");
        _InstallResources(":mozilla:xpfe:components:xfer:resources:MANIFEST",                                   "$global_content_chrome_dir");
        _InstallResources(":mozilla:xpfe:components:xfer:resources:locale:en-US:MANIFEST",                      "$global_locale_chrome_dir");

        {
                my($pref_content_chrome_dir) = "$communicator_content_chrome_dir"."pref:";
                my($pref_locale_chrome_dir) = "$communicator_locale_chrome_dir"."pref:";
                my($pref_skin_chrome_dir) = "$communicator_skin_chrome_dir"."pref:";

                _InstallResources(":mozilla:xpfe:components:prefwindow:resources:content:MANIFEST",             "$pref_content_chrome_dir", 0);
                _InstallResources(":mozilla:xpfe:components:prefwindow:resources:skin:MANIFEST",                        "$pref_skin_chrome_dir", 0);
                _InstallResources(":mozilla:xpfe:components:prefwindow:resources:locale:en-US:MANIFEST",        "$pref_locale_chrome_dir", 0);
        }

        _InstallResources(":mozilla:xpfe:components:console:resources:content:MANIFEST",                "$global_content_chrome_dir", 0);
        _InstallResources(":mozilla:xpfe:components:console:resources:skin:MANIFEST",                   "$global_skin_chrome_dir", 0);
        _InstallResources(":mozilla:xpfe:components:console:resources:locale:en-US:MANIFEST",   "$global_locale_chrome_dir", 0);

    # XXX autocomplete needs to move somewhere
        _InstallResources(":mozilla:xpfe:components:autocomplete:resources:skin:MANIFEST",              "$chrome_dir");

        {
                my($wallet_content_chrome_dir) = "$communicator_content_chrome_dir"."wallet:";
                my($wallet_locale_chrome_dir) = "$communicator_locale_chrome_dir"."wallet:";
                my($wallet_skin_chrome_dir) = "$communicator_skin_chrome_dir"."wallet:";

                _InstallResources(":mozilla:extensions:wallet:cookieviewer:MANIFEST",                           "$wallet_content_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:signonviewer:MANIFEST",                           "$wallet_content_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:walletpreview:MANIFEST",                          "$wallet_content_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:editor:MANIFEST",                                         "$wallet_content_chrome_dir", 0);

                _InstallResources(":mozilla:extensions:wallet:cookieviewer:MANIFEST_PROPERTIES",        "$wallet_locale_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:signonviewer:MANIFEST_PROPERTIES",        "$wallet_locale_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:walletpreview:MANIFEST_PROPERTIES",       "$wallet_locale_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:editor:MANIFEST_PROPERTIES",                      "$wallet_locale_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:src:MANIFEST_PROPERTIES",                         "$wallet_locale_chrome_dir", 0);

                _InstallResources(":mozilla:extensions:wallet:editor:MANIFEST_SKIN",                    "$wallet_skin_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:cookieviewer:MANIFEST_SKIN",                      "$wallet_skin_chrome_dir", 0);
                _InstallResources(":mozilla:extensions:wallet:signonviewer:MANIFEST_SKIN",                      "$wallet_skin_chrome_dir", 0);
        }
    {
                my($security_content_chrome_dir) = "$communicator_content_chrome_dir"."security:";
                my($security_locale_chrome_dir) = "$communicator_locale_chrome_dir"."security:";
                my($security_skin_chrome_dir) = "$communicator_skin_chrome_dir"."security:";
                _InstallResources(":mozilla:caps:src:MANIFEST_PROPERTIES",      "$security_locale_chrome_dir", 0);
        }

	# QA Menu
        _InstallResources(":mozilla:intl:strres:tests:MANIFEST",                        "$resource_dir");

	print("--- Resource copying complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// BuildJarFiles
#//--------------------------------------------------------------------------------------------------


sub BuildJarFiles()
{
	unless( $main::build{jars} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	print("--- Starting JAR building ----\n");

	my($chrome_dir) = "$dist_dir"."Chrome";

    CreateJarFile("$chrome_dir:communicator", "$chrome_dir:communicator.jar");
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
        _InstallFromManifest(":mozilla:build:mac:MANIFEST",                                                             "$distdirectory:mac:common:");
        _InstallFromManifest(":mozilla:lib:mac:NSRuntime:include:MANIFEST",                             "$distdirectory:mac:common:");
        _InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:MANIFEST",                              "$distdirectory:mac:common:");
        _InstallFromManifest(":mozilla:lib:mac:MoreFiles:MANIFEST",                                             "$distdirectory:mac:common:morefiles:");

	#GC_LEAK_DETECTOR
        _InstallFromManifest(":mozilla:gc:boehm:MANIFEST",                                                              "$distdirectory:gc:");

        #INCLUDE
        _InstallFromManifest(":mozilla:config:mac:MANIFEST",                                                    "$distdirectory:config:");
        _InstallFromManifest(":mozilla:config:mac:MANIFEST_config",                                             "$distdirectory:config:");
	
	#NSPR	
        _InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",                                    "$distdirectory:nspr:");
        _InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:MANIFEST",                                 "$distdirectory:nspr:mac:");
        _InstallFromManifest(":mozilla:nsprpub:lib:ds:MANIFEST",                                                "$distdirectory:nspr:");
        _InstallFromManifest(":mozilla:nsprpub:lib:libc:include:MANIFEST",                              "$distdirectory:nspr:");
        _InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:MANIFEST",                              "$distdirectory:nspr:");

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

        _InstallFromManifest(":mozilla:lib:mac:Misc:MANIFEST",                                                  "$distdirectory:mac:common:");
#       _InstallFromManifest(":mozilla:lib:mac:Instrumentation:MANIFEST",                               "$distdirectory:mac:inst:");

	#INCLUDE

	#// To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
        _MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h",       ":mozilla:dist:config:MacConfigInclude.h");

        _InstallFromManifest(":mozilla:include:MANIFEST",                                                               "$distdirectory:include:");

	#INTL
	#CHARDET
        _InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST",                                   "$distdirectory:chardet");

	#UCONV
        _InstallFromManifest(":mozilla:intl:uconv:idl:MANIFEST_IDL",                                    "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:intl:uconv:public:MANIFEST",                                             "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvlatin:MANIFEST",                                   "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvja:MANIFEST",                                              "$distdirectory:uconv:");
#       _InstallFromManifest(":mozilla:intl:uconv:ucvja2:MANIFEST",                                             "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvtw:MANIFEST",                                              "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvtw2:MANIFEST",                                             "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvcn:MANIFEST",                                              "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvko:MANIFEST",                                              "$distdirectory:uconv:");
#       _InstallFromManifest(":mozilla:intl:uconv:ucvth:MANIFEST",                                              "$distdirectory:uconv:");
#       _InstallFromManifest(":mozilla:intl:uconv:ucvvt:MANIFEST",                                              "$distdirectory:uconv:");
        _InstallFromManifest(":mozilla:intl:uconv:ucvibm:MANIFEST",                                             "$distdirectory:uconv:");

	#UNICHARUTIL
        _InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST",                               "$distdirectory:unicharutil");
#       _InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST_IDL",                   "$distdirectory:idl:");

	#LOCALE
        _InstallFromManifest(":mozilla:intl:locale:public:MANIFEST",                                    "$distdirectory:locale:");
        _InstallFromManifest(":mozilla:intl:locale:idl:MANIFEST_IDL",                                   "$distdirectory:idl:");

	#LWBRK
        _InstallFromManifest(":mozilla:intl:lwbrk:public:MANIFEST",                                             "$distdirectory:lwbrk:");

	#STRRES
        _InstallFromManifest(":mozilla:intl:strres:public:MANIFEST_IDL",                                "$distdirectory:idl:");

	#JPEG
        _InstallFromManifest(":mozilla:jpeg:MANIFEST",                                                                  "$distdirectory:jpeg:");

	#LIBREG
        _InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",                                "$distdirectory:libreg:");

	#XPCOM
        _InstallFromManifest(":mozilla:xpcom:base:MANIFEST_IDL",                                                "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpcom:io:MANIFEST_IDL",                                                  "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpcom:ds:MANIFEST_IDL",                                                  "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpcom:threads:MANIFEST_IDL",                                             "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpcom:components:MANIFEST_IDL",                                  "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpcom:components:MANIFEST_COMPONENTS",                   "${dist_dir}Components:");

        _InstallFromManifest(":mozilla:xpcom:base:MANIFEST",                                                    "$distdirectory:xpcom:");
        _InstallFromManifest(":mozilla:xpcom:components:MANIFEST",                                              "$distdirectory:xpcom:");
        _InstallFromManifest(":mozilla:xpcom:ds:MANIFEST",                                                              "$distdirectory:xpcom:");
        _InstallFromManifest(":mozilla:xpcom:io:MANIFEST",                                                              "$distdirectory:xpcom:");
        _InstallFromManifest(":mozilla:xpcom:threads:MANIFEST",                                                 "$distdirectory:xpcom:");
        _InstallFromManifest(":mozilla:xpcom:proxy:public:MANIFEST",                                    "$distdirectory:xpcom:");

        _InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST",                  "$distdirectory:xpcom:");
        _InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST_IDL",              "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpcom:reflect:xptcall:public:MANIFEST",                  "$distdirectory:xpcom:");

        _InstallFromManifest(":mozilla:xpcom:typelib:xpt:public:MANIFEST",                              "$distdirectory:xpcom:");
	
	#ZLIB
        _InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",                                              "$distdirectory:zlib:");

	#LIBJAR
        _InstallFromManifest(":mozilla:modules:libjar:MANIFEST",                                                "$distdirectory:libjar:");
        _InstallFromManifest(":mozilla:modules:libjar:MANIFEST_IDL",                                    "$distdirectory:idl:");

	#LIBUTIL
        _InstallFromManifest(":mozilla:modules:libutil:public:MANIFEST",                                "$distdirectory:libutil:");

	#SUN_JAVA
        _InstallFromManifest(":mozilla:sun-java:stubs:include:MANIFEST",                                "$distdirectory:sun-java:");
        _InstallFromManifest(":mozilla:sun-java:stubs:macjri:MANIFEST",                                 "$distdirectory:sun-java:");

	#JS
        _InstallFromManifest(":mozilla:js:src:MANIFEST",                                                                "$distdirectory:js:");

	#LIVECONNECT
        _InstallFromManifest(":mozilla:js:src:liveconnect:MANIFEST",                                    "$distdirectory:liveconnect:");
	
	#XPCONNECT	
        _InstallFromManifest(":mozilla:js:src:xpconnect:idl:MANIFEST",                                  "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:js:src:xpconnect:public:MANIFEST",                               "$distdirectory:xpconnect:");

	#CAPS
        _InstallFromManifest(":mozilla:caps:include:MANIFEST",                                                  "$distdirectory:caps:");
        _InstallFromManifest(":mozilla:caps:idl:MANIFEST",                                                              "$distdirectory:idl:");

	#LIBPREF
        _InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",                                "$distdirectory:libpref:");
        _InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST_IDL",                    "$distdirectory:idl:");

	#PROFILE
        _InstallFromManifest(":mozilla:profile:public:MANIFEST_IDL",                                    "$distdirectory:idl:");

	#PREF_MIGRATOR
        _InstallFromManifest(":mozilla:profile:pref-migrator:public:MANIFEST",                  "$distdirectory:profile:");

	#LIBIMAGE
        _InstallFromManifest(":mozilla:modules:libimg:png:MANIFEST",                                    "$distdirectory:libimg:");
        _InstallFromManifest(":mozilla:modules:libimg:src:MANIFEST",                                    "$distdirectory:libimg:");
        _InstallFromManifest(":mozilla:modules:libimg:public:MANIFEST",                                 "$distdirectory:libimg:");
        _InstallFromManifest(":mozilla:modules:libimg:public_com:MANIFEST",                             "$distdirectory:libimg:");

	#PLUGIN
        _InstallFromManifest(":mozilla:modules:plugin:nglsrc:MANIFEST",                                 "$distdirectory:plugin:");
        _InstallFromManifest(":mozilla:modules:plugin:public:MANIFEST",                                 "$distdirectory:plugin:");
        _InstallFromManifest(":mozilla:modules:plugin:src:MANIFEST",                                    "$distdirectory:plugin:");
        _InstallFromManifest(":mozilla:modules:oji:src:MANIFEST",                                               "$distdirectory:oji:");
        _InstallFromManifest(":mozilla:modules:oji:public:MANIFEST",                                    "$distdirectory:oji:");
	
	#DB
        _InstallFromManifest(":mozilla:db:mdb:public:MANIFEST",                                                 "$distdirectory:db:");
        _InstallFromManifest(":mozilla:db:mork:build:MANIFEST",                                                 "$distdirectory:db:");

	#DBM
        _InstallFromManifest(":mozilla:dbm:include:MANIFEST",                                                   "$distdirectory:dbm:");
	
	#URILOADER
        _InstallFromManifest(":mozilla:uriloader:base:MANIFEST_IDL",                                    "$distdirectory:idl:");
	
	#NETWERK
        _InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST",                                   "$distdirectory:netwerk:");
        _InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST_IDL",                               "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:socket:base:MANIFEST_IDL",                               "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:protocol:about:public:MANIFEST_IDL",             "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:protocol:data:public:MANIFEST_IDL",              "$distdirectory:idl:");
        #_InstallFromManifest(":mozilla:netwerk:protocol:file:public:MANIFEST_IDL",             "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST_IDL",              "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST",                  "$distdirectory:netwerk:");
        _InstallFromManifest(":mozilla:netwerk:protocol:jar:public:MANIFEST_IDL",               "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:protocol:res:public:MANIFEST_IDL",               "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:cache:public:MANIFEST",                                  "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:netwerk:mime:public:MANIFEST",                                   "$distdirectory:netwerk:");

        #SECURITY
        _InstallFromManifest(":mozilla:extensions:psm-glue:public:MANIFEST",                    "$distdirectory:idl:");

        _InstallFromManifest(":mozilla:security:psm:lib:client:MANIFEST",                               "$distdirectory:security:");
        _InstallFromManifest(":mozilla:security:psm:lib:protocol:MANIFEST",                             "$distdirectory:security:");
	
	#EXTENSIONS
        _InstallFromManifest(":mozilla:extensions:cookie:MANIFEST",                                             "$distdirectory:cookie:");
        _InstallFromManifest(":mozilla:extensions:wallet:public:MANIFEST",                              "$distdirectory:wallet:");

	#WEBSHELL
        _InstallFromManifest(":mozilla:webshell:public:MANIFEST",                                               "$distdirectory:webshell:");
        _InstallFromManifest(":mozilla:webshell:tests:viewer:public:MANIFEST",                  "$distdirectory:webshell:");

	#LAYOUT
        _InstallFromManifest(":mozilla:layout:build:MANIFEST",                                                  "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:base:public:MANIFEST",                                    "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:base:public:MANIFEST_IDL",                                "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:layout:html:content:public:MANIFEST",                    "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:document:src:MANIFEST",                              "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:document:public:MANIFEST",                   "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:style:public:MANIFEST",                              "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",                                 "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",                                  "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:forms:public:MANIFEST",                              "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:html:table:public:MANIFEST",                              "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:base:src:MANIFEST",                                               "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:events:public:MANIFEST",                                  "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:events:src:MANIFEST",                                             "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:xml:document:public:MANIFEST",                    "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:xml:content:public:MANIFEST",                             "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:xul:base:public:Manifest",                                "$distdirectory:layout:");
        _InstallFromManifest(":mozilla:layout:xbl:public:Manifest",                                             "$distdirectory:layout:");

	#GFX
        _InstallFromManifest(":mozilla:gfx:public:MANIFEST",                                                    "$distdirectory:gfx:");
        _InstallFromManifest(":mozilla:gfx:idl:MANIFEST_IDL",                                                   "$distdirectory:idl:");

	#VIEW
        _InstallFromManifest(":mozilla:view:public:MANIFEST",                                                   "$distdirectory:view:");

	#DOM
        _InstallFromManifest(":mozilla:dom:public:MANIFEST",                                                    "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:MANIFEST_IDL",                                                "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:dom:public:base:MANIFEST",                                               "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:coreDom:MANIFEST",                                    "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",                                 "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:events:MANIFEST",                                             "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:range:MANIFEST",                                              "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:html:MANIFEST",                                               "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:public:css:MANIFEST",                                                "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",                                                 "$distdirectory:dom:");
        _InstallFromManifest(":mozilla:dom:src:base:MANIFEST",                                                  "$distdirectory:dom:");

	#JSURL
        _InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST_IDL",                                             "$distdirectory:idl:");

	#HTMLPARSER
        _InstallFromManifest(":mozilla:htmlparser:src:MANIFEST",                                                "$distdirectory:htmlparser:");

	#EXPAT
        _InstallFromManifest(":mozilla:expat:xmlparse:MANIFEST",                                                "$distdirectory:expat:");
	
	#DOCSHELL
        _InstallFromManifest(":mozilla:docshell:base:MANIFEST_IDL",                                             "$distdirectory:idl:");

	#EMBEDDING
        _InstallFromManifest(":mozilla:embedding:browser:webbrowser:MANIFEST_IDL",              "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:embedding:browser:setup:MANIFEST_IDL",                   "$distdirectory:idl:");

	#WIDGET
        _InstallFromManifest(":mozilla:widget:public:MANIFEST",                                                 "$distdirectory:widget:");
        _InstallFromManifest(":mozilla:widget:public:MANIFEST_IDL",                                             "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",                                                "$distdirectory:widget:");
        _InstallFromManifest(":mozilla:widget:timer:public:MANIFEST",                                   "$distdirectory:widget:");

	#RDF
        _InstallFromManifest(":mozilla:rdf:base:idl:MANIFEST",                                                  "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:rdf:base:public:MANIFEST",                                               "$distdirectory:rdf:");
        _InstallFromManifest(":mozilla:rdf:util:public:MANIFEST",                                               "$distdirectory:rdf:");
        _InstallFromManifest(":mozilla:rdf:content:public:MANIFEST",                                    "$distdirectory:rdf:");
        _InstallFromManifest(":mozilla:rdf:datasource:public:MANIFEST",                                 "$distdirectory:rdf:");
        _InstallFromManifest(":mozilla:rdf:build:MANIFEST",                                                             "$distdirectory:rdf:");
        _InstallFromManifest(":mozilla:rdf:tests:domds:MANIFEST",                                               "$distdirectory:idl:");
	
	#CHROME
        _InstallFromManifest(":mozilla:rdf:chrome:public:MANIFEST",                                             "$distdirectory:idl:");
	
	#EDITOR
        _InstallFromManifest(":mozilla:editor:idl:MANIFEST",                                                    "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:editor:txmgr:idl:MANIFEST",                                              "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:editor:public:MANIFEST",                                                 "$distdirectory:editor:");
        _InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",                                   "$distdirectory:editor:txmgr");
        _InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",                                  "$distdirectory:editor:txtsvc");
	
	#SILENTDL
        #_InstallFromManifest(":mozilla:silentdl:MANIFEST",                                                             "$distdirectory:silentdl:");

	#XPINSTALL (the one and only!)
        _InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                                              "$distdirectory:xpinstall:");

	# XPFE COMPONENTS
        _InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST",                                "$distdirectory:xpfe:components");
        _InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST_IDL",                    "$distdirectory:idl:");

	my $dir = '';
	for $dir (qw(bookmarks find history related sample search shistory sidebar ucth urlbarhistory xfer)) {
        _InstallFromManifest(":mozilla:xpfe:components:$dir:public:MANIFEST_IDL",               "$distdirectory:idl:");
        }
        _InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST",                          "$distdirectory:xpfe:");
        _InstallFromManifest(":mozilla:xpfe:components:timebomb:MANIFEST_IDL",              "$distdirectory:idl:");

	# directory
        _InstallFromManifest(":mozilla:xpfe:components:directory:MANIFEST_IDL",                 "$distdirectory:idl:");
	# regviewer
         _InstallFromManifest(":mozilla:xpfe:components:regviewer:MANIFEST_IDL",                "$distdirectory:idl:");
	# autocomplete
	_InstallFromManifest(":mozilla:xpfe:components:autocomplete:public:MANIFEST_IDL", "$distdirectory:idl:");

	# XPAPPS
        _InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST",                                  "$distdirectory:xpfe:");
        _InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST_IDL",                              "$distdirectory:idl:");
        _InstallFromManifest(":mozilla:xpfe:browser:public:MANIFEST_IDL",                               "$distdirectory:idl:");
	
	# MAILNEWS
         _InstallFromManifest(":mozilla:mailnews:public:MANIFEST",                                              "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:public:MANIFEST_IDL",                                  "$distdirectory:idl:");
         _InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST",                                 "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST_IDL",                             "$distdirectory:idl:");
         _InstallFromManifest(":mozilla:mailnews:base:build:MANIFEST",                                  "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:base:src:MANIFEST",                                    "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:base:util:MANIFEST",                                   "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST",                  "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST_IDL",              "$distdirectory:idl:");
         _InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST",                              "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST_IDL",                  "$distdirectory:idl:");
         _InstallFromManifest(":mozilla:mailnews:compose:build:MANIFEST",                               "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:db:msgdb:public:MANIFEST",                             "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:db:msgdb:build:MANIFEST",                              "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:local:public:MANIFEST",                                "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:local:build:MANIFEST",                                 "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:local:src:MANIFEST",                                   "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:imap:public:MANIFEST",                                 "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:imap:build:MANIFEST",                                  "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:imap:src:MANIFEST",                                    "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST",                                 "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST_IDL",                             "$distdirectory:idl:");
         _InstallFromManifest(":mozilla:mailnews:mime:src:MANIFEST",                                    "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:mime:build:MANIFEST",                                  "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:mime:emitters:src:MANIFEST",                   "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:news:public:MANIFEST",                                 "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:news:build:MANIFEST",                                  "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST",                             "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST_IDL",                 "$distdirectory:idl:");
         _InstallFromManifest(":mozilla:mailnews:addrbook:src:MANIFEST",                                "$distdirectory:mailnews:");
         _InstallFromManifest(":mozilla:mailnews:addrbook:build:MANIFEST",                              "$distdirectory:mailnews:");
	 
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
	mkpath([ ":mozilla:dist:viewer:Plugins", ":mozilla:dist:viewer_debug:Plugins"]);
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
	_BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",				"Stubs");

	print("--- Stubs projects complete ----\n");
}


#//--------------------------------------------------------------------------------------------------
#// Build one project, and make the alias. Parameters
#// are project path, target name, make shlb alias (boolean), make xSYM alias (boolean)
#// 
#// Note that this routine assumes that the target name and the shared libary name
#// are the same.
#//--------------------------------------------------------------------------------------------------

sub BuildOneProject($$$$$$)
{
	my ($project_path, $target_name, $toc_file, $alias_shlb, $alias_xSYM, $component) = @_;

	unless ($project_path =~ m/^$main::BUILD_ROOT.+/) { return; }
	
	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();
	
	# Put libraries in "Essential Files" folder, Components in "Components" folder
	my($component_dir) = $component ? "Components:" : "Essential Files:";

	my($project_dir) = $project_path;
	$project_dir =~ s/:[^:]+$/:/;			# chop off leaf name

	if ($main::CLOBBER_LIBS)
	{
		unlink "$project_dir$target_name";				# it's OK if these fail
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
#			CodeWarriorLib::quit();
#			WaitNextEvent();
#			CodeWarriorLib::activate();
#			WaitNextEvent();
		}
	}
	
	BuildIDLProject(":mozilla:xpcom:macbuild:XPCOMIDL.mcp",							"xpcom");

	# necko
	BuildIDLProject(":mozilla:netwerk:macbuild:netwerkIDL.mcp","netwerk");

	# protocols
	BuildIDLProject(":mozilla:netwerk:protocol:about:macbuild:aboutIDL.mcp","about");
	BuildIDLProject(":mozilla:netwerk:protocol:data:macbuild:dataIDL.mcp","data");
	#BuildIDLProject(":mozilla:netwerk:protocol:file:macbuild:fileIDL.mcp","file");
	BuildIDLProject(":mozilla:netwerk:protocol:ftp:macbuild:ftpIDL.mcp","ftp");
	BuildIDLProject(":mozilla:netwerk:protocol:http:macbuild:httpIDL.mcp","http");
	BuildIDLProject(":mozilla:netwerk:protocol:jar:macbuild:jarIDL.mcp","jar");
	BuildIDLProject(":mozilla:netwerk:protocol:res:macbuild:resIDL.mcp","res");
	
	# mime service
	# Just a placeholder as mime.xpt is currently part of the netwerkIDL.mcp build

	# cache
	BuildIDLProject(":mozilla:netwerk:cache:macbuild:nkcacheIDL.mcp","nkcache");
	
	# stream conversion.
	BuildIDLProject(":mozilla:netwerk:streamconv:macbuild:streamconvIDL.mcp","streamconv");
	
	BuildIDLProject(":mozilla:uriloader:macbuild:uriLoaderIDL.mcp",					"uriloader");
	BuildIDLProject(":mozilla:uriloader:extprotocol:mac:extProtocolIDL.mcp",		"extprotocol");

	# psm glue
	BuildIDLProject(":mozilla:extensions:psm-glue:macbuild:psmglueIDL.mcp",			"psmglue");	
	
	BuildIDLProject(":mozilla:modules:libpref:macbuild:libprefIDL.mcp",				"libpref");
	BuildIDLProject(":mozilla:modules:libutil:macbuild:libutilIDL.mcp",				"libutil");
	BuildIDLProject(":mozilla:modules:libjar:macbuild:libjarIDL.mcp",				"libjar");
	BuildIDLProject(":mozilla:modules:oji:macbuild:ojiIDL.mcp",						"oji");
	BuildIDLProject(":mozilla:js:macbuild:XPConnectIDL.mcp",						"xpconnect");
	BuildIDLProject(":mozilla:dom:macbuild:domIDL.mcp",								"dom");

	BuildIDLProject(":mozilla:dom:src:jsurl:macbuild:JSUrlDL.mcp",					"jsurl");
	
	BuildIDLProject(":mozilla:gfx:macbuild:gfxIDL.mcp",								"gfx");
	BuildIDLProject(":mozilla:widget:macbuild:widgetIDL.mcp",						"widget");
	BuildIDLProject(":mozilla:editor:macbuild:EditorIDL.mcp",						"editor");
	BuildIDLProject(":mozilla:editor:txmgr:macbuild:txmgrIDL.mcp",						"txmgr");
	BuildIDLProject(":mozilla:profile:macbuild:ProfileServicesIDL.mcp", "profileservices");
	BuildIDLProject(":mozilla:profile:pref-migrator:macbuild:prefmigratorIDL.mcp",	"prefm");
		
	BuildIDLProject(":mozilla:layout:macbuild:layoutIDL.mcp",						"layout");

	BuildIDLProject(":mozilla:rdf:macbuild:RDFIDL.mcp",								"rdf");
	BuildIDLProject(":mozilla:rdf:tests:domds:macbuild:DOMDataSourceIDL.mcp",		"domds");

	BuildIDLProject(":mozilla:rdf:chrome:build:chromeIDL.mcp",						"chrome");
		
	BuildIDLProject(":mozilla:docshell:macbuild:docshellIDL.mcp",					"docshell");
	BuildIDLProject(":mozilla:embedding:browser:macbuild:browserIDL.mcp",			"embeddingbrowser");

	BuildIDLProject(":mozilla:extensions:wallet:macbuild:walletIDL.mcp","wallet");
	BuildIDLProject(":mozilla:xpfe:components:bookmarks:macbuild:BookmarksIDL.mcp", "bookmarks");
	BuildIDLProject(":mozilla:xpfe:components:directory:DirectoryIDL.mcp",			"directory");
	BuildIDLProject(":mozilla:xpfe:components:regviewer:RegViewerIDL.mcp",			"regviewer");
	BuildIDLProject(":mozilla:xpfe:components:history:macbuild:historyIDL.mcp",		"history");
	BuildIDLProject(":mozilla:xpfe:components:shistory:macbuild:shistoryIDL.mcp",	"shistory");
	BuildIDLProject(":mozilla:xpfe:components:related:macbuild:RelatedIDL.mcp",		"related");
	BuildIDLProject(":mozilla:xpfe:components:search:macbuild:SearchIDL.mcp",	"search");
	BuildIDLProject(":mozilla:xpfe:components:macbuild:mozcompsIDL.mcp",			"mozcomps");
	BuildIDLProject(":mozilla:xpfe:components:timebomb:macbuild:timebombIDL.mcp",	"tmbm");
	BuildIDLProject(":mozilla:xpfe:components:urlbarhistory:macbuild:urlbarhistoryIDL.mcp",	"urlbarhistory");
	BuildIDLProject(":mozilla:xpfe:components:autocomplete:macbuild:AutoCompleteIDL.mcp",	"autocomplete");

	BuildIDLProject(":mozilla:xpfe:appshell:macbuild:appshellIDL.mcp",				"appshell");
	
	BuildIDLProject(":mozilla:xpfe:browser:macbuild:mozBrowserIDL.mcp",				"mozBrowser");
	
	BuildIDLProject(":mozilla:xpinstall:macbuild:xpinstallIDL.mcp",					"xpinstall");

	BuildIDLProject(":mozilla:mailnews:base:macbuild:msgCoreIDL.mcp",				"mailnews");
	BuildIDLProject(":mozilla:mailnews:compose:macbuild:msgComposeIDL.mcp",			"MsgCompose");
	BuildIDLProject(":mozilla:mailnews:local:macbuild:msglocalIDL.mcp",				"MsgLocal");
	BuildIDLProject(":mozilla:mailnews:news:macbuild:msgnewsIDL.mcp",				"MsgNews");
	BuildIDLProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbookIDL.mcp",		"MsgAddrbook");
	BuildIDLProject(":mozilla:mailnews:db:macbuild:msgDBIDL.mcp",					"MsgDB");
	BuildIDLProject(":mozilla:mailnews:imap:macbuild:msgimapIDL.mcp",				"MsgImap");
	BuildIDLProject(":mozilla:mailnews:mime:macbuild:mimeIDL.mcp",					"Mime");
	BuildIDLProject(":mozilla:mailnews:import:macbuild:msgImportIDL.mcp",			"msgImport");

	BuildIDLProject(":mozilla:caps:macbuild:CapsIDL.mcp",							"caps");

	BuildIDLProject(":mozilla:intl:locale:macbuild:nsLocaleIDL.mcp",				"nsLocale");
	BuildIDLProject(":mozilla:intl:strres:macbuild:strresIDL.mcp",					"nsIStringBundle");
	BuildIDLProject(":mozilla:intl:unicharutil:macbuild:unicharutilIDL.mcp",		"unicharutil");
	BuildIDLProject(":mozilla:intl:uconv:macbuild:uconvIDL.mcp",					"uconv");

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
			_BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",			"Carbon Interfaces (Lite)");
		}
		else {
			_BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",			"Carbon Interfaces");		
		}
	}
	else
	{
	    if ($UNIVERSAL_INTERFACES_VERSION >= 0x0330) {
    		_BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",			"MacOS Interfaces (3.3)");
	    } else {
    		_BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",			"MacOS Interfaces");
    	}
	}
	
	#// Build all of the startup libraries, for Application, Component, and Shared Libraries. These are
	#// required for all subsequent libraries in the system.
	_BuildProject(":mozilla:lib:mac:NSStartup:NSStartup.mcp",							"NSStartup.all");
	
	#// for NSRuntime under Carbon, don't use BuildOneProject to alias the shlb or the xsym since the 
	#// target names differ from the output names. Make them by hand instead.
	if ( $main::CARBON ) {
		if ($main::PROFILE) {
			BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",					"NSRuntimeCarbonProfil.shlb", "", 0, 0, 0);
		}
		else {
			BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",					"NSRuntimeCarbon$D.shlb", "", 0, 0, 0);
		}
		_MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", ":mozilla:dist:viewer_debug:Essential Files:");
		if ( $main::ALIAS_SYM_FILES ) {
			_MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb.xSYM", ":mozilla:dist:viewer_debug:Essential Files:");		
		}
	}
	else {
		if ($main::PROFILE) {
			BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",					"NSRuntimeProfil.shlb", "", 0, 0, 0);
			_MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", ":mozilla:dist:viewer_debug:Essential Files:");
			if ( $main::ALIAS_SYM_FILES ) {
				_MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb.xSYM", ":mozilla:dist:viewer_debug:Essential Files:");		
			}
		}
		else {
			BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",					"NSRuntime$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);
		}
	}
	
	_BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",			"MoreFiles.o");
	
	#// for MemAllocator under Carbon, right now we have to use the MSL allocators because sfraser's heap zones
	#// don't exist in Carbon. Just use different targets. Since this is a static library, we don't have to fuss
	#// with the aliases and target name mismatches like we did above.
	if ( $main::CARBON ) {
		_BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",		"MemAllocatorCarbon$D.o");	
	}
	else {
		if ($main::GC_LEAK_DETECTOR) {
			_BuildProject(":mozilla:gc:boehm:macbuild:gc.mcp",						"gc.ppc.lib");
			_MakeAlias(":mozilla:gc:boehm:macbuild:gc.PPC.lib",						":mozilla:dist:gc:gc.PPC.lib");
			_BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",	"MemAllocatorGC.o");
		} else {
			_BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",	"MemAllocator$D.o");
		}
	}

	BuildOneProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",						"NSStdLib$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",						"NSPR20$D.shlb", "NSPR20.toc", 1, $main::ALIAS_SYM_FILES, 0);

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

	BuildOneProject(":mozilla:modules:libreg:macbuild:libreg.mcp",				"libreg$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",						"xpcom$D.shlb", "xpcom.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:js:macbuild:JavaScript.mcp",						"JavaScript$D.shlb", "JavaScript.toc", 1, $main::ALIAS_SYM_FILES, 0);	
	BuildOneProject(":mozilla:js:macbuild:JSLoader.mcp",						"JSLoader$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:js:macbuild:LiveConnect.mcp",						"LiveConnect$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",					"zlib$D.shlb", "zlib.toc", 1, $main::ALIAS_SYM_FILES, 0);	
	BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",					"zlib$D.Lib", "", 0, 0, 0);	
	BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",				"libjar$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",				"libjar$D.Lib", "", 0, 0, 0);

	BuildOneProject(":mozilla:modules:oji:macbuild:oji.mcp",					"oji$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:caps:macbuild:Caps.mcp",							"Caps$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.mcp",			"libpref$D.shlb", "libpref.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:js:macbuild:XPConnect.mcp",						"XPConnect$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.mcp",			"libutil$D.shlb", "libutil.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:db:mork:macbuild:mork.mcp",						"Mork$D.shlb", "Mork.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:dbm:macbuild:DBM.mcp",							"DBM$D.o", "", 0, 0, 0);
	
#// XXX moved this TEMPORARILY to layout while we sort out a dependency
#	BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",							"rdf$D.shlb", "rdf.toc", 1, $main::ALIAS_SYM_FILES, 1);

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

	BuildOneProject(":mozilla:jpeg:macbuild:JPEG.mcp",							"JPEG$D.o", "", 0, 0, 0);
	BuildOneProject(":mozilla:modules:libimg:macbuild:png.mcp",					"png$D.o", "", 0, 0, 0);
	BuildOneProject(":mozilla:modules:libimg:macbuild:libimg.mcp",				"libimg$D.shlb", "libimg.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:modules:libimg:macbuild:gifdecoder.mcp",			"gifdecoder$D.shlb", "gifdecoder.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libimg:macbuild:pngdecoder.mcp",			"pngdecoder$D.shlb", "pngdecoder.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libimg:macbuild:jpgdecoder.mcp",			"jpgdecoder$D.shlb", "jpgdecoder.toc", 1, $main::ALIAS_SYM_FILES, 1);

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

	BuildOneProject(":mozilla:intl:chardet:macbuild:chardet.mcp",				"chardet$D.shlb", "chardet.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:uconv.mcp",					"uconv$D.shlb", "uconv.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvlatin.mcp",				"ucvlatin$D.shlb", "uconvlatin.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja.mcp",					"ucvja$D.shlb", "ucvja.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw.mcp",					"ucvtw$D.shlb", "ucvtw.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw2.mcp",					"ucvtw2$D.shlb", "ucvtw2.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvcn.mcp",					"ucvcn$D.shlb", "ucvcn.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvko.mcp",					"ucvko$D.shlb", "ucvko.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvibm.mcp",					"ucvibm$D.shlb", "ucvibm.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:unicharutil:macbuild:unicharutil.mcp",		"unicharutil$D.shlb", "unicharutil.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:locale:macbuild:locale.mcp",					"nslocale$D.shlb", "nslocale.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:lwbrk:macbuild:lwbrk.mcp",					"lwbrk$D.shlb", "lwbrk.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:intl:strres:macbuild:strres.mcp",					"strres$D.shlb", "strres.toc", 1, $main::ALIAS_SYM_FILES, 1);

#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja2.mcp",					"ucvja2$D.shlb", "ucvja2.toc", 1, $main::ALIAS_SYM_FILES, 1);
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvvt.mcp",					"ucvvt$D.shlb", "ucvvt.toc", 1, $main::ALIAS_SYM_FILES, 1);
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvth.mcp",					"ucvth$D.shlb", "ucvth.toc", 1, $main::ALIAS_SYM_FILES, 1);

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

	BuildOneProject(":mozilla:netwerk:macbuild:netwerk.mcp",					"Network$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	# utils library
	BuildOneProject(":mozilla:netwerk:util:macbuild:netwerkUtil.mcp",			"NetworkModular$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	# cache
	BuildOneProject(":mozilla:netwerk:cache:macbuild:nkcache.mcp",				"nkcache$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	
	# protocols
	BuildOneProject(":mozilla:netwerk:protocol:about:macbuild:about.mcp",		"about$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:data:macbuild:data.mcp",			"data$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:datetime:macbuild:datetime.mcp",	"datetime$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:file:macbuild:file.mcp",			"file$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:finger:macbuild:finger.mcp",	"finger$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:ftp:macbuild:ftp.mcp",			"ftp$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:http:macbuild:http.mcp",			"http$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:jar:macbuild:jar.mcp",			"jar$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);	
	BuildOneProject(":mozilla:netwerk:protocol:keyword:macbuild:keyword.mcp",	"keyword$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:protocol:res:macbuild:res.mcp",			"res$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
#	BuildOneProject(":mozilla:netwerk:protocol:resource:macbuild:resource.mcp", "resource$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:dom:src:jsurl:macbuild:JSUrl.mcp",				"JSUrl$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	# mime service
	BuildOneProject(":mozilla:netwerk:mime:macbuild:mime.mcp",					"mimetype$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	
	# stream conversion
	BuildOneProject(":mozilla:netwerk:streamconv:macbuild:streamconv.mcp",		"streamconv$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:netwerk:streamconv:macbuild:multiMixedConv.mcp",	"multiMixedConv$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	# security stuff
#	BuildOneProject(":mozilla:security:psm:lib:macbuild:PSMClient.mcp",			"PSMClient$D.o", "", 0, 0, 0);
#	BuildOneProject(":mozilla:security:psm:lib:macbuild:PSMProtocol.mcp",		"PSMProtocol$D.o", "", 0, 0, 0);	

#	BuildOneProject(":mozilla:extensions:psm-glue:macbuild:PSMGlue.mcp",		"PSMGlue$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);	
	
	print("--- Necko projects complete ----\n");
} # necko


#//--------------------------------------------------------------------------------------------------
#// Build Browser utils projects
#//--------------------------------------------------------------------------------------------------

sub BuildBrowserUtilsProjects()
{
	unless( $main::build{browserutils} ) { return; }

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";

	print("--- Starting Browser utils projects ----\n");

	BuildOneProject(":mozilla:uriloader:macbuild:uriLoader.mcp",				"uriLoader$D.shlb", "uriloader.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:uriloader:extprotocol:mac:extProtocol.mcp",		"extProtocol$D.shlb", "extprotocol.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:profile:macbuild:profile.mcp",					"profile$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:profile:pref-migrator:macbuild:prefmigrator.mcp", "prefm$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:extensions:cookie:macbuild:cookie.mcp",			"Cookie$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.mcp",			"Wallet$D.shlb", "wallet.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:extensions:wallet:macbuild:walletviewers.mcp",	"WalletViewers$D.shlb", "walletviewer.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:rdf:chrome:build:chrome.mcp",						"ChomeRegistry$D.shlb", "chrome.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:rdf:tests:domds:macbuild:DOMDataSource.mcp",		"DOMDataSource$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

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

	BuildOneProject(":mozilla:expat:macbuild:expat.mcp",						"expat$D.o", "", 0, 0, 0);
	BuildOneProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",				"htmlparser$D.shlb", "htmlparser.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:gfx:macbuild:gfx.mcp",							"gfx$D.shlb", "gfx.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:dom:macbuild:dom.mcp",							"dom$D.shlb", "dom.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:modules:plugin:macbuild:plugin.mcp",				"plugin$D.shlb", "plugin.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:layout:macbuild:layout.mcp",						"layout$D.shlb", "layout.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:view:macbuild:view.mcp",							"view$D.shlb", "view.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:widget:macbuild:widget.mcp",						"widget$D.shlb", "widget.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:webshell:macbuild:webshell.mcp",					"webshell$D.shlb", "webshell.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:webshell:embed:mac:RaptorShell.mcp",				"RaptorShell$D.shlb", "RaptorShell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	#// XXX this is here because of a very TEMPORARY dependency
	BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",							"RDFLibrary$D.shlb", "rdf.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.mcp",				"xpinstall$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpinstall:macbuild:xpistub.mcp",					"xpistub$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);
	if (!($main::PROFILE)) {
		BuildOneProject(":mozilla:xpinstall:wizard:mac:macbuild:MIW.mcp",			"Mozilla Installer$D", "", 0, 0, 0);
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

	BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.mcp",					"EditorTxmgr$D.shlb", "txmgr.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:editor:txtsvc:macbuild:txtsvc.mcp",				"TextServices$D.shlb", "txtsvc.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:editor:macbuild:editor.mcp",						"EditorCore$D.shlb", "EditorCore.toc", 1, $main::ALIAS_SYM_FILES, 1);

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

	BuildOneProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",			"viewer$D", "viewer.toc", 0, 0, 0);
	BuildOneProject(":mozilla:embedding:browser:macbuild:webBrowser.mcp",		"webBrowser$D.shlb", "webBrowser.toc", 1, $main::ALIAS_SYM_FILES, 1);

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
	BuildOneProject(":mozilla:xpfe:components:timebomb:macbuild:timebomb.mcp", "tmbm$D.shlb", "tmbmComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:find:macbuild:FindComponent.mcp", "FindComponent$D.shlb", "FindComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:ucth:macbuild:ucth.mcp",	"ucth$D.shlb", "ucth.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:xfer:macbuild:xfer.mcp",	"xfer$D.shlb", "xfer.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:bookmarks:macbuild:Bookmarks.mcp", "Bookmarks$D.shlb", "BookmarksComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:search:macbuild:Search.mcp", "Search$D.shlb", "SearchComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:directory:Directory.mcp", "Directory$D.shlb", "DirectoryComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:regviewer:RegViewer.mcp", "RegViewer$D.shlb", "RegViewerComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:history:macbuild:history.mcp", "history$D.shlb", "historyComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:shistory:macbuild:shistory.mcp", "shistory$D.shlb", "shistoryComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:related:macbuild:Related.mcp", "Related$D.shlb", "RelatedComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:urlbarhistory:macbuild:urlbarhistory.mcp", "urlbarhistory$D.shlb", "urlbarhistoryComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:autocomplete:macbuild:AutoComplete.mcp", "AutoComplete$D.shlb", "AutoComplete.toc", 1, $main::ALIAS_SYM_FILES, 1);		

	# Applications
	BuildOneProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",				"AppShell$D.shlb", "AppShell.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:browser:macbuild:mozBrowser.mcp",			"mozBrowser$D.shlb", "mozBrowser.toc", 1, $main::ALIAS_SYM_FILES, 1);

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

	# not building any extensions yet. chatzilla is all JS and XUL!
	
	print("--- Extensions projects complete ----\n");
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

	BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.mcp",					"MsgUtil$D.lib", "MsgUtil.toc", 0, 0, 0);
	BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.mcp",						"mailnews$D.shlb", "mailnews.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.mcp",				"MsgCompose$D.shlb", "MsgCompose.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.mcp",							"MsgDB$D.shlb", "MsgDB.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.mcp",					"MsgLocal$D.shlb", "MsgLocal.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:imap:macbuild:msgimap.mcp",						"MsgImap$D.shlb", "MsgImap.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:news:macbuild:msgnews.mcp",						"MsgNews$D.shlb", "MsgNews.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbook.mcp",				"MsgAddrbook$D.shlb", "MsgAddrbook.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.mcp",							"Mime$D.shlb", "Mime.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:mime:emitters:macbuild:mimeEmitter.mcp",			"mimeEmitter$D.shlb", "mimeEmitter.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:mime:cthandlers:vcard:macbuild:vcard.mcp",		"vcard$D.shlb", "vcard.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:mime:cthandlers:smimestub:macbuild:smime.mcp",	"smime$D.shlb", "smime.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:mime:cthandlers:signstub:macbuild:signed.mcp",	"signed$D.shlb", "signed.toc", 1, $main::ALIAS_SYM_FILES, 1);
#	BuildOneProject(":mozilla:mailnews:mime:cthandlers:calendar:macbuild:calendar.mcp", "calendar$D.shlb", "calendar.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:import:macbuild:msgImport.mcp", 					"msgImport$D.shlb", "msgImport.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:import:text:macbuild:msgImportText.mcp", 		"msgImportText$D.shlb", "msgImportText.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:mailnews:import:eudora:macbuild:msgImportEudora.mcp", 	"msgImportEudora$D.shlb", "msgImportEudora.toc", 1, $main::ALIAS_SYM_FILES, 1);

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
	BuildOneProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.mcp",			"apprunner$D", "apprunner.toc", 0, 0, 1);


	# build tool to create Component Registry in release builds only.
	if (!($main::DEBUG)) {
		BuildOneProject(":mozilla:xpcom:tools:registry:macbuild:RegXPCOM.mcp", "RegXPCOM", "regxpcom.toc", 0, 0, 1);
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

}


#//--------------------------------------------------------------------------------------------------
#// Build everything
#//--------------------------------------------------------------------------------------------------

sub BuildProjects()
{
	MakeResourceAliases();
	# BuildJarFiles();
	MakeLibAliases();

	# activate CodeWarrior
	ActivateApplication('CWIE');

	BuildIDLProjects();
	BuildStubs();
	BuildRuntimeProjects();
	BuildCommonProjects();
	BuildImglibProjects();
	BuildNeckoProjects();
	BuildBrowserUtilsProjects();	
	BuildInternationalProjects();
	BuildLayoutProjects();
	BuildEditorProjects();
	BuildViewerProjects();
	BuildXPAppProjects();
	BuildExtensionsProjects();
	BuildMailNewsProjects();
	BuildMozilla();
}
