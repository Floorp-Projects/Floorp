#!/usr/bin/perl -w

#
# Create a mapping from symbolic component name to directory name(s).
#
# Tue Oct 16 16:48:36 PDT 2001
# <mcafee@netscape.com>

use strict;

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");


sub PrintUsage {
  die <<END_USAGE
  Prints out directories needed for a given list of components.
  usage: module2dir.pl [--list-only] <component-name1> <component-name2> ...
END_USAGE
}

my %map = (
  'absyncsvc',          'mailnews/absync',
  'access-proxy',       'extensions/access-builtin/accessproxy',
  'accessibility',      'accessible',
  'addrbook',           'mailnews/addrbook',
  'appcomps',           'xpfe/components',
  'appshell',           'xpfe/appshell',
  'appstartup',         'embedding/components/appstartup',
  'autoconfig',         'extensions/pref/autoconfig',
  'boehm',              'gc/boehm',
  'browser',            'xpfe/browser',
  'calendar',           'mailnews/mime/cthandlers/calendar',
  'caps',               'caps',
  'chardet',            'intl/chardet',
  'chardetc',           'intl/chardet/src/classic',
  'chrome',             'rdf/chrome',
  'commandhandler',     'embedding/components/commandhandler',
  'composer',           'editor/composer',
  'content',            'content',
  'cookie',             'extensions/cookie',
  'ctl',                'extensions/ctl',
  'dbm',                'dbm',
  'docshell',           'docshell',
  'dom',                'dom',
  'downloadmanager',    'xpfe/components/download-manager',
  'editor',             'editor',
  'embed_base',         'embedding/base',
  'embed_lite',         'embedding/lite',
  'embedcomponents',    'embedding/components/appstartup embedding/components/windowwatcher embedding/components/printingui embedding/components/build',
  'expat',              'expat',
  'exthandler',         'uriloader/exthandler',
  'filepicker',         'xpfe/components/filepicker',
  'find',               'xpfe/components/find embedding/components/find',
  'gfx',                'gfx',
  'gfx2',               'gfx2',
  'gtkembedmoz',        'embedding/browser/gtk/src embedding/browser/gtk/tests',
  'gtkxtbin',           'widget/src/gtkxtbin',
  'helperAppDlg',       'embedding/components/ui/helperAppDlg',
  'htmlparser',         'htmlparser',
  'iiextras',           'extensions/interfaceinfo',
  'imgbmp',             'modules/libpr0n/decoders/bmp',
  'imggif',             'modules/libpr0n/decoders/gif',
  'imgicon',            'modules/libpr0n/decoders/icon',
  'imgjpeg',            'modules/libpr0n/decoders/jpeg',
  'imglib2',            'modules/libpr0n/public  modules/libpr0n/src',
  'imgmng',             'modules/libpr0n/decoders/mng',
  'imgpng',             'modules/libpr0n/decoders/png',
  'imgppm',             'modules/libpr0n/decoders/ppm',
  'imgxbm',             'modules/libpr0n/decoders/xbm',
  'impComm4xMail',      'mailnews/import/comm4x',
  'impEudra',           'mailnews/import/eudora',
  'impOutlk',           'mailnews/import/outlook',
  'impText',            'mailnews/import/text',
  'import',             'mailnews/import',
  'importOE',           'mailnews/import/oexpress',
  'inspector',          'extensions/inspector',
  'intl',               'intl',
  'intlcmpt',           'intl/compatibility',
  'jar',                'modules/libjar',
  'java',               'sun-java/stubs js/jsd/classes',
  'jpeg',               'jpeg',
  'jprof',              'tools/jprof',
  'js',                 'js/src/fdlibm js/src',
  'jsconsole',          'embedding/components/jsconsole',
  'jsdebug',            'js/jsd',
  'jsd_refl',           'js/jsd/jsdb',
  'jsloader',           'js/src/xpconnect/loader',
  'layout',             'layout',
  'libreg',             'modules/libreg',
  'liveconnect',        'js/src/liveconnect',
  'locale',             'intl/locale',
  'lwbrk',              'intl/lwbrk',
  'mai',                'widget/src/gtk2/mai',
  'mailnews',           'mailnews',
  'MapiProxy',          'mailnews/mapi/mapihook/build',
  'mfcEmbed',           'embedding/tests/winEmbed',
  'mkdepend',           'config/mkdepend',
  'mime',               'mailnews/mime',
  'mimeemitter',        'mailnews/mime/emitters',
  'mimetype',           'netwerk/mime',
  'mng',                'modules/libimg/mng',
  'mork',               'db/mork db/mdb',
  'mozcomps',           'xpfe/components',
  'mozldap',            'directory/xpcom/base',
  'mozMapi32',          'mailnews/mapi',
  'mozpango',           'intl/ctl/src/pangoLite',
  'mozpango-thaix',     'intl/ctl/src/thaiShaper',
  'mozpango-dvngx',     'intl/ctl/src/hindiShaper',
  'mozucth',            'xpfe/components/ucth',
  'mozxfer',            'xpfe/components/xfer',
  'msgbase',            'mailnews/base',
  'msgbaseutil',        'mailnews/base/util',
  'msgcompose',         'mailnews/compose',
  'msgdb',              'mailnews/db/msgdb',
  'msgimap',            'mailnews/imap',
  'msglocal',           'mailnews/local',
  'msgMapi',            'mailnews/mapi/mapihook',
  'msgmdn',             'mailnews/extensions/mdn',
  'msgnews',            'mailnews/news',
  'msgsmime',           'mailnews/extensions/smime',
  'necko',              'netwerk/base netwerk/protocol/about netwerk/protocol/data netwerk/protocol/file netwerk/protocol/http netwerk/protocol/jar netwerk/protocol/keyword netwerk/protocol/res netwerk/dns netwerk/socket netwerk/streamconv netwerk/cookie netwerk/build',
  'necko2',             'netwerk/protocol/ftp netwerk/protocol/gopher netwerk/protocol/viewsource netwerk/build2',
  'nkcache',            'netwerk/cache',
  'npevents',           'modules/plugin/samples/testevents',
  'npsimple',           'modules/plugin/samples/simple',
  'oji',                'modules/oji',
  'p3p',                'extensions/p3p',
  'phembedmoz',         'embedding/browser/photon',
  'pics',               'extensions/pics',
  'pipboot',            'security/manager/boot',
  'pippki',             'security/manager/pki',
  'pipnss',             'security/manager/ssl',
  'plugin',             'modules/plugin',
  'png',                'modules/libimg/png',
  'pref',               'modules/libpref',
  'prefmigr',           'profile/pref-migrator',
  'profile',            'profile',
  'progressDlg',        'embedding/components/ui/progressDlg',
  'pyxpcom',            'extensions/python/xpcom',
  'rdf',                'rdf',
  'rdfutil',            'rdf/util',
  'rdfldapds',          'directory/xpcom/tests',
  'regviewer',          'xpfe/components/regviewer',
  'SanePlugin',         'modules/plugin/samples/SanePlugin',
  'shistory',           'xpfe/components/shistory',
  'sidebar',            'xpfe/components/sidebar',
  'smime',              'mailnews/mime/cthandlers/smimestub',
  'string',             'string',
  'svg_doc',            'content/svg/document',
  'test_necko',         'netwerk/test',
  'TestStreamConv',     'netwerk/streamconv/test',
  'timebombgen',        'xpfe/components/timebomb/tools',
  'transformiix',       'extensions/transformiix',
  'txmgr',              'editor/txmgr',
  'txtsvc',             'editor/txtsvc',
  'ucgendat',           'intl/unicharutil/tools/',
  'uconv',              'intl/uconv',
  'ucvcn',              'intl/uconv/ucvcn',
  'ucvibm',             'intl/uconv/ucvibm',
  'ucvja',              'intl/uconv/ucvja',
  'ucvko',              'intl/uconv/ucvko',
  'ucvlatin',           'intl/uconv/ucvlatin',
  'ucvmath',            'intl/uconv/ucvmath',
  'ucvtw',              'intl/uconv/ucvtw',
  'ucvtw2',             'intl/uconv/ucvtw2',
  'unicharutil',        'intl/unicharutil',
  'universalchardet',   'extensions/universalchardet',
  'uriloader',          'uriloader',
  'util',               'modules/libutil',
  'vcard',              'mailnews/mime/cthandlers/vcard',
  'view',               'view',
  'wallet',             'extensions/wallet',
  'walletviewers',      'extensions/wallet/build extensions/wallet/cookieviewer extensions/wallet/editor extensions/wallet/signonviewer extensions/wallet/walletpreview',
  'webbrwsr',           'embedding/browser/webBrowser embedding/browser/build',
  'webbrowserpersist',  'embedding/components/webbrowserpersist',
  'webshell',           'webshell',
  'webshell_tests',     'webshell/tests',
  'widget',             'widget',
  'windowwatcher',      'embedding/components/windowwatcher',
  'wsproxytest',        'extensions/xmlextras/proxy/src/tests',
  'xlibrgb',            'gfx/src/xlibrgb',
  'xml-rpc',            'extensions/xml-rpc',
  'xmlextras',          'extensions/xmlextras',
  'xmlterm',            'extensions/xmlterm',
  'xpcom',              'xpcom',
  'xpcomsample',        'xpcom/sample',
  'xpconnect',          'js/src/xpconnect',
  'xpcshell',           'js/src/xpconnect/shell',
  'xpctools',           'js/src/xpconnect/tools',
  'xpconnect_tests',    'js/src/xpconnect/tests',
  'xpctest',            'js/src/xpconnect/tests/components',
  'xpinstall',          'xpinstall',
  'xpistub',            'xpinstall/stub',
  'xpnet',              'xpinstall/wizard/libxpnet',
  'xremoteservice',     'xpfe/components/xremote',
  'xul',                'content/xul/content',
  'xuldoc',             'content/xul/document',
  'xultmpl',            'content/xul/templates',
  'zlib',               'modules/zlib',
);


sub dir_for_required_component {
  my ($component) = @_;
  my $rv;
  my $dir;

  $dir = $map{$component};
  if($dir) {
	# prepend "mozilla/" in front of directory names.
	$rv = "mozilla/$dir";
	$rv =~ s/\s+/ mozilla\//g;  # Hack for 2 or more directories.
  } else {
	$rv = 0;
  }
  return $rv;
}

my $list_only_mode = 0;
my $opt_list_only;
{

  # Add stdin to the commandline.  This makes commandline-only mode hang,
  # call it a bug.  Not sure how to get around this.
  push (@ARGV, split(' ',<STDIN>));

  PrintUsage() if !GetOptions('list-only' => \$opt_list_only);

  # Pick up arguments, if any.
  if($opt_list_only) {
  	$list_only_mode = 1;
  }

  my $arg;
  my $dir;
  while ($arg = shift @ARGV) {
	$dir = dir_for_required_component($arg);
	if($dir) {
      if($list_only_mode) {
		print $dir, " ";
	  } else {
		print "$arg: ", $dir, "\n";
	  }
	} else {
	  # do nothing
	}
  }
  if($dir && $list_only_mode) {
	print "\n";
  }
}
