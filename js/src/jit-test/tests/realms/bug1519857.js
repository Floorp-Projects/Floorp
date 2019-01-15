// |jit-test| skip-if: !("dis" in this)
const g = newGlobal({sameCompartmentAs: this});
g.eval(`function f() { y(); }`);
dis(g.f);
