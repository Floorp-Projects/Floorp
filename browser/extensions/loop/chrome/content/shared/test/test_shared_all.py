# need to get this dir in the path so that we make the import work
import os
import sys
sys.path.append(os.path.dirname(__file__))

from frontend_tester import BaseTestFrontendUnits


class TestSharedUnits(BaseTestFrontendUnits):

    def setUp(self):
        super(TestSharedUnits, self).setUp()
        self.set_server_prefix(".")

    def test_units(self):
        self.check_page("index.html")
