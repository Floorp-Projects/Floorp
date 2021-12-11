setJitCompilerOption('ion.forceinlineCaches', 1);

function warmup(fun, input_array) {
    for (var index = 0; index < input_array.length; index++) {
        input = input_array[index];
        input_lhs = input[0];
        input_rhs = input[1];
        output    = input[2];
        for (var i = 0; i < 30; i++) {
            var y = fun(input_lhs, input_rhs);
            assertEq(y, output)
        }
    }
}


// Add: Int32 + Int32 Overflow
var funAdd1 = (a, b) => { return a + b; }
warmup(funAdd1, [[1,2, 3], [3,4, 7], [4294967295, 2, 4294967297]]);

// Add: Doubles
var funAdd2 = (a, b) => { return a + b; }
warmup(funAdd2, [[1.2, 2, 3.2], [3.5, 4, 7.5], [4294967295.1, 2, 4294967297.1]]);

// Add: Type Change Int32 -> Double
var funAdd3 = (a, b) => { return a + b; }
warmup(funAdd3, [[1, 2, 3], [3, 4, 7], [4294967295, 2, 4294967297], [1.2, 2, 3.2]]);

//Add: String Concat
var funAdd4 = (a, b) => { return a + b; }
warmup(funAdd4, [["","a","a"], ["ab","ba","abba"], ["123","456","123456"]])

function D(name) { this.name = name; }
D.prototype.toString = function stringify() {
    return this.name;
}
obj1 = new D('A');

// Add: String Object Concat
var funAdd4 = (a, b) => { return a + b; }
warmup(funAdd4, [["x", obj1, "xA"], [obj1, "bba", "Abba"]]);

// Add: Int32 Boolean
var funAdd5 = (a, b) => { return a + b; }
warmup(funAdd5, [[true, 10, 11], [false, 1, 1], [10, true, 11], [1, false, 1],
                 [2147483647, true, 2147483648],[true, 2147483647, 2147483648]]);

// Add: String Number Concat
var funAdd6 = (a, b) => { return a + b; }
warmup(funAdd6, [["x", 10, "x10"], [10, "bba", "10bba"], ["x", 1.2, "x1.2"],
                 [1.2, "bba", "1.2bba"]]);

// Add: String Boolean
var funAddStrBool = (a, b) => { return a + b; }
warmup(funAddStrBool, [[true, "true", "truetrue"], [false, "true", "falsetrue"],
  ["a string", true, "a stringtrue"]]);

// Sub Int32
var funSub1 = (a, b) => { return a - b; }
warmup(funSub1, [[7, 0, 7], [7, 8, -1], [4294967295, 2, 4294967293], [0,0,0]]);

// Sub Double
var funSub2 = (a, b) => { return a - b; }
warmup(funSub2, [[7.5, 0, 7.5], [7, 8.125, -1.125], [4294967295.3125, 2, 4294967293.3125], [NaN,10,NaN]]);

// Sub Int32 + Boolean
var funSub3 = (a, b) => { return a - b; }
warmup(funSub3, [[7, false, 7], [7, true, 6], [false, 1, -1], [true,1,0]]);



// Mul: Int32+  Int32 Overflow
var funMul1 = (a, b) => { return a * b; }
warmup(funMul1, [[1, 2, 2], [10, 21, 210], [3, 4, 12], [2147483649, 2, 4294967298], [1073741824, 1024, 1099511627776 ]]);

// Mul: Doubles
var funMul2 = (a, b) => { return a * b; }
warmup(funMul2, [[3/32, 32, 3], [16/64, 32, 8], [3.0, 1.0, 3], [-1, 0, -0], [0, -20, -0]]);

// Mul: Type change Int32 -> Double
var funMul3 = (a, b) => { return a * b; }
warmup(funMul3, [[1,2, 2], [10, 21, 210], [3, 4, 12], [63/32, 32, 63], [16/64, 32, 8]]);

// Mul: Boolean
var funMul1 = (a, b) => { return a * b; }
warmup(funMul1, [[1, true, 1], [10, false, 0], [false, 4, 0], [2147483640, true, 2147483640]]);

