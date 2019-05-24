# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import textwrap

from marionette_harness.marionette_test import MarionetteTestCase


class TestEnginesOnRestart(MarionetteTestCase):

    def setUp(self):
        super(TestEnginesOnRestart, self).setUp()
        self.marionette.enforce_gecko_prefs({
            'browser.search.log': True,
            'browser.search.geoSpecificDefaults': False
        })

    def get_default_search_engine(self):
        """Retrieve the identifier of the default search engine."""

        script = """\
        let [resolve] = arguments;
        let searchService = Components.classes[
                "@mozilla.org/browser/search-service;1"]
            .getService(Components.interfaces.nsISearchService);
        return searchService.init().then(function () {
          resolve(searchService.defaultEngine.identifier);
        });
        """

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_async_script(textwrap.dedent(script))

    def test_engines(self):
        self.assertTrue(self.get_default_search_engine().startswith("google"))
        self.marionette.set_pref("intl.locale.requested", "kk_KZ")
        self.marionette.restart(clean=False, in_app=True)
        self.assertTrue(self.get_default_search_engine().startswith("google"))
