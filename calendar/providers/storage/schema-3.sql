CREATE TABLE cal_calendar_schema_version (
	version	INTEGER
);

CREATE TABLE cal_events (
	cal_id		INTEGER, --	REFERENCES cal_calendars.id,

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
	event_start_tz	VARCHAR,
	event_end	INTEGER,
	event_end_tz	VARCHAR,
	event_stamp	INTEGER,

	-- alarm time
	alarm_time	INTEGER,
	alarm_time_tz	VARCHAR
);

CREATE TABLE cal_todos (
	cal_id		INTEGER, --	REFERENCES cal_calendars.id,

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

	-- Todo bits
	-- date the todo is to be displayed
	todo_entry	INTEGER,
	todo_entry_tz	VARCHAR,
	-- date the todo is due
	todo_due	INTEGER,
	todo_due_tz	VARCHAR,
	-- date the todo is completed
	todo_completed	INTEGER,
	todo_completed_tz VARCHAR,
	-- percent the todo is complete (0-100)
	todo_complete	INTEGER,

	-- alarm time
	alarm_time	INTEGER,
	alarm_time_tz	VARCHAR
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
