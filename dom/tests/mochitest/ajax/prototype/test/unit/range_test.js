new Test.Unit.Runner({
  
  testInclude: function() {
    this.assert(!$R(0, 0, true).include(0));
    this.assert($R(0, 0, false).include(0));

    this.assert($R(0, 5, true).include(0));
    this.assert($R(0, 5, true).include(4));
    this.assert(!$R(0, 5, true).include(5));

    this.assert($R(0, 5, false).include(0));
    this.assert($R(0, 5, false).include(5));
    this.assert(!$R(0, 5, false).include(6));
  },

  testEach: function() {
    var results = [];
    $R(0, 0, true).each(function(value) {
      results.push(value);
    });

    this.assertEnumEqual([], results);

    results = [];
    $R(0, 3, false).each(function(value) {
      results.push(value);
    });

    this.assertEnumEqual([0, 1, 2, 3], results);
  },

  testAny: function() {
    this.assert(!$R(1, 1, true).any());
    this.assert($R(0, 3, false).any(function(value) {
      return value == 3;
    }));
  },

  testAll: function() {
    this.assert($R(1, 1, true).all());
    this.assert($R(0, 3, false).all(function(value) {
      return value <= 3;
    }));
  },

  testToArray: function() {
    this.assertEnumEqual([], $R(0, 0, true).toArray());
    this.assertEnumEqual([0], $R(0, 0, false).toArray());
    this.assertEnumEqual([0], $R(0, 1, true).toArray());
    this.assertEnumEqual([0, 1], $R(0, 1, false).toArray());
    this.assertEnumEqual([-3, -2, -1, 0, 1, 2], $R(-3, 3, true).toArray());
    this.assertEnumEqual([-3, -2, -1, 0, 1, 2, 3], $R(-3, 3, false).toArray());
  },
  
  testDefaultsToNotExclusive: function() {
    this.assertEnumEqual($R(-3,3), $R(-3,3,false));
  }
});