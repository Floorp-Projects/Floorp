//
// simple test bits
//


const eventContractID = "@mozilla.org/calendar/event;1";
const eventIID = Components.interfaces.calIEvent;

dump ("Creating mutable event...\n");
var me = Components.classes[eventContractID].createInstance(eventIID);

me.title = "Test Title";
dump ("Title is: " + me.title + " (jsobj: " + me.wrappedJSObject.mTitle + ")\n");
if (me.title != "Test Title") {
  throw("FAILED!  Title on mutable event contained unexpected value.");
}

dump ("Setting event-to be non-mutable...\n");
me.makeImmutable();

dump ("Trying to set title (should fail)\n");
try {
  // this should cause an exception
  me.title = "Fail";
  
  // so we'll only get here if something is wrong, and there's no exception
  dump("FAILED\n!");
} catch (ex) {
}

if (me.title != "Test Title") {
  throw("FAILED!  Title on immutable event contained unexpected value: " 
	+ me.title);
}

