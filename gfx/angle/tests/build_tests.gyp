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
      'target_name': 'preprocessor_tests',
      'type': 'executable',
      'dependencies': [
        '../src/build_angle.gyp:preprocessor',
        'gtest',
      ],
      'include_dirs': [
        '../src/compiler/preprocessor/new',
        '../third_party/googletest/include',
      ],
      'sources': [
        '../third_party/googletest/src/gtest_main.cc',
        'preprocessor_tests/identifier_test.cpp',
        'preprocessor_tests/number_test.cpp',
        'preprocessor_tests/token_test.cpp',
        'preprocessor_tests/space_test.cpp',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
