//
// simple test bits
//


const eventContractID = "@mozilla.org/calendar/event;1";
const eventIID = Components.interfaces.calIEvent;

const mutableEventContractID = "@mozilla.org/calendar/mutableevent;1";
const mutableEventIID = Components.interfaces.calIMutableEvent;

dump ("Creating mutable event...\n");
var me = Components.classes[mutableEventContractID].createInstance(mutableEventIID);

me.title = "Test Title";
dump ("Title is: " + me.title + " (jsobj: " + me.wrappedJSObject.mTitle + ")\n");

dump ("Creating non-mutable event...\n");
var e = Components.classes[eventContractID].createInstance(eventIID);
dump ("Title is: '" + e.title + "' (should be empty)\n");
dump ("Trying to set title (should fail)\n");
e.title = "Fail";
