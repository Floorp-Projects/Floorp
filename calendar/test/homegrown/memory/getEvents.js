var cal = createInstance("@mozilla.org/calendar/calendar;1?type=" 
			 + calendarType, CI.calICalendar);
cal.uri = URLFromSpec(calendarUri);

var listener = new calOpListener();

cal.getItems(CI.calICalendar.ITEM_FILTER_COMPLETED_ALL | 
	     CI.calICalendar.ITEM_FILTER_TYPE_EVENT, 0,
	     null, null, listener);

if (!done)
  runEventPump();
