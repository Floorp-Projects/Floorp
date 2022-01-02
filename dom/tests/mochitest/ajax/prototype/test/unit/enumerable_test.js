function prime(value) {
  for (var i = 2; i < value; i++)
    if (value % i == 0) return false;
  return true;
}

new Test.Unit.Runner({    
  testEachBreak: function() {
    var result = 0;
    Fixtures.Basic.each(function(value) {
      if ((result = value) == 2) throw $break;
    });
    
    this.assertEqual(2, result);
  },
  
  testEachReturnActsAsContinue: function() {
    var results = [];
    Fixtures.Basic.each(function(value) {
      if (value == 2) return;
      results.push(value);
    });
    
    this.assertEqual('1, 3', results.join(', '));
  },
  
  testEachChaining: function() {
    this.assertEqual(Fixtures.Primes, Fixtures.Primes.each(Prototype.emptyFunction));
    this.assertEqual(3, Fixtures.Basic.each(Prototype.emptyFunction).length);
  },

  testEnumContext: function() {
    var results = [];
    Fixtures.Basic.each(function(value) {
      results.push(value * this.i);
    }, { i: 2 });
    
    this.assertEqual('2 4 6', results.join(' '));

    this.assert(Fixtures.Basic.all(function(value){
      return value >= this.min && value <= this.max;
    }, { min: 1, max: 3 }));
    this.assert(!Fixtures.Basic.all(function(value){
      return value >= this.min && value <= this.max;
    }));
    this.assert(Fixtures.Basic.any(function(value){
      return value == this.target_value;
    }, { target_value: 2 }));
  },

  testAny: function() {
    this.assert(!([].any()));
    
    this.assert([true, true, true].any());
    this.assert([true, false, false].any());
    this.assert(![false, false, false].any());
    
    this.assert(Fixtures.Basic.any(function(value) {
      return value > 2;
    }));
    this.assert(!Fixtures.Basic.any(function(value) {
      return value > 5;
    }));
  },
  
  testAll: function() {
    this.assert([].all());
    
    this.assert([true, true, true].all());
    this.assert(![true, false, false].all());
    this.assert(![false, false, false].all());

    this.assert(Fixtures.Basic.all(function(value) {
      return value > 0;
    }));
    this.assert(!Fixtures.Basic.all(function(value) {
      return value > 1;
    }));
  },
  
  testCollect: function() {
    this.assertEqual(Fixtures.Nicknames.join(', '), 
      Fixtures.People.collect(function(person) {
        return person.nickname;
      }).join(", "));
    
    this.assertEqual(26,  Fixtures.Primes.map().length);
  },
  
  testDetect: function() {
    this.assertEqual('Marcel Molina Jr.', 
      Fixtures.People.detect(function(person) {
        return person.nickname.match(/no/);
      }).name);
  },
  
  testEachSlice: function() {
    this.assertEnumEqual([], [].eachSlice(2));
    this.assertEqual(1, [1].eachSlice(1).length);
    this.assertEnumEqual([1], [1].eachSlice(1)[0]);
    this.assertEqual(2, Fixtures.Basic.eachSlice(2).length);
    this.assertEnumEqual(
      [3, 2, 1, 11, 7, 5, 19, 17, 13, 31, 29, 23, 43, 41, 37, 59, 53, 47, 71, 67, 61, 83, 79, 73, 97, 89],
      Fixtures.Primes.eachSlice( 3, function(slice){ return slice.reverse() }).flatten()
    );
    this.assertEnumEqual(Fixtures.Basic, Fixtures.Basic.eachSlice(-10));
    this.assertEnumEqual(Fixtures.Basic, Fixtures.Basic.eachSlice(0));
    this.assertNotIdentical(Fixtures.Basic, Fixtures.Basic.eachSlice(0));
  },
  
  testEachWithIndex: function() {
    var nicknames = [], indexes = [];
    Fixtures.People.each(function(person, index) {
      nicknames.push(person.nickname);
      indexes.push(index);
    });
    
    this.assertEqual(Fixtures.Nicknames.join(', '), 
      nicknames.join(', '));
    this.assertEqual('0, 1, 2, 3', indexes.join(', '));
  },
  
  testFindAll: function() {
    this.assertEqual(Fixtures.Primes.join(', '),
      Fixtures.Z.findAll(prime).join(', '));
  },
  
  testGrep: function() {
    this.assertEqual('noradio, htonl', 
      Fixtures.Nicknames.grep(/o/).join(", "));
      
    this.assertEqual('NORADIO, HTONL', 
      Fixtures.Nicknames.grep(/o/, function(nickname) {
        return nickname.toUpperCase();
      }).join(", "))

    this.assertEnumEqual($('grepHeader', 'grepCell'),
      $('grepTable', 'grepTBody', 'grepRow', 'grepHeader', 'grepCell').grep(new Selector('.cell')));
  },
  
  testInclude: function() {
    this.assert(Fixtures.Nicknames.include('sam-'));
    this.assert(Fixtures.Nicknames.include('noradio'));
    this.assert(!Fixtures.Nicknames.include('gmosx'));
    this.assert(Fixtures.Basic.include(2));
    this.assert(Fixtures.Basic.include('2'));
    this.assert(!Fixtures.Basic.include('4'));
  },
  
  testInGroupsOf: function() {
    this.assertEnumEqual([], [].inGroupsOf(3));
    
    var arr = [1, 2, 3, 4, 5, 6].inGroupsOf(3);
    this.assertEqual(2, arr.length);
    this.assertEnumEqual([1, 2, 3], arr[0]);
    this.assertEnumEqual([4, 5, 6], arr[1]);
    
    arr = [1, 2, 3, 4, 5, 6].inGroupsOf(4);
    this.assertEqual(2, arr.length);
    this.assertEnumEqual([1, 2, 3, 4], arr[0]);
    this.assertEnumEqual([5, 6, null, null], arr[1]);
    
    var basic = Fixtures.Basic
    
    arr = basic.inGroupsOf(4,'x');
    this.assertEqual(1, arr.length);
    this.assertEnumEqual([1, 2, 3, 'x'], arr[0]);
    
    this.assertEnumEqual([1,2,3,'a'], basic.inGroupsOf(2, 'a').flatten());

    arr = basic.inGroupsOf(5, '');
    this.assertEqual(1, arr.length);
    this.assertEnumEqual([1, 2, 3, '', ''], arr[0]);

    this.assertEnumEqual([1,2,3,0], basic.inGroupsOf(2, 0).flatten());
    this.assertEnumEqual([1,2,3,false], basic.inGroupsOf(2, false).flatten());
  },
  
  testInject: function() {
    this.assertEqual(1061, 
      Fixtures.Primes.inject(0, function(sum, value) {
        return sum + value;
      }));
  },
  
  testInvoke: function() {
    var result = [[2, 1, 3], [6, 5, 4]].invoke('sort');
    this.assertEqual(2, result.length);
    this.assertEqual('1, 2, 3', result[0].join(', '));
    this.assertEqual('4, 5, 6', result[1].join(', '));
    
    result = result.invoke('invoke', 'toString', 2);
    this.assertEqual('1, 10, 11', result[0].join(', '));
    this.assertEqual('100, 101, 110', result[1].join(', '));
  },
  
  testMax: function() {
    this.assertEqual(100, Fixtures.Z.max());
    this.assertEqual(97, Fixtures.Primes.max());
    this.assertEqual(2, [ -9, -8, -7, -6, -4, -3, -2,  0, -1,  2 ].max());
    this.assertEqual('sam-', Fixtures.Nicknames.max()); // ?s > ?U
  },
  
  testMin: function() {
    this.assertEqual(1, Fixtures.Z.min());
    this.assertEqual(0, [  1, 2, 3, 4, 5, 6, 7, 8, 0, 9 ].min());
    this.assertEqual('Ulysses', Fixtures.Nicknames.min()); // ?U < ?h
  },
  
  testPartition: function() {
    var result = Fixtures.People.partition(function(person) {
      return person.name.length < 15;
    }).invoke('pluck', 'nickname');
    
    this.assertEqual(2, result.length);
    this.assertEqual('sam-, htonl', result[0].join(', '));
    this.assertEqual('noradio, Ulysses', result[1].join(', '));
  },
  
  testPluck: function() {
    this.assertEqual(Fixtures.Nicknames.join(', '),
      Fixtures.People.pluck('nickname').join(', '));
  },
  
  testReject: function() {
    this.assertEqual(0, 
      Fixtures.Nicknames.reject(Prototype.K).length);
      
    this.assertEqual('sam-, noradio, htonl',
      Fixtures.Nicknames.reject(function(nickname) {
        return nickname != nickname.toLowerCase();
      }).join(', '));
  },
  
  testSortBy: function() {
    this.assertEqual('htonl, noradio, sam-, Ulysses',
      Fixtures.People.sortBy(function(value) {
        return value.nickname.toLowerCase();
      }).pluck('nickname').join(', '));
  },
  
  testToArray: function() {
    var result = Fixtures.People.toArray();
    this.assert(result != Fixtures.People); // they're different objects...
    this.assertEqual(Fixtures.Nicknames.join(', '),
      result.pluck('nickname').join(', ')); // but the values are the same
  },
  
  testZip: function() {
    var result = [1, 2, 3].zip([4, 5, 6], [7, 8, 9]);
    this.assertEqual('[[1, 4, 7], [2, 5, 8], [3, 6, 9]]', result.inspect());
    
    result = [1, 2, 3].zip([4, 5, 6], [7, 8, 9], function(array) { return array.reverse() });
    this.assertEqual('[[7, 4, 1], [8, 5, 2], [9, 6, 3]]', result.inspect());
  },
  
  testSize: function() {
    this.assertEqual(4, Fixtures.People.size());
    this.assertEqual(4, Fixtures.Nicknames.size());
    this.assertEqual(26, Fixtures.Primes.size());
    this.assertEqual(0, [].size());
  }
});