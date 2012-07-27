from marionette_test import MarionetteTestCase
import os

class TestVoicemailStatusChanged(MarionetteTestCase):

    def testStatusChanged(self):
        this_dir = os.path.abspath(os.path.dirname(__file__))
        pdu_builder_path = os.path.join(this_dir, "pdu_builder.js")
        self.marionette.import_script(pdu_builder_path)

        test_path = os.path.join(this_dir, "test_voicemail_statuschanged.js")
        test = open(test_path, "r").read()
        self.marionette.set_script_timeout(30000)
        self.marionette.execute_async_script(test)
