# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import re
import copy

import simplejson as json
from cuddlefish.bunch import Bunch

MANIFEST_NAME = 'package.json'
DEFAULT_LOADER = 'addon-sdk'

# Is different from root_dir when running tests
env_root = os.environ.get('CUDDLEFISH_ROOT')

DEFAULT_PROGRAM_MODULE = 'main'

DEFAULT_ICON = 'icon.png'
DEFAULT_ICON64 = 'icon64.png'

METADATA_PROPS = ['name', 'description', 'keywords', 'author', 'version',
                  'translators', 'contributors', 'license', 'homepage', 'icon',
                  'icon64', 'main', 'directories', 'permissions', 'preferences']

RESOURCE_HOSTNAME_RE = re.compile(r'^[a-z0-9_\-]+$')

class Error(Exception):
    pass

class MalformedPackageError(Error):
    pass

class MalformedJsonFileError(Error):
    pass

class DuplicatePackageError(Error):
    pass

class PackageNotFoundError(Error):
    def __init__(self, missing_package, reason):
        self.missing_package = missing_package
        self.reason = reason
    def __str__(self):
        return "%s (%s)" % (self.missing_package, self.reason)

class BadChromeMarkerError(Error):
    pass

def validate_resource_hostname(name):
    """
    Validates the given hostname for a resource: URI.

    For more information, see:

      https://bugzilla.mozilla.org/show_bug.cgi?id=566812#c13

    Examples:

      >>> validate_resource_hostname('blarg')

      >>> validate_resource_hostname('bl arg')
      Traceback (most recent call last):
      ...
      ValueError: Error: the name of your package contains an invalid character.
      Package names can contain only lower-case letters, numbers, underscores, and dashes.
      Current package name: bl arg

      >>> validate_resource_hostname('BLARG')
      Traceback (most recent call last):
      ...
      ValueError: Error: the name of your package contains upper-case letters.
      Package names can contain only lower-case letters, numbers, underscores, and dashes.
      Current package name: BLARG

      >>> validate_resource_hostname('foo@bar')
      Traceback (most recent call last):
      ...
      ValueError: Error: the name of your package contains an invalid character.
      Package names can contain only lower-case letters, numbers, underscores, and dashes.
      Current package name: foo@bar
    """

    # See https://bugzilla.mozilla.org/show_bug.cgi?id=568131 for details.
    if not name.islower():
        raise ValueError("""Error: the name of your package contains upper-case letters.
Package names can contain only lower-case letters, numbers, underscores, and dashes.
Current package name: %s""" % name)

    if not RESOURCE_HOSTNAME_RE.match(name):
        raise ValueError("""Error: the name of your package contains an invalid character.
Package names can contain only lower-case letters, numbers, underscores, and dashes.
Current package name: %s""" % name)

def find_packages_with_module(pkg_cfg, name):
    # TODO: Make this support more than just top-level modules.
    filename = "%s.js" % name
    packages = []
    for cfg in pkg_cfg.packages.itervalues():
        if 'lib' in cfg:
            matches = [dirname for dirname in resolve_dirs(cfg, cfg.lib)
                       if os.path.exists(os.path.join(dirname, filename))]
            if matches:
                packages.append(cfg.name)
    return packages

def resolve_dirs(pkg_cfg, dirnames):
    for dirname in dirnames:
        yield resolve_dir(pkg_cfg, dirname)

def resolve_dir(pkg_cfg, dirname):
    return os.path.join(pkg_cfg.root_dir, dirname)

def validate_permissions(perms):
    if (perms.get('cross-domain-content') and
        not isinstance(perms.get('cross-domain-content'), list)):
        raise ValueError("Error: `cross-domain-content` permissions in \
 package.json file must be an array of strings:\n  %s" % perms)

def get_metadata(pkg_cfg, deps):
    metadata = Bunch()
    for pkg_name in deps:
        cfg = pkg_cfg.packages[pkg_name]
        metadata[pkg_name] = Bunch()
        for prop in METADATA_PROPS:
            if cfg.get(prop):
                if prop == 'permissions':
                    validate_permissions(cfg[prop])
                metadata[pkg_name][prop] = cfg[prop]
    return metadata

