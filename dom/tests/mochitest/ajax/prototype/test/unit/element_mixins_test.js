new Test.Unit.Runner({
  testInput: function() {
    this.assert($("input").present != null);
    this.assert(typeof $("input").present == 'function');
    this.assert($("input").select != null);
    this.assertRespondsTo('present', Form.Element);
    this.assertRespondsTo('present', Form.Element.Methods);
    this.assertRespondsTo('coffee', $('input'));
    this.assertIdentical(Prototype.K, Form.Element.coffee);
    this.assertIdentical(Prototype.K, Form.Element.Methods.coffee);
  },
  
  testForm: function() {
    this.assert($("form").reset != null);
    this.assert($("form").getInputs().length == 2);
  },
  
  testEvent: function() {
    this.assert($("form").observe != null)
    // Can't really test this one with TestUnit...
    $('form').observe("submit", function(e) { 
      alert("yeah!"); 
      Event.stop(e); 
    });
  },
  
  testCollections: function() {
    this.assert($$("input").all(function(input) {
      return (input.focus != null);
    }));
  }
});