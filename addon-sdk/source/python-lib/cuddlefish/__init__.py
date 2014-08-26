# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import optparse
import time

from copy import copy
import simplejson as json
from cuddlefish import packaging
from cuddlefish._version import get_versions

MOZRUNNER_BIN_NOT_FOUND = 'Mozrunner could not locate your binary'
MOZRUNNER_BIN_NOT_FOUND_HELP = """
I can't find the application binary in any of its default locations
on your system. Please specify one using the -b/--binary option.
"""

UPDATE_RDF_FILENAME = "%s.update.rdf"
XPI_FILENAME = "%s.xpi"

usage = """
%prog [options] command [command-specific options]

Supported Commands:
  init       - create a sample addon in an empty directory
  test       - run tests
  run        - run program
  xpi        - generate an xpi

Internal Commands:
  testcfx    - test the cfx tool
  testex     - test all example code
  testpkgs   - test all installed packages
  testall    - test whole environment

Experimental and internal commands and options are not supported and may be
changed or removed in the future.
"""

global_options = [
    (("-v", "--verbose",), dict(dest="verbose",
                                help="enable lots of output",
                                action="store_true",
                                default=False)),
    ]

parser_groups = (
    ("Supported Command-Specific Options", [
        (("", "--update-url",), dict(dest="update_url",
                                     help="update URL in install.rdf",
                                     metavar=None,
                                     default=None,
                                     cmds=['xpi'])),
        (("", "--update-link",), dict(dest="update_link",
                                      help="generate update.rdf",
                                      metavar=None,
                                      default=None,
                                      cmds=['xpi'])),
        (("-p", "--profiledir",), dict(dest="profiledir",
                                       help=("profile directory to pass to "
                                             "app"),
                                       metavar=None,
                                       default=None,
                                       cmds=['test', 'run', 'testex',
                                             'testpkgs', 'testall'])),
        (("-b", "--binary",), dict(dest="binary",
                                   help="path to app binary",
                                   metavar=None,
                                   default=None,
                                   cmds=['test', 'run', 'testex', 'testpkgs',
                                         'testall'])),
        (("", "--binary-args",), dict(dest="cmdargs",
                                 help=("additional arguments passed to the "
                                       "binary"),
                                 metavar=None,
                                 default=None,
                                 cmds=['run', 'test'])),
        (("", "--dependencies",), dict(dest="dep_tests",
                                       help="include tests for all deps",
                                       action="store_true",
                                       default=False,
                                       cmds=['test', 'testex', 'testpkgs',
                                             'testall'])),
        (("", "--times",), dict(dest="iterations",
                                type="int",
                                help="number of times to run tests",
                                default=1,
                                cmds=['test', 'testex', 'testpkgs',
                                      'testall'])),
        (("-f", "--filter",), dict(dest="filter",
                                   help=("only run tests whose filenames "
                                         "match FILENAME and optionally "
                                         "match TESTNAME, both regexps"),
                                   metavar="FILENAME[:TESTNAME]",
                                   default='',
                                   cmds=['test', 'testex', 'testaddons', 'testpkgs',
                                         'testall'])),
        (("-g", "--use-config",), dict(dest="config",
                                       help="use named config from local.json",
                                       metavar=None,
                                       default="default",
                                       cmds=['test', 'run', 'xpi', 'testex',
                                             'testpkgs', 'testall'])),
        (("", "--templatedir",), dict(dest="templatedir",
                                      help="XULRunner app/ext. template",
                                      metavar=None,
                                      default=None,
                                      cmds=['run', 'xpi'])),
        (("", "--package-path",), dict(dest="packagepath", action="append",
                                       help="extra directories for package search",
                                       metavar=None,
                                       default=[],
                                       cmds=['run', 'xpi', 'test'])),
        (("", "--extra-packages",), dict(dest="extra_packages",
                                         help=("extra packages to include, "
                                               "comma-separated. Default is "
                                               "'addon-sdk'."),
                                         metavar=None,
                                         default="addon-sdk",
                                         cmds=['run', 'xpi', 'test', 'testex',
                                               'testpkgs', 'testall',
                                               'testcfx'])),
        (("", "--pkgdir",), dict(dest="pkgdir",
                                 help=("package dir containing "
                                       "package.json; default is "
                                       "current directory"),
                                 metavar=None,
                                 default=None,
                                 cmds=['run', 'xpi', 'test'])),
        (("", "--static-args",), dict(dest="static_args",
                                      help="extra harness options as JSON",
                                      type="json",
                                      metavar=None,
                                      default="{}",
                                      cmds=['run', 'xpi'])),
        (("", "--parseable",), dict(dest="parseable",
                                    help="display test output in a parseable format",
                                    action="store_true",
                                    default=False,
                                    cmds=['run', 'test', 'testex', 'testpkgs',
                                          'testaddons', 'testall'])),
        ]
     ),

    ("Experimental Command-Specific Options", [
        (("-a", "--app",), dict(dest="app",
                                help=("app to run: firefox (default), fennec, "
                                      "fennec-on-device, xulrunner or "
                                      "thunderbird"),
                                metavar=None,
                                type="choice",
                                choices=["firefox", "fennec",
                                         "fennec-on-device", "thunderbird",
                                         "xulrunner"],
                                default="firefox",
                                cmds=['test', 'run', 'testex', 'testpkgs',
                                      'testall'])),
        (("-o", "--overload-modules",), dict(dest="overload_modules",
                                     help=("Overload JS modules integrated into"
                                           " Firefox with the one from your SDK"
                                           " repository"),
                                     action="store_true",
                                     default=False,
                                     cmds=['run', 'test', 'testex', 'testpkgs',
                                           'testall'])),
        (("", "--strip-sdk",), dict(dest="bundle_sdk",
                                    help=("Do not ship SDK modules in the xpi"),
                                    action="store_false",
                                    default=False,
                                    cmds=['run', 'test', 'testex', 'testpkgs',
                                          'testall', 'xpi'])),
        (("", "--force-use-bundled-sdk",), dict(dest="force_use_bundled_sdk",
                                    help=("When --strip-sdk isn't passed, "
                                          "force using sdk modules shipped in "
                                          "the xpi instead of firefox ones"),
                                    action="store_true",
                                    default=False,
                                    cmds=['run', 'test', 'testex', 'testpkgs',
                                          'testall', 'xpi'])),
        (("", "--no-run",), dict(dest="no_run",
                                     help=("Instead of launching the "
                                           "application, just show the command "
                                           "for doing so.  Use this to launch "
                                           "the application in a debugger like "
                                           "gdb."),
                                     action="store_true",
                                     default=False,
                                     cmds=['run', 'test'])),
        (("", "--no-strip-xpi",), dict(dest="no_strip_xpi",
                                    help="retain unused modules in XPI",
                                    action="store_true",
                                    default=False,
                                    cmds=['xpi'])),
        (("", "--force-mobile",), dict(dest="enable_mobile",
                                    help="Force compatibility with Firefox Mobile",
                                    action="store_true",
                                    default=False,
                                    cmds=['run', 'test', 'xpi', 'testall'])),
        (("", "--mobile-app",), dict(dest="mobile_app_name",
                                    help=("Name of your Android application to "
                                          "use. Possible values: 'firefox', "
                                          "'firefox_beta', 'fennec_aurora', "
                                          "'fennec' (for nightly)."),
                                    metavar=None,
                                    default=None,
                                    cmds=['run', 'test', 'testall'])),
        (("", "--harness-option",), dict(dest="extra_harness_option_args",
                                         help=("Extra properties added to "
                                               "harness-options.json"),
                                         action="append",
                                         metavar="KEY=VALUE",
                                         default=[],
                                         cmds=['xpi'])),
        (("", "--stop-on-error",), dict(dest="stopOnError",
                                  help="Stop running tests after the first failure",
                                  action="store_true",
                                  metavar=None,
                                  default=False,
                                  cmds=['test', 'testex', 'testpkgs'])),
        (("", "--check-memory",), dict(dest="check_memory",
                                       help="attempts to detect leaked compartments after a test run",
                                       action="store_true",
                                       default=False,
                                       cmds=['test', 'testpkgs', 'testaddons',
                                             'testall'])),
        (("", "--output-file",), dict(dest="output_file",
                                      help="Where to put the finished .xpi",
                                      default=None,
                                      cmds=['xpi'])),
        (("", "--manifest-overload",), dict(dest="manifest_overload",
                                      help="JSON file to overload package.json properties",
                                      default=None,
                                      cmds=['xpi'])),
        (("", "--abort-on-missing-module",), dict(dest="abort_on_missing",
                                      help="Abort if required module is missing",
                                      action="store_true",
                                      default=False,
                                      cmds=['test', 'run', 'xpi', 'testpkgs'])),
        (("", "--no-connections",), dict(dest="no_connections",
                                      help="disable/enable remote connections (on for cfx run only by default)",
                                      type="choice",
                                      choices=["on", "off", "default"],
                                      default="default",
                                      cmds=['test', 'run', 'testpkgs',
                                            'testall', 'testaddons', 'testex'])),
        ]
     ),

    ("Internal Command-Specific Options", [
        (("", "--addons",), dict(dest="addons",
                                 help=("paths of addons to install, "
                                       "comma-separated"),
                                 metavar=None,
                                 default=None,
                                 cmds=['test', 'run', 'testex', 'testpkgs',
                                       'testall'])),
        (("", "--test-runner-pkg",), dict(dest="test_runner_pkg",
                                          help=("name of package "
                                                "containing test runner "
                                                "program (default is "
                                                "test-harness)"),
                                          default="addon-sdk",
                                          cmds=['test', 'testex', 'testpkgs',
                                                'testall'])),
        # --keydir was removed in 1.0b5, but we keep it around in the options
        # parser to make life easier for frontends like FlightDeck which
        # might still pass it. It can go away once the frontends are updated.
        (("", "--keydir",), dict(dest="keydir",
                                 help=("obsolete, ignored"),
                                 metavar=None,
                                 default=None,
                                 cmds=['test', 'run', 'xpi', 'testex',
                                       'testpkgs', 'testall'])),
        (("", "--e10s",), dict(dest="enable_e10s",
                               help="enable remote windows",
                               action="store_true",
                               default=False,
                               cmds=['test', 'run', 'testex', 'testpkgs',
                                     'testaddons', 'testcfx', 'testall'])),
        (("", "--logfile",), dict(dest="logfile",
                                  help="log console output to file",
                                  metavar=None,
                                  default=None,
                                  cmds=['run', 'test', 'testex', 'testpkgs'])),
        # TODO: This should default to true once our memory debugging
        # issues are resolved; see bug 592774.
        (("", "--profile-memory",), dict(dest="profileMemory",
                                         help=("profile memory usage "
                                               "(default is false)"),
                                         type="int",
                                         action="store",
                                         default=0,
                                         cmds=['test', 'testex', 'testpkgs',
                                               'testall'])),
        ]
     ),
    )

