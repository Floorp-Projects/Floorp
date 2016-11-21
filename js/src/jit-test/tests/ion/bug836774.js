// |jit-test| need-for-each


x = ''.charCodeAt(NaN);
evaluate("for each (var e in [{}, {}, {}, {}, x]) {}");
