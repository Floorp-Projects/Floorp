// |jit-test| error: TypeError

for (l in [Math.h.h.h.h.h.I.h.h.h.h.h.h.h.I.h.I]) {
  t.x
}

/* Don't crash or assert. */

