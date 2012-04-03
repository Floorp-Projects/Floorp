from marionette_test import *


class MultiEmulatorDialTest(MarionetteTestCase):
    """A simple test which verifies the ability of one emulator to dial
       another and to detect an incoming call.
    """

    def test_dial_answer(self):
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
window.wrappedJSObject.incoming = null;
navigator.mozTelephony.addEventListener("incoming", function test_incoming(e) {
    navigator.mozTelephony.removeEventListener("incoming", test_incoming);
    window.wrappedJSObject.incoming = e.call;
});
""")

        # Dial the receiver from the sender.
        toPhoneNumber = "1555521%d" % receiver.emulator.port
        fromPhoneNumber = "1555521%d" % sender.emulator.port
        sender.set_context("chrome")
        sender.execute_script("""
window.wrappedJSObject.call = navigator.mozTelephony.dial("%s");
""" % toPhoneNumber)

        # On the receiver, wait up to 30s for an incoming call to be 
        # detected, by checking the value of the global var that the 
        # listener will change.
        receiver.set_script_timeout(30000)
        received = receiver.execute_async_script("""
window.wrappedJSObject.callstate = null;
waitFor(function() {
    let call = window.wrappedJSObject.incoming;
    call.addEventListener("connected", function test_connected(e) {
        call.removeEventListener("connected", test_connected);
        window.wrappedJSObject.callstate = e.call.state;
    });
    marionetteScriptFinished(call.number);
},
function() {
    return window.wrappedJSObject.incoming != null;
});
""")
        # Verify the phone number of the incoming call.
        self.assertEqual(received, fromPhoneNumber)

        # On the sender, add a listener to verify that the call changes
        # state to connected when it's answered.
        sender.execute_script("""
let call = window.wrappedJSObject.call;
window.wrappedJSObject.callstate = null;
call.addEventListener("connected", function test_connected(e) {
    call.removeEventListener("connected", test_connected);
    window.wrappedJSObject.callstate = e.call.state;
});
""")

        # Answer the call and verify that the callstate changes to
        # connected.
        receiver.execute_async_script("""
window.wrappedJSObject.incoming.answer();
waitFor(function() {
    marionetteScriptFinished(true);
}, function() {
    return window.wrappedJSObject.callstate == "connected";
});
""")

        # Verify that the callstate changes to connected on the caller as well.
        self.assertTrue(receiver.execute_async_script("""
waitFor(function() {
    marionetteScriptFinished(true);
}, function() {
    return window.wrappedJSObject.callstate == "connected";
});
"""))

