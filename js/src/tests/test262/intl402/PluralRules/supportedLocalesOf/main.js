// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras')) -- needs Intl, needs addIntlExtras
// Copyright 2016 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-Intl.PluralRules.supportedLocalesOf
description: >
    Tests that Intl.PluralRules has a supportedLocalesOf property, and
    it works as planned.
author: Zibi Braniecki
---*/

var defaultLocale = new Intl.PluralRules().resolvedOptions().locale;
var notSupported = 'zxx'; // "no linguistic content"
var requestedLocales = [defaultLocale, notSupported];
    
var supportedLocales;

if (!Intl.PluralRules.hasOwnProperty('supportedLocalesOf')) {
    $ERROR("Intl.PluralRules doesn't have a supportedLocalesOf property.");
}
    
supportedLocales = Intl.PluralRules.supportedLocalesOf(requestedLocales);
if (supportedLocales.length !== 1) {
    $ERROR('The length of supported locales list is not 1.');
}
    
if (supportedLocales[0] !== defaultLocale) {
    $ERROR('The default locale is not returned in the supported list.');
}

reportCompare(0, 0);
