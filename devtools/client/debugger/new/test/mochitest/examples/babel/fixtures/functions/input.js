export default function root() {

  function decl(p1) {
    const inner = function inner(p2) {
      const arrow = (p3) => {
        console.log("pause here", p3, arrow, p2, inner, p1, decl, root);
      };
      arrow();
    };
    inner();
  }

  decl();
}
