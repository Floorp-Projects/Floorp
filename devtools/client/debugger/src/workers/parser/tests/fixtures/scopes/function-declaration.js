export {};

function outer(p1) {}

{
  function middle(p2) {
    function inner(p3) {}

    console.log(this);
  }
}
