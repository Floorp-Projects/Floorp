# Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'variables':
    {
        'angle_code': 1,
        'angle_post_build_script%': 0,
    },
    'includes':
    [
        'compiler.gypi',
        'libGLESv2.gypi',
        'libEGL.gypi'
    ],

    'targets':
    [
        {
            'target_name': 'copy_scripts',
            'type': 'none',
            'copies':
            [
                {
                    'destination': '<(SHARED_INTERMEDIATE_DIR)',
                    'files': [ 'commit_id.bat', 'copy_compiler_dll.bat', 'commit_id.py' ],
                },
            ],
        },

        {
            'target_name': 'commit_id',
            'type': 'none',
            'includes': [ '../build/common_defines.gypi', ],
            'dependencies': [ 'copy_scripts', ],
            'conditions':
            [
                ['OS=="win"',
                {
                    'actions':
                    [
                        {
                            'action_name': 'Generate Commit ID Header',
                            'message': 'Generating commit ID header...',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<(SHARED_INTERMEDIATE_DIR)/commit_id.bat', '<(angle_path)/.git/index' ],
                            'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/commit.h' ],
                            'action': [ '<(SHARED_INTERMEDIATE_DIR)/commit_id.bat', '<(SHARED_INTERMEDIATE_DIR)' ],
                        },
                    ],
                },
                { # OS != win
                    'actions':
                    [
                        {
                            'action_name': 'Generate Commit ID Header',
                            'message': 'Generating commit ID header...',
                            'inputs': [ '<(SHARED_INTERMEDIATE_DIR)/commit_id.py', '<(angle_path)/.git/index' ],
                            'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/commit.h' ],
                            'action': [ 'python', '<(SHARED_INTERMEDIATE_DIR)/commit_id.py', '<(SHARED_INTERMEDIATE_DIR)/commit.h' ],
                        },
                    ],
                }],
            ],
            'direct_dependent_settings':
            {
                'include_dirs':
                [
                    '<(SHARED_INTERMEDIATE_DIR)',
                ],
            },
        },
    ],
    'conditions':
    [
        ['OS=="win"',
        {
            'targets':
            [
                {
                    'target_name': 'copy_compiler_dll',
                    'type': 'none',
                    'dependencies': [ 'copy_scripts', ],
                    'includes': [ '../build/common_defines.gypi', ],
                    'actions':
                    [
                        {
                            'action_name': 'copy_dll',
                            'message': 'Copying D3D Compiler DLL...',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ 'copy_compiler_dll.bat' ],
                            'outputs': [ '<(PRODUCT_DIR)/D3DCompiler_46.dll' ],
                            'action':
                            [
                                "<(SHARED_INTERMEDIATE_DIR)/copy_compiler_dll.bat",
                                "$(PlatformName)",
                                "<(windows_sdk_path)",
                                "<(PRODUCT_DIR)"
                            ],
                        },
                    ], #actions
                },
            ], # targets
        }],
        ['angle_post_build_script!=0 and OS=="win"',
        {
            'targets':
            [
                {
                    'target_name': 'post_build',
                    'type': 'none',
                    'includes': [ '../build/common_defines.gypi', ],
                    'dependencies': [ 'libGLESv2', 'libEGL' ],
                    'actions':
                    [
                        {
                            'action_name': 'ANGLE Post-Build Script',
                            'message': 'Running <(angle_post_build_script)...',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<(angle_post_build_script)', '<!@(["python", "<(angle_post_build_script)", "inputs", "<(angle_path)", "<(CONFIGURATION_NAME)", "$(PlatformName)", "<(PRODUCT_DIR)"])' ],
                            'outputs': [ '<!@(python <(angle_post_build_script) outputs "<(angle_path)" "<(CONFIGURATION_NAME)" "$(PlatformName)" "<(PRODUCT_DIR)")' ],
                            'action': ['python', '<(angle_post_build_script)', 'run', '<(angle_path)', '<(CONFIGURATION_NAME)', '$(PlatformName)', '<(PRODUCT_DIR)'],
                        },
                    ], #actions
                },
            ], # targets
        }],
    ] # conditions
}
