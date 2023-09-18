// |jit-test| skip-if: typeof Intl === 'undefined'; skip-if: getBuildConfiguration("wasi")

// Clobbering `Symbol` should not impact creation of %Intl%.[[FallbackSymbol]]
globalThis.Symbol = null;

const IntlFallbackSymbol =
        Object.getOwnPropertySymbols(
                Intl.DateTimeFormat.call(
                        Object.create(Intl.DateTimeFormat.prototype)))[0];
