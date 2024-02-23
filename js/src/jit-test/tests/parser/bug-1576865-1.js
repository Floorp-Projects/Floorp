var sourceText = `
  function Outer() {
    var X00, X01, X02, X03, X04, X05, X06, X07;
    var X08, X09, X10, X11, X12, X13, X14, X15;
    var X16, X17, X18, X19, X20, X21, X22, X23;
    var X24, X25, X26, X27, X28, X29, X30, X31;

    function LazyFunction() {
      // Lots of closed-over bindings.
      { X00 = true; };
      { X01 = true; };
      { X02 = true; };
      { X03 = true; };
      { X04 = true; };
      { X05 = true; };
      { X06 = true; };
      { X07 = true; };
      { X08 = true; };
      { X09 = true; };
      { X10 = true; };
      { X11 = true; };
      { X12 = true; };
      { X13 = true; };
      { X14 = true; };
      { X15 = true; };
      { X16 = true; };
      { X17 = true; };
      { X18 = true; };
      { X19 = true; };
      { X20 = true; };
      { X21 = true; };
      { X22 = true; };
      { X23 = true; };
      { X24 = true; };
      { X25 = true; };
      { X26 = true; };
      { X27 = true; };
      { X28 = true; };
      { X29 = true; };
      { X30 = true; };
      { X31 = true; };
    }
  }
`;

oomTest(function() {
  evaluate(sourceText);
  });
