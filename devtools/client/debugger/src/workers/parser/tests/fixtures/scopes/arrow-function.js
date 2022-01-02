export {};

let outer = (p1) => {
  console.log(this);

  (function() {
    var inner = (p2) => {
      console.log(this);
    };
  })();
};
