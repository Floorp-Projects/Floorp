load("calshell.js");
load("testuri.js"); // all this has to do is set testUri

var cal = createInstance("@mozilla.org/calendar/calendar;1?type=caldav", 
			 CI.calICalendar);
cal.uri = URLFromSpec(testUri);

var listener = new calOpListener();

cal.getItems(CI.calICalendar.ITEM_FILTER_COMPLETED_ALL | 
	     CI.calICalendar.ITEM_FILTER_TYPE_EVENT, 0,
	     null, null, listener);

runEventPump();
