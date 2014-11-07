if (this.hasOwnProperty('Intl')) {
    gc();
    gcslice(1);
    var thisValues = [ "x" ];
    thisValues.forEach(function (value) {
            var format = Intl.DateTimeFormat.call(value);
    });
}
