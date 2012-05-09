# Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'all',
      'type': 'none',
      'dependencies': [
        # TODO(alokp): build_ prefix should be removed from the gyp files
        # as soon as we can get rid of manually-maintained sln files.
        # Otherwise auto-generated sln files will overwrite/conflict the
        # manually maintained ones.
        '../samples/build_samples.gyp:*',
        '../src/build_angle.gyp:*',
        '../tests/build_tests.gyp:*',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
