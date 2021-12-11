// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// ApplyOptionsToTag canonicalises the locale identifier before applying the
// options. That means "und-Armn-SU" is first canonicalised to "und-Armn-AM",
// then the language is changed to "ru". If "ru" were applied first, the result
// would be "ru-Armn-RU" instead.
assertEq(new Intl.Locale("und-Armn-SU", {language:"ru"}).toString(),
         "ru-Armn-AM");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