//Div: Int32
var funDiv1 = (a,b) => { return a / b;}
warmup(funDiv1,[[8, 4, 2], [16, 32, 0.5], [10, 0, Infinity], [0, 0, NaN]]);

//Div: Double
var funDiv2 = (a,b) => { return a / b;}
warmup(funDiv2, [[8.8, 4, 2.2], [16.8, 32, 0.525], [10, 0.5, 20]]);

//Div: Type change Int32 -> Double
var funDiv3 = (a,b) => { return a / b;}
warmup(funDiv1, [[8, 4, 2], [16, 32, 0.5], [10, 0, Infinity], [0, 0, NaN], [8.8, 4, 2.2],
                 [16.8, 32, 0.525], [10, 0.5, 20]]);

//Div: Boolean w/ Int32
var funDiv4 = (a,b) => { return a / b;}
warmup(funDiv4,[[8, true, 8], [true, 2, 0.5], [10, false, Infinity], [false, false, NaN]]);

//Mod: Int32
var funMod1 = (a,b) => {return a % b};
warmup(funMod1, [[8, 4, 0], [9, 4, 1], [-1, 2, -1], [4294967297, 2, 1],
                 [10, -3, 1]]);

//Mod: Double
var funMod2 = (a,b) => {return a % b};
warmup(funMod2, [[8.5, 1, 0.5], [9.5, 0.5, 0], [-0.03125, 0.0625, -0.03125], [1.64, 1.16, 0.48]]);

//Mod: Type change Int32 -> Double
var funMod3 = (a,b) => {return a % b};
warmup(funMod3, [[10, 0, NaN], [8, 4, 0], [9, 4, 1], [-1, 2, -1], [4294967297, 2, 1],
                 [8.5, 1, 0.5], [9.5, 0.5, 0], [-0.03125, 0.0625, -0.03125],
                 [1.64, 1.16, 0.48]]);

//Mod: Boolean w/ Int32
var funMod4 = (a,b) => {return a % b};
warmup(funMod4, [[10, false, NaN], [8, true, 0], [false, 4, 0], [true, 2, 1]]);

//Pow: Int32
var funPow1 = (a,b) => {return a ** b};
warmup(funPow1, [[8, 4, 4096], [9, 4, 6561], [-1, 2, 1], [2, -10000, 0],
               [-3, 3, -27]]);

//Pow: Double
var funPow2 = (a,b) => {return a ** b};
warmup(funPow2, [[8.5, 1, 8.5], [16, 0.5, 4], [4.5, 5, 1845.28125], [18.0625, 0.5, 4.25],
                 [4, -1, 0.25]]);

//Pow: Type change Int32 -> Double
var funPow3 = (a,b) => {return a ** b};
warmup(funPow3, [[10, 0, 1], [8, 4, 4096], [9, 4, 6561], [-1, 2, 1], [2, -10000, 0],
                 [8.5, 1, 8.5], [16, 0.5, 4], [4.5, 5, 1845.28125], [18.0625, 0.5, 4.25],
                 [4, -1, 0.25]]);

//Pow: Boolean w/ Int32
var funPow4 = (a,b) => {return a ** b};
warmup(funPow4, [[10, 2, 100], [8, true, 8], [false, 4, 0], [true, 2, 1]]);

//BitOr Int32
var funBitOr1 = (a, b) => { return a | b; }
warmup(funBitOr1, [[1, 1, 1], [8, 1, 9], [0, 1233, 1233], [5, 0, 5],
                   [4294967295, 123231, -1], [2147483647, 1243524, 2147483647]]);

//BitOr  Boolean w/ Int32
var funBitOr3 = (a, b) => { return a | b; }
warmup(funBitOr3, [[1, true, 1], [8, true, 9], [false, 1233, 1233], [5, false, 5]]);

//BitOr  null w/ Int32
var funBitOr4 = (a, b) => { return a | b; }
warmup(funBitOr4, [[1, null, 1], [8, null, 8], [null, 1233, 1233], [5, null, 5]]);

