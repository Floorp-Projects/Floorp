# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os

def welcome():
    """
    Perform a bunch of sanity tests to make sure the Add-on SDK
    environment is sane, and then display a welcome message.
    """

    try:
        if sys.version_info[0] > 2:
            print ("Error: You appear to be using Python %d, but "
                   "the Add-on SDK only supports the Python 2.x line." %
                   (sys.version_info[0]))
            return

        import mozrunner

        if 'CUDDLEFISH_ROOT' not in os.environ:
            print ("Error: CUDDLEFISH_ROOT environment variable does "
                   "not exist! It should point to the root of the "
                   "Add-on SDK repository.")
            return

        env_root = os.environ['CUDDLEFISH_ROOT']

        bin_dir = os.path.join(env_root, 'bin')
        python_lib_dir = os.path.join(env_root, 'python-lib')
        path = os.environ['PATH'].split(os.path.pathsep)

        if bin_dir not in path:
            print ("Warning: the Add-on SDK binary directory %s "
                   "does not appear to be in your PATH. You may "
                   "not be able to run 'cfx' or other SDK tools." %
                   bin_dir)

        if python_lib_dir not in sys.path:
            print ("Warning: the Add-on SDK python-lib directory %s "
                   "does not appear to be in your sys.path, which "
                   "is odd because I'm running from it." % python_lib_dir)

        if not mozrunner.__path__[0].startswith(env_root):
            print ("Warning: your mozrunner package is installed at %s, "
                   "which does not seem to be located inside the Jetpack "
                   "SDK. This may cause problems, and you may want to "
                   "uninstall the other version. See bug 556562 for "
                   "more information." % mozrunner.__path__[0])
    except Exception:
        # Apparently we can't get the actual exception object in the
        # 'except' clause in a way that's syntax-compatible for both
        # Python 2.x and 3.x, so we'll have to use the traceback module.

        import traceback
        _, e, _ = sys.exc_info()
        print ("Verification of Add-on SDK environment failed (%s)." % e)
        print ("Your SDK may not work properly.")
        return

    print ("Welcome to the Add-on SDK. For the docs, visit https://developer.mozilla.org/en-US/Add-ons/SDK")

if __name__ == '__main__':
    welcome()
