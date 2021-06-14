// |jit-test| skip-if: typeof Intl === 'undefined'

for (i = 0; i < 10000; i++, bailAfter(10)) {
  if (hasOwnProperty) {
    Intl.DateTimeFormat(0, {}).format();
  }
}