//BitOr  undefined w/ Int32
var funBitOr5 = (a, b) => { return a | b; }
warmup(funBitOr5, [[1, void 0, 1], [8, void 0, 8], [void 0, 1233, 1233], [5, void 0, 5]]);

//BitXOr Int32
var funBitXOr1 = (a, b) => { return a ^ b; }
warmup(funBitXOr1, [[1, 1, 0], [5, 1, 4], [63, 31, 32], [4294967295, 2147483647, -2147483648 ] ]);

//BitXOr Int32
var funBitXOr2 = (a, b) => { return a ^ b; }
warmup(funBitXOr2, [[1, true, 0], [5, true, 4], [5, false, 5], [false, 1, 1]]);

//BitXOr Double+int32
var funBitXOr3 = (a, b) => { return a ^ b; }
warmup(funBitXOr3, [[1.3, 1, 0], [5, 1.4, 4], [63.1, 31, 32], [4294967295.9, 2147483647, -2147483648 ] ]);

//BitXOr Number Number
var funBitXOr4 = (a, b) => { return a ^ b; }
warmup(funBitXOr4, [[54772703898, 2890608493, 1828589047],
                    [-54772703898, 2890608493, -1828589045],
                    [18446744073709551615, 54772703898, -1061870950], //UINT64 Max
                    [-18446744073709551615, 54772703898, -1061870950],
                    [4294967295, -1, 0]]);

//BitXOr null+int32
var funBitXOr5 = (a, b) => { return a ^ b; }
warmup(funBitXOr5, [[1, null, 1], [5, null, 5], [5, null, 5], [null, 1, 1]]);

//BitXOr undefined+int32
var funBitXOr6 = (a, b) => { return a ^ b; }
warmup(funBitXOr6, [[1, void 0, 1], [5, void 0, 5], [5, void 0, 5], [void 0, 1, 1]]);

//BitAnd Int32
var funBitAnd1 = (a, b) => { return a & b; }
warmup(funBitAnd1, [[1,1,1], [5,1,1], [63,31,31], [4294967295,2147483647,2147483647],
                    [-2,10,10], [-15,-2,-16], [4,128,0]]);
//BitAnd Double w/ Int32
var funBitAnd2 = (a, b) => { return a & b; }
warmup(funBitAnd2, [[1.2 ,1, 1], [5, 1.4, 1], [63,31.99,31],
                    [4294967295.98,2147483647,2147483647],
                    [-2.9, 10, 10], [-15,-2.9,-16], [4,128.01,0]]);

//BitAnd Int32 + Boolean
var funBitAnd1 = (a, b) => { return a & b; }
warmup(funBitAnd1, [[1,true,1], [5,false,0], [true, 6, 0], [false, 12, 0]]);

//BitAnd Int32 + null
var funBitAnd4 = (a, b) => { return a & b; }
warmup(funBitAnd4, [[1, null, 0], [5, null, 0], [null, 6, 0], [null, 12, 0]]);

//BitAnd Int32 + undefined
var funBitAnd5 = (a, b) => { return a & b; }
warmup(funBitAnd5, [[1, void 0, 0], [5, void 0, 0], [void 0, 6, 0], [void 0, 12, 0]]);

//Lsh Int32
var funLsh1 = (a, b) => { return a << b; }
warmup(funLsh1, [[5,1,10], [1,1,2], [63,31,-2147483648],
                 [4294967295,2147483647,-2147483648], [-2,10,-2048], [-15,-2,1073741824],
                 [4,128,4], [1,10,1024], [1024,2,4096]]);

//Lsh Boolean w/ Int32
var funLsh2 = (a, b) => { return a << b; }
warmup(funLsh2, [[5,true,10], [true,1,2], [63,false,63], [false, 12, 0]]);

//Lsh Number Number
var funLsh3 = (a, b) => { return a << b; }
warmup(funLsh3, [[54772703898, 1, -2123741900],[2147483658, 0, -2147483638]]);