def set_section_dir(base_json, name, base_path, dirnames, allow_root=False):
    resolved = compute_section_dir(base_json, base_path, dirnames, allow_root)
    if resolved:
        base_json[name] = os.path.abspath(resolved)

def compute_section_dir(base_json, base_path, dirnames, allow_root):
    # PACKAGE_JSON.lib is highest priority
    # then PACKAGE_JSON.directories.lib
    # then lib/ (if it exists)
    # then . (but only if allow_root=True)
    for dirname in dirnames:
        if base_json.get(dirname):
            return os.path.join(base_path, base_json[dirname])
    if "directories" in base_json:
        for dirname in dirnames:
            if dirname in base_json.directories:
                return os.path.join(base_path, base_json.directories[dirname])
    for dirname in dirnames:
        if os.path.isdir(os.path.join(base_path, dirname)):
            return os.path.join(base_path, dirname)
    if allow_root:
        return os.path.abspath(base_path)
    return None

def normalize_string_or_array(base_json, key):
    if base_json.get(key):
        if isinstance(base_json[key], basestring):
            base_json[key] = [base_json[key]]

def load_json_file(path):
    data = open(path, 'r').read()
    try:
        return Bunch(json.loads(data))
    except ValueError, e:
        raise MalformedJsonFileError('%s when reading "%s"' % (str(e),
                                                               path))

def get_config_in_dir(path):
    package_json = os.path.join(path, MANIFEST_NAME)
    if not (os.path.exists(package_json) and
            os.path.isfile(package_json)):
        raise MalformedPackageError('%s not found in "%s"' % (MANIFEST_NAME,
                                                              path))
    base_json = load_json_file(package_json)

    if 'name' not in base_json:
        base_json.name = os.path.basename(path)

    # later processing steps will expect to see the following keys in the
    # base_json that we return:
    #
    #  name: name of the package
    #  lib: list of directories with .js files
    #  test: list of directories with test-*.js files
    #  doc: list of directories with documentation .md files
    #  data: list of directories with bundled arbitrary data files
    #  packages: ?

    if (not base_json.get('tests') and
        os.path.isdir(os.path.join(path, 'test'))):
        base_json['tests'] = 'test'

    set_section_dir(base_json, 'lib', path, ['lib'], True)
    set_section_dir(base_json, 'tests', path, ['test', 'tests'], False)
    set_section_dir(base_json, 'doc', path, ['doc', 'docs'])
    set_section_dir(base_json, 'data', path, ['data'])
    set_section_dir(base_json, 'packages', path, ['packages'])
    set_section_dir(base_json, 'locale', path, ['locale'])

    if (not base_json.get('icon') and
        os.path.isfile(os.path.join(path, DEFAULT_ICON))):
        base_json['icon'] = DEFAULT_ICON

    if (not base_json.get('icon64') and
        os.path.isfile(os.path.join(path, DEFAULT_ICON64))):
        base_json['icon64'] = DEFAULT_ICON64

    for key in ['lib', 'tests', 'dependencies', 'packages']:
        # TODO: lib/tests can be an array?? consider interaction with
        # compute_section_dir above
        normalize_string_or_array(base_json, key)

    if 'main' not in base_json and 'lib' in base_json:
        for dirname in base_json['lib']:
            program = os.path.join(path, dirname,
                                   '%s.js' % DEFAULT_PROGRAM_MODULE)
            if os.path.exists(program):
                base_json['main'] = DEFAULT_PROGRAM_MODULE
                break

    base_json.root_dir = path

    if "dependencies" in base_json:
      deps = base_json["dependencies"]
      deps = [x for x in deps if x not in ["addon-kit", "api-utils"]]
      deps.append("addon-sdk")
      base_json["dependencies"] = deps

    return base_json

def _is_same_file(a, b):
    if hasattr(os.path, 'samefile'):
        return os.path.samefile(a, b)
    return a == b

