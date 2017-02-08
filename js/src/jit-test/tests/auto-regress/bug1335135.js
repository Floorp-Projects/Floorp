if (this.Intl) {
    Object.defineProperty(Object.prototype, 0, {
        set: function() {}
    });
    function checkDisplayNames(names, expected) {}
    addIntlExtras(Intl);
    let gDN = Intl.getDisplayNames;
    checkDisplayNames(gDN('ar', {
        keys: ['dates/fields/month', ]
    }), {});
}
