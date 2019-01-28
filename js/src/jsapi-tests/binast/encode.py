#!/usr/bin/python -B

from __future__ import print_function
from optparse import OptionParser
import os
import re
import shutil
import subprocess
import sys

from sets import Set

parser = OptionParser()
parser.add_option('--topsrcdir', dest='topsrcdir',
                  help='path to mozilla-central')
parser.add_option('--binjsdir', dest='binjsdir',
                  help='cwd when running binjs_encode')
parser.add_option('--binjs_encode', dest='binjs_encode',
                  help='path to binjs_encode commad')
(options, filters) = parser.parse_args()


def ensure_dir(path, name):
    """ Ensure the given directory exists.
    If the directory doesn't exist, exits with non-zero status.

    :param path (string)
           The path to the directory
    :param name (string)
           The name of the directory used in the command line arguments.
    """
    if not os.path.isdir(path):
        print('{} directory {} does not exit'.format(name, path),
              file=sys.stderr)
        sys.exit(1)


def ensure_file(path, name):
    """ Ensure the given file exists.
    If the file doesn't exist, exits with non-zero status.

    :param path (string)
           The path to the file
    :param name (string)
           The name of the file used in the command line arguments.
    """
    if not os.path.isfile(path):
        print('{} {} does not exit'.format(name, path),
              file=sys.stderr)
        sys.exit(1)


ensure_dir(options.topsrcdir, 'topsrcdir')
ensure_dir(options.binjsdir, 'binjsdir')
ensure_file(options.binjs_encode, 'binjs_encode command')

jittest_dir = os.path.join(options.topsrcdir, 'js', 'src', 'jit-test', 'tests')
ensure_dir(jittest_dir, 'jit-test')


def check_filter(outfile_path):
    """ Check if the output file is the target.

    :return (bool)
            True if the file is target and should be written.
    """
    if len(filters) == 0:
        return True

    for pattern in filters:
        if pattern in outfile_path:
            return True
    return False


def encode(infile_path, outfile_path, binjs_encode_args=[],
           dir_path=None, ignore_fail=False):
    """ Encodes the given .js file into .binjs.

    :param infile_path (string)
           The path to the input .js file.
    :param outfile_path (string)
           The path to the output .binjs file.
    :param binjs_encode_args (list)
           The command line arguments passed to binjs_encode command.
    :param dir_path (string)
           The path to the jit-test directive .dir file for the .binjs file.
           If specified and the .js file contains |jit-test| line, it's
           copied into this file.
    :param ignore_fail (bool)
            If true, ignore binjs_encode command's status.
            If false, exit if binjs_encode command exits with non-zero
            status.
    """

    if not check_filter(outfile_path):
        return

    print('encoding', infile_path)
    print('      to', outfile_path)

    if dir_path:
        COOKIE = '|jit-test|'
        with open(infile_path) as infile:
            line = infile.readline()
            if COOKIE in line:
                with open(dir_path, 'w') as dirfile:
                    dirfile.write(line)

    infile = open(infile_path)
    outfile = open(outfile_path, 'w')

    binjs_encode = subprocess.Popen([options.binjs_encode] + binjs_encode_args,
                                    cwd=options.binjsdir,
                                    stdin=infile, stdout=outfile)

    if binjs_encode.wait() != 0:
        print('binjs_encode failed',
              file=sys.stderr)
        if not ignore_fail:
            sys.exit(1)


def encode_file(fromdir, todir, filename,
                copy_jit_test_directive=False, **kwargs):
    """ Encodes the js file in fromdir into .binjs file in todir.

    :param fromdir (string)
           The directory that contains .js file.
    :param todir (string)
           The directory that .binjs file is written into.
    :param filename (string)
           The filename of .js file in fromdir.
    :param copy_jit_test_directive (bool)
           If true, copy |jit-test| directive in the file to .dir file.

    Keyword arguments are passed to encode function.
    """
    if not os.path.isdir(todir):
        os.makedirs(todir)

    js_pat = re.compile('\.js$')

    binjs_filename = js_pat.sub('.binjs', filename)
    infile_path = os.path.join(fromdir, filename)
    outfile_path = os.path.join(todir, binjs_filename)

    if 'copy_jit_test_directive' in kwargs:
        if kwargs['copy_jit_test_directive']:
            dir_filename = js_pat.sub('.dir', filename)
            dir_path = os.path.join(todir, dir_filename)
            kwargs['dir_path'] = dir_path
        del kwargs['copy_jit_test_directive']

    encode(infile_path, outfile_path, **kwargs)


def match_ignore(path, ignore_list):
    """ Check if the given path is in the ignore list.

    :param path (string)
           The path to the .js file.
    :param ignore_list (list)
           The list of files to ignore.
           The result of load_ignore_list function.
    :return True if the file should be ignored.
    """
    for item in ignore_list:
        if path.startswith(item['pattern']):
            return True
    return False


def copy_directive_file(dir_path, to_dir_path):
    """ Copy single directives.txt file

    :param dir_path (string)
           The path to the source directives.txt file.
    :param to_dir_path (string)
           The path to the destination directives.txt file.
    """

    if not check_filter(to_dir_path):
        return

    print('copying', dir_path)
    print('     to', to_dir_path)
    shutil.copyfile(dir_path, to_dir_path)


