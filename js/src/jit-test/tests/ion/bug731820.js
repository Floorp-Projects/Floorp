function BigInteger(a, b, c) {
    this.array = new Array();
    if (a != null) {
	var this_array = this.array;
	this.t = 0;
	var i = a.length;
	while (--i >= 0) {
	    this_array[this.t++] = 0;
	}
    }
}
function bnpCopyTo(r, g) {
    var this_array = g.array;
    for (var i = g.t - 1; i >= 0; --i) 
	r.array[i] = g.array[i];
    r.t = g.t;
}
function montConvert(x) {
    var r = new BigInteger(null);
    r.t = 56;
    return r;
}
var ba = new Array();
a = new BigInteger(ba);
new BigInteger("afdsafdsafdsaafdsafdsafdsafdsafdsafdsafdsafdsafdsafdsfds");
g = montConvert(a);
var r = new BigInteger(null);
bnpCopyTo(r, g);

