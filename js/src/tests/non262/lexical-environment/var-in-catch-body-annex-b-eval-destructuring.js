// |reftest| skip-if(!xulRuntime.shell)

assertThrowsInstanceOf(() => evaluate(`try { throw {} } catch ({e}) { var e; }`), SyntaxError);
assertThrowsInstanceOf(() => evaluate(`try { throw {} } catch ({e}) { eval('var e'); }`), SyntaxError);

assertThrowsInstanceOf(() => new Function(`try { throw {} } catch ({e}) { var e; }`), SyntaxError);
assertThrowsInstanceOf(new Function(`try { throw {} } catch ({e}) { eval('var e'); }`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
