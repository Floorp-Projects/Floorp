// Add |Intl.Locale| to the Intl object if not already present.
function addIntlLocale(global) {
    if (!global.Intl.Locale && typeof global.addIntlExtras === "function") {
        let obj = {};
        global.addIntlExtras(obj);
        global.Intl.Locale = obj.Locale;
    }
}

addIntlLocale(this);
