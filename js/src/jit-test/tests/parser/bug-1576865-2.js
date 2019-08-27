var sourceText = `
  function Outer() {
    function LazyFunction() {
      // Lots of inner functions.
      function F00() { }
      function F01() { }
      function F02() { }
      function F03() { }
      function F04() { }
      function F05() { }
      function F06() { }
      function F07() { }
      function F08() { }
      function F09() { }
      function F10() { }
      function F11() { }
      function F12() { }
      function F13() { }
      function F14() { }
      function F15() { }
      function F16() { }
      function F17() { }
      function F18() { }
      function F19() { }
      function F20() { }
      function F21() { }
      function F22() { }
      function F23() { }
      function F24() { }
      function F25() { }
      function F26() { }
      function F27() { }
      function F28() { }
      function F29() { }
      function F30() { }
      function F31() { }
    }
  }
`;

oomTest(function() {
  evaluate(sourceText);
  });
