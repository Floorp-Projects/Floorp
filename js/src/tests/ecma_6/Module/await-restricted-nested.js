// |reftest| error:SyntaxError module

// 'await' is always a keyword when parsing modules.
function f() {
    await;
}
