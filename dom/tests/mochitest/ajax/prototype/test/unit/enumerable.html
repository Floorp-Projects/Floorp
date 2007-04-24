<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
  <title>Prototype Unit test file</title>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
  <script src="../../dist/prototype.js" type="text/javascript"></script>
  <script src="../lib/unittest.js" type="text/javascript"></script>
  <link rel="stylesheet" href="../test.css" type="text/css" />
  <style type="text/css" media="screen">
  /* <![CDATA[ */
    #testcss1 { font-size:11px; color: #f00; }
    #testcss2 { font-size:12px; color: #0f0; display: none; }
  /* ]]> */
  </style>
</head>
<body>
<h1>Prototype Unit test file</h1>
<p>
  Test of utility functions in enumerable.js
</p>

<!-- Log output -->
<div id="testlog"> </div>

<!-- Tests follow -->
<script type="text/javascript" language="javascript" charset="utf-8">
// <![CDATA[
  var Fixtures = {
    People: [
      {name: 'Sam Stephenson',    nickname: 'sam-'},
      {name: 'Marcel Molina Jr.', nickname: 'noradio'},
      {name: 'Scott Barron',      nickname: 'htonl'},
      {name: 'Nicholas Seckar',   nickname: 'Ulysses'}
    ],
    
    Nicknames: $w('sam- noradio htonl Ulysses'),
    
    Primes: [
       1,  2,  3,  5,  7,  11, 13, 17, 19, 23,
      29, 31, 37, 41, 43,  47, 53, 59, 61, 67,
      71, 73, 79, 83, 89,  97
    ],
    
    Z: []
  };
  
  for (var i = 1; i <= 100; i++) 
    Fixtures.Z.push(i);    

  function prime(value) {
    for (var i = 2; i < value; i++)
      if (value % i == 0) return false;
    return true;
  }

  new Test.Unit.Runner({    
    testEachBreak: function() {with(this) {
      var result = 0;
      [1, 2, 3].each(function(value) {
        if ((result = value) == 2) throw $break;
      });
      
      assertEqual(2, result);
    }},
    
    testEachReturnActsAsContinue: function() {with(this) {
      var results = [];
      [1, 2, 3].each(function(value) {
        if (value == 2) return;
        results.push(value);
      });
      
      assertEqual('1, 3', results.join(', '));
    }},
    
    testEachChaining: function() {with(this) {
      assertEqual(Fixtures.Primes, Fixtures.Primes.each(Prototype.emptyFunction));
      assertEqual(3, [1, 2, 3].each(Prototype.emptyFunction).length);
    }},

    testAny: function() {with(this) {
      assert(!([].any()));
      
      assert([true, true, true].any());
      assert([true, false, false].any());
      assert(![false, false, false].any());
      
      assert([1, 2, 3, 4, 5].any(function(value) {
        return value > 3;
      }));
      assert(![1, 2, 3, 4, 5].any(function(value) {
        return value > 10;
      }));
    }},
    
    testAll: function() {with(this) {
      assert([].all());
      
      assert([true, true, true].all());
      assert(![true, false, false].all());
      assert(![false, false, false].all());

      assert([1, 2, 3, 4, 5].all(function(value) {
        return value > 0;
      }));
      assert(![1, 2, 3, 4, 5].all(function(value) {
        return value > 3;
      }));
    }},
    
    testCollect: function() {with(this) {
      assertEqual(Fixtures.Nicknames.join(', '), 
        Fixtures.People.collect(function(person) {
          return person.nickname;
        }).join(", "));
      
      assertEqual(26,  Fixtures.Primes.map().length);
    }},
    
    testDetect: function() {with(this) {
      assertEqual('Marcel Molina Jr.', 
        Fixtures.People.detect(function(person) {
          return person.nickname.match(/no/);
        }).name);
    }},
    
    testEachSlice: function() {with(this) {
      assertEnumEqual([], [].eachSlice(2));
      assertEqual(1, [1].eachSlice(1).length);
      assertEnumEqual([1], [1].eachSlice(1)[0]);
      assertEqual(2, [1,2,3].eachSlice(2).length);
      assertEnumEqual(
        [3, 2, 1, 11, 7, 5, 19, 17, 13, 31, 29, 23, 43, 41, 37, 59, 53, 47, 71, 67, 61, 83, 79, 73, 97, 89],
        Fixtures.Primes.eachSlice( 3, function(slice){ return slice.reverse() }).flatten()
      );
    }},
    
    testEachWithIndex: function() {with(this) {
      var nicknames = [], indexes = [];
      Fixtures.People.each(function(person, index) {
        nicknames.push(person.nickname);
        indexes.push(index);
      });
      
      assertEqual(Fixtures.Nicknames.join(', '), 
        nicknames.join(', '));
      assertEqual('0, 1, 2, 3', indexes.join(', '));
    }},
    
    testFindAll: function() {with(this) {
      assertEqual(Fixtures.Primes.join(', '),
        Fixtures.Z.findAll(prime).join(', '));
    }},
    
    testGrep: function() {with(this) {
      assertEqual('noradio, htonl', 
        Fixtures.Nicknames.grep(/o/).join(", "));
        
      assertEqual('NORADIO, HTONL', 
        Fixtures.Nicknames.grep(/o/, function(nickname) {
          return nickname.toUpperCase();
        }).join(", "))
    }},
    
    testInclude: function() {with(this) {
      assert(Fixtures.Nicknames.include('sam-'));
      assert(Fixtures.Nicknames.include('noradio'));
      assert(Fixtures.Nicknames.include('htonl'));
      assert(Fixtures.Nicknames.include('Ulysses'));
      assert(!Fixtures.Nicknames.include('gmosx'));
    }},
    
    testInGroupsOf: function() { with(this) {
      assertEnumEqual([], [].inGroupsOf(3));
      
      var arr = [1, 2, 3, 4, 5, 6].inGroupsOf(3);
      assertEqual(2, arr.length);
      assertEnumEqual([1, 2, 3], arr[0]);
      assertEnumEqual([4, 5, 6], arr[1]);
      
      arr = [1, 2, 3, 4, 5, 6].inGroupsOf(4);
      assertEqual(2, arr.length);
      assertEnumEqual([1, 2, 3, 4], arr[0]);
      assertEnumEqual([5, 6, null, null], arr[1]);
      
      arr = [1, 2, 3].inGroupsOf(4,'x');
      assertEqual(1, arr.length);
      assertEnumEqual([1, 2, 3, 'x'], arr[0]);
      
      assertEnumEqual([1,2,3,'a'], [1,2,3].inGroupsOf(2, 'a').flatten());

      arr = [1, 2, 3].inGroupsOf(5, '');
      assertEqual(1, arr.length);
      assertEnumEqual([1, 2, 3, '', ''], arr[0]);

      assertEnumEqual([1,2,3,0], [1,2,3].inGroupsOf(2, 0).flatten());
      assertEnumEqual([1,2,3,false], [1,2,3].inGroupsOf(2, false).flatten());
    }},
    
    testInject: function() {with(this) {
      assertEqual(1061, 
        Fixtures.Primes.inject(0, function(sum, value) {
          return sum + value;
        }));
    }},
    
    testInvoke: function() {with(this) {
      var result = [[2, 1, 3], [6, 5, 4]].invoke('sort');
      assertEqual(2, result.length);
      assertEqual('1, 2, 3', result[0].join(', '));
      assertEqual('4, 5, 6', result[1].join(', '));
      
      result = result.invoke('invoke', 'toString', 2);
      assertEqual('1, 10, 11', result[0].join(', '));
      assertEqual('100, 101, 110', result[1].join(', '));
    }},
    
    testMax: function() {with(this) {
      assertEqual(100, Fixtures.Z.max());
      assertEqual(97, Fixtures.Primes.max());
      assertEqual(2, [ -9, -8, -7, -6, -4, -3, -2,  0, -1,  2 ].max());
      assertEqual('sam-', Fixtures.Nicknames.max()); // ?s > ?U
    }},
    
    testMin: function() {with(this) {
      assertEqual(1, Fixtures.Z.min());
      assertEqual(0, [  1, 2, 3, 4, 5, 6, 7, 8, 0, 9 ].min());
      assertEqual('Ulysses', Fixtures.Nicknames.min()); // ?U < ?h
    }},
    
    testPartition: function() {with(this) {
      var result = Fixtures.People.partition(function(person) {
        return person.name.length < 15;
      }).invoke('pluck', 'nickname');
      
      assertEqual(2, result.length);
      assertEqual('sam-, htonl', result[0].join(', '));
      assertEqual('noradio, Ulysses', result[1].join(', '));
    }},
    
    testPluck: function() {with(this) {
      assertEqual(Fixtures.Nicknames.join(', '),
        Fixtures.People.pluck('nickname').join(', '));
    }},
    
    testReject: function() {with(this) {
      assertEqual(0, 
        Fixtures.Nicknames.reject(Prototype.K).length);
        
      assertEqual('sam-, noradio, htonl',
        Fixtures.Nicknames.reject(function(nickname) {
          return nickname != nickname.toLowerCase();
        }).join(', '));
    }},
    
    testSortBy: function() {with(this) {
      assertEqual('htonl, noradio, sam-, Ulysses',
        Fixtures.People.sortBy(function(value) {
          return value.nickname.toLowerCase();
        }).pluck('nickname').join(', '));
    }},
    
    testToArray: function() {with(this) {
      var result = Fixtures.People.toArray();
      assert(result != Fixtures.People); // they're different objects...
      assertEqual(Fixtures.Nicknames.join(', '),
        result.pluck('nickname').join(', ')); // but the values are the same
    }},
    
    testZip: function() {with(this) {
      var result = [1, 2, 3].zip([4, 5, 6], [7, 8, 9]);
      assertEqual('[[1, 4, 7], [2, 5, 8], [3, 6, 9]]', result.inspect());
      
      result = [1, 2, 3].zip([4, 5, 6], [7, 8, 9], function(array) { return array.reverse() });
      assertEqual('[[7, 4, 1], [8, 5, 2], [9, 6, 3]]', result.inspect());
    }},
    
    testSize: function() {with(this) {
      assertEqual(4, Fixtures.People.size());
      assertEqual(4, Fixtures.Nicknames.size());
      assertEqual(26, Fixtures.Primes.size());
      assertEqual(0, [].size());
    }}
  }, 'testlog');
// ]]>
</script>
</body>
</html>