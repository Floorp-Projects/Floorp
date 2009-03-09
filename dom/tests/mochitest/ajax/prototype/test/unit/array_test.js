var globalArgsTest = 'nothing to see here';

new Test.Unit.Runner({
  test$A: function(){
    this.assertEnumEqual([], $A({}));
  },
  
  testToArrayOnArguments: function(){
    function toArrayOnArguments(){
      globalArgsTest = $A(arguments);
    }
    toArrayOnArguments();
    this.assertEnumEqual([], globalArgsTest);
    toArrayOnArguments('foo');
    this.assertEnumEqual(['foo'], globalArgsTest);
    toArrayOnArguments('foo','bar');
    this.assertEnumEqual(['foo','bar'], globalArgsTest);
  },
  
  testToArrayOnNodeList: function(){
    // direct HTML
    this.assertEqual(3, $A($('test_node').childNodes).length);
    
    // DOM
    var element = document.createElement('div');
    element.appendChild(document.createTextNode('22'));
    (2).times(function(){ element.appendChild(document.createElement('span')) });
    this.assertEqual(3, $A(element.childNodes).length);
    
    // HTML String
    element = document.createElement('div');
    $(element).update('22<span></span><span></span');
    this.assertEqual(3, $A(element.childNodes).length);
  },
  
  testClear: function(){
    this.assertEnumEqual([], [].clear());
    this.assertEnumEqual([], [1].clear());
    this.assertEnumEqual([], [1,2].clear());
  },
  
  testClone: function(){
    this.assertEnumEqual([], [].clone());
    this.assertEnumEqual([1], [1].clone());
    this.assertEnumEqual([1,2], [1,2].clone());
    this.assertEnumEqual([0,1,2], [0,1,2].clone());
    var a = [0,1,2];
    var b = a;
    this.assertIdentical(a, b);
    b = a.clone();
    this.assertNotIdentical(a, b);
  },
  
  testFirst: function(){
    this.assertUndefined([].first());
    this.assertEqual(1, [1].first());
    this.assertEqual(1, [1,2].first());
  },
  
  testLast: function(){
    this.assertUndefined([].last());
    this.assertEqual(1, [1].last());
    this.assertEqual(2, [1,2].last());
  },
  
  testCompact: function(){
    this.assertEnumEqual([],      [].compact());
    this.assertEnumEqual([1,2,3], [1,2,3].compact());
    this.assertEnumEqual([0,1,2,3], [0,null,1,2,undefined,3].compact());
    this.assertEnumEqual([1,2,3], [null,1,2,3,null].compact());
  },
  
  testFlatten: function(){
    this.assertEnumEqual([],      [].flatten());
    this.assertEnumEqual([1,2,3], [1,2,3].flatten());
    this.assertEnumEqual([1,2,3], [1,[[[2,3]]]].flatten());
    this.assertEnumEqual([1,2,3], [[1],[2],[3]].flatten());
    this.assertEnumEqual([1,2,3], [[[[[[[1]]]]]],2,3].flatten());
  },
  
  testIndexOf: function(){
    this.assertEqual(-1, [].indexOf(1));
    this.assertEqual(-1, [0].indexOf(1));
    this.assertEqual(0, [1].indexOf(1));
    this.assertEqual(1, [0,1,2].indexOf(1));
    this.assertEqual(0, [1,2,1].indexOf(1));
    this.assertEqual(2, [1,2,1].indexOf(1, -1));
    this.assertEqual(1, [undefined,null].indexOf(null));
  },

  testLastIndexOf: function(){
    this.assertEqual(-1,[].lastIndexOf(1));
    this.assertEqual(-1, [0].lastIndexOf(1));
    this.assertEqual(0, [1].lastIndexOf(1));
    this.assertEqual(2, [0,2,4,6].lastIndexOf(4));
    this.assertEqual(3, [4,4,2,4,6].lastIndexOf(4));
    this.assertEqual(3, [0,2,4,6].lastIndexOf(6,3));
    this.assertEqual(-1, [0,2,4,6].lastIndexOf(6,2));
    this.assertEqual(0, [6,2,4,6].lastIndexOf(6,2));
    
    var fixture = [1,2,3,4,3];
    this.assertEqual(4, fixture.lastIndexOf(3));
    this.assertEnumEqual([1,2,3,4,3],fixture);
    
    //tests from http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:lastIndexOf
    var array = [2, 5, 9, 2];
    this.assertEqual(3,array.lastIndexOf(2));
    this.assertEqual(-1,array.lastIndexOf(7));
    this.assertEqual(3,array.lastIndexOf(2,3));
    this.assertEqual(0,array.lastIndexOf(2,2));
    this.assertEqual(0,array.lastIndexOf(2,-2));
    this.assertEqual(3,array.lastIndexOf(2,-1));
  },
  
  testInspect: function(){
    this.assertEqual('[]',[].inspect());
    this.assertEqual('[1]',[1].inspect());
    this.assertEqual('[\'a\']',['a'].inspect());
    this.assertEqual('[\'a\', 1]',['a',1].inspect());
  },
  
  testIntersect: function(){
    this.assertEnumEqual([1,3], [1,1,3,5].intersect([1,2,3]));
    this.assertEnumEqual([1], [1,1].intersect([1,1]));
    this.assertEnumEqual([0], [0,2].intersect([1,0]));
    this.assertEnumEqual([], [1,1,3,5].intersect([4]));
    this.assertEnumEqual([], [1].intersect(['1']));
    
    this.assertEnumEqual(
      ['B','C','D'], 
      $R('A','Z').toArray().intersect($R('B','D').toArray())
    );
  },
  
  testToJSON: function(){
    this.assertEqual('[]', [].toJSON());
    this.assertEqual('[\"a\"]', ['a'].toJSON());
    this.assertEqual('[\"a\", 1]', ['a', 1].toJSON());
    this.assertEqual('[\"a\", {\"b\": null}]', ['a', {'b': null}].toJSON());
  },
      
  testReduce: function(){
    this.assertUndefined([].reduce());
    this.assertNull([null].reduce());
    this.assertEqual(1, [1].reduce());
    this.assertEnumEqual([1,2,3], [1,2,3].reduce());
    this.assertEnumEqual([1,null,3], [1,null,3].reduce());
  },
  
  testReverse: function(){
    this.assertEnumEqual([], [].reverse());
    this.assertEnumEqual([1], [1].reverse());
    this.assertEnumEqual([2,1], [1,2].reverse());
    this.assertEnumEqual([3,2,1], [1,2,3].reverse());
  },
  
  testSize: function(){
    this.assertEqual(4, [0, 1, 2, 3].size());
    this.assertEqual(0, [].size());
  },

  testUniq: function(){
    this.assertEnumEqual([1], [1, 1, 1].uniq());
    this.assertEnumEqual([1], [1].uniq());
    this.assertEnumEqual([], [].uniq());
    this.assertEnumEqual([0, 1, 2, 3], [0, 1, 2, 2, 3, 0, 2].uniq());
    this.assertEnumEqual([0, 1, 2, 3], [0, 0, 1, 1, 2, 3, 3, 3].uniq(true));
  },
  
  testWithout: function(){
    this.assertEnumEqual([], [].without(0));
    this.assertEnumEqual([], [0].without(0));
    this.assertEnumEqual([1], [0,1].without(0));
    this.assertEnumEqual([1,2], [0,1,2].without(0));
  },
  
  test$w: function(){
    this.assertEnumEqual(['a', 'b', 'c', 'd'], $w('a b c d'));
    this.assertEnumEqual([], $w(' '));
    this.assertEnumEqual([], $w(''));
    this.assertEnumEqual([], $w(null));
    this.assertEnumEqual([], $w(undefined));
    this.assertEnumEqual([], $w());
    this.assertEnumEqual([], $w(10));
    this.assertEnumEqual(['a'], $w('a'));
    this.assertEnumEqual(['a'], $w('a '));
    this.assertEnumEqual(['a'], $w(' a'));
    this.assertEnumEqual(['a', 'b', 'c', 'd'], $w(' a   b\nc\t\nd\n'));
  }
});