def find_parent_package(cur_dir):
    tail = True
    while tail:
        if os.path.exists(os.path.join(cur_dir, 'package.json')):
            return cur_dir
        cur_dir, tail = os.path.split(cur_dir)
    return None

def check_json(option, opt, value):
    # We return the parsed JSON here; see bug 610816 for background on why.
    try:
        return json.loads(value)
    except ValueError:
        raise optparse.OptionValueError("Option %s must be JSON." % opt)

class CfxOption(optparse.Option):
    TYPES = optparse.Option.TYPES + ('json',)
    TYPE_CHECKER = copy(optparse.Option.TYPE_CHECKER)
    TYPE_CHECKER['json'] = check_json

def parse_args(arguments, global_options, usage, version, parser_groups,
               defaults=None):
    parser = optparse.OptionParser(usage=usage.strip(), option_class=CfxOption,
                                   version=version)

    def name_cmp(a, b):
        # a[0]    = name sequence
        # a[0][0] = short name (possibly empty string)
        # a[0][1] = long name
        names = []
        for seq in (a, b):
            names.append(seq[0][0][1:] if seq[0][0] else seq[0][1][2:])
        return cmp(*names)

    global_options.sort(name_cmp)
    for names, opts in global_options:
        parser.add_option(*names, **opts)

    for group_name, options in parser_groups:
        group = optparse.OptionGroup(parser, group_name)
        options.sort(name_cmp)
        for names, opts in options:
            if 'cmds' in opts:
                cmds = opts['cmds']
                del opts['cmds']
                cmds.sort()
                if not 'help' in opts:
                    opts['help'] = ""
                opts['help'] += " (%s)" % ", ".join(cmds)
            group.add_option(*names, **opts)
        parser.add_option_group(group)

    if defaults:
        parser.set_defaults(**defaults)

    (options, args) = parser.parse_args(args=arguments)

    if not args:
        parser.print_help()
        parser.exit()

    return (options, args)

