// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const tzMapper = [
    x => x,
    x => x.toUpperCase(),
    x => x.toLowerCase(),
];

// Backzone names derived from IANA Time Zone Database, version tzdata2016g.
const backZones = [
    "Africa/Addis_Ababa", // Africa/Nairobi
    "Africa/Asmara", // Africa/Nairobi
    "Africa/Bamako", // Africa/Abidjan
    "Africa/Bangui", // Africa/Lagos
    "Africa/Banjul", // Africa/Abidjan
    "Africa/Blantyre", // Africa/Maputo
    "Africa/Brazzaville", // Africa/Lagos
    "Africa/Bujumbura", // Africa/Maputo
    "Africa/Conakry", // Africa/Abidjan
    "Africa/Dakar", // Africa/Abidjan
    "Africa/Dar_es_Salaam", // Africa/Nairobi
    "Africa/Djibouti", // Africa/Nairobi
    "Africa/Douala", // Africa/Lagos
    "Africa/Freetown", // Africa/Abidjan
    "Africa/Gaborone", // Africa/Maputo
    "Africa/Harare", // Africa/Maputo
    "Africa/Juba", // Africa/Khartoum
    "Africa/Kampala", // Africa/Nairobi
    "Africa/Kigali", // Africa/Maputo
    "Africa/Kinshasa", // Africa/Lagos
    "Africa/Libreville", // Africa/Lagos
    "Africa/Lome", // Africa/Abidjan
    "Africa/Luanda", // Africa/Lagos
    "Africa/Lubumbashi", // Africa/Maputo
    "Africa/Lusaka", // Africa/Maputo
    "Africa/Malabo", // Africa/Lagos
    "Africa/Maseru", // Africa/Johannesburg
    "Africa/Mbabane", // Africa/Johannesburg
    "Africa/Mogadishu", // Africa/Nairobi
    "Africa/Niamey", // Africa/Lagos
    "Africa/Nouakchott", // Africa/Abidjan
    "Africa/Ouagadougou", // Africa/Abidjan
    "Africa/Porto-Novo", // Africa/Lagos
    "Africa/Sao_Tome", // Africa/Abidjan
    "Africa/Timbuktu", // Africa/Abidjan
    "America/Anguilla", // America/Port_of_Spain
    "America/Antigua", // America/Port_of_Spain
    "America/Argentina/ComodRivadavia", // America/Argentina/Catamarca
    "America/Aruba", // America/Curacao
    "America/Cayman", // America/Panama
    "America/Coral_Harbour", // America/Atikokan
    "America/Dominica", // America/Port_of_Spain
    "America/Ensenada", // America/Tijuana
    "America/Grenada", // America/Port_of_Spain
    "America/Guadeloupe", // America/Port_of_Spain
    "America/Montreal", // America/Toronto
    "America/Montserrat", // America/Port_of_Spain
    "America/Rosario", // America/Argentina/Cordoba
    "America/St_Kitts", // America/Port_of_Spain
    "America/St_Lucia", // America/Port_of_Spain
    "America/St_Thomas", // America/Port_of_Spain
    "America/St_Vincent", // America/Port_of_Spain
    "America/Tortola", // America/Port_of_Spain
    "Antarctica/McMurdo", // Pacific/Auckland
    "Asia/Aden", // Asia/Riyadh
    "Asia/Bahrain", // Asia/Qatar
    "Asia/Chongqing", // Asia/Shanghai
    "Asia/Harbin", // Asia/Shanghai
    "Asia/Kashgar", // Asia/Urumqi
    "Asia/Kuwait", // Asia/Riyadh
    "Asia/Muscat", // Asia/Dubai
    "Asia/Phnom_Penh", // Asia/Bangkok
    "Asia/Tel_Aviv", // Asia/Jerusalem
    "Asia/Vientiane", // Asia/Bangkok
    "Atlantic/Jan_Mayen", // Europe/Oslo
    "Atlantic/St_Helena", // Africa/Abidjan
    "Europe/Belfast", // Europe/London
    "Europe/Guernsey", // Europe/London
    "Europe/Isle_of_Man", // Europe/London
    "Europe/Jersey", // Europe/London
    "Europe/Ljubljana", // Europe/Belgrade
    "Europe/Sarajevo", // Europe/Belgrade
    "Europe/Skopje", // Europe/Belgrade
    "Europe/Tiraspol", // Europe/Chisinau
    "Europe/Vaduz", // Europe/Zurich
    "Europe/Zagreb", // Europe/Belgrade
    "Indian/Antananarivo", // Africa/Nairobi
    "Indian/Comoro", // Africa/Nairobi
    "Indian/Mayotte", // Africa/Nairobi
    "Pacific/Johnston", // Pacific/Honolulu
    "Pacific/Midway", // Pacific/Pago_Pago
    "Pacific/Saipan", // Pacific/Guam
];

for (let timeZone of backZones) {
    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(undefined, {timeZone: map(timeZone)});
        assertEq(dtf.resolvedOptions().timeZone, timeZone);
    }
}


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