def encode_dir_impl(fromdir, get_todir,
                    ignore_list=None,
                    copy_jit_test_directive=False, **kwargs):
    """ Encode .js files in fromdir into .binjs files, recursively.

    :param fromdir (string)
           The path to the base directory that contains .js files.
    :param get_todir (function)
           The function that returns the directory that the .binjs file is
           written into.  The function receives the path to the directory
           that .js file is stored.
    :param ignore_list (optional, list)
           The list of files to ignore.
           The result of load_ignore_list function.
    :param copy_jit_test_directive (bool)
           If true, copy directives.txt file in each directory to the
           .binjs file's directory.

    Keyword arguments are passed to encode_file function.
    """
    dir_filename = 'directives.txt'
    copied_dir = Set()

    for root, dirs, files in os.walk(fromdir):
        for filename in files:
            if filename.endswith('.js'):
                todir = get_todir(root)

                if ignore_list:
                    if match_ignore(os.path.join(root, filename), ignore_list):
                        continue

                encode_file(root, todir, filename,
                            copy_jit_test_directive=copy_jit_test_directive,
                            **kwargs)

                if copy_jit_test_directive:
                    # Copy directives.txt.
                    if todir not in copied_dir:
                        dir_path = os.path.join(root, dir_filename)
                        if os.path.exists(dir_path):
                            to_dir_path = os.path.join(todir, dir_filename)
                            copy_directive_file(dir_path, to_dir_path)
                            copied_dir.add(todir)


def encode_dir(fromdir, todir, **kwargs):
    """ Encode .js files in fromdir into .binjs files in todir, recursively.

    :param fromdir (string)
           The path to the base directory that contains .js files.
    :param todir (string)
           The path to the base directory that .binjs files are written
           into.

    Keyword arguments are passed to encode_dir_impl function.
    """
    encode_dir_impl(fromdir, lambda root: root.replace(fromdir, todir), **kwargs)


def encode_inplace(dir, **kwargs):
    """ Encode .js files in dir into .binjs files in the same directory,
    recursively.

    :param dir (string)
           The path to the base directory that contains .js files.

    Keyword arguments are passed to encode_dir_impl function.
    """
    encode_dir_impl(dir, lambda root: root, **kwargs)


def load_ignore_list(path):
    """ Load the list of files to ignore.
    This is used for jit-test.

    :param path
           The path to the file that contains the list of files to ignore.

    The file's format is the following:

      # comment
      {relative path to the file to be ignored}
      {relative path to the file to be ignored}
      # comment
      {relative path to the file to be ignored}
      [{options for this file}] {relative path to the file to be ignored}

    Options is comma-separated list of the following:
      only-nonlazy
        By default, jit-test files are encoded with and without laziness.
        If this option is specified, only non-lazy variant is generated.
      only-lazy
        If this option is specified, only lazy variant is generated.
    """
    option_pat = re.compile('\[([^\]]+)\] (.+)')
    ignore_list = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith('#'):
                continue
            m = option_pat.search(line)
            if m:
                options = m.group(1).split(',')
                line = m.group(2)
            else:
                options = []
            ignore_list.append({
                'options': options,
                'pattern': os.path.join(jittest_dir, line),
            })
    return ignore_list


def convert_jittest():
    """ Convert jit-test files into .binjs.
    """
    current_dir = os.path.dirname(os.path.realpath(__file__))
    ignore_list = load_ignore_list(os.path.join(current_dir, 'jit-test.ignore'))

    encode_dir(os.path.join(jittest_dir),
               os.path.join(jittest_dir, 'binast', 'lazy'),
               binjs_encode_args=['--lazify=100'],
               ignore_fail=True,
               copy_jit_test_directive=True,
               ignore_list=filter(lambda item: 'only-nonlazy' not in item['options'],
                                  ignore_list))

    encode_dir(os.path.join(jittest_dir),
               os.path.join(jittest_dir, 'binast', 'nonlazy'),
               ignore_fail=True,
               copy_jit_test_directive=True,
               ignore_list=filter(lambda item: 'only-lazy' not in item['options'],
                                  ignore_list))


def convert_wpt():
    """ Convert web platform files into .binjs.
    """
    wpt_dir = os.path.join(options.topsrcdir, 'testing', 'web-platform')
    ensure_dir(wpt_dir, 'wpt')

    wpt_binast_dir = os.path.join(wpt_dir, 'mozilla', 'tests', 'binast')
    ensure_dir(wpt_binast_dir, 'binast in wpt')

    encode(os.path.join(wpt_binast_dir, 'large-binjs.js'),
           os.path.join(wpt_binast_dir, 'large.binjs'))
    encode(os.path.join(wpt_binast_dir, 'small-binjs.js'),
           os.path.join(wpt_binast_dir, 'small.binjs'))


def convert_jsapi_test():
    """ Convert jsapi-test files into .binjs.
    """
    binast_test_dir = os.path.join(options.topsrcdir, 'js', 'src', 'jsapi-tests', 'binast')
    ensure_dir(binast_test_dir, 'binast in jsapi-tests')

    encode_inplace(os.path.join(binast_test_dir, 'parser', 'multipart'))


convert_jittest()

convert_wpt()

convert_jsapi_test()
