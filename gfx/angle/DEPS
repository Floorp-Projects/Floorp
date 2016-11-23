vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {
  'third_party/gyp':
      Var('chromium_git') + '/external/gyp' + '@' + '81c2e5ff92af29bab61c982808076ddce3d200a2',

  'testing/gtest':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '6f8a66431cb592dad629028a50b3dd418a408c87',

  'testing/gmock':
    Var('chromium_git') + '/external/googlemock.git' + '@' + '0421b6f358139f02e102c9c332ce19a33faf75be', # from svn revision 566

  # Cherry is a dEQP management GUI written in Go. We use it for viewing test results.
  'third_party/cherry':
      'https://android.googlesource.com/platform/external/cherry' + '@' + 'd2e26b4d864ec2a6757e7f1174e464949ca5bf73',

  'third_party/deqp/src':
      'https://android.googlesource.com/platform/external/deqp' + '@' + 'f4f3d8079e7a37d7675ab93583e6438d0bca0e58',

  'third_party/libpng':
      'https://android.googlesource.com/platform/external/libpng' + '@' + '094e181e79a3d6c23fd005679025058b7df1ad6c',

  'third_party/zlib':
      Var('chromium_git') + '/chromium/src/third_party/zlib' + '@' + 'afd8c4593c010c045902f6c0501718f1823064a3',

  'buildtools':
      Var('chromium_git') + '/chromium/buildtools.git' + '@' + '39b1db2ab4aa4b2ccaa263c29bdf63e7c1ee28aa',
}

hooks = [
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull GN binaries using checked-in hashes.
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/linux64/gn.sha1',
    ],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'pattern': '.',
    'action': ['python', 'gyp/gyp_angle'],
  },
]

recursedeps = [
  # buildtools provides clang_format.
  'buildtools',
]
