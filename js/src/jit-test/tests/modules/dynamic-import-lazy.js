// |jit-test| error: SyntaxError

function lazyilyParsedFunction()
{
    return import("/module1.js");
}
