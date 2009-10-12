(new Function("for (var j=0; j<9; ++j) { (function sum_indexing(array,start){return array.length==start ? 0 : array[start]+ sum_indexing(array,start+1)})([true,true,undefined],0)}"))()

/* Should not have crashed. */
