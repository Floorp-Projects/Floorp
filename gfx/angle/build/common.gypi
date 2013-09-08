# Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'component%': 'static_library',
    # angle_code is set to 1 for the core ANGLE targets defined in src/build_angle.gyp.
    # angle_code is set to 0 for test code, sample code, and third party code.
    # When angle_code is 1, we build with additional warning flags on Mac and Linux.
    'angle_code%': 0,
    'gcc_or_clang_warnings': [
      '-Wall',
      '-Wchar-subscripts',
      '-Werror',
      '-Wextra',
      '-Wformat=2',
      '-Winit-self',
      '-Wno-sign-compare',
      '-Wno-unused-function',
      '-Wno-unused-parameter',
      '-Wno-unknown-pragmas',
      '-Wpacked',
      '-Wpointer-arith',
      '-Wundef',
      '-Wwrite-strings',
    ],
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'variables': {
      'warn_as_error%': 1,
    },
    'target_conditions': [
      ['warn_as_error == 1', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarnAsError': 'true',
          },
        },
      }],
    ],
    'configurations': {
      'Common': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',  # UNICODE
        },
        'msvs_configuration_platform': 'Win32',
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',
            'DebugInformationFormat': '3',
            # TODO(alokp): Disable exceptions before integrating with chromium.
            #'ExceptionHandling': '0',
            'EnableFunctionLevelLinking': 'true',
            'MinimalRebuild': 'false',
            'PreprocessorDefinitions': [
              '_CRT_SECURE_NO_DEPRECATE',
              '_HAS_EXCEPTIONS=0',
              '_WIN32_WINNT=0x0600',
              '_WINDOWS',
              'NOMINMAX',
              'WIN32',
              'WIN32_LEAN_AND_MEAN',
              'WINVER=0x0600',
            ],
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '4',
            'DisableSpecificWarnings': [4100, 4127, 4189, 4239, 4244, 4245, 4512, 4702],
          },
          'VCLinkerTool': {
            'FixedBaseAddress': '1',
            'GenerateDebugInformation': 'true',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            # Most of the executables we'll ever create are tests
            # and utilities with console output.
            'SubSystem': '1',  # /SUBSYSTEM:CONSOLE
            'AdditionalLibraryDirectories': [
              '$(ProgramFiles)/Windows Kits/8.0/Lib/win8/um/x86',
            ],
          },
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories': [
              '$(ProgramFiles)/Windows Kits/8.0/Lib/win8/um/x86',
            ],
          },
          'VCResourceCompilerTool': {
            'Culture': '1033',
          },
        },
        'msvs_system_include_dirs': [
          '$(ProgramFiles)/Windows Kits/8.0/Include/shared',
          '$(ProgramFiles)/Windows Kits/8.0/Include/um',
        ],
      },  # Common
      'Debug': {
        'inherit_from': ['Common'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',  # /Od
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '3',
            'RuntimeLibrary': '1',  # /MTd (debug static)
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
          },
        },
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '0',
        },
      },  # Debug
      'Release': {
        'inherit_from': ['Common'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',  # /Os
            'PreprocessorDefinitions': ['NDEBUG'],
            'RuntimeLibrary': '0',  # /MT (static)
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
          },
        },
      },  # Release
    },  # configurations
    'conditions': [
      ['component=="shared_library"', {
        'defines': ['COMPONENT_BUILD'],
      }],
    ],
  },  # target_defaults
  'conditions': [
    ['OS=="win"', {
      'target_defaults': {
        'msvs_cygwin_dirs': ['../third_party/cygwin'],
      },
    }],
    ['OS!="win" and OS!="mac"', {
      'target_defaults': {
        'cflags': [
          '-pthread',
          '-fno-exceptions',
        ],
        'ldflags': [
          '-pthread',
        ],
        'configurations': {
          'Debug': {
            'variables': {
              'debug_optimize%': '0',
            },
            'defines': [
              '_DEBUG',
            ],
            'cflags': [
              '-O>(debug_optimize)',
              '-g',
            ],
          }
        },
      },
    }],
    ['angle_code==1', {
      'target_defaults': {
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'WARNING_CFLAGS': ['<@(gcc_or_clang_warnings)']
            },
          }],
          ['OS!="win" and OS!="mac"', {
            'cflags': ['<@(gcc_or_clang_warnings)']
          }],
        ]
      }
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
