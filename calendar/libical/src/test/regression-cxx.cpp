#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "icalparameter_cxx.h"
#include "icalproperty_cxx.h"
#include "vcomponent.h"
#include "regression.h"

char content[] = "BEGIN:VCALENDAR\n\
VERSION:2.1\n\
BEGIN:VEVENT\n\
UID:abcd12345\n\
DTSTART:20020307T180000Z\n\
DTEND:20020307T190000Z\n\
SUMMARY:Important Meeting\n\
END:VEVENT\n\
END:VCALENDAR";

void test_cxx(void)
{
    ICalProperty            *summProp  = new ICalProperty(ICAL_SUMMARY_PROPERTY);
    ICalProperty            *startProp = new ICalProperty(ICAL_DTSTART_PROPERTY);
    ICalProperty            *endProp   = new ICalProperty(ICAL_DTEND_PROPERTY);
    ICalProperty	    *locationProp = new ICalProperty(ICAL_LOCATION_PROPERTY);
    ICalProperty            *descProp = new ICalProperty(ICAL_DESCRIPTION_PROPERTY);

    ok("Valid SUMMARY     Property", (summProp != 0));
    ok("Valid DTSTART     Property", (startProp != 0));
    ok("Valid DTEND       Property", (endProp != 0));
    ok("Valid LOCATION    Property", (locationProp != 0));
    ok("Valid DESCRIPTION Property", (descProp != 0));

    struct icaltimetype     starttime  = icaltime_from_string("20011221T180000Z");   // UTC time ends in Z
    struct icaltimetype     endtime    = icaltime_from_string("20020101T080000Z");   // UTC time ends in Z

    summProp->set_summary("jon said: change dir to c:\\rest\\test\\nest to get the file called <foo.dat>\nthis should be in the next line.");
    startProp->set_dtstart(starttime);
    endProp->set_dtend(endtime);
    locationProp->set_location("SF, California; Seattle, Washington");
    descProp->set_description("The best cities on the west coast, hit 'NO' if you don't agree!\n");

    VEvent *vEvent = new VEvent();

    ok("Create a new VEvent", (vEvent!=0));

    vEvent->add_property(summProp);
    vEvent->add_property(startProp);
    vEvent->add_property(endProp);
    vEvent->add_property(locationProp);
    vEvent->add_property(descProp);
 
    // 
    is ("vEvent->get_summary()", 
	vEvent->get_summary(), 
	"jon said: change dir to c:\\rest\\test\\nest to get the file called <foo.dat>\nthis should be in the next line.");

    is ("vEvent->get_dtstart()",
	icaltime_as_ical_string(vEvent->get_dtstart()),
	"20011221T180000Z");

    is ("vEvent->get_dtend()",
	icaltime_as_ical_string(vEvent->get_dtend()),
	"20020101T080000Z");

    ok ("vEvent->as_ical_string()",
	(vEvent->as_ical_string() != 0));

    if (VERBOSE) {
      printf("Summary: %s\n", vEvent->get_summary());
      printf("DTSTART: %s\n",  icaltime_as_ical_string(vEvent->get_dtstart()));
      printf("DTEND: %s\n",   icaltime_as_ical_string(vEvent->get_dtend()));
      printf("LOCATION: %s\n", vEvent->get_location());
      printf("DESCRIPTION: %s\n", vEvent->get_description());
      
      printf("vcomponent: %s", vEvent->as_ical_string());
    }

    VComponent ic(icalparser_parse_string((const char*)content));
    ok("Parsing component", (ic.is_valid()));

    if (VERBOSE)
      printf("%s\n", ic.as_ical_string());

    // component is wrapped within BEGIN:VCALENDAR END:VCALENDAR
    // we need to unwrap it.

    VEvent* sub_ic = dynamic_cast<VEvent*>(ic.get_first_component(ICAL_VEVENT_COMPONENT));

    int_is("Getting VEvent subcomponent", 
       sub_ic->isa(),
       ICAL_VEVENT_COMPONENT);

    while (sub_ic != NULL) {
      if (VERBOSE)
	printf("subcomponent: %s\n", sub_ic->as_ical_string());
      
      sub_ic = dynamic_cast<VEvent*>(ic.get_next_component(ICAL_VEVENT_COMPONENT));
    }

    VCalendar* cal = new VCalendar();
    VAgenda* vAgenda = new VAgenda();

    ok("Create a new VCalendar object", (cal != 0));
    ok("Create a new VAgenda object", (vAgenda != 0));

    ICalProperty* prop = new ICalProperty(ICAL_OWNER_PROPERTY);
    prop->set_owner("fred@flintstone.net");
    vAgenda->add_property(prop);

    prop = new ICalProperty(ICAL_SUMMARY_PROPERTY);
    prop->set_summary("CPMain");
    vAgenda->add_property(prop);

    prop = new ICalProperty(ICAL_TZID_PROPERTY);
    prop->set_tzid("America/Los_Angeles");
    vAgenda->add_property(prop);

    cal->add_component(vAgenda);

    ok("Complex VCALENDAR/VAGENDA", (cal->as_ical_string() != 0));

    if (VERBOSE)
      printf("vAgenda: %s\n", cal->as_ical_string());

    int caughtException = 0;
    try {
      string foo = "HFHFHFHF";
      VComponent v = VComponent(foo);
    } catch (icalerrorenum err) {
	    if (err == ICAL_BADARG_ERROR) {
		    caughtException = 1;
            }
    }
    int_is("Testing exception handling", caughtException, 1);

}

