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
use MacCVS;
use MANIFESTO;

@ISA		= qw(Exporter);
@EXPORT		= qw(Checkout BuildDist BuildProjects BuildCommonProjects BuildLayoutProjects BuildOneProject);

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
	BuildOneProject($project_path, 	"headers", "", 0, 0, 0);
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
#// Check out everything
#//--------------------------------------------------------------------------------------------------

sub Checkout()
{
	unless ( $main::pull{all} || $main::pull{runtime} ) { return;}

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

	#//
	#//	Checkout commands
	#//
	if ($main::pull{all})
	{
		$session->checkout("SeaMonkeyEditor")						|| die "checkout failure";
  
		#// beard:  additional libraries needed to make shared libraries link.
		#//$session->checkout("mozilla/lib/mac/PowerPlant")	|| die "checkout failure";
		#//$session->checkout("mozilla/lib/xlate")				|| die "checkout failure";
	} elsif ($main::pull{runtime}) {
		$session->checkout("mozilla/build")							|| die "checkout failure";
		$session->checkout("mozilla/lib/mac/InterfaceLib")	        || die "checkout failure";
		$session->checkout("mozilla/config")						|| die "checkout failure";
		$session->checkout("mozilla/lib/mac/NSStdLib")				|| die "checkout failure";
		$session->checkout("mozilla/lib/mac/NSRuntime")				|| die "checkout failure";
		$session->checkout("mozilla/lib/mac/MoreFiles")				|| die "checkout failure";
		$session->checkout("mozilla/lib/mac/MacMemoryAllocator")	|| die "checkout failure";
		$session->checkout("mozilla/nsprpub")						|| die "checkout failure";  
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
#// Build the runtime 'dist' directories
#//--------------------------------------------------------------------------------------------------

sub BuildRuntimeDist()
{
	unless ( $main::build{dist} || $main::build{dist_runtime} ) { return;}
	_assertRightDirectory();
	
	my $distdirectory = ":mozilla:dist"; # the parent directory in dist, including all the headers

	#MAC_COMMON
	_InstallFromManifest(":mozilla:build:mac:MANIFEST",								"$distdirectory:mac:common:");
	_InstallFromManifest(":mozilla:lib:mac:NSRuntime:include:MANIFEST",				"$distdirectory:mac:common:");
	_InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:MANIFEST",				"$distdirectory:mac:common:");
	_InstallFromManifest(":mozilla:lib:mac:MacMemoryAllocator:include:MANIFEST",		"$distdirectory:mac:common:");
	_InstallFromManifest(":mozilla:lib:mac:MoreFiles:MANIFEST",						"$distdirectory:mac:common:morefiles:");

	#INCLUDE
	_InstallFromManifest(":mozilla:config:mac:MANIFEST",								"$distdirectory:config:");
	_InstallFromManifest(":mozilla:config:mac:MANIFEST_config",						"$distdirectory:config:");
	
	#NSPR	
    _InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",						"$distdirectory:nspr:");		
    _InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:MANIFEST",					"$distdirectory:nspr:mac:");		
    _InstallFromManifest(":mozilla:nsprpub:lib:ds:MANIFEST",							"$distdirectory:nspr:");		
    _InstallFromManifest(":mozilla:nsprpub:lib:libc:include:MANIFEST",				"$distdirectory:nspr:");		
    _InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:MANIFEST",				"$distdirectory:nspr:");

	print("--- Runtime Dist export complete ----\n")
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

	_InstallFromManifest(":mozilla:lib:mac:Misc:MANIFEST",							"$distdirectory:mac:common:");

	#INCLUDE

	#// To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
	_MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h",	":mozilla:dist:config:MacConfigInclude.h");

	_InstallFromManifest(":mozilla:include:MANIFEST",								"$distdirectory:include:");		
	_InstallFromManifest(":mozilla:cmd:macfe:pch:MANIFEST",							"$distdirectory:include:");
	_InstallFromManifest(":mozilla:cmd:macfe:utility:MANIFEST",						"$distdirectory:include:");

	#INTL
	#CHARDET
	_InstallFromManifest(":mozilla:intl:chardet:public:MANIFEST",				"$distdirectory:chardet");

	#UCONV
	_InstallFromManifest(":mozilla:intl:uconv:public:MANIFEST",						"$distdirectory:uconv:");
	_InstallFromManifest(":mozilla:intl:uconv:ucvlatin:MANIFEST",					"$distdirectory:uconv:");
	_InstallFromManifest(":mozilla:intl:uconv:ucvja:MANIFEST",						"$distdirectory:uconv:");
#	_InstallFromManifest(":mozilla:intl:uconv:ucvja2:MANIFEST",						"$distdirectory:uconv:");
	_InstallFromManifest(":mozilla:intl:uconv:ucvtw:MANIFEST",						"$distdirectory:uconv:");
	_InstallFromManifest(":mozilla:intl:uconv:ucvtw2:MANIFEST",						"$distdirectory:uconv:");
	_InstallFromManifest(":mozilla:intl:uconv:ucvcn:MANIFEST",						"$distdirectory:uconv:");
	_InstallFromManifest(":mozilla:intl:uconv:ucvko:MANIFEST",						"$distdirectory:uconv:");
#	_InstallFromManifest(":mozilla:intl:uconv:ucvth:MANIFEST",						"$distdirectory:uconv:");
#	_InstallFromManifest(":mozilla:intl:uconv:ucvvt:MANIFEST",						"$distdirectory:uconv:");


	#UNICHARUTIL
	_InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST",				"$distdirectory:unicharutil");

	#LOCALE
	_InstallFromManifest(":mozilla:intl:locale:public:MANIFEST",						"$distdirectory:locale:");
	_InstallFromManifest(":mozilla:intl:locale:idl:MANIFEST_IDL",			"$distdirectory:idl:");

	#LWBRK
	_InstallFromManifest(":mozilla:intl:lwbrk:public:MANIFEST",						"$distdirectory:lwbrk:");

	#STRRES
	_InstallFromManifest(":mozilla:intl:strres:public:MANIFEST_IDL",				"$distdirectory:idl:");

	#JPEG
    _InstallFromManifest(":mozilla:jpeg:MANIFEST",									"$distdirectory:jpeg:");

	#LIBREG
    _InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",					"$distdirectory:libreg:");

	#XPCOM
	_InstallFromManifest(":mozilla:xpcom:base:MANIFEST_IDL",							"$distdirectory:idl:");
	_InstallFromManifest(":mozilla:xpcom:io:MANIFEST_IDL",							"$distdirectory:idl:");
	_InstallFromManifest(":mozilla:xpcom:ds:MANIFEST_IDL",							"$distdirectory:idl:");

	_InstallFromManifest(":mozilla:xpcom:base:MANIFEST",								"$distdirectory:xpcom:");
	_InstallFromManifest(":mozilla:xpcom:components:MANIFEST",						"$distdirectory:xpcom:");
	_InstallFromManifest(":mozilla:xpcom:ds:MANIFEST",								"$distdirectory:xpcom:");
	_InstallFromManifest(":mozilla:xpcom:io:MANIFEST",								"$distdirectory:xpcom:");
	_InstallFromManifest(":mozilla:xpcom:threads:MANIFEST",							"$distdirectory:xpcom:");
	_InstallFromManifest(":mozilla:xpcom:proxy:public:MANIFEST",						"$distdirectory:xpcom:");

	_InstallFromManifest(":mozilla:xpcom:reflect:xptinfo:public:MANIFEST",			"$distdirectory:xpcom:");
	_InstallFromManifest(":mozilla:xpcom:reflect:xptcall:public:MANIFEST",			"$distdirectory:xpcom:");

	_InstallFromManifest(":mozilla:xpcom:typelib:xpt:public:MANIFEST",				"$distdirectory:xpcom:");
	
	#PREFS
	_InstallFromManifest(":mozilla:modules:libpref:src:MANIFEST_PREFS",				$dist_dir."Components:", 1);

	#ZLIB
    _InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",						"$distdirectory:zlib:");

    #LIBJAR
    _InstallFromManifest(":mozilla:modules:libjar:MANIFEST",                         "$distdirectory:libjar:");
    _InstallFromManifest(":mozilla:modules:libjar:MANIFEST_IDL",						"$distdirectory:idl:");

	#LIBUTIL
    _InstallFromManifest(":mozilla:modules:libutil:public:MANIFEST",					"$distdirectory:libutil:");

	#SUN_JAVA
    _InstallFromManifest(":mozilla:sun-java:stubs:include:MANIFEST",					"$distdirectory:sun-java:");
    _InstallFromManifest(":mozilla:sun-java:stubs:macjri:MANIFEST",					"$distdirectory:sun-java:");

	#NAV_JAVA
    _InstallFromManifest(":mozilla:nav-java:stubs:include:MANIFEST",					"$distdirectory:nav-java:");
    _InstallFromManifest(":mozilla:nav-java:stubs:macjri:MANIFEST",					"$distdirectory:nav-java:");

	#JS
    _InstallFromManifest(":mozilla:js:src:MANIFEST",									"$distdirectory:js:");

	#LIVECONNECT
	_InstallFromManifest(":mozilla:js:src:liveconnect:MANIFEST",						"$distdirectory:liveconnect:");
	
	#XPCONNECT	
	_InstallFromManifest(":mozilla:js:src:xpconnect:idl:MANIFEST",					"$distdirectory:idl:");
	_InstallFromManifest(":mozilla:js:src:xpconnect:public:MANIFEST",				"$distdirectory:xpconnect:");

	#CAPS
	_InstallFromManifest(":mozilla:caps:include:MANIFEST",							"$distdirectory:caps:");
	_InstallFromManifest(":mozilla:caps:idl:MANIFEST",							"$distdirectory:idl:");

	#SECURITY_freenav
    _InstallFromManifest(":mozilla:modules:security:freenav:MANIFEST",				"$distdirectory:security:");

	#LIBPREF
    _InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",					"$distdirectory:libpref:");

	#PROFILE
    _InstallFromManifest(":mozilla:profile:public:MANIFEST",							"$distdirectory:profile:");
    _InstallFromManifest(":mozilla:profile:idlservices:MANIFEST",								"$distdirectory:idl:");

	#LIBIMAGE
    _InstallFromManifest(":mozilla:modules:libimg:png:MANIFEST",						"$distdirectory:libimg:");
    _InstallFromManifest(":mozilla:modules:libimg:src:MANIFEST",						"$distdirectory:libimg:");
    _InstallFromManifest(":mozilla:modules:libimg:public:MANIFEST",					"$distdirectory:libimg:");
    _InstallFromManifest(":mozilla:modules:libimg:public_com:MANIFEST",				"$distdirectory:libimg:");

	#PLUGIN
    _InstallFromManifest(":mozilla:modules:plugin:nglsrc:MANIFEST",					"$distdirectory:plugin:");
    _InstallFromManifest(":mozilla:modules:plugin:public:MANIFEST",					"$distdirectory:plugin:");
    _InstallFromManifest(":mozilla:modules:plugin:src:MANIFEST",						"$distdirectory:plugin:");
    _InstallFromManifest(":mozilla:modules:oji:src:MANIFEST",						"$distdirectory:oji:");
    _InstallFromManifest(":mozilla:modules:oji:public:MANIFEST",						"$distdirectory:oji:");

	#DBM
    _InstallFromManifest(":mozilla:dbm:include:MANIFEST",							"$distdirectory:dbm:");
	
	#LAYERS (IS THIS STILL NEEDED)
	_InstallFromManifest(":mozilla:lib:liblayer:include:MANIFEST",					"$distdirectory:layers:");

	if ( $main::NECKO )
	{
	#NETWERK
		_InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST",					"$distdirectory:netwerk:");
		_InstallFromManifest(":mozilla:netwerk:util:public:MANIFEST",					"$distdirectory:netwerk:");
   		_InstallFromManifest(":mozilla:netwerk:base:public:MANIFEST_IDL",				"$distdirectory:idl:");
   		_InstallFromManifest(":mozilla:netwerk:protocol:http:public:MANIFEST",			"$distdirectory:netwerk:");
	} else {
	#NETWORK
	    _InstallFromManifest(":mozilla:network:public:MANIFEST",						"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:cache:MANIFEST",							"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:client:MANIFEST",						"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:cnvts:MANIFEST",							"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:cstream:MANIFEST",						"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:main:MANIFEST",							"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:mimetype:MANIFEST",						"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:util:MANIFEST",							"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:about:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:certld:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:dataurl:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:file:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:ftp:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:gopher:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:http:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:js:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:mailbox:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:marimba:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:nntp:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:pop3:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:remote:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:smtp:MANIFEST",					"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:protocol:sockstub:MANIFEST",				"$distdirectory:network:");
	    _InstallFromManifest(":mozilla:network:module:MANIFEST",						"$distdirectory:network:module");
	}

	#EXTENSIONS
    _InstallFromManifest(":mozilla:extensions:cookie:MANIFEST",				"$distdirectory:cookie:");
    _InstallFromManifest(":mozilla:extensions:wallet:public:MANIFEST",				"$distdirectory:wallet:");

	#WEBSHELL
    _InstallFromManifest(":mozilla:webshell:public:MANIFEST",						"$distdirectory:webshell:");
    _InstallFromManifest(":mozilla:webshell:tests:viewer:public:MANIFEST",  			"$distdirectory:webshell:");

	#LAYOUT
    _InstallFromManifest(":mozilla:layout:build:MANIFEST",							"$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:base:public:MANIFEST",						"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:html:content:public:MANIFEST",				"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:html:document:src:MANIFEST",				"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:html:document:public:MANIFEST",			"$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:style:public:MANIFEST",				"$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",					"$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",					"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:html:forms:public:MANIFEST",				"$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:html:table:public:MANIFEST",				"$distdirectory:layout:");
    _InstallFromManifest(":mozilla:layout:base:src:MANIFEST",						"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:events:public:MANIFEST",					"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:events:src:MANIFEST",						"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:xml:document:public:MANIFEST",				"$distdirectory:layout:");
	_InstallFromManifest(":mozilla:layout:xml:content:public:MANIFEST",				"$distdirectory:layout:");

	#GFX
    _InstallFromManifest(":mozilla:gfx:public:MANIFEST",								"$distdirectory:gfx:");

	#VIEW
    _InstallFromManifest(":mozilla:view:public:MANIFEST",							"$distdirectory:view:");

	#DOM
   _InstallFromManifest(":mozilla:dom:public:MANIFEST",								"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:MANIFEST_IDL",							"$distdirectory:idl:");
   _InstallFromManifest(":mozilla:dom:public:base:MANIFEST",							"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:coreDom:MANIFEST",						"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",					"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:events:MANIFEST",						"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:range:MANIFEST",						"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:html:MANIFEST",							"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:public:css:MANIFEST",							"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",							"$distdirectory:dom:");
   _InstallFromManifest(":mozilla:dom:src:base:MANIFEST",							"$distdirectory:dom:");

	#HTMLPARSER
	_InstallFromManifest(":mozilla:htmlparser:src:MANIFEST",							"$distdirectory:htmlparser:");

	#EXPAT
	_InstallFromManifest(":mozilla:expat:xmlparse:MANIFEST",							"$distdirectory:expat:");
      
	#WIDGET
    _InstallFromManifest(":mozilla:widget:public:MANIFEST",							"$distdirectory:widget:");
    _InstallFromManifest(":mozilla:widget:public:MANIFEST_IDL",						"$distdirectory:idl:");
    _InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",							"$distdirectory:widget:");

    #RDF
    _InstallFromManifest(":mozilla:rdf:base:idl:MANIFEST",							"$distdirectory:idl:");
    _InstallFromManifest(":mozilla:rdf:base:public:MANIFEST",						"$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:util:public:MANIFEST",						"$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:content:public:MANIFEST",						"$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:datasource:public:MANIFEST",					"$distdirectory:rdf:");
    _InstallFromManifest(":mozilla:rdf:build:MANIFEST",								"$distdirectory:rdf:");
    
    #BRPROF
	_InstallFromManifest(":mozilla:rdf:brprof:public:MANIFEST",						"$distdirectory:brprof:");
    
    #CHROME
	_InstallFromManifest(":mozilla:rdf:chrome:public:MANIFEST",                      "$distdirectory:chrome:");
    
	#EDITOR
	_InstallFromManifest(":mozilla:editor:idl:MANIFEST",								"$distdirectory:idl:");
	_InstallFromManifest(":mozilla:editor:public:MANIFEST",							"$distdirectory:editor:");
	_InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",					"$distdirectory:editor:txmgr");
	_InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",					"$distdirectory:editor:txtsvc");
  
    #SILENTDL
    _InstallFromManifest(":mozilla:silentdl:MANIFEST",								"$distdirectory:silentdl:");

    #XPINSTALL (the one and only!)
    _InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                       "$distdirectory:xpinstall:");
    _InstallFromManifest(":mozilla:xpinstall:public:MANIFEST_PREFS",				    $dist_dir."Components:", 1);

	#FULL CIRCLE    
	if ($main::MOZ_FULLCIRCLE)
	{
		_InstallFromManifest(":ns:fullsoft:public:MANIFEST",							"$distdirectory");
	
		if ($main::DEBUG)
		{
			#_InstallFromManifest(":ns:fullsoft:public:MANIFEST",					"$distdirectory:viewer_debug:");
		} 
		else
		{
			#_InstallFromManifest(":ns:fullsoft:public:MANIFEST",					"$distdirectory:viewer:");
			_InstallFromManifest(":ns:fullsoft:public:MANIFEST",						"$distdirectory");
		}
	}


	# XPFE COMPONENTS
   _InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST",					"$distdirectory:xpfe:components");
   _InstallFromManifest(":mozilla:xpfe:components:public:MANIFEST_IDL",				"$distdirectory:idl:");

   # find
   _InstallFromManifest(":mozilla:xpfe:components:find:public:MANIFEST_IDL",			"$distdirectory:idl:");

   # bookmarks
   _InstallFromManifest(":mozilla:xpfe:components:bookmarks:public:MANIFEST_IDL",	"$distdirectory:idl:");

   # history
   _InstallFromManifest(":mozilla:xpfe:components:history:public:MANIFEST_IDL",		"$distdirectory:idl:");
   
   # related
   _InstallFromManifest(":mozilla:xpfe:components:related:public:MANIFEST_IDL",		"$distdirectory:idl:");

   # prefwindow
   _InstallFromManifest(":mozilla:xpfe:components:prefwindow:public:MANIFEST_IDL",	"$distdirectory:idl:");

   # sample
   _InstallFromManifest(":mozilla:xpfe:components:sample:public:MANIFEST_IDL",		"$distdirectory:idl:");
   # ucth
   _InstallFromManifest(":mozilla:xpfe:components:ucth:public:MANIFEST_IDL",			"$distdirectory:idl:");
   # xfer
   _InstallFromManifest(":mozilla:xpfe:components:xfer:public:MANIFEST_IDL",			"$distdirectory:idl:");
	
	# XPAPPS
	_InstallFromManifest(":mozilla:xpfe:AppCores:public:MANIFEST",					"$distdirectory:xpfe:");
	_InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST",					"$distdirectory:xpfe:");
	_InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST_IDL",				"$distdirectory:idl:");

	# MAILNEWS
   _InstallFromManifest(":mozilla:mailnews:public:MANIFEST",							"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:public:MANIFEST_IDL",						"$distdirectory:idl:");
   _InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST_IDL",				"$distdirectory:idl:");
   _InstallFromManifest(":mozilla:mailnews:base:build:MANIFEST",						"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:base:src:MANIFEST",						"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:base:util:MANIFEST",						"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST",				"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:base:search:public:MANIFEST_IDL",			"$distdirectory:idl:");
   _InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:compose:build:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:db:mdb:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:db:mork:build:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:db:msgdb:public:MANIFEST",				"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:db:msgdb:build:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:local:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:local:build:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:local:src:MANIFEST",						"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:imap:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:imap:build:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:imap:src:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:mime:src:MANIFEST",						"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:mime:emitters:src:MANIFEST",				"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:news:public:MANIFEST",					"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:news:build:MANIFEST",						"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:addrbook:public:MANIFEST",				"$distdirectory:mailnews:");
   _InstallFromManifest(":mozilla:mailnews:addrbook:build:MANIFEST",					"$distdirectory:mailnews:");
   
   print("--- Client Dist export complete ----\n")
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
	
	if ($main::MOZ_FULLCIRCLE)
	{
		mkpath([ $dist_dir."TalkBack"]);
	}

	BuildRuntimeDist();
	BuildClientDist();
	
	print("--- Dist export complete ----\n")
}


#//--------------------------------------------------------------------------------------------------
#// Build stub projects
#//--------------------------------------------------------------------------------------------------

sub BuildStubs()
{

	unless( $main::build{stubs} ) { return; }
	_assertRightDirectory();

	#//
	#// Clean projects
	#//
	_BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              	"Stubs");

	print("--- Stubs projects complete ----\n")
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
	
	if ($toc_file ne "")
	{
		ReconcileProject("$project_path", "$project_dir$toc_file");
	}
	
	BuildProject($project_path, $target_name);
	
	$alias_shlb ? MakeAlias("$project_dir$target_name", "$dist_dir$component_dir") : 0;
	$alias_xSYM ? MakeAlias("$project_dir$target_name.xSYM", "$dist_dir$component_dir") : 0;
}


