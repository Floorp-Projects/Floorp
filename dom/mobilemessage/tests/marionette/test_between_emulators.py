from marionette_test import *


class SMSTest(MarionetteTestCase):

    @unittest.expectedFailure
    def test_sms_between_emulators(self):
        # Tests always have one emulator available as self.marionette; we'll
        # use this for the receiving emulator.  We'll also launch a second
        # emulator to use as the sender.
        sender = self.get_new_emulator()
        receiver = self.marionette

        self.set_up_test_page(sender, "test.html", ["sms"])
        self.set_up_test_page(receiver, "test.html", ["sms"])

        # Setup the event listsener on the receiver, which should store
        # a global variable when an SMS is received.
        message = 'hello world!'
        self.assertTrue(receiver.execute_script("return window.navigator.mozSms != null;"))
        receiver.execute_script("""
global.smsreceived = null;
window.navigator.mozSms.addEventListener("received", function(e) {
    global.smsreceived = e.message.body;
});
""", new_sandbox=False)

        # Send the SMS from the sender.
        sender.execute_script("""
window.navigator.mozSms.send("%d", "%s");
""" % (receiver.emulator.port, message))

        # On the receiver, wait up to 10s for an SMS to be received, by 
        # checking the value of the global var that the listener will change.
        receiver.set_script_timeout(0) # TODO no timeout for now since the test fails
        received = receiver.execute_async_script("""
        waitFor(function () {
            marionetteScriptFinished(global.smsreceived);
        }, function () {
            return global.smsreceived
        });
        """, new_sandbox=False)
        self.assertEqual(received, message)