def build_config(root_dir, target_cfg, packagepath=[]):
    dirs_to_scan = [env_root] # root is addon-sdk dir, diff from root_dir in tests

    def add_packages_from_config(pkgconfig):
        if 'packages' in pkgconfig:
            for package_dir in resolve_dirs(pkgconfig, pkgconfig.packages):
                dirs_to_scan.append(package_dir)

    add_packages_from_config(target_cfg)

    packages_dir = os.path.join(root_dir, 'packages')
    if os.path.exists(packages_dir) and os.path.isdir(packages_dir):
        dirs_to_scan.append(packages_dir)
    dirs_to_scan.extend(packagepath)

    packages = Bunch({target_cfg.name: target_cfg})

    while dirs_to_scan:
        packages_dir = dirs_to_scan.pop()
        if os.path.exists(os.path.join(packages_dir, "package.json")):
            package_paths = [packages_dir]
        else:
            package_paths = [os.path.join(packages_dir, dirname)
                             for dirname in os.listdir(packages_dir)
                             if not dirname.startswith('.')]
            package_paths = [dirname for dirname in package_paths
                             if os.path.isdir(dirname)]

        for path in package_paths:
            pkgconfig = get_config_in_dir(path)
            if pkgconfig.name in packages:
                otherpkg = packages[pkgconfig.name]
                if not _is_same_file(otherpkg.root_dir, path):
                    raise DuplicatePackageError(path, otherpkg.root_dir)
            else:
                packages[pkgconfig.name] = pkgconfig
                add_packages_from_config(pkgconfig)

    return Bunch(packages=packages)

def get_deps_for_targets(pkg_cfg, targets):
    visited = []
    deps_left = [[dep, None] for dep in list(targets)]

    while deps_left:
        [dep, required_by] = deps_left.pop()
        if dep not in visited:
            visited.append(dep)
            if dep not in pkg_cfg.packages:
                required_reason = ("required by '%s'" % (required_by)) \
                                    if required_by is not None \
                                    else "specified as target"
                raise PackageNotFoundError(dep, required_reason)
            dep_cfg = pkg_cfg.packages[dep]
            deps_left.extend([[i, dep] for i in dep_cfg.get('dependencies', [])])
            deps_left.extend([[i, dep] for i in dep_cfg.get('extra_dependencies', [])])

    return visited

def generate_build_for_target(pkg_cfg, target, deps,
                              include_tests=True,
                              include_dep_tests=False,
                              is_running_tests=False,
                              default_loader=DEFAULT_LOADER):

    build = Bunch(# Contains section directories for all packages:
                  packages=Bunch(),
                  locale=Bunch()
                  )

    def add_section_to_build(cfg, section, is_code=False,
                             is_data=False):
        if section in cfg:
            dirnames = cfg[section]
            if isinstance(dirnames, basestring):
                # This is just for internal consistency within this
                # function, it has nothing to do w/ a non-canonical
                # configuration dict.
                dirnames = [dirnames]
            for dirname in resolve_dirs(cfg, dirnames):
                # ensure that package name is valid
                try:
                    validate_resource_hostname(cfg.name)
                except ValueError, err:
                    print err
                    sys.exit(1)
                # ensure that this package has an entry
                if not cfg.name in build.packages:
                    build.packages[cfg.name] = Bunch()
                # detect duplicated sections
                if section in build.packages[cfg.name]:
                    raise KeyError("package's section already defined",
                                   cfg.name, section)
                # Register this section (lib, data, tests)
                build.packages[cfg.name][section] = dirname

    def add_locale_to_build(cfg):
        # Bug 730776: Ignore locales for addon-kit, that are only for unit tests
        if not is_running_tests and cfg.name == "addon-sdk":
            return

        path = resolve_dir(cfg, cfg['locale'])
        files = os.listdir(path)
        for filename in files:
            fullpath = os.path.join(path, filename)
            if os.path.isfile(fullpath) and filename.endswith('.properties'):
                language = filename[:-len('.properties')]

                from property_parser import parse_file, MalformedLocaleFileError
                try:
                    content = parse_file(fullpath)
                except MalformedLocaleFileError, msg:
                    print msg[0]
                    sys.exit(1)

                # Merge current locales into global locale hashtable.
                # Locale files only contains one big JSON object
                # that act as an hastable of:
                # "keys to translate" => "translated keys"
                if language in build.locale:
                    merge = (build.locale[language].items() +
                             content.items())
                    build.locale[language] = Bunch(merge)
                else:
                    build.locale[language] = content

    def add_dep_to_build(dep):
        dep_cfg = pkg_cfg.packages[dep]
        add_section_to_build(dep_cfg, "lib", is_code=True)
        add_section_to_build(dep_cfg, "data", is_data=True)
        if include_tests and include_dep_tests:
            add_section_to_build(dep_cfg, "tests", is_code=True)
        if 'locale' in dep_cfg:
            add_locale_to_build(dep_cfg)
        if ("loader" in dep_cfg) and ("loader" not in build):
            build.loader = "%s/%s" % (dep,
                                                 dep_cfg.loader)

    target_cfg = pkg_cfg.packages[target]

    if include_tests and not include_dep_tests:
        add_section_to_build(target_cfg, "tests", is_code=True)

    for dep in deps:
        add_dep_to_build(dep)

    if 'loader' not in build:
        add_dep_to_build(DEFAULT_LOADER)

    if 'icon' in target_cfg:
        build['icon'] = os.path.join(target_cfg.root_dir, target_cfg.icon)
        del target_cfg['icon']

    if 'icon64' in target_cfg:
        build['icon64'] = os.path.join(target_cfg.root_dir, target_cfg.icon64)
        del target_cfg['icon64']

    if 'id' in target_cfg:
        # NOTE: logic duplicated from buildJID()
        jid = target_cfg['id']
        if not ('@' in jid or jid.startswith('{')):
            jid += '@jetpack'
        build['preferencesBranch'] = jid

    if 'preferences-branch' in target_cfg:
        # check it's a non-empty, valid branch name
        preferencesBranch = target_cfg['preferences-branch']
        if re.match('^[\w{@}-]+$', preferencesBranch):
            build['preferencesBranch'] = preferencesBranch
        elif not is_running_tests:
            print >>sys.stderr, "IGNORING preferences-branch (not a valid branch name)"

    return build