#//--------------------------------------------------------------------------------------------------
#// Build IDL projects
#//--------------------------------------------------------------------------------------------------

sub BuildIDLProjects()
{
	unless( $main::build{idl} ) { return; }
	_assertRightDirectory();

	if ( $main::build{xpidl} )
	{
		#// beard:  build the IDL compiler itself.
		BuildProject(":mozilla:xpcom:typelib:xpidl:macbuild:xpidl.mcp", "build all");
	}
	
	BuildIDLProject(":mozilla:xpcom:macbuild:XPCOMIDL.mcp", 						"xpcom");

	if ( $main::NECKO )
	{
		BuildIDLProject(":mozilla:netwerk:macbuild:netwerkIDL.mcp","netwerk");

		# protocols
		BuildIDLProject(":mozilla:netwerk:protocol:about:macbuild:aboutIDL.mcp","about");
		BuildIDLProject(":mozilla:netwerk:protocol:file:macbuild:fileIDL.mcp","file");
		BuildIDLProject(":mozilla:netwerk:protocol:ftp:macbuild:ftpIDL.mcp","ftp");
		BuildIDLProject(":mozilla:netwerk:protocol:http:macbuild:httpIDL.mcp","http");
		
		# mime service
		# Just a placeholder as mime.xpt is currently part of the netwerkIDL.mcp build
	}

	BuildIDLProject(":mozilla:modules:libpref:macbuild:libprefIDL.mcp",				"libpref");
	BuildIDLProject(":mozilla:modules:libjar:macbuild:libjarIDL.mcp",				"libjar");
	BuildIDLProject(":mozilla:js:macbuild:XPConnectIDL.mcp", 						"xpconnect");
	BuildIDLProject(":mozilla:dom:macbuild:domIDL.mcp", 							"dom");
	BuildIDLProject(":mozilla:widget:macbuild:widgetIDL.mcp", 						"widget");
	BuildIDLProject(":mozilla:editor:macbuild:EditorIDL.mcp", 						"editor");
	BuildIDLProject(":mozilla:profile:macbuild:ProfileServicesIDL.mcp",				"profileservices");
		
	BuildIDLProject(":mozilla:rdf:macbuild:RDFIDL.mcp",								"rdf");
	BuildIDLProject(":mozilla:xpinstall:macbuild:xpinstallIDL.mcp",            		"xpinstall");
	BuildIDLProject(":mozilla:extensions:wallet:macbuild:walletIDL.mcp","wallet");
	BuildIDLProject(":mozilla:xpfe:components:bookmarks:macbuild:BookmarksIDL.mcp",	"bookmarks");
	BuildIDLProject(":mozilla:xpfe:components:history:macbuild:historyIDL.mcp",		"history");
	BuildIDLProject(":mozilla:xpfe:components:related:macbuild:RelatedIDL.mcp",		"related");
	BuildIDLProject(":mozilla:xpfe:components:prefwindow:macbuild:prefwindowIDL.mcp","prefwindow");
	BuildIDLProject(":mozilla:xpfe:components:macbuild:mozcompsIDL.mcp",			"mozcomps");

	BuildIDLProject(":mozilla:xpfe:appshell:macbuild:appshellIDL.mcp",				"appshell");
	
	BuildIDLProject(":mozilla:mailnews:base:macbuild:msgCoreIDL.mcp",				"mailnews");
	BuildIDLProject(":mozilla:mailnews:compose:macbuild:msgComposeIDL.mcp",			"MsgCompose");
	BuildIDLProject(":mozilla:mailnews:local:macbuild:msglocalIDL.mcp",				"MsgLocal");
	BuildIDLProject(":mozilla:mailnews:news:macbuild:msgnewsIDL.mcp",				"MsgNews");
	BuildIDLProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbookIDL.mcp",		"MsgAddrbook");
	BuildIDLProject(":mozilla:mailnews:db:macbuild:msgDBIDL.mcp",					"MsgDB");
	BuildIDLProject(":mozilla:mailnews:mime:macbuild:mimeIDL.mcp",					"Mime");

	BuildIDLProject(":mozilla:caps:macbuild:CapsIDL.mcp",							"caps");

	BuildIDLProject(":mozilla:intl:locale:macbuild:nsLocaleIDL.mcp",					"nsLocale");
	BuildIDLProject(":mozilla:intl:strres:macbuild:strresIDL.mcp",					"nsIStringBundle");

	print("--- IDL projects complete ----\n")
}

