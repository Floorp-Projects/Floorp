// see bug 470795
var o = [{},{},{i:42}]
var i;
var s = [];
for (var j = 0; j < 3; ++j) {
    with (o[j]) {
	for (i = 0; i < 2; ++i) {
	    s.push(j);
            s.push(i);
	} 
    }
}
assertEq(s.join(), '0,0,0,1,1,0,1,1,2,0,2,1');

