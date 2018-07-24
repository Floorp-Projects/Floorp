// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Ensure we provide 'long' and 'short' descriptive names for non-meta time
// zones. (CLDR stores names for Etc/UTC, Europe/Dublin, and Europe/London as
// non-meta zones.)

const date = new Date(Date.UTC(2018, 7-1, 24));
const tests = [
    {
        locale: "en-US",
        timeZoneName: "long",
        zones: {
            "Etc/UTC": "7/24/2018, Coordinated Universal Time",
            "Europe/Dublin": "7/24/2018, Irish Standard Time",
            "Europe/London": "7/24/2018, British Summer Time",
        }
    },
    {
        locale: "de",
        timeZoneName: "long",
        zones: {
            "Etc/UTC": "24.7.2018, Koordinierte Weltzeit",
            "Europe/Dublin": "24.7.2018, Irische Sommerzeit",
            "Europe/London": "24.7.2018, Britische Sommerzeit",
        }
    },

    {
        locale: "en-US",
        timeZoneName: "short",
        zones: {
            "Europe/Dublin": "7/24/2018, GMT+1",
            "Europe/London": "7/24/2018, GMT+1",
        }
    },
    {
        locale: "en-GB",
        timeZoneName: "short",
        zones: {
            "Europe/Dublin": "24/07/2018, GMT+1",
            "Europe/London": "24/07/2018, BST",
        }
    },
    {
        locale: "en-IE",
        timeZoneName: "short",
        zones: {
            "Europe/Dublin": "24/7/2018, IST",
            "Europe/London": "24/7/2018, GMT+1",
        }
    },
];

for (let {locale, timeZoneName, zones} of tests) {
    for (let [timeZone, expected] of Object.entries(zones)) {
        assertEq(new Intl.DateTimeFormat(locale, {timeZone, timeZoneName}).format(date),
                 expected);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