# all tests emit progress messages to stderr, not stdout. (the mozrunner
# console output goes to stderr and is hard to change, and
# unittest.TextTestRunner prefers stderr, so we send everything else there
# too, to keep all the messages in order)

def test_all(env_root, defaults):
    fail = False

    starttime = time.time()

    if not defaults['filter']:
        print >>sys.stderr, "Testing cfx..."
        sys.stderr.flush()
        result = test_cfx(env_root, defaults['verbose'])
        if result.failures or result.errors:
            fail = True

    if not fail or not defaults.get("stopOnError"):
        print >>sys.stderr, "Testing all examples..."
        sys.stderr.flush()

        try:
            test_all_examples(env_root, defaults)
        except SystemExit, e:
            fail = (e.code != 0) or fail

    if not fail or not defaults.get("stopOnError"):
        print >>sys.stderr, "Testing all unit-test addons..."
        sys.stderr.flush()

        try:
            test_all_testaddons(env_root, defaults)
        except SystemExit, e:
            fail = (e.code != 0) or fail

    if not fail or not defaults.get("stopOnError"):
        print >>sys.stderr, "Testing all packages..."
        sys.stderr.flush()
        try:
            test_all_packages(env_root, defaults)
        except SystemExit, e:
            fail = (e.code != 0) or fail

    print >>sys.stderr, "Total time for all tests: %f seconds" % (time.time() - starttime)

    if fail:
        print >>sys.stderr, "Some tests were unsuccessful."
        sys.exit(1)
    print >>sys.stderr, "All tests were successful. Ship it!"
    sys.exit(0)

