from marionette_test import MarionetteTestCase
import os

class TestVoicemailStatusChanged(MarionetteTestCase):

    def testStatusChanged(self):
        this_dir = os.path.abspath(os.path.dirname(__file__))
        system_gonk_dir = os.path.abspath(os.path.join(this_dir,
            os.path.pardir, os.path.pardir, os.path.pardir, "system", "gonk"))

        ril_consts_path = os.path.join(system_gonk_dir, "ril_consts.js")
        self.marionette.import_script(ril_consts_path)

        pdu_builder_path = os.path.join(this_dir, "pdu_builder.js")
        self.marionette.import_script(pdu_builder_path)

        test_path = os.path.join(this_dir, "test_voicemail_statuschanged.js")
        test = open(test_path, "r").read()
        self.marionette.set_script_timeout(30000)
        self.marionette.execute_async_script(test)