#//--------------------------------------------------------------------------------------------------
#// Build runtime projects
#//--------------------------------------------------------------------------------------------------

sub BuildRuntimeProjects()
{
	unless( $main::build{runtime} ) { return; }
	_assertRightDirectory();

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
		_BuildProject(":mozilla:lib:mac:InterfaceLib:Interface.mcp",			"MacOS Interfaces");
	}
	
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
		_BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",		"MemAllocator$D.o");
	}

	BuildOneProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",					"NSStdLib$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",					"NSPR20$D.shlb", "NSPR20.toc", 1, $main::ALIAS_SYM_FILES, 0);

	print("--- Runtime projects complete ----\n")
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

	#//
	#// Stub libraries
	#//

	_BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",			"Security.o");
	
	#//
	#// Shared libraries
	#//

	BuildOneProject(":mozilla:jpeg:macbuild:JPEG.mcp",							"JPEG$D.shlb", "JPEG.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:libreg:macbuild:libreg.mcp",				"libreg$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",						"xpcom$D.shlb", "xpcom.toc", 1, $main::ALIAS_SYM_FILES, 0);
		
	BuildOneProject(":mozilla:js:macbuild:JavaScript.mcp",						"JavaScript$D.shlb", "JavaScript.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:js:macbuild:LiveConnect.mcp",						"LiveConnect$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",					"zlib$D.shlb", "zlib.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
    BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",              "libjar$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:caps:macbuild:Caps.mcp",						 	"Caps$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:modules:oji:macbuild:oji.mcp",					"oji$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.mcp",			"libpref$D.shlb", "libpref.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:js:macbuild:XPConnect.mcp",						"XPConnect$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.mcp",			"libutil$D.shlb", "libutil.toc", 1, $main::ALIAS_SYM_FILES, 0);

	ReconcileProject(":mozilla:modules:libimg:macbuild:png.mcp", 				":mozilla:modules:libimg:macbuild:png.toc");
	_BuildProject(":mozilla:modules:libimg:macbuild:png.mcp",					"png$D.o");

	BuildOneProject(":mozilla:modules:libimg:macbuild:libimg.mcp",				"libimg$D.shlb", "libimg.toc", 1, $main::ALIAS_SYM_FILES, 0);
	BuildOneProject(":mozilla:modules:libimg:macbuild:gifdecoder.mcp",			"gifdecoder$D.shlb", "gifdecoder.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libimg:macbuild:pngdecoder.mcp",			"pngdecoder$D.shlb", "pngdecoder.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:modules:libimg:macbuild:jpgdecoder.mcp",			"jpgdecoder$D.shlb", "jpgdecoder.toc", 1, $main::ALIAS_SYM_FILES, 1);

	if ( $main::NECKO )
	{
		BuildOneProject(":mozilla:netwerk:macbuild:netwerk.mcp",					"Network$D.shlb", "netwerk.toc", 1, $main::ALIAS_SYM_FILES, 1);

		# utils library
		BuildOneProject(":mozilla:netwerk:util:macbuild:netwerkUtil.mcp",			"NetworkModular$D.shlb", "netwerkUtil.toc", 1, $main::ALIAS_SYM_FILES, 0);

		# protocols
		BuildOneProject(":mozilla:netwerk:protocol:about:macbuild:about.mcp",		"about$D.shlb", "about.toc", 1, $main::ALIAS_SYM_FILES, 1);
		BuildOneProject(":mozilla:netwerk:protocol:file:macbuild:file.mcp",			"file$D.shlb", "file.toc", 1, $main::ALIAS_SYM_FILES, 1);
		BuildOneProject(":mozilla:netwerk:protocol:ftp:macbuild:ftp.mcp",			"ftp$D.shlb", "ftp.toc", 1, $main::ALIAS_SYM_FILES, 1);
		BuildOneProject(":mozilla:netwerk:protocol:http:macbuild:http.mcp",			"http$D.shlb", "http.toc", 1, $main::ALIAS_SYM_FILES, 1);
		BuildOneProject(":mozilla:netwerk:protocol:resource:macbuild:resource.mcp",	"resource$D.shlb", "resource.toc", 1, $main::ALIAS_SYM_FILES, 1);

		# mime service
		BuildOneProject(":mozilla:netwerk:mime:macbuild:mime.mcp",					"mimetype$D.shlb", "mimetype.toc", 1, $main::ALIAS_SYM_FILES, 1);
	}
	else
	{
		BuildOneProject(":mozilla:network:macbuild:network.mcp",					"NetworkModular$D.shlb", "network.toc", 1, $main::ALIAS_SYM_FILES, 0);
	}

	BuildOneProject(":mozilla:profile:macbuild:profile.mcp",					"profile$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:profile:macbuild:profileservices.mcp",					"profileservices$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.mcp",			"Wallet$D.shlb", "wallet.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:extensions:wallet:macbuild:walletviewers.mcp",	"WalletViewers$D.shlb", "walletviewer.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:rdf:brprof:build:brprof.mcp",						"BrowsingProfile$D.shlb", "brprof.toc", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:rdf:chrome:build:chrome.mcp",                     "ChomeRegistry$D.shlb", "chrome.toc", 1, $main::ALIAS_SYM_FILES, 1);
    
#// XXX moved this TEMPORARILY to layout while we sort out a dependency
#	BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",							"rdf$D.shlb", "rdf.toc", 1, $main::ALIAS_SYM_FILES, 1);

	print("--- Common projects complete ----\n")
}

#//--------------------------------------------------------------------------------------------------
#// Build international projects
#//--------------------------------------------------------------------------------------------------

sub BuildInternationalProjects()
{
	unless( $main::build{intl} ) { return; }

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";

	BuildOneProject(":mozilla:intl:chardet:macbuild:chardet.mcp",				"chardet$D.shlb", "chardet.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:uconv:macbuild:uconv.mcp",					"uconv$D.shlb", "uconv.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvlatin.mcp",				"ucvlatin$D.shlb", "uconvlatin.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja.mcp",					"ucvja$D.shlb", "ucvja.toc", 1, $main::ALIAS_SYM_FILES, 1);

#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja2.mcp",					"ucvja2$D.shlb", "ucvja2.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw.mcp",					"ucvtw$D.shlb", "ucvtw.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw2.mcp",					"ucvtw2$D.shlb", "ucvtw2.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvcn.mcp",					"ucvcn$D.shlb", "ucvcn.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvko.mcp",					"ucvko$D.shlb", "ucvko.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvvt.mcp",					"ucvvt$D.shlb", "ucvvt.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvth.mcp",					"ucvth$D.shlb", "ucvth.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:unicharutil:macbuild:unicharutil.mcp",		"unicharutil$D.shlb", "unicharutil.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:locale:macbuild:locale.mcp",					"nslocale$D.shlb", "nslocale.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:intl:lwbrk:macbuild:lwbrk.mcp",					"lwbrk$D.shlb", "lwbrk.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
    BuildOneProject(":mozilla:intl:strres:macbuild:strres.mcp",					"strres$D.shlb", "strres.toc", 1, $main::ALIAS_SYM_FILES, 1);

	print("--- International projects complete ----\n")
} # intl
	

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
			print("   $_ is an alias\n");
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


	#//
	#// Most resources should all go into the chrome dir eventually
	#//
	my($chrome_dir) = "$dist_dir" . "Chrome:";
	my($resource_dir) = "$dist_dir" . "res:";
	my($samples_dir) = "$resource_dir" . "samples:";
	my($defaults_dir) = "$dist_dir" . "Defaults:";

	#//
	#// Make aliases of resource files
	#//
	_MakeAlias(":mozilla:layout:html:document:src:ua.css",								"$resource_dir");
	_MakeAlias(":mozilla:layout:html:document:src:arrow.gif",								"$resource_dir");	
	_MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",			"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:charsetalias.properties",						"$resource_dir");
	_MakeAlias(":mozilla:intl:uconv:src:maccharset.properties",							"$resource_dir");

	_MakeAlias(":mozilla:extensions:wallet:src:cookie.properties",						"$resource_dir");
	_MakeAlias(":mozilla:extensions:wallet:src:wallet.properties",						"$resource_dir");

	my($html_dir) = "$resource_dir" . "html:";
    _InstallResources(":mozilla:layout:html:base:src:MANIFEST_RES",						"$html_dir");

	my($throbber_dir) = "$resource_dir" . "throbber:";
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:throbber:",				"$throbber_dir");
	
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:samples:",				"$samples_dir");
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:resources:",				"$samples_dir");
	
	my($rdf_dir) = "$resource_dir" . "rdf:";
	BuildFolderResourceAliases(":mozilla:rdf:resources:",								"$rdf_dir");

	
	my($profile_dir) = "$resource_dir" . "profile:";
	BuildFolderResourceAliases(":mozilla:profile:resources:",							"$profile_dir");
	BuildFolderResourceAliases(":mozilla:profile:defaults:",							"$defaults_dir");
	
	# NOTE: this will change as we move the toolbar/appshell chrome files to a real place
	 my($navigator_chrome_dir) = "$chrome_dir" . "Navigator";
    _InstallResources(":mozilla:xpfe:browser:resources:content:MANIFEST",             "$navigator_chrome_dir:content:default");
    _InstallResources(":mozilla:xpfe:browser:resources:skin:MANIFEST",                "$navigator_chrome_dir:skin:default");

	 my($global_chrome_dir) = "$chrome_dir" . "Global";
    _InstallResources(":mozilla:xpfe:global:resources:content:MANIFEST",			"$global_chrome_dir:content:default");
    _InstallResources(":mozilla:xpfe:global:resources:skin:MANIFEST",               "$global_chrome_dir:skin:default");
	_InstallResources(":mozilla:xpfe:global:resources:locale:MANIFEST",				"$global_chrome_dir:locale:en-US:", 0);


	_InstallResources(":mozilla:xpfe:browser:src:MANIFEST",								"$samples_dir");

	BuildFolderResourceAliases(":mozilla:xpfe:browser:samples:", 						"$samples_dir");
	_MakeAlias(":mozilla:xpfe:browser:samples:sampleimages:", 							"$samples_dir");
	BuildFolderResourceAliases(":mozilla:xpfe:AppCores:xul:",							"$samples_dir");

	my($toolbar_dir) = "$resource_dir" . "toolbar:";
	BuildFolderResourceAliases(":mozilla:xpfe:AppCores:xul:resources:",					"$toolbar_dir");
	_MakeAlias(":mozilla:xpfe:AppCores:xul:resources:throbbingN.gif",					"$throbber_dir");
		
	if ($main::build{editor})
	{
		my($editor_chrome_dir) = "$chrome_dir" . "Editor";

		_InstallResources(":mozilla:editor:ui:composer:content:MANIFEST",			"$editor_chrome_dir:content:default:", 0);
		_InstallResources(":mozilla:editor:ui:composer:skin:MANIFEST",				"$editor_chrome_dir:skin:default:", 0);
		_InstallResources(":mozilla:editor:ui:composer:locale:en-US:MANIFEST",	"$editor_chrome_dir:locale:en-US:", 0);

		_InstallResources(":mozilla:editor:ui:dialogs:content:MANIFEST",				"$editor_chrome_dir:content:default:", 0);
		_InstallResources(":mozilla:editor:ui:dialogs:skin:MANIFEST",				    "$editor_chrome_dir:skin:default:", 0);
		_InstallResources(":mozilla:editor:ui:dialogs:locale:en-US:MANIFEST",		"$editor_chrome_dir:locale:en-US:", 0);
	}

	# if ($main::build{mailnews})
	{
        my($mailnews_dir) = "$resource_dir" . "mailnews";
        my($messenger_chrome_dir) = "$chrome_dir" . "messenger";
        my($messengercomposer_chrome_dir) = "$chrome_dir" . "messengercompose";
        my($addressbook_chrome_dir) = "$chrome_dir" . "addressbook";
        
        _InstallResources(":mozilla:mailnews:base:resources:content:MANIFEST",           "$messenger_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:base:resources:skin:MANIFEST",              "$messenger_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:base:resources:locale:en-US:MANIFEST",      "$messenger_chrome_dir:locale:en-US:", 0);
        _InstallResources(":mozilla:mailnews:base:prefs:resources:content:MANIFEST",     "$messenger_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:base:prefs:resources:locale:en-US:MANIFEST", "$messenger_chrome_dir:locale:en-US:", 0);
        _InstallResources(":mozilla:mailnews:base:prefs:resources:skin:MANIFEST",        "$messenger_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:base:search:resources:content:MANIFEST",    "$messenger_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:mime:resources:skin:MANIFEST",              "$messenger_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:mime:emitters:resources:skin:MANIFEST",     "$messenger_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:local:resources:skin:MANIFEST",             "$messenger_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:news:resources:skin:MANIFEST",              "$messenger_chrome_dir:skin:default:", 0);
		_InstallResources(":mozilla:mailnews:news:resources:content:MANIFEST",    		 "$messenger_chrome_dir:content:default:", 0);

        _InstallResources(":mozilla:mailnews:mime:resources:MANIFEST",                   "$mailnews_dir:messenger:", 0); 
		_InstallResources(":mozilla:mailnews:mime:cthandlers:resources:MANIFEST",		"$mailnews_dir:messenger:", 0);	

        _InstallResources(":mozilla:mailnews:compose:resources:content:MANIFEST",        "$messengercomposer_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:compose:resources:skin:MANIFEST",           "$messengercomposer_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:compose:resources:locale:en-US:MANIFEST",   "$messengercomposer_chrome_dir:locale:en-US:", 0);
        _InstallResources(":mozilla:mailnews:compose:prefs:resources:content:MANIFEST",  "$messengercomposer_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:compose:prefs:resources:locale:en-US:MANIFEST",  "$messengercomposer_chrome_dir:locale:en-US:", 0);

        _InstallResources(":mozilla:mailnews:addrbook:resources:content:MANIFEST",       "$addressbook_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:addrbook:resources:skin:MANIFEST",          "$addressbook_chrome_dir:skin:default:", 0);
        _InstallResources(":mozilla:mailnews:addrbook:resources:locale:en-US:MANIFEST",  "$addressbook_chrome_dir:locale:en-US:", 0);
        _InstallResources(":mozilla:mailnews:addrbook:prefs:resources:content:MANIFEST", "$addressbook_chrome_dir:content:default:", 0);
        _InstallResources(":mozilla:mailnews:addrbook:prefs:resources:locale:en-US:MANIFEST", "$addressbook_chrome_dir:locale:en-US:", 0);
    }
	
	# copy the chrome registry. We want an actual copy so that changes for custom UI's
	# don't accidentally get checked into the tree. (pinkerton, bug#5296).
	_copy( ":mozilla:rdf:chrome:build:registry.rdf", "$chrome_dir" . "registry.rdf" );
		
	# Install XPFE component resources
	_InstallResources(":mozilla:xpfe:components:find:resources:MANIFEST",			"$global_chrome_dir:content:default");
	_InstallResources(":mozilla:xpfe:components:find:resources:locale:MANIFEST",	"$global_chrome_dir:locale");
	_InstallResources(":mozilla:xpfe:components:bookmarks:resources:MANIFEST",		"$samples_dir");
	_InstallResources(":mozilla:xpfe:components:bookmarks:resources:locale:en-US:MANIFEST",		"$samples_dir");
        {
          my($history_dir) = "$chrome_dir"."History";
	  _InstallResources(":mozilla:xpfe:components:history:resources:MANIFEST-content",			"$history_dir:content:default");
	  _InstallResources(":mozilla:xpfe:components:history:resources:MANIFEST-skin",			"$history_dir:skin:default");
	  _InstallResources(":mozilla:xpfe:components:history:resources:locale:MANIFEST",			"$history_dir:locale");
        }
        {
          my($related_dir) = "$chrome_dir"."Related";
	  _InstallResources(":mozilla:xpfe:components:related:resources:MANIFEST-content",			"$related_dir:content:default");
	  _InstallResources(":mozilla:xpfe:components:related:resources:MANIFEST-skin",			"$related_dir:skin:default");
	  _InstallResources(":mozilla:xpfe:components:related:resources:locale:MANIFEST",			"$related_dir:locale");
        }
	_InstallResources(":mozilla:xpfe:components:search:resources:MANIFEST",			"$samples_dir");
        {
          my($sidebar_chrome_dir) = "$chrome_dir" . "Sidebar";
          _InstallResources(":mozilla:xpfe:components:sidebar:resources:MANIFEST-content",		"$sidebar_chrome_dir:content:default");
          _InstallResources(":mozilla:xpfe:components:sidebar:resources:MANIFEST-skin",		"$sidebar_chrome_dir:skin:default");
          _InstallResources(":mozilla:xpfe:components:sidebar:resources:locale:MANIFEST",		"$sidebar_chrome_dir:locale");
        }
	_InstallResources(":mozilla:xpfe:components:ucth:resources:MANIFEST",			"$global_chrome_dir:content:default");
	_InstallResources(":mozilla:xpfe:components:ucth:resources:locale:MANIFEST",	"$global_chrome_dir:locale");
	_InstallResources(":mozilla:xpfe:components:xfer:resources:MANIFEST",			"$global_chrome_dir:content:default");
	_InstallResources(":mozilla:xpfe:components:xfer:resources:locale:MANIFEST",	"$global_chrome_dir:locale");
	# the WALLET
	_InstallResources(":mozilla:extensions:wallet:cookieviewer:MANIFEST",			"$samples_dir");
	_InstallResources(":mozilla:extensions:wallet:signonviewer:MANIFEST",			"$samples_dir");
	_InstallResources(":mozilla:extensions:wallet:walletpreview:MANIFEST",			"$samples_dir");
	_InstallResources(":mozilla:extensions:wallet:editor:MANIFEST",					"$samples_dir");
	{
		my($pref_chrome_dir) = "$chrome_dir" . "pref";
		_InstallResources(":mozilla:xpfe:components:prefwindow:resources:content:MANIFEST",	"$pref_chrome_dir:content:default:", 0);
		_InstallResources(":mozilla:xpfe:components:prefwindow:resources:skin:MANIFEST",		"$pref_chrome_dir:skin:default:", 0);
	}

	print("--- Resource copying complete ----\n")
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

		#// WasteLib
		my($wastelibpath) = $appath;
		$wastelibpath =~ s/[^:]*$/MacOS Support:WASTE 1.3 Distribution:WASTELib/;
		MakeAlias("$wastelibpath", "$dist_dir"."Essential Files:");

		#// ProfilerLib
		if ($main::DEBUG)
		{
			my($profilerlibpath) = $appath;
			$profilerlibpath =~ s/[^:]*$/MacOS Support:Libraries:Profiler Common:ProfilerLib/;
			MakeAlias("$profilerlibpath", "$dist_dir"."Essential Files:");
		}
	}
	else {
		print STDERR "Can't find $filepath\n";
	}
}


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
	

	#//
	#// Build Layout projects
	#//

	_BuildProject(":mozilla:expat:macbuild:expat.mcp",						"expat$D.o");

	BuildOneProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",				"htmlparser$D.shlb", "htmlparser.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:gfx:macbuild:gfx.mcp",							"gfx$D.shlb", "gfx.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:dom:macbuild:dom.mcp",							"dom$D.shlb", "dom.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:plugin:macbuild:plugin.mcp",				"plugin$D.shlb", "plugin.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:layout:macbuild:layout.mcp",						"layout$D.shlb", "layout.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:view:macbuild:view.mcp",							"view$D.shlb", "view.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:widget:macbuild:widget.mcp",						"widget$D.shlb", "widget.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:webshell:macbuild:webshell.mcp",					"webshell$D.shlb", "webshell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:webshell:embed:mac:RaptorShell.mcp",				"RaptorShell$D.shlb", "RaptorShell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	#// XXX this is here because of a very TEMPORARY dependency
	BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",							"RDFLibrary$D.shlb", "rdf.toc", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.mcp",                "xpinstall$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

	print("--- Layout projects complete ---\n")
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

	BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.mcp",					"EditorTxmgr$D.shlb", "txmgr.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:editor:txtsvc:macbuild:txtsvc.mcp",				"TextServices$D.shlb", "txtsvc.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:editor:macbuild:editor.mcp",						"EditorCore$D.shlb", "EditorCore.toc", 1, $main::ALIAS_SYM_FILES, 1);

	print("--- Editor projects complete ----\n")
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

	BuildOneProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",			"viewer$D", "viewer.toc", 0, 0, 0);

