from __future__ import print_function

from marionette_harness import MarionetteTestCase


GLOBAL_SHARING_PREF = 'jsloader.shareGlobal'
GLOBAL_SHARING_VAR = 'MOZ_LOADER_SHARE_GLOBAL'


class TestLoaderGlobalSharing(MarionetteTestCase):
    sandbox_name = 'loader-global-sharing'

    def execute_script(self, code, *args, **kwargs):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(code,
                                                  new_sandbox=False,
                                                  sandbox=self.sandbox_name,
                                                  *args, **kwargs)

    def get_global_sharing_enabled(self):
        return self.execute_script(r'''
          Cu.import("resource://gre/modules/XPCOMUtils.jsm");
          return (Cu.getGlobalForObject(Services) ===
                  Cu.getGlobalForObject(XPCOMUtils))
        ''')

    def set_env(self, env, value):
        self.execute_script('env.set(arguments[0], arguments[1]);',
                            script_args=(env, value))

    def get_env(self, env):
        return self.execute_script('return env.get(arguments[0]);',
                                   script_args=(env,))

    def restart(self, prefs=None, env=None):
        if prefs:
            self.marionette.set_prefs(prefs)

        if env:
            for name, value in env.items():
                self.set_env(name, value)

        self.marionette.restart(in_app=True, clean=False)
        self.setUpSession()

        # Sanity check our environment.
        if prefs:
            for key, val in prefs.items():
                self.assertEqual(self.marionette.get_pref(key), val)
        if env:
            for key, val in env.items():
                self.assertEqual(self.get_env(key), val or '')

    def setUpSession(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        self.execute_script(r'''
          const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} =
                    Components;

          // We're running in a function, in a sandbox, that inherits from an
          // X-ray wrapped window. Anything we want to be globally available
          // needs to be defined on that window.
          Object.assign(window, {Cc, Ci, Cu, Cr});

          Cu.import("resource://gre/modules/Services.jsm");
          window.env = Cc["@mozilla.org/process/environment;1"]
                    .getService(Ci.nsIEnvironment);
        ''')

    def setUp(self):
        super(TestLoaderGlobalSharing, self).setUp()

        self.setUpSession()

    def tearDown(self):
        self.marionette.restart(clean=True)

        super(TestLoaderGlobalSharing, self).tearDown()

    def test_global_sharing_settings(self):
        # The different cases to test, with the first element being the value
        # of the MOZ_LOADER_SHARE_GLOBAL environment variable, and the second
        # being the value of the "jsloader.shareGlobal" preference.
        #
        # The browser is restarted after each change, but the profile is not
        # reset.
        CASES = (
            (None, False),
            (None, True),
            ('0', True),
            ('1', True),
            ('0', False),
            ('1', False),
        )

        for var, pref in CASES:
            print('Testing %s=%r %s=%r' % (GLOBAL_SHARING_VAR, var,
                                           GLOBAL_SHARING_PREF, pref))

            # The value of the environment variable takes precedence if it's
            # defined.
            expect_sharing = pref if var is None else var != '0'

            self.restart(prefs={GLOBAL_SHARING_PREF: pref},
                         env={GLOBAL_SHARING_VAR: var})

            have_sharing = self.get_global_sharing_enabled()

            self.assertEqual(
                have_sharing, expect_sharing,
                'Global sharing state should match settings: %r != %r'
                % (have_sharing, expect_sharing))
