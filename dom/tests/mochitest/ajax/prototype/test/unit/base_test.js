new Test.Unit.Runner({
  testFunctionArgumentNames: function() {
    this.assertEnumEqual([], (function() {}).argumentNames());
    this.assertEnumEqual(["one"], (function(one) {}).argumentNames());
    this.assertEnumEqual(["one", "two", "three"], (function(one, two, three) {}).argumentNames());
    this.assertEnumEqual(["one", "two", "three"], (function(  one  , two 
       , three   ) {}).argumentNames());
    this.assertEqual("$super", (function($super) {}).argumentNames().first());
    
    function named1() {};
    this.assertEnumEqual([], named1.argumentNames());
    function named2(one) {};
    this.assertEnumEqual(["one"], named2.argumentNames());
    function named3(one, two, three) {};
    this.assertEnumEqual(["one", "two", "three"], named3.argumentNames());
  },
  
  testFunctionBind: function() {
    function methodWithoutArguments() { return this.hi };
    function methodWithArguments()    { return this.hi + ',' + $A(arguments).join(',') };
    var func = Prototype.emptyFunction;

    this.assertIdentical(func, func.bind());
    this.assertIdentical(func, func.bind(undefined));
    this.assertNotIdentical(func, func.bind(null));

    this.assertEqual('without', methodWithoutArguments.bind({ hi: 'without' })());
    this.assertEqual('with,arg1,arg2', methodWithArguments.bind({ hi: 'with' })('arg1','arg2'));
    this.assertEqual('withBindArgs,arg1,arg2',
      methodWithArguments.bind({ hi: 'withBindArgs' }, 'arg1', 'arg2')());
    this.assertEqual('withBindArgsAndArgs,arg1,arg2,arg3,arg4',
      methodWithArguments.bind({ hi: 'withBindArgsAndArgs' }, 'arg1', 'arg2')('arg3', 'arg4'));
  },
  
  testFunctionCurry: function() {
    var split = function(delimiter, string) { return string.split(delimiter); };
    var splitOnColons = split.curry(":");
    this.assertNotIdentical(split, splitOnColons);
    this.assertEnumEqual(split(":", "0:1:2:3:4:5"), splitOnColons("0:1:2:3:4:5"));
    this.assertIdentical(split, split.curry());
  },
  
  testFunctionDelay: function() {
    window.delayed = undefined;
    var delayedFunction = function() { window.delayed = true; };
    var delayedFunctionWithArgs = function() { window.delayedWithArgs = $A(arguments).join(' '); };
    delayedFunction.delay(0.8);
    delayedFunctionWithArgs.delay(0.8, 'hello', 'world');
    this.assertUndefined(window.delayed);
    this.wait(1000, function() {
      this.assert(window.delayed);
      this.assertEqual('hello world', window.delayedWithArgs);
    });
  },
  
  testFunctionWrap: function() {
    function sayHello(){
      return 'hello world';
    };
    
    this.assertEqual('HELLO WORLD', sayHello.wrap(function(proceed) {
      return proceed().toUpperCase();
    })());
    
    var temp = String.prototype.capitalize;
    String.prototype.capitalize = String.prototype.capitalize.wrap(function(proceed, eachWord) {
      if (eachWord && this.include(' ')) return this.split(' ').map(function(str){
        return str.capitalize();
      }).join(' ');
      return proceed();
    });
    this.assertEqual('Hello world', 'hello world'.capitalize());
    this.assertEqual('Hello World', 'hello world'.capitalize(true));
    this.assertEqual('Hello', 'hello'.capitalize());
    String.prototype.capitalize = temp;
  },
  
  testFunctionDefer: function() {
    window.deferred = undefined;
    var deferredFunction = function() { window.deferred = true; };
    deferredFunction.defer();
    this.assertUndefined(window.deferred);      
    this.wait(50, function() {
      this.assert(window.deferred);
      
      window.deferredValue = 0;
      var deferredFunction2 = function(arg) { window.deferredValue = arg; };
      deferredFunction2.defer('test');
      this.wait(50, function() {
        this.assertEqual('test', window.deferredValue);
      });
    });
  },
  
  testFunctionMethodize: function() {
    var Foo = { bar: function(baz) { return baz } };
    var baz = { quux: Foo.bar.methodize() };
    
    this.assertEqual(Foo.bar.methodize(), baz.quux);
    this.assertEqual(baz, Foo.bar(baz));
    this.assertEqual(baz, baz.quux());
  },

  testObjectExtend: function() {
    var object = {foo: 'foo', bar: [1, 2, 3]};
    this.assertIdentical(object, Object.extend(object));
    this.assertHashEqual({foo: 'foo', bar: [1, 2, 3]}, object);
    this.assertIdentical(object, Object.extend(object, {bla: 123}));
    this.assertHashEqual({foo: 'foo', bar: [1, 2, 3], bla: 123}, object);
    this.assertHashEqual({foo: 'foo', bar: [1, 2, 3], bla: null},
      Object.extend(object, {bla: null}));
  },
  
  testObjectToQueryString: function() {
    this.assertEqual('a=A&b=B&c=C&d=D%23', Object.toQueryString({a: 'A', b: 'B', c: 'C', d: 'D#'}));
  },
  
  testObjectClone: function() {
    var object = {foo: 'foo', bar: [1, 2, 3]};
    this.assertNotIdentical(object, Object.clone(object));
    this.assertHashEqual(object, Object.clone(object));
    this.assertHashEqual({}, Object.clone());
    var clone = Object.clone(object);
    delete clone.bar;
    this.assertHashEqual({foo: 'foo'}, clone, 
      "Optimizing Object.clone perf using prototyping doesn't allow properties to be deleted.");
  },

  testObjectInspect: function() {
    this.assertEqual('undefined', Object.inspect());
    this.assertEqual('undefined', Object.inspect(undefined));
    this.assertEqual('null', Object.inspect(null));
    this.assertEqual("'foo\\\\b\\\'ar'", Object.inspect('foo\\b\'ar'));
    this.assertEqual('[]', Object.inspect([]));
    this.assertNothingRaised(function() { Object.inspect(window.Node) });
  },
  
  testObjectToJSON: function() {
    this.assertUndefined(Object.toJSON(undefined));
    this.assertUndefined(Object.toJSON(Prototype.K));
    this.assertEqual('\"\"', Object.toJSON(''));
    this.assertEqual('[]', Object.toJSON([]));
    this.assertEqual('[\"a\"]', Object.toJSON(['a']));
    this.assertEqual('[\"a\", 1]', Object.toJSON(['a', 1]));
    this.assertEqual('[\"a\", {\"b\": null}]', Object.toJSON(['a', {'b': null}]));
    this.assertEqual('{\"a\": \"hello!\"}', Object.toJSON({a: 'hello!'}));
    this.assertEqual('{}', Object.toJSON({}));
    this.assertEqual('{}', Object.toJSON({a: undefined, b: undefined, c: Prototype.K}));
    this.assertEqual('{\"b\": [false, true], \"c\": {\"a\": \"hello!\"}}',
      Object.toJSON({'b': [undefined, false, true, undefined], c: {a: 'hello!'}}));
    this.assertEqual('{\"b\": [false, true], \"c\": {\"a\": \"hello!\"}}',
      Object.toJSON($H({'b': [undefined, false, true, undefined], c: {a: 'hello!'}})));
    this.assertEqual('true', Object.toJSON(true));
    this.assertEqual('false', Object.toJSON(false));
    this.assertEqual('null', Object.toJSON(null));
    var sam = new Person('sam');
    this.assertEqual('-sam', Object.toJSON(sam));
    this.assertEqual('-sam', sam.toJSON());
    var element = $('test');
    this.assertUndefined(Object.toJSON(element));
    element.toJSON = function(){return 'I\'m a div with id test'};
    this.assertEqual('I\'m a div with id test', Object.toJSON(element));
  },
  
  testObjectToHTML: function() {
    this.assertIdentical('', Object.toHTML());
    this.assertIdentical('', Object.toHTML(''));
    this.assertIdentical('', Object.toHTML(null));
    this.assertIdentical('0', Object.toHTML(0));
    this.assertIdentical('123', Object.toHTML(123));
    this.assertEqual('hello world', Object.toHTML('hello world'));
    this.assertEqual('hello world', Object.toHTML({toHTML: function() { return 'hello world' }}));
  },
  
  testObjectIsArray: function() {
    this.assert(Object.isArray([]));
    this.assert(Object.isArray([0]));
    this.assert(Object.isArray([0, 1]));
    this.assert(!Object.isArray({}));
    this.assert(!Object.isArray($('list').childNodes));
    this.assert(!Object.isArray());
    this.assert(!Object.isArray(''));
    this.assert(!Object.isArray('foo'));
    this.assert(!Object.isArray(0));
    this.assert(!Object.isArray(1));
    this.assert(!Object.isArray(null));
    this.assert(!Object.isArray(true));
    this.assert(!Object.isArray(false));
    this.assert(!Object.isArray(undefined));
  },
  
  testObjectIsHash: function() {
    this.assert(Object.isHash($H()));
    this.assert(Object.isHash(new Hash()));
    this.assert(!Object.isHash({}));
    this.assert(!Object.isHash(null));
    this.assert(!Object.isHash());
    this.assert(!Object.isHash(''));
    this.assert(!Object.isHash(2));
    this.assert(!Object.isHash(false));
    this.assert(!Object.isHash(true));
    this.assert(!Object.isHash([]));
  },
  
  testObjectIsElement: function() {
    this.assert(Object.isElement(document.createElement('div')));
    this.assert(Object.isElement(new Element('div')));
    this.assert(Object.isElement($('testlog')));
    this.assert(!Object.isElement(document.createTextNode('bla')));

    // falsy variables should not mess up return value type
    this.assertIdentical(false, Object.isElement(0));
    this.assertIdentical(false, Object.isElement(''));
    this.assertIdentical(false, Object.isElement(NaN));
    this.assertIdentical(false, Object.isElement(null));
    this.assertIdentical(false, Object.isElement(undefined));
  },
  
  testObjectIsFunction: function() {
    this.assert(Object.isFunction(function() { }));
    this.assert(Object.isFunction(Class.create()));
    this.assert(!Object.isFunction("a string"));
    this.assert(!Object.isFunction($("testlog")));
    this.assert(!Object.isFunction([]));
    this.assert(!Object.isFunction({}));
    this.assert(!Object.isFunction(0));
    this.assert(!Object.isFunction(false));
    this.assert(!Object.isFunction(undefined));
    this.assert(!Object.isFunction(/foo/));
    this.assert(!Object.isFunction(document.getElementsByTagName('div')));
  },
  
  testObjectIsString: function() {
    this.assert(!Object.isString(function() { }));
    this.assert(Object.isString("a string"));
    this.assert(!Object.isString(0));
    this.assert(!Object.isString([]));
    this.assert(!Object.isString({}));
    this.assert(!Object.isString(false));
    this.assert(!Object.isString(undefined));
  },
  
  testObjectIsNumber: function() {
    this.assert(Object.isNumber(0));
    this.assert(Object.isNumber(1.0));
    this.assert(!Object.isNumber(function() { }));
    this.assert(!Object.isNumber("a string"));
    this.assert(!Object.isNumber([]));
    this.assert(!Object.isNumber({}));
    this.assert(!Object.isNumber(false));
    this.assert(!Object.isNumber(undefined));
  },
  
  testObjectIsUndefined: function() {
    this.assert(Object.isUndefined(undefined));
    this.assert(!Object.isUndefined(null));
    this.assert(!Object.isUndefined(false));
    this.assert(!Object.isUndefined(0));
    this.assert(!Object.isUndefined(""));
    this.assert(!Object.isUndefined(function() { }));
    this.assert(!Object.isUndefined([]));
    this.assert(!Object.isUndefined({}));
  },
  
  // sanity check
  testDoesntExtendObjectPrototype: function() {
    // for-in is supported with objects
    var iterations = 0, obj = { a: 1, b: 2, c: 3 };
    for (property in obj) iterations++;
    this.assertEqual(3, iterations);
    
    // for-in is not supported with arrays
    iterations = 0;
    var arr = [1,2,3];
    for (property in arr) iterations++;
    this.assert(iterations > 3);
  },
  

  testBindAsEventListener: function() {
    for ( var i = 0; i < 10; ++i ) {
      var div = document.createElement('div');
      div.setAttribute('id','test-'+i);
      document.body.appendChild(div);
      var tobj = new TestObj();
      var eventTest = { test: true };
      var call = tobj.assertingEventHandler.bindAsEventListener(tobj,
        this.assertEqual.bind(this, eventTest),
        this.assertEqual.bind(this, arg1),
        this.assertEqual.bind(this, arg2),
        this.assertEqual.bind(this, arg3), arg1, arg2, arg3 );
      call(eventTest);
    }
  },
  
  testDateToJSON: function() {
    this.assertEqual('\"1970-01-01T00:00:00Z\"', new Date(Date.UTC(1970, 0, 1)).toJSON());
  },
  
  testRegExpEscape: function() {
    this.assertEqual('word', RegExp.escape('word'));
    this.assertEqual('\\/slashes\\/', RegExp.escape('/slashes/'));
    this.assertEqual('\\\\backslashes\\\\', RegExp.escape('\\backslashes\\'));
    this.assertEqual('\\\\border of word', RegExp.escape('\\border of word'));
    
    this.assertEqual('\\(\\?\\:non-capturing\\)', RegExp.escape('(?:non-capturing)'));
    this.assertEqual('non-capturing', new RegExp(RegExp.escape('(?:') + '([^)]+)').exec('(?:non-capturing)')[1]);
    
    this.assertEqual('\\(\\?\\=positive-lookahead\\)', RegExp.escape('(?=positive-lookahead)'));
    this.assertEqual('positive-lookahead', new RegExp(RegExp.escape('(?=') + '([^)]+)').exec('(?=positive-lookahead)')[1]);
    
    this.assertEqual('\\(\\?<\\=positive-lookbehind\\)', RegExp.escape('(?<=positive-lookbehind)'));
    this.assertEqual('positive-lookbehind', new RegExp(RegExp.escape('(?<=') + '([^)]+)').exec('(?<=positive-lookbehind)')[1]);
    
    this.assertEqual('\\(\\?\\!negative-lookahead\\)', RegExp.escape('(?!negative-lookahead)'));
    this.assertEqual('negative-lookahead', new RegExp(RegExp.escape('(?!') + '([^)]+)').exec('(?!negative-lookahead)')[1]);
    
    this.assertEqual('\\(\\?<\\!negative-lookbehind\\)', RegExp.escape('(?<!negative-lookbehind)'));
    this.assertEqual('negative-lookbehind', new RegExp(RegExp.escape('(?<!') + '([^)]+)').exec('(?<!negative-lookbehind)')[1]);
    
    this.assertEqual('\\[\\\\w\\]\\+', RegExp.escape('[\\w]+'));
    this.assertEqual('character class', new RegExp(RegExp.escape('[') + '([^\\]]+)').exec('[character class]')[1]);      
    
    this.assertEqual('<div>', new RegExp(RegExp.escape('<div>')).exec('<td><div></td>')[0]);      
    
    this.assertEqual('false', RegExp.escape(false));
    this.assertEqual('undefined', RegExp.escape());
    this.assertEqual('null', RegExp.escape(null));
    this.assertEqual('42', RegExp.escape(42));
    
    this.assertEqual('\\\\n\\\\r\\\\t', RegExp.escape('\\n\\r\\t'));
    this.assertEqual('\n\r\t', RegExp.escape('\n\r\t'));
    this.assertEqual('\\{5,2\\}', RegExp.escape('{5,2}'));
    
    this.assertEqual(
      '\\/\\(\\[\\.\\*\\+\\?\\^\\=\\!\\:\\$\\{\\}\\(\\)\\|\\[\\\\\\]\\\\\\\/\\\\\\\\\\]\\)\\/g',
      RegExp.escape('/([.*+?^=!:${}()|[\\]\\/\\\\])/g')
    );
  },
  
  testBrowserDetection: function() {
    var results = $H(Prototype.Browser).map(function(engine){
      return engine;
    }).partition(function(engine){
      return engine[1] === true
    });
    var trues = results[0], falses = results[1];
    
    this.info('User agent string is: ' + navigator.userAgent);
    
    this.assert(trues.size() == 0 || trues.size() == 1, 
      'There should be only one or no browser detected.');
    
    // we should have definite trues or falses here
    trues.each(function(result) {
      this.assert(result[1] === true);
    }, this);
    falses.each(function(result) {
      this.assert(result[1] === false);
    }, this);
    
    if (navigator.userAgent.indexOf('AppleWebKit/') > -1) {
      this.info('Running on WebKit');
      this.assert(Prototype.Browser.WebKit);
    }
    
    if (!!window.opera) {
      this.info('Running on Opera');
      this.assert(Prototype.Browser.Opera);
    }
    
    if (!!(window.attachEvent && !window.opera)) {
      this.info('Running on IE');
      this.assert(Prototype.Browser.IE);
    }
    
    if (navigator.userAgent.indexOf('Gecko') > -1 && navigator.userAgent.indexOf('KHTML') == -1) {
      this.info('Running on Gecko');
      this.assert(Prototype.Browser.Gecko);
    } 
  },
  
  testClassCreate: function() { 
    this.assert(Object.isFunction(Animal), 'Animal is not a constructor');
    this.assertEnumEqual([Cat, Mouse, Dog, Ox], Animal.subclasses);
    Animal.subclasses.each(function(subclass) {
      this.assertEqual(Animal, subclass.superclass);
    }, this);

    var Bird = Class.create(Animal);
    this.assertEqual(Bird, Animal.subclasses.last());
    // for..in loop (for some reason) doesn't iterate over the constructor property in top-level classes
    this.assertEnumEqual(Object.keys(new Animal).sort(), Object.keys(new Bird).without('constructor').sort());
  },

  testClassInstantiation: function() { 
    var pet = new Animal("Nibbles");
    this.assertEqual("Nibbles", pet.name, "property not initialized");
    this.assertEqual('Nibbles: Hi!', pet.say('Hi!'));
    this.assertEqual(Animal, pet.constructor, "bad constructor reference");
    this.assertUndefined(pet.superclass);

    var Empty = Class.create();
    this.assert('object', typeof new Empty);
  },

  testInheritance: function() {
    var tom = new Cat('Tom');
    this.assertEqual(Cat, tom.constructor, "bad constructor reference");
    this.assertEqual(Animal, tom.constructor.superclass, 'bad superclass reference');
    this.assertEqual('Tom', tom.name);
    this.assertEqual('Tom: meow', tom.say('meow'));
    this.assertEqual('Tom: Yuk! I only eat mice.', tom.eat(new Animal));
  },

  testSuperclassMethodCall: function() {
    var tom = new Cat('Tom');
    this.assertEqual('Tom: Yum!', tom.eat(new Mouse));

    // augment the constructor and test
    var Dodo = Class.create(Animal, {
      initialize: function($super, name) {
        $super(name);
        this.extinct = true;
      },
      
      say: function($super, message) {
        return $super(message) + " honk honk";
      }
    });

    var gonzo = new Dodo('Gonzo');
    this.assertEqual('Gonzo', gonzo.name);
    this.assert(gonzo.extinct, 'Dodo birds should be extinct');
    this.assertEqual("Gonzo: hello honk honk", gonzo.say("hello"));
  },

  testClassAddMethods: function() {
    var tom   = new Cat('Tom');
    var jerry = new Mouse('Jerry');
    
    Animal.addMethods({
      sleep: function() {
        return this.say('ZZZ');
      }
    });
    
    Mouse.addMethods({
      sleep: function($super) {
        return $super() + " ... no, can't sleep! Gotta steal cheese!";
      },
      escape: function(cat) {
        return this.say('(from a mousehole) Take that, ' + cat.name + '!');
      }
    });
    
    this.assertEqual('Tom: ZZZ', tom.sleep(), "added instance method not available to subclass");
    this.assertEqual("Jerry: ZZZ ... no, can't sleep! Gotta steal cheese!", jerry.sleep());
    this.assertEqual("Jerry: (from a mousehole) Take that, Tom!", jerry.escape(tom));
    // insure that a method has not propagated *up* the prototype chain:
    this.assertUndefined(tom.escape);
    this.assertUndefined(new Animal().escape);
    
    Animal.addMethods({
      sleep: function() {
        return this.say('zZzZ');
      }
    });
    
    this.assertEqual("Jerry: zZzZ ... no, can't sleep! Gotta steal cheese!", jerry.sleep());
  },
  
  testBaseClassWithMixin: function() {
    var grass = new Plant('grass', 3);
    this.assertRespondsTo('getValue', grass);      
    this.assertEqual('#<Plant: grass>', grass.inspect());
  },
  
  testSubclassWithMixin: function() {
    var snoopy = new Dog('Snoopy', 12, 'male');
    this.assertRespondsTo('reproduce', snoopy);      
  },
 
  testSubclassWithMixins: function() {
    var cow = new Ox('cow', 400, 'female');
    this.assertEqual('#<Ox: cow>', cow.inspect());
    this.assertRespondsTo('reproduce', cow);
    this.assertRespondsTo('getValue', cow);
  },
 
  testClassWithToStringAndValueOfMethods: function() {
    var Foo = Class.create({
      toString: function() { return "toString" },
      valueOf: function() { return "valueOf" }
    });
    
    var Parent = Class.create({
      m1: function(){ return 'm1' },
      m2: function(){ return 'm2' }
    });
    var Child = Class.create(Parent, {
      m1: function($super) { return 'm1 child' },
      m2: function($super) { return 'm2 child' }
    });
    
    this.assert(new Child().m1.toString().indexOf('m1 child') > -1);
    
    this.assertEqual("toString", new Foo().toString());
    this.assertEqual("valueOf", new Foo().valueOf());
  }
});