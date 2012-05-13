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

        self.set_up_test_page(sender, "test.html", ["dom.telephony.app.phone.url"])
        self.set_up_test_page(receiver, "test.html", ["dom.telephony.app.phone.url"])

        # Setup the event listsener on the receiver, which should store
        # a global variable when an incoming call is received.
        self.assertTrue(receiver.execute_script("""
return window.navigator.mozTelephony != null
"""))
        receiver.execute_script("""
global.incoming = null;
window.navigator.mozTelephony.addEventListener("incoming", function test_incoming(e) {
    window.navigator.mozTelephony.removeEventListener("incoming", test_incoming);
    global.incoming = e.call;
});
""", new_sandbox=False)

        # Dial the receiver from the sender.
        toPhoneNumber = "1555521%d" % receiver.emulator.port
        fromPhoneNumber = "1555521%d" % sender.emulator.port
        sender.execute_script("""
global.call = window.navigator.mozTelephony.dial("%s");
""" % toPhoneNumber, new_sandbox=False)

        # On the receiver, wait up to 30s for an incoming call to be 
        # detected, by checking the value of the global var that the 
        # listener will change.
        receiver.set_script_timeout(30000)
        received = receiver.execute_async_script("""
global.callstate = null;
waitFor(function() {
    let call = global.incoming;
    call.addEventListener("connected", function test_connected(e) {
        call.removeEventListener("connected", test_connected);
        global.callstate = e.call.state;
    });
    marionetteScriptFinished(call.number);
},
function() {
    return global.incoming != null;
});
""", new_sandbox=False)
        # Verify the phone number of the incoming call.
        self.assertEqual(received, fromPhoneNumber)

        # On the sender, add a listener to verify that the call changes
        # state to connected when it's answered.
        sender.execute_script("""
let call = global.call;
global.callstate = null;
call.addEventListener("connected", function test_connected(e) {
    call.removeEventListener("connected", test_connected);
    global.callstate = e.call.state;
});
""", new_sandbox=False)

        # Answer the call and verify that the callstate changes to
        # connected.
        receiver.execute_async_script("""
global.incoming.answer();
waitFor(function() {
    marionetteScriptFinished(true);
}, function() {
    return global.callstate == "connected";
});
""", new_sandbox=False)

        # Verify that the callstate changes to connected on the caller as well.
        self.assertTrue(receiver.execute_async_script("""
waitFor(function() {
    global.incoming.hangUp();
    marionetteScriptFinished(true);
 }, function() {
    return global.callstate == "connected";
});
""", new_sandbox=False))
