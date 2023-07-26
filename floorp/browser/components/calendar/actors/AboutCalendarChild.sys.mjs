const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

export class AboutCalendarChild extends JSWindowActorChild {
    actorCreated() {
        Cu.exportFunction(
            this.PrefCalendar.bind(this),
            this.contentWindow,
            {
              defineAs: "PrefCalendar",
            }
        );
    }
    PrefCalendar(action, data) {
        if (action == "get") {
            return Services.prefs.getStringPref(
                "floorp.calendar.eventsData",
                "[]"
            );
        } else if (action == "set") {
            this.sendAsyncMessage("set", data);
        }
    }
}