//Lsh Boolean w/ null
var funLsh4 = (a, b) => { return a << b; }
warmup(funLsh4, [[5, null, 5], [null, 1, 0], [63, null, 63], [null, 12, 0]]);

//Lsh Boolean w/ undefined
var funLsh5 = (a, b) => { return a << b; }
warmup(funLsh5, [[5, void 0, 5], [void 0, 1, 0], [63, void 0, 63], [void 0, 12, 0]]);

//Rsh Int32
var funRsh1 = (a, b) => { return a >> b; }
warmup(funRsh1, [[1,1,0], [5,1,2], [63,31,0], [4294967295,2147483647,-1], [-2,10,-1],
                 [-15,-2,-1], [4,128,4], [1,10,0], [1024,2,256]]);

//Rsh Int32
var funRsh2 = (a, b) => { return a >> b; }
warmup(funRsh2, [[true,1,0], [1,true,0], [false, 3, 0], [3, false, 3]]);

//Rsh Number Number
var funRsh3 = (a, b) => { return a >> b; }
warmup(funRsh3, [[54772703898, 11, -518492 ], [2147483658, 0, -2147483638]]);

//Rsh Int32 null
var funRsh4 = (a, b) => { return a >> b; }
warmup(funRsh4, [[null, 1, 0], [1, null, 1], [null, 3, 0], [3, null, 3]]);

//Rsh Int32 undefined
var funRsh5 = (a, b) => { return a >> b; }
warmup(funRsh5, [[void 0, 1, 0], [1, void 0, 1], [void 0, 3, 0], [3, void 0, 3]]);

//URsh Int32
var funURsh1 = (a, b) => { return a >>> b; }
warmup(funURsh1, [[1,1,0], [5,1,2], [63,31,0], [4294967295,2147483647,1], [-2,10,4194303],
                  [-15,-2,3], [4,128,4], [1,10,0], [1024,2,256], [0, -6, 0], [0, 6, 0],
                  [0x55005500, 2, 0x15401540]]);

//URsh Boolean Int32
var funURsh2 = (a, b) => { return a >>> b; }
warmup(funURsh2, [[true,1,0], [5,true,2], [63,false,63], [false, 20, 0]]);

//URsh Int32
var funURsh3 = (a, b) => { return a >>> b; }
warmup(funURsh3, [[4294967295, 0, 4294967295]]);

//URsh Number Number
var funURsh4 = (a, b) => { return a >>> b; }
warmup(funURsh4, [[54772703898, 11, 1578660], [2147483658, 11, 1048576],
                  [4294967295, 0, 4294967295]]);

//URsh null Int32
var funURsh5 = (a, b) => { return a >>> b; }
warmup(funURsh5, [[null, 1, 0], [5, null, 5], [63, null, 63], [null, 20, 0]]);

//URsh undefined Int32
var funURsh6 = (a, b) => { return a >>> b; }
warmup(funURsh6, [[void 0, 1, 0], [5, void 0, 5], [63, void 0, 63], [void 0, 20, 0]]);


// Other Test cases that Have been useful:
for (var k=0; k < 30; k++) {
    A="01234567";
    res =""
    for (var i = 0; i < 8; ++i) {
        var v = A[7 - i];
        res+=v;
    }
    assertEq(res, "76543210");
}

// Begin OOM testing:
if (!('oomTest' in this))
    quit();

// Add: String Number Concat OOM test
var addOom = (a, b) => { return a + b; }

function generate_digits(prefix, start) {
    digits = []
    number = ""+start+".25";
    for (var i = 1; i < 7; i++) {
        number = i + number;
        digits.push([prefix, Number(number), prefix + number]);
    }
    return digits;
}

// Trying to defeat dtoacache: Warm the IC with one set of digits, then actually oomTest
// using another set.
var warmup_digits = generate_digits("x", 1);
var test_digits = generate_digits("x", 2);

function ot(digits) {
    warmup(addOom, digits);
}

// Ensure ICs are warmed
ot(warmup_digits);

oomTest(() => { ot(test_digits); });