def test_cfx(env_root, verbose):
    import cuddlefish.tests

    # tests write to stderr. flush everything before and after to avoid
    # confusion later.
    sys.stdout.flush(); sys.stderr.flush()
    olddir = os.getcwd()
    os.chdir(env_root)
    retval = cuddlefish.tests.run(verbose)
    os.chdir(olddir)
    sys.stdout.flush(); sys.stderr.flush()
    return retval

def test_all_testaddons(env_root, defaults):
    addons_dir = os.path.join(env_root, "test", "addons")
    addons = [dirname for dirname in os.listdir(addons_dir)
                if os.path.isdir(os.path.join(addons_dir, dirname))]
    addons.sort()
    fail = False
    for dirname in addons:
        # apply the filter
        if (not defaults['filter'].split(":")[0] in dirname):
            continue

        print >>sys.stderr, "Testing %s..." % dirname
        sys.stderr.flush()
        try:
            run(arguments=["testrun",
                           "--pkgdir",
                           os.path.join(addons_dir, dirname)],
                defaults=defaults,
                env_root=env_root)
        except SystemExit, e:
            fail = (e.code != 0) or fail
        if fail and defaults.get("stopOnError"):
            break

    if fail:
        print >>sys.stderr, "Some test addons tests were unsuccessful."
        sys.exit(-1)

def test_all_examples(env_root, defaults):
    examples_dir = os.path.join(env_root, "examples")
    examples = [dirname for dirname in os.listdir(examples_dir)
                if os.path.isdir(os.path.join(examples_dir, dirname))]
    examples.sort()
    fail = False
    for dirname in examples:
        if (not defaults['filter'].split(":")[0] in dirname):
            continue

        print >>sys.stderr, "Testing %s..." % dirname
        sys.stderr.flush()
        try:
            run(arguments=["test",
                           "--pkgdir",
                           os.path.join(examples_dir, dirname)],
                defaults=defaults,
                env_root=env_root)
        except SystemExit, e:
            fail = (e.code != 0) or fail
        if fail and defaults.get("stopOnError"):
            break

    if fail:
        print >>sys.stderr, "Some examples tests were unsuccessful."
        sys.exit(-1)