def _get_files_in_dir(path):
    data = {}
    files = os.listdir(path)
    for filename in files:
        fullpath = os.path.join(path, filename)
        if os.path.isdir(fullpath):
            data[filename] = _get_files_in_dir(fullpath)
        else:
            try:
                info = os.stat(fullpath)
                data[filename] = ("file", dict(size=info.st_size))
            except OSError:
                pass
    return ("directory", data)

def build_pkg_index(pkg_cfg):
    pkg_cfg = copy.deepcopy(pkg_cfg)
    for pkg in pkg_cfg.packages:
        root_dir = pkg_cfg.packages[pkg].root_dir
        files = _get_files_in_dir(root_dir)
        pkg_cfg.packages[pkg].files = files
        try:
            readme = open(root_dir + '/README.md').read()
            pkg_cfg.packages[pkg].readme = readme
        except IOError:
            pass
        del pkg_cfg.packages[pkg].root_dir
    return pkg_cfg.packages

def build_pkg_cfg(root):
    pkg_cfg = build_config(root, Bunch(name='dummy'))
    del pkg_cfg.packages['dummy']
    return pkg_cfg

def call_plugins(pkg_cfg, deps):
    for dep in deps:
        dep_cfg = pkg_cfg.packages[dep]
        dirnames = dep_cfg.get('python-lib', [])
        for dirname in resolve_dirs(dep_cfg, dirnames):
            sys.path.append(dirname)
        module_names = dep_cfg.get('python-plugins', [])
        for module_name in module_names:
            module = __import__(module_name)
            module.init(root_dir=dep_cfg.root_dir)

def call_cmdline_tool(env_root, pkg_name):
    pkg_cfg = build_config(env_root, Bunch(name='dummy'))
    if pkg_name not in pkg_cfg.packages:
        print "This tool requires the '%s' package." % pkg_name
        sys.exit(1)
    cfg = pkg_cfg.packages[pkg_name]
    for dirname in resolve_dirs(cfg, cfg['python-lib']):
        sys.path.append(dirname)
    module_name = cfg.get('python-cmdline-tool')
    module = __import__(module_name)
    module.run()
