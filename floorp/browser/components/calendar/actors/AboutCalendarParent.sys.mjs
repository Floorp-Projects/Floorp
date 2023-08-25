const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

export class AboutCalendarParent extends JSWindowActorParent {
    async receiveMessage(message) {
        if (message.name === "set") {
            Services.prefs.setStringPref(
                "floorp.calendar.eventsData",
                message.data
            );
        }
    }
}
