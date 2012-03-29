from marionette_test import *


class MultiEmulatorDialTest(MarionetteTestCase):
    """A simple test which verifies the ability of one emulator to dial
       another and to detect an incoming call.
    """

    def test_dial_between_emulators(self):
        # Tests always have one emulator available as self.marionette; we'll
        # use this for the receiving emulator.  We'll also launch a second
        # emulator to use as the sender.
        sender = self.get_new_emulator()
        receiver = self.marionette

        # Setup the event listsener on the receiver, which should store
        # a global variable when an incoming call is received.
        receiver.set_context("chrome")
        self.assertTrue(receiver.execute_script("""
return navigator.mozTelephony != undefined && navigator.mozTelephony != null;
"""))
        receiver.execute_script("""
window.wrappedJSObject.incoming = "none";
navigator.mozTelephony.addEventListener("incoming", function(e) {
    window.wrappedJSObject.incoming = e.call.number;
});
""")

        # Dial the receiver from the sender.
        toPhoneNumber = "1555521%d" % receiver.emulator.port
        fromPhoneNumber = "1555521%d" % sender.emulator.port
        sender.set_context("chrome")
        sender.execute_script("""
navigator.mozTelephony.dial("%s");
""" % toPhoneNumber)

        # On the receiver, wait up to 30s for an incoming call to be 
        # detected, by checking the value of the global var that the 
        # listener will change.
        receiver.set_script_timeout(30000)
        received = receiver.execute_async_script("""
        function check_incoming() {
            if (window.wrappedJSObject.incoming != "none") {
                marionetteScriptFinished(window.wrappedJSObject.incoming);
            }
            else {
                setTimeout(check_incoming, 500);
            }
        }
        setTimeout(check_incoming, 0);
    """)
        # Verify the phone number of the incoming call.
        self.assertEqual(received, fromPhoneNumber)


