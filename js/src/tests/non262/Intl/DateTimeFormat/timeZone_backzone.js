// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Generated by make_intl_data.py. DO NOT EDIT.
// tzdata version = 2025a

const tzMapper = [
    x => x,
    x => x.toUpperCase(),
    x => x.toLowerCase(),
];

// This file was generated with historical, pre-1970 backzone information
// respected. Therefore, every zone key listed below is its own Zone, not
// a Link to a modern-day target as IANA ignoring backzones would say.

// Backzone zones derived from IANA Time Zone Database.
const links = {
    "Africa/Accra": "Africa/Accra",
    "Africa/Addis_Ababa": "Africa/Addis_Ababa",
    "Africa/Asmara": "Africa/Asmara",
    "Africa/Bamako": "Africa/Bamako",
    "Africa/Bangui": "Africa/Bangui",
    "Africa/Banjul": "Africa/Banjul",
    "Africa/Blantyre": "Africa/Blantyre",
    "Africa/Brazzaville": "Africa/Brazzaville",
    "Africa/Bujumbura": "Africa/Bujumbura",
    "Africa/Conakry": "Africa/Conakry",
    "Africa/Dakar": "Africa/Dakar",
    "Africa/Dar_es_Salaam": "Africa/Dar_es_Salaam",
    "Africa/Djibouti": "Africa/Djibouti",
    "Africa/Douala": "Africa/Douala",
    "Africa/Freetown": "Africa/Freetown",
    "Africa/Gaborone": "Africa/Gaborone",
    "Africa/Harare": "Africa/Harare",
    "Africa/Kampala": "Africa/Kampala",
    "Africa/Kigali": "Africa/Kigali",
    "Africa/Kinshasa": "Africa/Kinshasa",
    "Africa/Libreville": "Africa/Libreville",
    "Africa/Lome": "Africa/Lome",
    "Africa/Luanda": "Africa/Luanda",
    "Africa/Lubumbashi": "Africa/Lubumbashi",
    "Africa/Lusaka": "Africa/Lusaka",
    "Africa/Malabo": "Africa/Malabo",
    "Africa/Maseru": "Africa/Maseru",
    "Africa/Mbabane": "Africa/Mbabane",
    "Africa/Mogadishu": "Africa/Mogadishu",
    "Africa/Niamey": "Africa/Niamey",
    "Africa/Nouakchott": "Africa/Nouakchott",
    "Africa/Ouagadougou": "Africa/Ouagadougou",
    "Africa/Porto-Novo": "Africa/Porto-Novo",
    "Africa/Timbuktu": "Africa/Timbuktu",
    "America/Anguilla": "America/Anguilla",
    "America/Antigua": "America/Antigua",
    "America/Argentina/ComodRivadavia": "America/Argentina/ComodRivadavia",
    "America/Aruba": "America/Aruba",
    "America/Atikokan": "America/Atikokan",
    "America/Blanc-Sablon": "America/Blanc-Sablon",
    "America/Cayman": "America/Cayman",
    "America/Coral_Harbour": "America/Coral_Harbour",
    "America/Creston": "America/Creston",
    "America/Curacao": "America/Curacao",
    "America/Dominica": "America/Dominica",
    "America/Ensenada": "America/Ensenada",
    "America/Grenada": "America/Grenada",
    "America/Guadeloupe": "America/Guadeloupe",
    "America/Montreal": "America/Montreal",
    "America/Montserrat": "America/Montserrat",
    "America/Nassau": "America/Nassau",
    "America/Nipigon": "America/Nipigon",
    "America/Pangnirtung": "America/Pangnirtung",
    "America/Port_of_Spain": "America/Port_of_Spain",
    "America/Rainy_River": "America/Rainy_River",
    "America/Rosario": "America/Rosario",
    "America/St_Kitts": "America/St_Kitts",
    "America/St_Lucia": "America/St_Lucia",
    "America/St_Thomas": "America/St_Thomas",
    "America/St_Vincent": "America/St_Vincent",
    "America/Thunder_Bay": "America/Thunder_Bay",
    "America/Tortola": "America/Tortola",
    "America/Yellowknife": "America/Yellowknife",
    "Antarctica/DumontDUrville": "Antarctica/DumontDUrville",
    "Antarctica/McMurdo": "Antarctica/McMurdo",
    "Antarctica/Syowa": "Antarctica/Syowa",
    "Asia/Aden": "Asia/Aden",
    "Asia/Bahrain": "Asia/Bahrain",
    "Asia/Brunei": "Asia/Brunei",
    "Asia/Chongqing": "Asia/Chongqing",
    "Asia/Harbin": "Asia/Harbin",
    "Asia/Kashgar": "Asia/Kashgar",
    "Asia/Kuala_Lumpur": "Asia/Kuala_Lumpur",
    "Asia/Kuwait": "Asia/Kuwait",
    "Asia/Muscat": "Asia/Muscat",
    "Asia/Phnom_Penh": "Asia/Phnom_Penh",
    "Asia/Tel_Aviv": "Asia/Tel_Aviv",
    "Asia/Vientiane": "Asia/Vientiane",
    "Atlantic/Jan_Mayen": "Atlantic/Jan_Mayen",
    "Atlantic/Reykjavik": "Atlantic/Reykjavik",
    "Atlantic/St_Helena": "Atlantic/St_Helena",
    "Australia/Currie": "Australia/Currie",
    "CET": "CET",
    "CST6CDT": "CST6CDT",
    "EET": "EET",
    "EST": "EST",
    "EST5EDT": "EST5EDT",
    "Europe/Amsterdam": "Europe/Amsterdam",
    "Europe/Belfast": "Europe/Belfast",
    "Europe/Copenhagen": "Europe/Copenhagen",
    "Europe/Guernsey": "Europe/Guernsey",
    "Europe/Isle_of_Man": "Europe/Isle_of_Man",
    "Europe/Jersey": "Europe/Jersey",
    "Europe/Ljubljana": "Europe/Ljubljana",
    "Europe/Luxembourg": "Europe/Luxembourg",
    "Europe/Monaco": "Europe/Monaco",
    "Europe/Oslo": "Europe/Oslo",
    "Europe/Sarajevo": "Europe/Sarajevo",
    "Europe/Skopje": "Europe/Skopje",
    "Europe/Stockholm": "Europe/Stockholm",
    "Europe/Tiraspol": "Europe/Tiraspol",
    "Europe/Uzhgorod": "Europe/Uzhgorod",
    "Europe/Vaduz": "Europe/Vaduz",
    "Europe/Zagreb": "Europe/Zagreb",
    "Europe/Zaporozhye": "Europe/Zaporozhye",
    "HST": "HST",
    "Indian/Antananarivo": "Indian/Antananarivo",
    "Indian/Christmas": "Indian/Christmas",
    "Indian/Cocos": "Indian/Cocos",
    "Indian/Comoro": "Indian/Comoro",
    "Indian/Kerguelen": "Indian/Kerguelen",
    "Indian/Mahe": "Indian/Mahe",
    "Indian/Mayotte": "Indian/Mayotte",
    "Indian/Reunion": "Indian/Reunion",
    "MET": "MET",
    "MST": "MST",
    "MST7MDT": "MST7MDT",
    "PST8PDT": "PST8PDT",
    "Pacific/Chuuk": "Pacific/Chuuk",
    "Pacific/Enderbury": "Pacific/Enderbury",
    "Pacific/Funafuti": "Pacific/Funafuti",
    "Pacific/Johnston": "Pacific/Johnston",
    "Pacific/Majuro": "Pacific/Majuro",
    "Pacific/Midway": "Pacific/Midway",
    "Pacific/Pohnpei": "Pacific/Pohnpei",
    "Pacific/Saipan": "Pacific/Saipan",
    "Pacific/Wake": "Pacific/Wake",
    "Pacific/Wallis": "Pacific/Wallis",
    "WET": "WET",
};

for (let [linkName, target] of Object.entries(links)) {
    if (target === "Etc/UTC" || target === "Etc/GMT")
        target = "UTC";

    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(undefined, {timeZone: map(linkName)});
        let resolvedTimeZone = dtf.resolvedOptions().timeZone;
        assertEq(resolvedTimeZone, target, `${linkName} -> ${target}`);
    }
}


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");

