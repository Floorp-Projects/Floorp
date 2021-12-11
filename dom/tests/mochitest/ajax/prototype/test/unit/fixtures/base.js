var Person = function(name){
    this.name = name;
};

Person.prototype.toJSON = function() {
  return '-' + this.name;
};

var arg1 = 1;
var arg2 = 2;
var arg3 = 3;
function TestObj() { };
TestObj.prototype.assertingEventHandler = 
  function(event, assertEvent, assert1, assert2, assert3, a1, a2, a3) {
    assertEvent(event);
    assert1(a1);
    assert2(a2);
    assert3(a3);
  };
  
var globalBindTest = null;


// base class
var Animal = Class.create({
  initialize: function(name) {
    this.name = name;
  },
  name: "",
  eat: function() {
    return this.say("Yum!");
  },
  say: function(message) {
    return this.name + ": " + message;
  }
});

// subclass that augments a method
var Cat = Class.create(Animal, {
  eat: function($super, food) {
    if (food instanceof Mouse) return $super();
    else return this.say("Yuk! I only eat mice.");
  }
});

// empty subclass
var Mouse = Class.create(Animal, {});

//mixins 
var Sellable = {
  getValue: function(pricePerKilo) {
    return this.weight * pricePerKilo;
  },
  
  inspect: function() {
    return '#<Sellable: #{weight}kg>'.interpolate(this);
  }
};

var Reproduceable = {
  reproduce: function(partner) {
    if (partner.constructor != this.constructor || partner.sex == this.sex)
      return null;
    var weight = this.weight / 10, sex = Math.random(1).round() ? 'male' : 'female';
    return new this.constructor('baby', weight, sex);
  }
};

// base class with mixin
var Plant = Class.create(Sellable, {
  initialize: function(name, weight) {
    this.name = name;
    this.weight = weight;
  },

  inspect: function() {
    return '#<Plant: #{name}>'.interpolate(this);
  }
});

// subclass with mixin
var Dog = Class.create(Animal, Reproduceable, {
  initialize: function($super, name, weight, sex) {
    this.weight = weight;
    this.sex = sex;
    $super(name);
  }
});

// subclass with mixins
var Ox = Class.create(Animal, Sellable, Reproduceable, {
  initialize: function($super, name, weight, sex) {
    this.weight = weight;
    this.sex = sex;
    $super(name);
  },
  
  eat: function(food) {
    if (food instanceof Plant)
      this.weight += food.weight;
  },
  
  inspect: function() {
    return '#<Ox: #{name}>'.interpolate(this);
  }
});