#	BuildOneProject(":mozilla:xpfe:macbuild:xpfeviewer.mcp",					"xpfeviewer$D.shlb", "xpfeviewer.toc", 0, 0, 0);

	print("--- Viewer projects complete ----\n")
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

	# Components
	BuildOneProject(":mozilla:xpfe:components:find:macbuild:FindComponent.mcp",	"FindComponent$D.shlb", "FindComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:ucth:macbuild:ucth.mcp",	"ucth$D.shlb", "ucth.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:xfer:macbuild:xfer.mcp",	"xfer$D.shlb", "xfer.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:bookmarks:macbuild:Bookmarks.mcp", "Bookmarks$D.shlb", "BookmarksComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:history:macbuild:history.mcp", "history$D.shlb", "historyComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:prefwindow:macbuild:prefwindow.mcp", "prefwindow$D.shlb", "prefwindowComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	BuildOneProject(":mozilla:xpfe:components:related:macbuild:Related.mcp", "Related$D.shlb", "RelatedComponent.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	# Applications
	BuildOneProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",				"AppShell$D.shlb", "AppShell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:xpfe:AppCores:macbuild:AppCores.mcp",				"AppCores$D.shlb", "AppCores.toc", 1, $main::ALIAS_SYM_FILES, 0);

	print("--- XPApp projects complete ----\n")

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

	BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.mcp",					"MsgUtil$D.lib", "MsgUtil.toc", 0, 0, 0);

	BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.mcp",						"mailnews$D.shlb", "mailnews.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.mcp",				"MsgCompose$D.shlb", "MsgCompose.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.mcp",							"MsgDB$D.shlb", "MsgDB.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:db:mork:macbuild:mork.mcp",						"Mork$D.shlb", "Mork.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.mcp",					"MsgLocal$D.shlb", "MsgLocal.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:imap:macbuild:msgimap.mcp",						"MsgImap$D.shlb", "MsgImap.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:news:macbuild:msgnews.mcp",						"MsgNews$D.shlb", "MsgNews.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:addrbook:macbuild:msgAddrbook.mcp",				"MsgAddrbook$D.shlb", "MsgAddrbook.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.mcp",							"Mime$D.shlb", "Mime.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:emitters:macbuild:mimeEmitter.mcp",			"mimeEmitter$D.shlb", "mimeEmitter.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:cthandlers:vcard:macbuild:vcard.mcp",		"vcard$D.shlb", "vcard.toc", 1, $main::ALIAS_SYM_FILES, 1);

#	BuildOneProject(":mozilla:mailnews:mime:cthandlers:calendar:macbuild:calendar.mcp",	"calendar$D.shlb", "calendar.toc", 1, $main::ALIAS_SYM_FILES, 1);

	print("--- MailNews projects complete ----\n")
}

