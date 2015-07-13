# vim: set ts=8 sts=4 et sw=4 tw=99:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#----------------------------------------------------------------------------
# This script checks that SpiderMonkey MacroAssembler methods are properly
# annotated.
#
# The MacroAssembler has one interface for all platforms, but it might have one
# definition per platform. The code of the MacroAssembler use a macro to
# annotate the method declarations, in order to delete the function if it is not
# present on the current platform, and also to locate the files in which the
# methods are defined.
#
# This script scans the MacroAssembler.h header, for method declarations.
# It also scans MacroAssembler-/arch/.cpp, MacroAssembler-/arch/-inl.h, and
# MacroAssembler-inl.h for method definitions. The result of both scans are
# uniformized, and compared, to determine if the MacroAssembler.h header as
# proper methods annotations.
#----------------------------------------------------------------------------

from __future__ import print_function

import difflib
import os
import re
import subprocess
import sys
from check_utils import get_all_toplevel_filenames

architecture_independent = set([ 'generic' ])
all_architecture_names = set([ 'x86', 'x64', 'arm', 'arm64', 'mips' ])
all_shared_architecture_names = set([ 'x86_shared', 'arm', 'arm64', 'mips' ])

def get_normalized_signatures(signature, fileAnnot = None):
    # Remove semicolon.
    signature = signature.replace(';', ' ')
    # Normalize spaces.
    signature = re.sub(r'\s+', ' ', signature).strip()
    # Remove argument names.
    signature = re.sub(r'(?P<type>(?:[(]|,\s)[\w\s:*&]+)(?P<name>\s\w+)(?=[,)])', '\g<type>', signature)
    # Remove class name
    signature = signature.replace('MacroAssembler::', '')

    # Extract list of architectures
    archs = ['generic']
    if fileAnnot:
        archs = [fileAnnot['arch']]

    if 'DEFINED_ON(' in signature:
        archs = re.sub(r'.*DEFINED_ON\((?P<archs>[^()]*)\).*', '\g<archs>', signature).split(',')
        archs = [a.strip() for a in archs]
        signature = re.sub(r'\s+DEFINED_ON\([^()]*\)', '', signature)

    elif 'PER_ARCH' in signature:
        archs = all_architecture_names
        signature = re.sub(r'\s+PER_ARCH', '', signature)

    elif 'PER_SHARED_ARCH' in signature:
        archs = all_shared_architecture_names
        signature = re.sub(r'\s+PER_SHARED_ARCH', '', signature)

    else:
        # No signature annotation, the list of architectures remains unchanged.
        pass

    # Extract inline annotation
    inline = False
    if fileAnnot:
        inline = fileAnnot['inline']

    if 'inline ' in signature:
        signature = re.sub(r'inline\s+', '', signature)
        inline = True

    return [
        { 'arch': a, 'sig': 'inline ' + signature }
        for a in archs
    ]

file_suffixes = set([
    a.replace('_', '-') for a in
    all_architecture_names.union(all_shared_architecture_names)
])
def get_file_annotation(filename):
    origFilename = filename
    filename = filename.split('/')[-1]

    inline = False
    if filename.endswith('.cpp'):
        filename = filename[:-len('.cpp')]
    elif filename.endswith('-inl.h'):
        inline = True
        filename = filename[:-len('-inl.h')]
    else:
        raise Exception('unknown file name', origFilename)

    arch = 'generic'
    for suffix in file_suffixes:
        if filename == 'MacroAssembler-' + suffix:
            arch = suffix
            break

    return {
        'inline': inline,
        'arch': arch.replace('-', '_')
    }

