// |jit-test| --ion-eager

if (helperThreadCount() === 0)
  quit(0);

// (1) Poison an element in the ionLazyLinkList with a builder whose
//     script is in a different compartment.
evaluate('offThreadCompileScript("var x = -1"); runOffThreadScript()',
         { global: newGlobal() });

// (2) Spam the ionLazyLinkList with pending builders until it pops off the one
//     for the other compartment's script.
for (var i = 0; i < 1000; ++i) {
  offThreadCompileScript('var x = ' + i);
  runOffThreadScript();
}
