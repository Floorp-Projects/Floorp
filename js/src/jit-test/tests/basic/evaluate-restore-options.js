// Bug 791157: the shell 'evaluate' function should properly restore the
// context's options.

try {
    evaluate('%', {noScriptRval: true});
} catch(e) {}
new Function("");

try {
  evaluate('new Function("");', {noScriptRval: true});
} catch (e) {}
