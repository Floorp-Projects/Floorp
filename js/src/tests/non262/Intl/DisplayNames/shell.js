// Add |Intl.DisplayNames| to the Intl object if not already present.
function addIntlDisplayNames(global) {
    if (!global.Intl.DisplayNames && typeof global.addIntlExtras === "function") {
        let obj = {};
        global.addIntlExtras(obj);
        global.Intl.DisplayNames = obj.DisplayNames;
    }
}

addIntlDisplayNames(this);