def test_all_packages(env_root, defaults):
    packages_dir = os.path.join(env_root, "packages")
    if os.path.isdir(packages_dir):
      packages = [dirname for dirname in os.listdir(packages_dir)
                  if os.path.isdir(os.path.join(packages_dir, dirname))]
    else:
      packages = []
    packages.append(env_root)
    packages.sort()
    print >>sys.stderr, "Testing all available packages: %s." % (", ".join(packages))
    sys.stderr.flush()
    fail = False
    for dirname in packages:
        print >>sys.stderr, "Testing %s..." % dirname
        sys.stderr.flush()
        try:
            run(arguments=["test",
                           "--pkgdir",
                           os.path.join(packages_dir, dirname)],
                defaults=defaults,
                env_root=env_root)
        except SystemExit, e:
            fail = (e.code != 0) or fail
        if fail and defaults.get('stopOnError'):
            break
    if fail:
        print >>sys.stderr, "Some package tests were unsuccessful."
        sys.exit(-1)

def get_config_args(name, env_root):
    local_json = os.path.join(env_root, "local.json")
    if not (os.path.exists(local_json) and
            os.path.isfile(local_json)):
        if name == "default":
            return []
        else:
            print >>sys.stderr, "File does not exist: %s" % local_json
            sys.exit(1)
    local_json = packaging.load_json_file(local_json)
    if 'configs' not in local_json:
        print >>sys.stderr, "'configs' key not found in local.json."
        sys.exit(1)
    if name not in local_json.configs:
        if name == "default":
            return []
        else:
            print >>sys.stderr, "No config found for '%s'." % name
            sys.exit(1)
    config = local_json.configs[name]
    if type(config) != list:
        print >>sys.stderr, "Config for '%s' must be a list of strings." % name
        sys.exit(1)
    return config

def initializer(env_root, args, out=sys.stdout, err=sys.stderr):
    from templates import PACKAGE_JSON, TEST_MAIN_JS
    from preflight import create_jid
    path = os.getcwd()
    addon = os.path.basename(path)
    # if more than two arguments
    if len(args) > 2:
        print >>err, 'Too many arguments.'
        return {"result":1}
    if len(args) == 2:
        path = os.path.join(path,args[1])
        try:
            os.mkdir(path)
            print >>out, '*', args[1], 'package directory created'
        except OSError:
            print >>out, '*', args[1], 'already exists, testing if directory is empty'
    # avoid clobbering existing files, but we tolerate things like .git
    existing = [fn for fn in os.listdir(path) if not fn.startswith(".")]
    if existing:
        print >>err, 'This command must be run in an empty directory.'
        return {"result":1}
    for d in ['lib','data','test']:
        os.mkdir(os.path.join(path,d))
        print >>out, '*', d, 'directory created'
    jid = create_jid()
    print >>out, '* generated jID automatically:', jid
    open(os.path.join(path,'package.json'),'w').write(PACKAGE_JSON % {'name':addon.lower(),
                                                   'title':addon,
                                                   'id':jid })
    print >>out, '* package.json written'
    open(os.path.join(path,'test','test-main.js'),'w').write(TEST_MAIN_JS)
    print >>out, '* test/test-main.js written'
    open(os.path.join(path,'lib','main.js'),'w').write('')
    print >>out, '* lib/main.js written'
    if len(args) == 1:
        print >>out, '\nYour sample add-on is now ready.'
        print >>out, 'Do "cfx test" to test it and "cfx run" to try it.  Have fun!'
    else:
        print >>out, '\nYour sample add-on is now ready in the \'' + args[1] +  '\' directory.'
        print >>out, 'Change to that directory, then do "cfx test" to test it, \nand "cfx run" to try it.  Have fun!'
    return {"result":0, "jid":jid}

def buildJID(target_cfg):
    if "id" in target_cfg:
        jid = target_cfg["id"]
    else:
        import uuid
        jid = str(uuid.uuid4())
    if not ("@" in jid or jid.startswith("{")):
        jid = jid + "@jetpack"
    return jid

