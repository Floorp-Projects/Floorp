// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
assertThrowsInstanceOf(() => Tuple.from(null), TypeError,
                       'Tuple.from(null) should throw');

reportCompare(0, 0);
