load(libdir + "asserts.js");

class C {
  #f = 1;
  static test() {
    assertTypeErrorMessage(
      () => [].#f,
      "can't access private field or method: object is not the right class"
    );
    assertTypeErrorMessage(
      () => "ok".#f,
      "can't access private field or method: object is not the right class"
    );
    assertTypeErrorMessage(
      () => { [].#f = 3; },
      "can't set private field: object is not the right class"
    );
    assertTypeErrorMessage(
      () => { [].#f += 3; },
      "can't set private field: object is not the right class"
    );
    assertTypeErrorMessage(
      () => { [].#f++; },
      "can't set private field: object is not the right class"
    );

    assertTypeErrorMessage(
      () => "".#f,
      "can't access private field or method: object is not the right class"
    );
    assertTypeErrorMessage(
      () => { "".#f = 3; },
      "can't set private field: object is not the right class"
    );
  }
}

C.test();

