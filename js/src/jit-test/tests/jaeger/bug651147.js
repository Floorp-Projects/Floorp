var BUGNUMBER = 96284;
var BUGNUMBER = "410725";
function iteratorToArray(iterator) {
    var result = [];
    for (var i in iterator) BUGNUMBER[result.length];
}
try { obj = { a: 1, }('["a", "b"]', iteratorToArray(), 'uneval(iteratorToArray(new Iterator(obj,true)))'); } catch (e) { }
