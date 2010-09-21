for (a = 0; a < HOTLOOP + 5; a++) {
  (function n() {
    with({}) {
      yield
    }
  } ())
}

/* Don't assert. */

