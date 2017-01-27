Note:

While conceptually similar to the tests for Web Authentication (dom/webauthn),
the tests for U2F require an iframe while `window.u2f` remains hidden behind a
preference, though WebAuthn does not. The reason is that the `window` object
doesn't mutate upon a call by SpecialPowers.setPrefEnv() the way that the
`navigator` objects do, rather you have to load a different page with a different
`window` object for the preference change to be honored.
