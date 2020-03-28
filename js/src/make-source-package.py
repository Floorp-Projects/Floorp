#!/usr/bin/env python3 -B
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import enum
import logging
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)


def find_command(names):
    """Search for command in `names`, and returns the first one that exists.
    """

    for name in names:
        path = shutil.which(name)
        if path is not None:
            return name

    return None


def assert_command(env_var, name):
    """Assert that the command is not empty
    The command name comes from either environment variable or find_command.
    """
    if not name:
        logging.error('{} command not found'.format(env_var))
        sys.exit(1)


def parse_version(topsrc_dir):
    """Parse milestone.txt and return the entire milestone and major version.
    """
    milestone_file = topsrc_dir / 'config' / 'milestone.txt'
    if not milestone_file.is_file():
        return ('', '', '')

    with milestone_file.open('r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith('#'):
                continue

            v = line.split('.')
            return tuple((v + ['', ''])[:3])

    return ('', '', '')


tmp_dir = Path('/tmp')

tar = os.environ.get('TAR', find_command(['tar']))
assert_command('TAR', tar)

rsync = os.environ.get('RSYNC', find_command(['rsync']))
assert_command('RSYNC', rsync)

autoconf = os.environ.get('AUTOCONF', find_command([
    'autoconf-2.13',
    'autoconf2.13',
    'autoconf213',
]))
assert_command('AUTOCONF', autoconf)

src_dir = Path(os.environ.get('SRC_DIR', Path(__file__).parent.absolute()))
mozjs_name = os.environ.get('MOZJS_NAME', 'mozjs')
staging_dir = Path(os.environ.get('STAGING', tmp_dir / 'mozjs-src-pkg'))
dist_dir = Path(os.environ.get('DIST', tmp_dir))
topsrc_dir = src_dir.parent.parent.absolute()

parsed_major_version, parsed_minor_version, parsed_patch_version = parse_version(topsrc_dir)

major_version = os.environ.get('MOZJS_MAJOR_VERSION', parsed_major_version)
minor_version = os.environ.get('MOZJS_MINOR_VERSION', parsed_minor_version)
patch_version = os.environ.get('MOZJS_PATCH_VERSION', parsed_patch_version)
alpha = os.environ.get('MOZJS_ALPHA', '')

version = '{}-{}.{}.{}'.format(mozjs_name,
                               major_version,
                               minor_version,
                               patch_version or alpha or '0')
target_dir = staging_dir / version
package_name = '{}.tar.bz2'.format(version)
package_file = dist_dir / package_name
tar_opts = ['-jcf']

# Given there might be some external program that reads the following output,
# use raw `print`, instead of logging.
print('Environment:')
print('    TAR = {}'.format(tar))
print('    RSYNC = {}'.format(rsync))
print('    AUTOCONF = {}'.format(autoconf))
print('    STAGING = {}'.format(staging_dir))
print('    DIST = {}'.format(dist_dir))
print('    SRC_DIR = {}'.format(src_dir))
print('    MOZJS_NAME = {}'.format(mozjs_name))
print('    MOZJS_MAJOR_VERSION = {}'.format(major_version))
print('    MOZJS_MINOR_VERSION = {}'.format(minor_version))
print('    MOZJS_PATCH_VERSION = {}'.format(patch_version))
print('    MOZJS_ALPHA = {}'.format(alpha))
print('')

rsync_filter_list = """
# Top-level config and build files

+ /configure.py
+ /LICENSE
+ /Makefile.in
+ /moz.build
+ /moz.configure
+ /test.mozbuild

# Additional libraries (optionally) used by SpiderMonkey

+ /mfbt/**
+ /nsprpub/**

- /intl/icu/source/data
- /intl/icu/source/test
- /intl/icu/source/tools
+ /intl/icu/**

+ /memory/build/**
+ /memory/moz.build
+ /memory/mozalloc/**

+ /modules/fdlibm/**
+ /modules/zlib/**

+ /mozglue/baseprofiler/**
+ /mozglue/build/**
+ /mozglue/misc/**
+ /mozglue/moz.build
+ /mozglue/static/**

+ /tools/fuzzing/moz.build
+ /tools/fuzzing/interface/**
+ /tools/fuzzing/registry/**
+ /tools/fuzzing/libfuzzer/**

# Build system and dependencies

+ /Cargo.lock
+ /build/**
+ /config/**
+ /python/**

+ /.cargo/config.in

- /third_party/python/gyp
+ /third_party/python/**
+ /third_party/rust/**

+ /layout/tools/reftest/reftest/**

+ /testing/mozbase/**
+ /testing/web-platform/tests/streams/**

+ /toolkit/crashreporter/tools/symbolstore.py
+ /toolkit/mozapps/installer/package-name.mk

# SpiderMonkey itself

+ /js/src/**
+ /js/app.mozbuild
+ /js/*.configure
+ /js/examples/**
+ /js/public/**
+ /js/rust/**

+ */
- /**
"""

INSTALL_CONTENT = """\
Full build documentation for SpiderMonkey is hosted on MDN:
  https://developer.mozilla.org/en-US/docs/SpiderMonkey/Build_Documentation

Note that the libraries produced by the build system include symbols,
causing the binaries to be extremely large. It is highly suggested that `strip`
be run over the binaries before deploying them.

Building with default options may be performed as follows:
  cd js/src
  mkdir obj
  cd obj
  ../configure
  make # or mozmake on Windows
"""

README_CONTENT = """\
This directory contains SpiderMonkey {major_version}.

This release is based on a revision of Mozilla {major_version}:
  https://hg.mozilla.org/releases/
The changes in the patches/ directory were applied.

MDN hosts the latest SpiderMonkey {major_version} release notes:
  https://developer.mozilla.org/en-US/docs/SpiderMonkey/{major_version}
""".format(major_version=major_version)


def is_mozjs_cargo_member(line):
    """Checks if the line in workspace.members is mozjs-related
    """

    return '"js/' in line


def is_mozjs_crates_io_local_patch(line):
    """Checks if the line in patch.crates-io is mozjs-related
    """

    return 'path = "js' in line


def clean():
    """Remove temporary directory and package file.
    """
    logging.info('Cleaning {} and {} ...'.format(package_file, target_dir))
    package_file.unlink()
    shutil.rmtree(str(target_dir))


def assert_clean():
    """Assert that target directory does not contain generated files.
    """
    makefile_file = target_dir / 'js' / 'src' / 'Makefile'
    if makefile_file.exists():
        logging.error('found js/src/Makefile. Please clean before packaging.')
        sys.exit(1)


def create_target_dir():
    if target_dir.exists():
        logging.warning('dist tree {} already exists!'.format(target_dir))
    else:
        target_dir.mkdir(parents=True)


def sync_files():
    # Output of the command should directly go to stdout/stderr.
    p = subprocess.Popen([str(rsync),
                          '--delete-excluded',
                          '--prune-empty-dirs',
                          '--quiet',
                          '--recursive',
                          '{}/'.format(topsrc_dir),
                          '{}/'.format(target_dir),
                          '--filter=. -'],
                         stdin=subprocess.PIPE)

    p.communicate(rsync_filter_list.encode())

    if p.returncode != 0:
        sys.exit(p.returncode)


def copy_cargo_toml():
    cargo_toml_file = topsrc_dir / 'Cargo.toml'
    target_cargo_toml_file = target_dir / 'Cargo.toml'

    with cargo_toml_file.open('r') as f:
        class State(enum.Enum):
            BEFORE_MEMBER = 1
            INSIDE_MEMBER = 2
            AFTER_MEMBER = 3
            INSIDE_PATCH = 4
            AFTER_PATCH = 5

        content = ''
        state = State.BEFORE_MEMBER
        for line in f:
            if state == State.BEFORE_MEMBER:
                if line.strip() == 'members = [':
                    state = State.INSIDE_MEMBER
            elif state == State.INSIDE_MEMBER:
                if line.strip() == ']':
                    state = State.AFTER_MEMBER
                elif not is_mozjs_cargo_member(line):
                    continue
            elif state == State.AFTER_MEMBER:
                if line.strip() == '[patch.crates-io]':
                    state = State.INSIDE_PATCH
            elif state == State.INSIDE_PATCH:
                if line.startswith('['):
                    state = State.AFTER_PATCH
                if 'path = ' in line:
                    if not is_mozjs_crates_io_local_patch(line):
                        continue

            content += line

    with target_cargo_toml_file.open('w') as f:
        f.write(content)


def generate_configure():
    """Generate configure files to avoid build dependency on autoconf-2.13
    """

    src_configure_in_file = topsrc_dir / 'js' / 'src' / 'configure.in'
    src_old_configure_in_file = topsrc_dir / 'js' / 'src' / 'old-configure.in'
    dest_configure_file = target_dir / 'js' / 'src' / 'configure'
    dest_old_configure_file = target_dir / 'js' / 'src' / 'old-configure'

    shutil.copy2(str(src_configure_in_file), str(dest_configure_file),
                 follow_symlinks=False)
    st = dest_configure_file.stat()
    dest_configure_file.chmod(
        st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    js_src_dir = topsrc_dir / 'js' / 'src'

    with dest_old_configure_file.open('w') as f:
        subprocess.run([str(autoconf),
                        '--localdir={}'.format(js_src_dir),
                        str(src_old_configure_in_file)],
                       stdout=f,
                       check=True)


def copy_install():
    """Copy or create INSTALL.
    """

    staging_install_file = staging_dir / 'INSTALL'
    target_install_file = target_dir / 'INSTALL'

    if staging_install_file.exists():
        shutil.copy2(str(staging_install_file), str(target_install_file))
    else:
        with target_install_file.open('w') as f:
            f.write(INSTALL_CONTENT)


def copy_readme():
    """Copy or create README.
    """

    staging_readme_file = staging_dir / 'README'
    target_readme_file = target_dir / 'README'

    if staging_readme_file.exists():
        shutil.copy2(str(staging_readme_file), str(target_readme_file))
    else:
        with target_readme_file.open('w') as f:
            f.write(README_CONTENT)


def copy_patches():
    """Copy patches dir, if it exists.
    """

    staging_patches_dir = staging_dir / 'patches'
    top_patches_dir = topsrc_dir / 'patches'
    target_patches_dir = target_dir / 'patches'

    if staging_patches_dir.is_dir():
        shutil.copytree(str(staging_patches_dir), str(target_patches_dir))
    elif top_patches_dir.is_dir():
        shutil.copytree(str(top_patches_dir), str(target_patches_dir))


def remove_python_cache():
    """Remove *.pyc and *.pyo files if any.
    """
    for f in target_dir.glob('**/*.pyc'):
        f.unlink()
    for f in target_dir.glob('**/*.pyo'):
        f.unlink()


def stage():
    """Stage source tarball content.
    """
    logging.info('Staging source tarball in {}...'.format(target_dir))

    create_target_dir()
    sync_files()
    copy_cargo_toml()
    generate_configure()
    copy_install()
    copy_readme()
    copy_patches()
    remove_python_cache()


def create_tar():
    """Roll the tarball.
    """

    logging.info('Packaging source tarball at {}...'.format(package_file))

    subprocess.run([str(tar)] + tar_opts + [
        str(package_file),
        '-C',
        str(staging_dir),
        version
    ], check=True)


def build():
    assert_clean()
    stage()
    create_tar()


parser = argparse.ArgumentParser(description="Make SpiderMonkey source package")
subparsers = parser.add_subparsers(dest='COMMAND')
subparser_update = subparsers.add_parser('clean',
                                         help='')
subparser_update = subparsers.add_parser('build',
                                         help='')
args = parser.parse_args()

if args.COMMAND == 'clean':
    clean()
elif not args.COMMAND or args.COMMAND == 'build':
    build()
