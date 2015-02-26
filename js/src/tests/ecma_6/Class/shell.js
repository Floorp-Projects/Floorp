// Enable "let" in shell builds. So silly.
if (typeof version != 'undefined')
  version(185);

function classesEnabled() {
    try {
        new Function("class Foo { constructor() { } }");
        return true;
    } catch (e if e instanceof SyntaxError) {
        return false;
    }
}
