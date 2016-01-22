# need to get this dir in the path so that we make the import work
import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'shared', 'test'))

from frontend_tester import BaseTestFrontendUnits


class TestDesktopUnits(BaseTestFrontendUnits):

    def setUp(self):
        super(TestDesktopUnits, self).setUp()
        # Set the server prefix to the top of the src directory for the mozilla-central
        # repository.
        self.set_server_prefix("../../../../")

    def test_units(self):
        self.check_page("chrome/content/panels/test/index.html")
