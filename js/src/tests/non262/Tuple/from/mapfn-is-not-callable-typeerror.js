// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
let vals = [[null, "null"],
            [{}, "{}"],
            ['string', "\'string\'"],
            [true, "true"],
            [42, "42"],
            [Symbol('1'), "Symbol(\'1\')"]];

for (p of vals) {
    let mapfn = p[0];
    assertThrowsInstanceOf(() => Tuple.from([], mapfn),
                           TypeError, 'Tuple.from([],' + p[1] + ' should throw');
}

reportCompare(0, 0);
