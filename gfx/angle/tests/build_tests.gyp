# Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'include_dirs': [
        '../third_party/googletest',
        '../third_party/googletest/include',
      ],
      'sources': [
        '../third_party/googletest/src/gtest-all.cc',
      ],
    },
    {
      'target_name': 'gmock',
      'type': 'static_library',
      'include_dirs': [
        '../third_party/googlemock',
        '../third_party/googlemock/include',
        '../third_party/googletest/include',
      ],
      'sources': [
        '../third_party/googlemock/src/gmock-all.cc',
      ],
    },
    {
      'target_name': 'preprocessor_tests',
      'type': 'executable',
      'dependencies': [
        '../src/build_angle.gyp:preprocessor',
        'gtest',
        'gmock',
      ],
      'include_dirs': [
        '../src/compiler/preprocessor/new',
        '../third_party/googletest/include',
        '../third_party/googlemock/include',
      ],
      'sources': [
        '../third_party/googlemock/src/gmock_main.cc',
        'preprocessor_tests/char_test.cpp',
        'preprocessor_tests/comment_test.cpp',
        'preprocessor_tests/define_test.cpp',
        'preprocessor_tests/error_test.cpp',
        'preprocessor_tests/extension_test.cpp',
        'preprocessor_tests/identifier_test.cpp',
        'preprocessor_tests/if_test.cpp',
        'preprocessor_tests/input_test.cpp',
        'preprocessor_tests/location_test.cpp',
        'preprocessor_tests/MockDiagnostics.h',
        'preprocessor_tests/MockDirectiveHandler.h',
        'preprocessor_tests/number_test.cpp',
        'preprocessor_tests/operator_test.cpp',
        'preprocessor_tests/pragma_test.cpp',
        'preprocessor_tests/PreprocessorTest.cpp',
        'preprocessor_tests/PreprocessorTest.h',
        'preprocessor_tests/space_test.cpp',
        'preprocessor_tests/token_test.cpp',
        'preprocessor_tests/version_test.cpp',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
