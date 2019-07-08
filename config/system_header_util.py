from __future__ import absolute_import
import os


def find_in_path(file, searchpath):
    for dir in searchpath.split(os.pathsep):
        f = os.path.join(dir, file)
        if os.path.exists(f):
            return f
    return ''


def header_path(header, compiler):
    if compiler == 'gcc':
        # we use include_next on gcc
        return header
    elif compiler == 'msvc':
        return find_in_path(header, os.environ.get('INCLUDE', ''))
    else:
        # hope someone notices this ...
        raise NotImplementedError(compiler)
