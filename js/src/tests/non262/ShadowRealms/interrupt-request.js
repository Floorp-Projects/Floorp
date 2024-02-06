// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

// Request interrupt from shadow realm evaluation.

expectExitCode(6);

new ShadowRealm().evaluate(`
  (interruptIf => {
    interruptIf(true);

    for (;;) {}  // Wait for interrupt.
  });
`)(interruptIf);

if (typeof reportCompare === 'function')
  reportCompare(true, true);
