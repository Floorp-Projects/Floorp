var str = "BEGIN:VCALENDAR\n";
   str += "BEGIN:VEVENT\n";
   str += "CREATED:20050104T211235\n";
   str += "LAST-MODIFIED:20050108T182742\n";
   str += "DTSTAMP:20050104T211235\n";
   str += "UID:uuid:1104873174977\n";
   str += "SUMMARY:teste\n";
   str += "DTSTART:20050107T094500\n";
   str += "DTEND:20050107T104500\n";
   str += "RRULE:FREQ=DAILY;COUNT=43;INTERVAL=5\n";
   str += "ATTENDEE:MAILTO:test@example.com\n";
   str += "END:VEVENT\n";
   str += "END:VCALENDAR\n";

var icsServ = Components.classes["@mozilla.org/calendar/ics-service;1"]
                        .getService(Components.interfaces.calIICSService);

var calComp = icsServ.parseICS(str);
var subComp = calComp.getFirstSubcomponent("VEVENT");
var event = Components.classes["@mozilla.org/calendar/event;1"]
                      .createInstance(Components.interfaces.calIEvent);
event.icalComponent = subComp;

var newCalComp = event.icalComponent;
dump(newCalComp.serializeToICS());

