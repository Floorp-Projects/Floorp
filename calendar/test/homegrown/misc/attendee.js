var eventClass = C.classes["@mozilla.org/calendar/event;1"];
var eventIID = C.interfaces.calIEvent;

var attendeeClass = C.classes["@mozilla.org/calendar/attendee;1"];
var attendeeIID = C.interfaces.calIAttendee;

dump("* Creating event.\n");
var e = eventClass.createInstance(eventIID);
dump("* Creating attendee.\n");
var a1 = attendeeClass.createInstance(attendeeIID);
dump("* Testing attendee set/get.\n");
var properties = ["id", "commonName", "rsvp", "role", "participationStatus",
		  "userType"];
var values = ["myid", "mycn", true, attendeeIID.ROLE_CHAIR,
	      attendeeIID.PARTSTAT_DECLINED,
	      attendeeIID.CUTYPE_RESOURCE];
if (properties.length != values.length)
    throw "Test bug: mismatched properties and values arrays!";

for (var i = 0; i < properties.length; i++) {
    a1[properties[i]] = values[i];
    if (a1[properties[i]] != values[i]) {
	throw "FAILED! " + properties[i] + " not set to " + values[i];
    }
}

dump("* Adding attendee to event.\n");
e.addAttendee(a1);
dump("* Adding 2nd attendee to event.\n");
var a2 = attendeeClass.createInstance(attendeeIID);
a2.id = "myid2";
e.addAttendee(a2);

dump("* Finding by ID.\n");

function findById(id, a) {
    var foundAttendee = e.getAttendeeById(id);
    if (foundAttendee != a) {
	throw "FAILED! wrong attendee returned for + '" +
	    id + "' (got " + foundAttendee + ", expected " + a + ")";
    }
}    

findById("myid", a1);
findById("myid2", a2);

dump("* Searching getAttendees results\n");
var found1, found2;

function findAttendeesInResults(expectedCount) {
    if (expectedCount == undefined)
	throw "TEST BUG: expectedCount not passed to findAttendeesInResults";
    var countObj = {};
    dump("    Getting all attendees.\n");
    var allAttendees = e.getAttendees(countObj);
    if (countObj.value != allAttendees.length) {
	throw "FAILED! out count (" + countObj.value + ") != .length (" +
	    allAttendees.length + ")";
    }
    
    if (allAttendees.length != expectedCount) {
	throw "FAILED! expected to get back " + expectedCount +
    " attendees, got " + allAttendees.length;
    }

    found1 = false, found2 = false;
    for (var i = 0; i < expectedCount; i++) {
	if (allAttendees[i] == a1)
	    found1 = true;
	else if (allAttendees[i] == a2)
	    found2 = true;
	else {
	    throw "FAILED! unknown attendee " + allAttendees[i] + 
		" (we added " + a1 + " and " + a2 + + ")";
	}
    }
}
findAttendeesInResults(2);
if (!found1)
    throw "FAILED! didn't find attendee1 (" + a1 + ") in results";
if (!found2)
    throw "FAILED! didn't find attendee2 (" + a2 + ") in results";

dump("* Removing attendee.\n");
e.removeAttendee(a1);
if (e.getAttendeeById(a1.id) != null)
    throw "FAILED! got back removed attendee " + a1 + " by id";
findById("myid2", a2);
found1 = false, found2 = false;
findAttendeesInResults(1);
if (found1)
    throw "FAILED! found removed attendee " + a1 + " in getAttendees results";
if (!found2) {
    throw "FAILED! didn't find remaining attendee " + a2 +
	" in getAttendees results";
}

dump("* Readding attendee.\n");
e.addAttendee(a1);
findById("myid", a1);
findById("myid2", a2);
findAttendeesInResults(2);
if (!found1)
    throw "FAILED! didn't find attendee1 (" + a1 + ") in results";
if (!found2)
    throw "FAILED! didn't find attendee2 (" + a2 + ") in results";

dump("* Making attendee immutable.\n");
a1.makeImmutable();
function testImmutability(a) {
    if (a.isMutable) {
	throw "FAILED! Attendee " + a + 
	    " should be immutable, but claims otherwise";
    }
    for (var i = 0; i < properties.length; i++) {
	var old = a[properties[i]];
	var threw;
	try {
	    a[properties[i]] = old + 1;
	    threw = false;
	} catch (e) {
	    threw = true;
	}
	if (!threw) {
	    throw "FAILED! no error thrown setting " + properties[i] +
		"on immutable attendee " + a;
	}
	if (a[properties[i]] != old) {
	    throw "FAILED! setting " + properties[i] + " on " + a +
		" threw, but changed value anyway!";
	}
    }
}	    

testImmutability(a1);
dump("* Testing cascaded immutability (event -> attendee).\n");
e.makeImmutable();
testImmutability(a2);

dump("* Testing cloning\n");
var ec = e.clone();
var clonedatts = ec.getAttendees({});
var atts = e.getAttendees({});
if (atts.length != clonedatts.length) {
    throw "FAILED! cloned event has " + clonedatts.length +
        "attendees, original had " + atts.length;
}
for (i = 0; i < clonedatts.length; i++) {
    if (atts[i] == clonedatts[i])
        throw "FAILED! attendee " + atts[i].id + " shared with clone!";
    if (atts[i].id != clonedatts[i].id) {
        throw "FAILED! id mismatch on index " + i + ": " +
            atts[i].id + " != " + clonedatts[i].id;
    }
}
dump("PASSED!\n");