def run(arguments=sys.argv[1:], target_cfg=None, pkg_cfg=None,
        defaults=None, env_root=os.environ.get('CUDDLEFISH_ROOT'),
        stdout=sys.stdout):
    versions = get_versions()
    sdk_version = versions["version"]
    display_version = "Add-on SDK %s (%s)" % (sdk_version, versions["full"])
    parser_kwargs = dict(arguments=arguments,
                         global_options=global_options,
                         parser_groups=parser_groups,
                         usage=usage,
                         version=display_version,
                         defaults=defaults)

    (options, args) = parse_args(**parser_kwargs)

    config_args = get_config_args(options.config, env_root);

    # reparse configs with arguments from local.json
    if config_args:
        parser_kwargs['arguments'] += config_args
        (options, args) = parse_args(**parser_kwargs)

    command = args[0]

    if command == "init":
        initializer(env_root, args)
        return
    if command == "testpkgs":
        test_all_packages(env_root, defaults=options.__dict__)
        return
    elif command == "testaddons":
        test_all_testaddons(env_root, defaults=options.__dict__)
        return
    elif command == "testex":
        test_all_examples(env_root, defaults=options.__dict__)
        return
    elif command == "testall":
        test_all(env_root, defaults=options.__dict__)
        return
    elif command == "testcfx":
        if options.filter:
            print >>sys.stderr, "The filter option is not valid with the testcfx command"
            return
        test_cfx(env_root, options.verbose)
        return
    elif command not in ["xpi", "test", "run", "testrun"]:
        print >>sys.stderr, "Unknown command: %s" % command
        print >>sys.stderr, "Try using '--help' for assistance."
        sys.exit(1)

    target_cfg_json = None
    if not target_cfg:
        if not options.pkgdir:
            options.pkgdir = find_parent_package(os.getcwd())
            if not options.pkgdir:
                print >>sys.stderr, ("cannot find 'package.json' in the"
                                     " current directory or any parent.")
                sys.exit(1)
        else:
            options.pkgdir = os.path.abspath(options.pkgdir)
        if not os.path.exists(os.path.join(options.pkgdir, 'package.json')):
            print >>sys.stderr, ("cannot find 'package.json' in"
                                 " %s." % options.pkgdir)
            sys.exit(1)

        target_cfg_json = os.path.join(options.pkgdir, 'package.json')
        target_cfg = packaging.get_config_in_dir(options.pkgdir)

    if options.manifest_overload:
        for k, v in packaging.load_json_file(options.manifest_overload).items():
            target_cfg[k] = v

    # At this point, we're either building an XPI or running Jetpack code in
    # a Mozilla application (which includes running tests).

    use_main = False
    inherited_options = ['verbose', 'enable_e10s', 'parseable', 'check_memory',
                         'abort_on_missing']
    enforce_timeouts = False

    if command == "xpi":
        use_main = True
    elif command == "test":
        if 'tests' not in target_cfg:
            target_cfg['tests'] = []
        inherited_options.extend(['iterations', 'filter', 'profileMemory',
                                  'stopOnError'])
        enforce_timeouts = True
    elif command == "run":
        use_main = True
    elif command == "testrun":
        use_main = True
        enforce_timeouts = True
    else:
        assert 0, "shouldn't get here"

    if use_main and 'main' not in target_cfg:
        # If the user supplies a template dir, then the main
        # program may be contained in the template.
        if not options.templatedir:
            print >>sys.stderr, "package.json does not have a 'main' entry."
            sys.exit(1)

    if not pkg_cfg:
        pkg_cfg = packaging.build_config(env_root, target_cfg, options.packagepath)

    target = target_cfg.name

    # TODO: Consider keeping a cache of dynamic UUIDs, based
    # on absolute filesystem pathname, in the root directory
    # or something.
    if command in ('xpi', 'run', 'testrun'):
        from cuddlefish.preflight import preflight_config
        if target_cfg_json:
            config_was_ok, modified = preflight_config(target_cfg,
                                                       target_cfg_json)
            if not config_was_ok:
                if modified:
                    # we need to re-read package.json . The safest approach
                    # is to re-run the "cfx xpi"/"cfx run" command.
                    print >>sys.stderr, ("package.json modified: please re-run"
                                         " 'cfx %s'" % command)
                else:
                    print >>sys.stderr, ("package.json needs modification:"
                                         " please update it and then re-run"
                                         " 'cfx %s'" % command)
                sys.exit(1)
        # if we make it this far, we have a JID
    else:
        assert command == "test"

    jid = buildJID(target_cfg)

    targets = [target]
    if command == "test":
        targets.append(options.test_runner_pkg)

    extra_packages = []
    if options.extra_packages:
        extra_packages = options.extra_packages.split(",")
    if extra_packages:
        targets.extend(extra_packages)
        target_cfg.extra_dependencies = extra_packages

    deps = packaging.get_deps_for_targets(pkg_cfg, targets)

    from cuddlefish.manifest import build_manifest, ModuleNotFoundError, \
                                    BadChromeMarkerError
    # Figure out what loader files should be scanned. This is normally
    # computed inside packaging.generate_build_for_target(), by the first
    # dependent package that defines a "loader" property in its package.json.
    # This property is interpreted as a filename relative to the top of that
    # file, and stored as a path in build.loader . generate_build_for_target()
    # cannot be called yet (it needs the list of used_deps that
    # build_manifest() computes, but build_manifest() needs the list of
    # loader files that it computes). We could duplicate or factor out this
    # build.loader logic, but that would be messy, so instead we hard-code
    # the choice of loader for manifest-generation purposes. In practice,
    # this means that alternative loaders probably won't work with
    # --strip-xpi.
    assert packaging.DEFAULT_LOADER == "addon-sdk"
    assert pkg_cfg.packages["addon-sdk"].loader == "lib/sdk/loader/cuddlefish.js"
    cuddlefish_js_path = os.path.join(pkg_cfg.packages["addon-sdk"].root_dir,
                                      "lib", "sdk", "loader", "cuddlefish.js")
    loader_modules = [("addon-sdk", "lib", "sdk/loader/cuddlefish", cuddlefish_js_path)]
    scan_tests = command == "test"

    try:
        manifest = build_manifest(target_cfg, pkg_cfg, deps, scan_tests,
                                  None, loader_modules,
                                  abort_on_missing=options.abort_on_missing)
    except ModuleNotFoundError, e:
        print str(e)
        sys.exit(1)
    except BadChromeMarkerError, e:
        # An error had already been displayed on stderr in manifest code
        sys.exit(1)
    used_deps = manifest.get_used_packages()
    if command == "test":
        # The test runner doesn't appear to link against any actual packages,
        # because it loads everything at runtime (invisible to the linker).
        # If we believe that, we won't set up URI mappings for anything, and
        # tests won't be able to run.
        used_deps = deps
    for xp in extra_packages:
        if xp not in used_deps:
            used_deps.append(xp)

    build = packaging.generate_build_for_target(
        pkg_cfg, target, used_deps,
        include_dep_tests=options.dep_tests,
        is_running_tests=(command == "test")
        )

    harness_options = {
        'jetpackID': jid,
        'staticArgs': options.static_args,
        'name': target,
        }

    harness_options.update(build)

    # When cfx is run from sdk root directory, we will strip sdk modules and
    # override them with local modules.
    # So that integration tools will continue to work and use local modules
    if os.getcwd() == env_root:
        options.bundle_sdk = True
        options.force_use_bundled_sdk = False
        options.overload_modules = True

    if options.pkgdir == env_root:
        options.bundle_sdk = True
        options.overload_modules = True

    extra_environment = {}
    if command == "test":
        # This should be contained in the test runner package.
        harness_options['main'] = 'sdk/test/runner'
        harness_options['mainPath'] = 'sdk/test/runner'
    else:
        harness_options['main'] = target_cfg.get('main')
        harness_options['mainPath'] = manifest.top_path
    extra_environment["CFX_COMMAND"] = command

    for option in inherited_options:
        harness_options[option] = getattr(options, option)

    harness_options['metadata'] = packaging.get_metadata(pkg_cfg, used_deps)

    harness_options['sdkVersion'] = sdk_version

    packaging.call_plugins(pkg_cfg, used_deps)

    retval = 0

    if options.templatedir:
        app_extension_dir = os.path.abspath(options.templatedir)
    elif os.path.exists(os.path.join(options.pkgdir, "app-extension")):
      app_extension_dir = os.path.join(options.pkgdir, "app-extension")
    else:
        mydir = os.path.dirname(os.path.abspath(__file__))
        app_extension_dir = os.path.join(mydir, "../../app-extension")

    # Do not add entries for SDK modules
    harness_options['manifest'] = manifest.get_harness_options_manifest(False)

    # Gives an hint to tell if sdk modules are bundled or not
    harness_options['is-sdk-bundled'] = options.bundle_sdk or options.no_strip_xpi

    if options.force_use_bundled_sdk:
        if not harness_options['is-sdk-bundled']:
            print >>sys.stderr, ("--force-use-bundled-sdk "
                                 "can't be used if sdk isn't bundled.")
            sys.exit(1)
        if options.overload_modules:
            print >>sys.stderr, ("--force-use-bundled-sdk and --overload-modules "
                                 "can't be used at the same time.")
            sys.exit(1)
        # Pass a flag in order to force using sdk modules shipped in the xpi
        harness_options['force-use-bundled-sdk'] = True

    from cuddlefish.rdf import gen_manifest, RDFUpdate

    manifest_rdf = gen_manifest(template_root_dir=app_extension_dir,
                                target_cfg=target_cfg,
                                jid=jid,
                                update_url=options.update_url,
                                bootstrap=True,
                                enable_mobile=options.enable_mobile)

    if command == "xpi" and options.update_link:
        if not options.update_link.startswith("https"):
            raise optparse.OptionValueError("--update-link must start with 'https': %s" % options.update_link)
        rdf_name = UPDATE_RDF_FILENAME % target_cfg.name
        print >>stdout, "Exporting update description to %s." % rdf_name
        update = RDFUpdate()
        update.add(manifest_rdf, options.update_link)
        open(rdf_name, "w").write(str(update))

    # ask the manifest what files were used, so we can construct an XPI
    # without the rest. This will include the loader (and everything it
    # uses) because of the "loader_modules" starting points we passed to
    # build_manifest earlier
    used_files = None
    if command == "xpi":
        used_files = set(manifest.get_used_files(options.bundle_sdk))

    if options.no_strip_xpi:
        used_files = None # disables the filter, includes all files

    if command == 'xpi':
        from cuddlefish.xpi import build_xpi
        # Generate extra options
        extra_harness_options = {}
        for kv in options.extra_harness_option_args:
            key,value = kv.split("=", 1)
            extra_harness_options[key] = value
        # Generate xpi filepath
        if options.output_file:
          xpi_path = options.output_file
        else:
          xpi_path = XPI_FILENAME % target_cfg.name

        print >>stdout, "Exporting extension to %s." % xpi_path
        build_xpi(template_root_dir=app_extension_dir,
                  manifest=manifest_rdf,
                  xpi_path=xpi_path,
                  harness_options=harness_options,
                  limit_to=used_files,
                  extra_harness_options=extra_harness_options,
                  bundle_sdk=True,
                  pkgdir=options.pkgdir)
    else:
        from cuddlefish.runner import run_app

        if options.no_connections == "default":
            if command == "run":
              no_connections = False
            else:
              no_connections = True
        elif options.no_connections == "on":
            no_connections = True
        else:
            no_connections = False

        if options.profiledir:
            options.profiledir = os.path.expanduser(options.profiledir)
            options.profiledir = os.path.abspath(options.profiledir)

        if options.addons is not None:
            options.addons = options.addons.split(",")

        enable_e10s = options.enable_e10s or target_cfg.get('e10s', False)

        try:
            retval = run_app(harness_root_dir=app_extension_dir,
                             manifest_rdf=manifest_rdf,
                             harness_options=harness_options,
                             app_type=options.app,
                             binary=options.binary,
                             profiledir=options.profiledir,
                             verbose=options.verbose,
                             parseable=options.parseable,
                             enforce_timeouts=enforce_timeouts,
                             logfile=options.logfile,
                             addons=options.addons,
                             args=options.cmdargs,
                             extra_environment=extra_environment,
                             norun=options.no_run,
                             used_files=used_files,
                             enable_mobile=options.enable_mobile,
                             mobile_app_name=options.mobile_app_name,
                             env_root=env_root,
                             is_running_tests=(command == "test"),
                             overload_modules=options.overload_modules,
                             bundle_sdk=options.bundle_sdk,
                             pkgdir=options.pkgdir,
                             enable_e10s=enable_e10s,
                             no_connections=no_connections)
        except ValueError, e:
            print ""
            print "A given cfx option has an inappropriate value:"
            print >>sys.stderr, "  " + "  \n  ".join(str(e).split("\n"))
            retval = -1
        except Exception, e:
            if str(e).startswith(MOZRUNNER_BIN_NOT_FOUND):
                print >>sys.stderr, MOZRUNNER_BIN_NOT_FOUND_HELP.strip()
                retval = -1
            else:
                raise
    sys.exit(retval)
