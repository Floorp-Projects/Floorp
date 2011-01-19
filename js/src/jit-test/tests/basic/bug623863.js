// Contributor: Christian Holler <decoder@own-hero.net>
if (typeof gczeal === 'function') gczeal(2);
Function.prototype.prototype = function() { return 42; };
try { foo(Function); } catch (e) { }
Function.prototype.prototype = function() { return 42; };
