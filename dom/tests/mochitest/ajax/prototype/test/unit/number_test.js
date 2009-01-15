new Test.Unit.Runner({
  
  testNumberMathMethods: function() {
    this.assertEqual(1, (0.9).round());
    this.assertEqual(-2, (-1.9).floor());
    this.assertEqual(-1, (-1.9).ceil());

    $w('abs floor round ceil').each(function(method) {
      this.assertEqual(Math[method](Math.PI), Math.PI[method]());
    }, this);
  },

  testNumberToColorPart: function() {
    this.assertEqual('00', (0).toColorPart());
    this.assertEqual('0a', (10).toColorPart());
    this.assertEqual('ff', (255).toColorPart());
  },

  testNumberToPaddedString: function() {
    this.assertEqual('00', (0).toPaddedString(2, 16));
    this.assertEqual('0a', (10).toPaddedString(2, 16));
    this.assertEqual('ff', (255).toPaddedString(2, 16));
    this.assertEqual('000', (0).toPaddedString(3));
    this.assertEqual('010', (10).toPaddedString(3));
    this.assertEqual('100', (100).toPaddedString(3));
    this.assertEqual('1000', (1000).toPaddedString(3));
  },

  testNumberToJSON: function() {
    this.assertEqual('null', Number.NaN.toJSON());
    this.assertEqual('0', (0).toJSON());
    this.assertEqual('-293', (-293).toJSON());
  },
  
  testNumberTimes: function() {
    var results = [];
    (5).times(function(i) { results.push(i) });
    this.assertEnumEqual($R(0, 4), results);
    
    results = [];
    (5).times(function(i) { results.push(i * this.i) }, { i: 2 });
    this.assertEnumEqual([0, 2, 4, 6, 8], results);
  }
});