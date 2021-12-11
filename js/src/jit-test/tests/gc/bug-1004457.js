var argObj = (function () { return arguments })();
gczeal(4);
delete argObj.callee;
