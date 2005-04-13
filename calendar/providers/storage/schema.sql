
CREATE TABLE cal_calendar_schema_version (
	version	INTEGER
);

CREATE TABLE cal_items (
	cal_id		INTEGER, --	REFERENCES cal_calendars.id,
	-- 0: event, 1: todo
	item_type	INTEGER,

	-- ItemBase bits
	id		STRING,
	time_created	INTEGER,
	last_modified	INTEGER,
	title		STRING,
	priority	INTEGER,
	privacy		STRING,

	ical_status	STRING,

	-- CAL_ITEM_FLAG_PRIVATE = 1
	-- CAL_ITEM_FLAG_HAS_ATTENDEES = 2
	-- CAL_ITEM_FLAG_HAS_PROPERTIES = 4
	-- CAL_ITEM_FLAG_EVENT_ALLDAY = 8
	-- CAL_ITEM_FLAG_HAS_RECURRENCE = 16
	flags		INTEGER,

	-- Event bits
	event_start	INTEGER,
	event_end	INTEGER,
	event_stamp	INTEGER,

	-- Todo bits
	todo_entry	INTEGER,
	todo_due	INTEGER,
	todo_completed	INTEGER,
	todo_complete	INTEGER,

	-- internal bits
	alarm_id	INTEGER  -- REFERENCES cal_alarms.id ON DELETE CASCADE
);

CREATE TABLE cal_attendees (
	item_id         STRING,
	attendee_id	STRING,
	common_name	STRING,
	rsvp		INTEGER,
	role		STRING,
	status		STRING,
	type		STRING
);

CREATE TABLE cal_alarms (
	id		INTEGER PRIMARY KEY,

	alarm_data	BLOB
);

CREATE TABLE cal_recurrence (
	item_id		STRING,
	recur_index	INTEGER, -- the index in the recurrence array of this thing
	recur_type	STRING, -- values from calIRecurrenceInfo; if null, date-based.

	is_negative	BOOLEAN,

	--
	-- these are for date-based recurrence
	--

	-- comma-separated list of dates
	dates		STRING,

	--
	-- these are for rule-based recurrence
	--
	count		INTEGER,
	end_date	INTEGER,
	interval	INTEGER,

	-- components, comma-separated list or null
	second		STRING,
	minute		STRING,
	hour		STRING,
	day		STRING,
	monthday	STRING,
	yearday		STRING,
	weekno		STRING,
	month		STRING,
	setpos		STRING
);

CREATE TABLE cal_properties (
	item_id		STRING,
	
	key		STRING,
	value		BLOB
);
