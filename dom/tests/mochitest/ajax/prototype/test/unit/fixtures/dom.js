var testVar = 'to be updated', testVar2 = '', documentViewportProperties;

Element.addMethods({
  hashBrowns: function(element) { return 'hash browns'; }
});

Element.addMethods("LI", {
  pancakes: function(element) { return "pancakes"; }
});

Element.addMethods("DIV", {
  waffles: function(element) { return "waffles"; }
});

Element.addMethods($w("li div"), {
  orangeJuice: function(element) { return "orange juice"; }
});