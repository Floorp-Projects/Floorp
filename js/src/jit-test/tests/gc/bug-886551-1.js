if (this.hasOwnProperty('Intl')) {
    gc();
    gcslice(0);
    var thisValues = [ "x" ];
    thisValues.forEach(function (value) {
            var format = Intl.DateTimeFormat.call(value);
    });
}