sub BuildAppRunner()
{
	unless( $main::build{apprunner} ) { return; }

	_assertRightDirectory();
	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	BuildOneProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.mcp",			"apprunner$D", "apprunner.toc", 0, 0, 1);
	
	# copy command line documents into the Apprunner folder and set correctly the signature
	my($dist_dir) = _getDistDirectory();
	my($cmd_file_path) = ":mozilla:xpfe:bootstrap:";
	my($cmd_file) = "";

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

	$cmd_file = "Mozilla Preference";
	_copy( $cmd_file_path . "Mozilla_Preference", $dist_dir . $cmd_file );
	MacPerl::SetFileInfo("MOZZ", "CMDL", $dist_dir . $cmd_file);
}

#//--------------------------------------------------------------------------------------------------
#// Build everything
#//--------------------------------------------------------------------------------------------------

sub BuildProjects()
{
	MakeResourceAliases();
	MakeLibAliases();

 	# activate CodeWarrior
	ActivateApplication('CWIE');

	BuildIDLProjects();
	BuildStubs();
	BuildRuntimeProjects();
	BuildCommonProjects();
	BuildInternationalProjects();
	BuildLayoutProjects();
	BuildEditorProjects();
	BuildViewerProjects();
	BuildXPAppProjects();
	BuildMailNewsProjects();
	BuildAppRunner();
}
