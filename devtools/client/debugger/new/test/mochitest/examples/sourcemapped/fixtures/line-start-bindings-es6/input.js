
export default function root() {
  function aFunc(){
    // Since these bindings are on their own linem, the mappings will
    // extend to the start of the line, rather than starting at the first
    // character of the binding.
    const {
      one,
      two
    } = {
      one: 1,
      two: 2,
    };

    // The 'this' binding here is also its own line, so the comment above
    // applies here too.
    this.thing = 4;

    console.log("pause here", root);
  }

  aFunc.call({ });
}
