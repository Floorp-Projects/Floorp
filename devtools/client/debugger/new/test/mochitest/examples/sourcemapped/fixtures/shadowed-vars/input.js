export default function() {
  var aVar = "var1";
  let aLet = "let1";
  const aConst = "const1";

  class Outer {}
  {
    function Outer() {}

    var aVar = "var2";
    let aLet = "let2";
    const aConst = "const2";
    {
      var aVar = "var3";
      let aLet = "let3";
      const aConst = "const3";

      console.log("pause here");
    }
  }
}
