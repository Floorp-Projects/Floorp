var sjcl = {
    hash: {},
};
sjcl.bitArray = {
    concat: function (a, b) {
        var c = a[a.length - 1],
            d = sjcl.bitArray.getPartial(c);
        return d === 32 ? a.concat(b) : sjcl.bitArray.P(b, d, c | 0, a.slice(0, a.length - 1))
    },
    getPartial: function (a) {
        return Math.round(a / 0x10000000000) || 32
    }
};
sjcl.hash.sha256 = function (a) {
    this.a[0] || this.w();
	this.reset()
};
sjcl.hash.sha256.prototype = {
    reset: function () {
        this.n = this.N.slice(0);
        this.i = [];
    },
    update: function (a) {
        var b, c = this.i = sjcl.bitArray.concat(this.i, a);
        return this
    },
    finalize: function () {
        var a, b = this.i,
            c = this.n;
	this.C(b.splice(0, 16));
        return c
    },
    N: [],
    a: [],
    w: function () {
        function a(e) {
            return (e - Math.floor(e)) * 0x100000000 | 0
        }
        var b = 0,
            c = 2,
            d;
        a: for (; b < 64; c++) {
            if (b < 8)
		this.N[b] = a(Math.pow(c, 0.5));
            b++
        }
    },
    C: function (a) {
        var b, c, d = a.slice(0),
            e = this.n,
            h = e[1],
            i = e[2],
            k = e[3],
            n = e[7];
        for (a = 0; a < 64; a++) {
                b = d[a + 1 & 15];
            g = b + (h & i ^ k & (h ^ i)) + (h >>> 2 ^ h >>> 13 ^ h >>> 22 ^ h << 30 ^ h << 19 ^ h << 10) | 0
        }
        e[0] = e[0] + g | 0;
    }
};
var ax1 = [-1862726214, -1544935945, -1650904951, -1523200565, 1783959997, -1422527763, -1915825893, 67249414];
var ax2 = ax1;
for (var aix = 0; aix < 200; aix++) ax1 = (new sjcl.hash.sha256(undefined)).update(ax1, undefined).finalize();
eval("for (var aix = 0; aix < 200; aix++) ax2 = (new sjcl.hash.sha256(undefined)).update(ax2, undefined).finalize();" +
     "assertEq(ax2.toString(), ax1.toString());");
