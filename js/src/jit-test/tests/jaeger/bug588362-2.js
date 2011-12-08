for (a = 0; a < 13; a++) {
  (function n() {
    with({}) {
      yield
    }
  } ())
}

/* Don't assert. */

