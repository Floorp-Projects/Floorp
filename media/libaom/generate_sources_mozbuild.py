# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import cmakeparser as cp

import copy
import datetime
import os
import re
import subprocess

AOM_DIR = '../../third_party/aom'

def write_aom_config(system, arch, variables, cache_variables):
    # read template cmake file
    variables['year'] = datetime.datetime.now().year
    cp.parse(variables, [], os.path.join(AOM_DIR, 'build', 'cmake',
          'generate_aom_config_templates.cmake'))

    # filter variables
    cache_variables = [x for x in sorted(cache_variables)
                       if x and not x.startswith((' ', 'CMAKE', 'AOM'))]

    # inherit this from the mozilla build config
    cache_variables.remove('HAVE_PTHREAD_H')

    outdir = os.path.join('config', system, arch, 'config')
    try:
        os.makedirs(outdir)
    except OSError:
        pass

    with open(os.path.join(outdir, 'aom_config.h'), 'w') as f:
        header = variables['h_file_header_block']
        f.write(header)
        f.write('\n')
        for var in cache_variables:
            f.write('#define %s %s\n' % (var, variables[var]))
        f.write('#endif /* AOM_CONFIG_H_ */\n')

    with open(os.path.join(outdir, 'aom_config.asm'), 'w') as f:
        header = variables['asm_file_header_block']
        f.write(header)
        f.write('\n')
        for var in cache_variables:
            if var in ['INCLUDE_INSTALL_DIR', 'INLINE',
                       'LIB_INSTALL_DIR', 'RESTRICT']:
                continue
            if arch == 'arm':
                f.write('.equ %s, %s\n' % (var, variables[var]))
            else:
                f.write('%s equ %s\n' % (var, variables[var]))

        if arch == 'arm':
            f.write('.section	.note.GNU-stack,"",%progbits')


if __name__ == '__main__':
    import sys

    shared_variables = {
        'CMAKE_CURRENT_SOURCE_DIR': AOM_DIR,
        'CONFIG_AV1_DECODER': 1,
        'CONFIG_AV1_ENCODER': 0,
        'CONFIG_BGSPRITE': 0,
        'CONFIG_CDEF_SINGLEPASS': 0,
        'CONFIG_CFL': 0,
        'CONFIG_HASH_ME': 0,
        'CONFIG_HIGH_BITDEPTH': 0,
        'CONFIG_INSPECTION': 0,
        'CONFIG_INTERNAL_STATS': 0,
        'CONFIG_LIBYUV': 0,
        'CONFIG_LOWBITDEPTH': 0,
        'CONFIG_LV_MAP': 0,
        'CONFIG_MOTION_VAR': 0,
        'CONFIG_MULTITHREAD': 1,
        'CONFIG_NCOBMC_ADAPT_WEIGHT': 0,
        'CONFIG_PIC': 0,
        'CONFIG_PVQ': 0,
        'CONFIG_UNIT_TESTS': 0,
        'CONFIG_WEBM_IO': 0,
        'CONFIG_XIPHRC': 0,
        'CMAKE_CURRENT_BINARY_DIR': 'OBJDIR',
        'CMAKE_INSTALL_PREFIX': 'INSTALLDIR',
        'CMAKE_SYSTEM_NAME': 'Linux',
        'CMAKE_SYSTEM_PROCESSOR': 'x86_64',
        'ENABLE_EXAMPLES': 0,
        'ENABLE_TESTS': 0,
        'ENABLE_TOOLS': 0,
        'AOM_TEST_TEST_CMAKE_': 1, #prevent building tests
    }

    f = open('sources.mozbuild', 'wb')
    f.write('# This file is generated. Do not edit.\n\n')
    f.write('files = {\n')

    platforms = [
        ('armv7', 'linux', 'arm', True),
        ('generic', '', 'generic', True),
        ('x86', 'linux', 'ia32', True),
        ('x86', 'win', 'mingw32', False),
        ('x86', 'win', 'ia32', False),
        ('x86_64', 'linux', 'x64', True),
        ('x86_64', 'mac', 'x64', False),
        ('x86_64', 'win', 'x64', False),
        ('x86_64', 'win', 'mingw64', False),
    ]
    for cpu, system, arch, generate_sources in platforms:
        print('Running CMake for %s (%s)' % (cpu, system))
        variables = shared_variables.copy()
        variables['AOM_TARGET_CPU'] = cpu

        # We skip compiling test programs that detect these
        variables['HAVE_FEXCEPT'] = 1
        variables['INLINE'] = 'inline'
        if cpu == 'x86' and system == 'linux':
            variables['CONFIG_PIC'] = 1
        if system == 'win' and not arch.startswith('mingw'):
            variables['MSVC'] = 1

        cache_variables = []
        sources = cp.parse(variables, cache_variables,
                           os.path.join(AOM_DIR, 'CMakeLists.txt'))

        # Disable HAVE_UNISTD_H.
        cache_variables.remove('HAVE_UNISTD_H')

        write_aom_config(system, arch, variables, cache_variables)

        # Currently, the sources are the same for each supported cpu
        # regardless of operating system / compiler. If that changes, we'll
        # have to generate sources for each combination.
        if generate_sources:
            # Remove spurious sources and perl files
            sources = filter(lambda x: x.startswith(AOM_DIR), sources)
            sources = filter(lambda x: not x.endswith('.pl'), sources)

            # Filter out exports
            exports = filter(lambda x: re.match(os.path.join(AOM_DIR, '(aom|aom_mem|aom_ports|aom_scale)/.*h$'), x), sources)
            exports = filter(lambda x: not re.search('(internal|src)', x), exports)
            exports = filter(lambda x: not re.search('(emmintrin_compat.h|mem_.*|msvc.h|aom_once.h)$', x), exports)

            for export in exports:
                sources.remove(export)

            # Remove header files
            sources = sorted(filter(lambda x: not x.endswith('.h'), sources))

            # The build system is unhappy if two files have the same prefix
            # In libaom, sometimes .asm and .c files share the same prefix
            for i in xrange(len(sources) - 1):
                if sources[i].endswith('.asm'):
                    if os.path.splitext(sources[i])[0] == os.path.splitext(sources[i + 1])[0]:
                        old = sources[i]
                        sources[i] = sources[i].replace('.asm', '_asm.asm')
                        if not os.path.exists(sources[i]):
                            os.rename(old, sources[i])

            f.write('  \'%s_EXPORTS\': [\n' % arch.upper())
            for export in sorted(exports):
                f.write('    \'%s\',\n' % export)
            f.write("  ],\n")

            f.write('  \'%s_SOURCES\': [\n' % arch.upper())
            for source in sorted(sources):
                f.write('    \'%s\',\n' % source)
            f.write('  ],\n')

        print('\n')

    f.write('}\n')
    f.close()