def get_macroassembler_definitions(filename):
    try:
        fileAnnot = get_file_annotation(filename)
    except:
        return []

    style_section = False
    code_section = False
    lines = ''
    signatures = []
    with open(os.path.join('../..', filename)) as f:
        for line in f:
            if '//{{{ check_macroassembler_style' in line:
                style_section = True
            elif '//}}} check_macroassembler_style' in line:
                style_section = False
            if not style_section:
                continue

            line = re.sub(r'//.*', '', line)
            if line.startswith('{'):
                if 'MacroAssembler::' in lines:
                    signatures.extend(get_normalized_signatures(lines, fileAnnot))
                code_section = True
                continue
            if line.startswith('}'):
                code_section = False
                lines = ''
                continue
            if code_section:
                continue

            if len(line.strip()) == 0:
                lines = ''
                continue
            lines = lines + line
            # Continue until we have a complete declaration
            if '{' not in lines:
                continue
            # Skip variable declarations
            if ')' not in lines:
                lines = ''
                continue

    return signatures

def get_macroassembler_declaration(filename):
    style_section = False
    lines = ''
    signatures = []
    with open(os.path.join('../..', filename)) as f:
        for line in f:
            if '//{{{ check_macroassembler_style' in line:
                style_section = True
            elif '//}}} check_macroassembler_style' in line:
                style_section = False
            if not style_section:
                continue

            line = re.sub(r'//.*', '', line)
            if len(line.strip()) == 0:
                lines = ''
                continue
            lines = lines + line
            # Continue until we have a complete declaration
            if ';' not in lines:
                continue
            # Skip variable declarations
            if ')' not in lines:
                lines = ''
                continue

            signatures.extend(get_normalized_signatures(lines))
            lines = ''

    return signatures

def append_signatures(d, sigs):
    for s in sigs:
        if s['sig'] not in d:
            d[s['sig']] = []
        d[s['sig']].append(s['arch']);
    return d

def generate_file_content(signatures):
    output = []
    for s in sorted(signatures.keys()):
        archs = set(sorted(signatures[s]))
        if len(archs.symmetric_difference(architecture_independent)) == 0:
            output.append(s + ';\n')
            if s.startswith('inline'):
                output.append('    is defined in MacroAssembler-inl.h\n')
            else:
                output.append('    is defined in MacroAssembler.cpp\n')
        else:
            if len(archs.symmetric_difference(all_architecture_names)) == 0:
                output.append(s + ' PER_ARCH;\n')
            elif len(archs.symmetric_difference(all_shared_architecture_names)) == 0:
                output.append(s + ' PER_SHARED_ARCH;\n')
            else:
                output.append(s + ' DEFINED_ON(' + ', '.join(archs) + ');\n')
            for a in archs:
                a = a.replace('_', '-')
                masm = '%s/MacroAssembler-%s' % (a, a)
                if s.startswith('inline'):
                    output.append('    is defined in %s-inl.h\n' % masm)
                else:
                    output.append('    is defined in %s.cpp\n' % masm)
    return output

def check_style():
    # We read from the header file the signature of each function.
    decls = dict()      # type: dict(signature => ['x86', 'x64'])

    # We infer from each file the signature of each MacroAssembler function.
    defs = dict()       # type: dict(signature => ['x86', 'x64'])

    # Select the appropriate files.
    for filename in get_all_toplevel_filenames():
        if not filename.startswith('js/src/jit/'):
            continue
        if 'MacroAssembler' not in filename:
            continue

        if filename.endswith('MacroAssembler.h'):
            decls = append_signatures(decls, get_macroassembler_declaration(filename))
        else:
            defs = append_signatures(defs, get_macroassembler_definitions(filename))

    # Compare declarations and definitions output.
    difflines = difflib.unified_diff(generate_file_content(decls),
                                     generate_file_content(defs),
                                     fromfile='check_macroassembler_style.py declared syntax',
                                     tofile='check_macroassembler_style.py found definitions')
    ok = True
    for diffline in difflines:
        ok = False
        print(diffline, end='')

    return ok


def main():
    ok = check_style()

    if ok:
        print('TEST-PASS | check_macroassembler_style.py | ok')
    else:
        print('TEST-UNEXPECTED-FAIL | check_macroassembler_style.py | actual output does not match expected output;  diff is above')

    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
