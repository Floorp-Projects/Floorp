load("calshell.js");
load("testuri.js"); // all this has to do is set testUri

var cal = createInstance("@mozilla.org/calendar/calendar;1?type=caldav", 
			 CI.calICalendar);
cal.uri = URLFromSpec(testUri);

var listener = new calOpListener();

cal.getItem("20041119T052348Z-100040d-1-248609e9-Oracle", listener);

runEventPump();
