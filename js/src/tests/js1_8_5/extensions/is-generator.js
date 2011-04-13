/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * Bug 648355: Function.prototype.isGenerator
 */

reportCompare(true, "isGenerator" in Function.prototype, "Function.prototype.isGenerator present");

reportCompare(false, (function(){}).isGenerator(), "lambda is not a generator fn");
reportCompare(false, Function.prototype.toString.isGenerator(), "native is not a generator fn");
reportCompare(false, (function(){with(obj){}}).isGenerator(), "heavyweight is not a generator fn");
reportCompare(false, (function(){obj}).isGenerator(), "upvar function is not a generator fn");

reportCompare(true, (function(){yield}).isGenerator(), "simple generator fn");
reportCompare(true, (function(){with(obj){yield}}).isGenerator(), "heavyweight generator fn");
reportCompare(true, (function(){yield; obj}).isGenerator(), "upvar generator fn");

reportCompare(false, Function.prototype.isGenerator.call(42), "number is not a generator fn");
reportCompare(false, Function.prototype.isGenerator.call({}), "vanilla object is not a generator fn");
reportCompare(false, Function.prototype.isGenerator.call(new Date()), "date object is not a generator fn");
